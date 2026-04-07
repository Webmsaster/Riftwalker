#!/usr/bin/env python3
"""Riftwalker Music Generator v3 — SoundFont-based rendering for professional audio.

Renders existing MIDI compositions through FluidSynth with GM SoundFonts,
producing dramatically better audio than the v2 sine-wave synthesizer.

Requirements:
    pip install pyfluidsynth mido numpy scipy
    Download a GM SoundFont (e.g., FluidR3_GM.sf2) and place in assets/music/

Usage:
    python tools/gen_music_v3.py                    # Render all tracks
    python tools/gen_music_v3.py --track zone1      # Render single track
    python tools/gen_music_v3.py --soundfont path    # Custom SoundFont path
    python tools/gen_music_v3.py --fallback          # Use v2 synth as fallback
"""

import os
import sys
import glob
import struct
import wave
import subprocess

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(SCRIPT_DIR)
MUSIC_DIR = os.path.join(PROJECT_DIR, "assets", "music")
MIDI_DIR = os.path.join(MUSIC_DIR, "midi")

# SoundFont search paths (first found is used)
SOUNDFONT_SEARCH = [
    os.path.join(MUSIC_DIR, "FluidR3_GM.sf2"),
    os.path.join(MUSIC_DIR, "GeneralUser_GS.sf2"),
    os.path.join(MUSIC_DIR, "soundfont.sf2"),
    os.path.join(PROJECT_DIR, "FluidR3_GM.sf2"),
    r"C:\soundfonts\FluidR3_GM.sf2",
    r"C:\Program Files\FluidSynth\share\soundfonts\default.sf2",
    "/usr/share/sounds/sf2/FluidR3_GM.sf2",
    "/usr/share/soundfonts/FluidR3_GM.sf2",
]

TRACKS = [
    "menu_theme", "zone1", "zone2", "zone3",
    "boss_theme", "victory", "gameover"
]

def find_soundfont(custom_path=None):
    """Find a GM SoundFont file."""
    if custom_path and os.path.isfile(custom_path):
        return custom_path
    for path in SOUNDFONT_SEARCH:
        if os.path.isfile(path):
            return path
    return None

def render_with_fluidsynth(midi_path, wav_path, soundfont, gain=0.6):
    """Render MIDI to WAV using FluidSynth CLI."""
    # Try fluidsynth CLI first (most reliable)
    for cmd in ["fluidsynth", r"C:\Program Files\FluidSynth\bin\fluidsynth.exe"]:
        try:
            result = subprocess.run([
                cmd,
                "-ni",            # No interactive mode
                "-g", str(gain),  # Gain
                "-R", "1",        # Reverb on
                "-C", "1",        # Chorus on
                "-F", wav_path,   # Output WAV
                soundfont,
                midi_path
            ], capture_output=True, text=True, timeout=120)
            if result.returncode == 0:
                return True
            print(f"    FluidSynth CLI warning: {result.stderr[:200]}")
        except (FileNotFoundError, subprocess.TimeoutExpired):
            continue
    return False

def render_with_pyfluidsynth(midi_path, wav_path, soundfont, gain=0.6):
    """Render MIDI to WAV using pyfluidsynth Python bindings."""
    try:
        import fluidsynth
        import mido
        import numpy as np
    except ImportError as e:
        print(f"    Missing dependency: {e}")
        return False

    sr = 44100
    fs = fluidsynth.Synth(samplerate=float(sr), gain=gain)
    fs.setting('synth.reverb.active', 1)
    fs.setting('synth.chorus.active', 1)

    sfid = fs.sfload(soundfont)
    if sfid < 0:
        print(f"    Failed to load SoundFont: {soundfont}")
        fs.delete()
        return False

    # Select default programs for all channels
    for ch in range(16):
        fs.program_select(ch, sfid, 0, 0)

    mid = mido.MidiFile(midi_path)
    total_time = mid.length + 2.0  # Extra for reverb tail
    total_samples = int(total_time * sr)

    # Render MIDI events through FluidSynth
    audio_L = np.zeros(total_samples, dtype=np.float32)
    audio_R = np.zeros(total_samples, dtype=np.float32)

    sample_pos = 0
    for msg in mid:
        if msg.time > 0:
            n_samples = int(msg.time * sr)
            if n_samples > 0 and sample_pos + n_samples <= total_samples:
                chunk = fs.get_samples(n_samples)
                # chunk is interleaved stereo int16
                chunk = np.frombuffer(chunk, dtype=np.int16).astype(np.float32) / 32768.0
                L = chunk[0::2]
                R = chunk[1::2]
                end = min(sample_pos + len(L), total_samples)
                audio_L[sample_pos:end] = L[:end - sample_pos]
                audio_R[sample_pos:end] = R[:end - sample_pos]
                sample_pos = end

        if msg.is_meta:
            continue

        if msg.type == 'program_change':
            fs.program_select(msg.channel, sfid, 0, msg.program)
        elif msg.type == 'note_on':
            fs.noteon(msg.channel, msg.note, msg.velocity)
        elif msg.type == 'note_off':
            fs.noteoff(msg.channel, msg.note)
        elif msg.type == 'control_change':
            fs.cc(msg.channel, msg.control, msg.value)
        elif msg.type == 'pitchwheel':
            fs.pitch_bend(msg.channel, msg.pitch + 8192)

    # Render remaining reverb tail
    remaining = total_samples - sample_pos
    if remaining > 0:
        chunk = fs.get_samples(remaining)
        chunk = np.frombuffer(chunk, dtype=np.int16).astype(np.float32) / 32768.0
        L = chunk[0::2]
        R = chunk[1::2]
        end = min(sample_pos + len(L), total_samples)
        audio_L[sample_pos:end] = L[:end - sample_pos]
        audio_R[sample_pos:end] = R[:end - sample_pos]

    fs.delete()

    # Normalize
    peak = max(np.max(np.abs(audio_L)), np.max(np.abs(audio_R)))
    if peak > 0.01:
        norm = 0.85 / peak
        audio_L *= norm
        audio_R *= norm

    # Write WAV
    stereo = np.column_stack((audio_L, audio_R))
    int16_data = (stereo * 32767).astype(np.int16)

    with wave.open(wav_path, 'w') as wf:
        wf.setnchannels(2)
        wf.setsampwidth(2)
        wf.setframerate(sr)
        wf.writeframes(int16_data.tobytes())

    return True

def wav_to_ogg(wav_path, ogg_path, quality=6):
    """Convert WAV to OGG using ffmpeg."""
    for cmd in ["ffmpeg", r"C:\ffmpeg\bin\ffmpeg.exe"]:
        try:
            result = subprocess.run([
                cmd, "-y", "-i", wav_path,
                "-c:a", "libvorbis", "-q:a", str(quality),
                ogg_path
            ], capture_output=True, text=True, timeout=60)
            if result.returncode == 0:
                return True
        except (FileNotFoundError, subprocess.TimeoutExpired):
            continue
    return False

def render_track(track_name, soundfont, use_fallback=False):
    """Render a single track."""
    midi_path = os.path.join(MIDI_DIR, f"{track_name}.mid")
    wav_path = os.path.join(MUSIC_DIR, f"{track_name}.wav")
    ogg_path = os.path.join(MUSIC_DIR, f"{track_name}.ogg")

    if not os.path.isfile(midi_path):
        print(f"  [SKIP] {track_name}: No MIDI file at {midi_path}")
        return False

    print(f"  Rendering {track_name}...")

    # Try FluidSynth CLI first, then Python bindings
    rendered = False
    if soundfont:
        rendered = render_with_fluidsynth(midi_path, wav_path, soundfont)
        if not rendered:
            print(f"    FluidSynth CLI failed, trying Python bindings...")
            rendered = render_with_pyfluidsynth(midi_path, wav_path, soundfont)

    if not rendered:
        if use_fallback:
            print(f"    Falling back to v2 synth...")
            # Import the v2 synth from gen_music.py
            sys.path.insert(0, SCRIPT_DIR)
            try:
                from gen_music import synth_render_v2
                synth_render_v2(midi_path, wav_path)
                rendered = True
            except Exception as e:
                print(f"    v2 synth failed: {e}")
        else:
            print(f"    [FAIL] No renderer available. Install FluidSynth or use --fallback")
            return False

    if not rendered:
        return False

    # Convert to OGG
    if wav_to_ogg(wav_path, ogg_path):
        wav_size = os.path.getsize(wav_path) / 1024 / 1024
        ogg_size = os.path.getsize(ogg_path) / 1024 / 1024
        print(f"    OK: {wav_size:.1f}MB WAV, {ogg_size:.1f}MB OGG")
        return True
    else:
        print(f"    WAV rendered but OGG conversion failed (ffmpeg not found?)")
        return True  # WAV still usable

def main():
    import argparse
    parser = argparse.ArgumentParser(description="Riftwalker Music Generator v3")
    parser.add_argument("--track", help="Render single track (e.g., zone1)")
    parser.add_argument("--soundfont", help="Path to GM SoundFont (.sf2)")
    parser.add_argument("--fallback", action="store_true",
                        help="Fall back to v2 synth if FluidSynth unavailable")
    parser.add_argument("--list", action="store_true", help="List available tracks")
    args = parser.parse_args()

    if args.list:
        print("Available tracks:")
        for t in TRACKS:
            midi = os.path.join(MIDI_DIR, f"{t}.mid")
            exists = "[OK]" if os.path.isfile(midi) else "[MISSING]"
            print(f"  {exists} {t}")
        return

    soundfont = find_soundfont(args.soundfont)
    if soundfont:
        print(f"SoundFont: {soundfont}")
    else:
        print("No SoundFont found. Searching:")
        for p in SOUNDFONT_SEARCH[:4]:
            print(f"  {p}")
        if not args.fallback:
            print("\nTo get professional music:")
            print("  1. Download FluidR3_GM.sf2 (141MB) from:")
            print("     https://member.keymusician.com/Member/FluidR3_GM/index.html")
            print("  2. Place in assets/music/FluidR3_GM.sf2")
            print("  3. Run this script again")
            print("\nOr use --fallback for v2 synth rendering")
            return

    tracks = [args.track] if args.track else TRACKS
    success = 0
    for t in tracks:
        if render_track(t, soundfont, args.fallback):
            success += 1

    print(f"\nDone: {success}/{len(tracks)} tracks rendered")

if __name__ == "__main__":
    main()
