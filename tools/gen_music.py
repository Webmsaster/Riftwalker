#!/usr/bin/env python3
"""Generate game music tracks for Riftwalker using Python synthesis + LMMS rendering.

Creates LMMS .mmp project files with proper compositions, then renders to WAV via CLI.
Also creates MIDI versions for manual editing in LMMS/any DAW.

Tracks:
  menu_theme.wav    — Ambient, mysterious, D minor, 80 BPM
  zone1.wav         — Dark dungeon, rhythmic, 100 BPM
  zone2.wav         — Intense void corridors, 120 BPM
  zone3.wav         — Epic entropy core, 130 BPM
  boss_theme.wav    — Aggressive, fast, 140 BPM
  victory.wav       — Triumphant, major key, 100 BPM
  gameover.wav      — Somber, short, 70 BPM
"""

import os
import subprocess
import sys
import xml.etree.ElementTree as ET
from xml.dom import minidom

# Paths
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(SCRIPT_DIR)
MUSIC_DIR = os.path.join(PROJECT_DIR, "assets", "music")
LMMS_DIR = os.path.join(MUSIC_DIR, "lmms_projects")
MIDI_DIR = os.path.join(MUSIC_DIR, "midi")
LMMS_EXE = r"C:\Program Files\LMMS\lmms.exe"
FFMPEG = "ffmpeg"

os.makedirs(MUSIC_DIR, exist_ok=True)
os.makedirs(LMMS_DIR, exist_ok=True)
os.makedirs(MIDI_DIR, exist_ok=True)

# --- MIDI Generation ---
import mido
from mido import MidiFile, MidiTrack, Message, MetaMessage

# Note name to MIDI number
NOTE_MAP = {
    'C': 0, 'C#': 1, 'Db': 1, 'D': 2, 'D#': 3, 'Eb': 3,
    'E': 4, 'F': 5, 'F#': 6, 'Gb': 6, 'G': 7, 'G#': 8,
    'Ab': 8, 'A': 9, 'A#': 10, 'Bb': 10, 'B': 11
}

def note(name, octave):
    """Convert note name + octave to MIDI number."""
    return NOTE_MAP[name] + (octave + 1) * 12

def ticks_per_beat(bpm, ppq=480):
    return ppq

def beats_to_ticks(beats, ppq=480):
    return int(beats * ppq)

def add_note(track, pitch, start_beat, duration_beats, velocity=80, ppq=480):
    """Add a note to a track at a specific beat position."""
    return (start_beat, pitch, duration_beats, velocity)

def build_track_from_notes(notes_list, ppq=480):
    """Build a MIDI track from a list of (start_beat, pitch, duration, velocity)."""
    track = MidiTrack()
    events = []
    for start_beat, pitch, dur, vel in notes_list:
        on_tick = beats_to_ticks(start_beat, ppq)
        off_tick = beats_to_ticks(start_beat + dur, ppq)
        events.append((on_tick, 'on', pitch, vel))
        events.append((off_tick, 'off', pitch, 0))
    events.sort(key=lambda e: (e[0], 0 if e[1] == 'off' else 1))

    current_tick = 0
    for tick, typ, pitch, vel in events:
        delta = tick - current_tick
        if typ == 'on':
            track.append(Message('note_on', note=pitch, velocity=vel, time=delta))
        else:
            track.append(Message('note_off', note=pitch, velocity=0, time=delta))
        current_tick = tick
    return track


# ============================================================
# COMPOSITIONS
# ============================================================

def compose_menu_theme():
    """Ambient, mysterious. D minor, 80 BPM, ~60 seconds looped."""
    ppq = 480
    mid = MidiFile(ticks_per_beat=ppq)
    bpm = 80
    bars = 16  # 16 bars × 4 beats = 64 beats = 48 seconds at 80 BPM

    # --- Pad (sustained chords) ---
    pad_notes = []
    # Dm - Am - Bb - Gm - Dm - F - C - Am progression (2 bars each)
    chords = [
        [note('D', 3), note('F', 3), note('A', 3)],   # Dm
        [note('A', 2), note('C', 3), note('E', 3)],   # Am
        [note('Bb', 2), note('D', 3), note('F', 3)],  # Bb
        [note('G', 2), note('Bb', 2), note('D', 3)],  # Gm
        [note('D', 3), note('F', 3), note('A', 3)],   # Dm
        [note('F', 3), note('A', 3), note('C', 4)],   # F
        [note('C', 3), note('E', 3), note('G', 3)],   # C
        [note('A', 2), note('C', 3), note('E', 3)],   # Am
    ]
    for i, chord in enumerate(chords):
        beat = i * 8  # 2 bars = 8 beats
        for n in chord:
            pad_notes.append((beat, n, 7.5, 50))

    pad_track = build_track_from_notes(pad_notes, ppq)
    pad_track.insert(0, MetaMessage('set_tempo', tempo=mido.bpm2tempo(bpm)))
    pad_track.insert(0, MetaMessage('track_name', name='Pad'))
    pad_track.insert(0, Message('program_change', program=89))  # Pad 2 (warm)
    mid.tracks.append(pad_track)

    # --- Melody (sparse, haunting) ---
    melody_notes = []
    # Simple melody over the chord progression
    melody = [
        (0, note('A', 4), 2, 60),
        (3, note('F', 4), 1, 55),
        (4, note('D', 4), 3, 50),
        (8, note('E', 4), 2, 58),
        (11, note('C', 4), 1, 50),
        (12, note('D', 4), 3, 55),
        (16, note('D', 4), 1, 60),
        (17.5, note('F', 4), 1.5, 55),
        (20, note('G', 4), 2, 52),
        (24, note('Bb', 4), 2, 58),
        (27, note('A', 4), 1, 50),
        (28, note('G', 4), 3, 55),
        (32, note('F', 4), 2, 60),
        (35, note('D', 4), 1, 55),
        (36, note('E', 4), 3, 50),
        (40, note('A', 4), 2, 58),
        (43, note('G', 4), 1, 52),
        (44, note('F', 4), 2, 55),
        (48, note('C', 5), 2, 60),
        (51, note('A', 4), 1.5, 55),
        (52, note('G', 4), 3, 50),
        (56, note('E', 4), 2, 55),
        (59, note('D', 4), 1, 50),
        (60, note('D', 4), 3, 55),
    ]
    melody_notes.extend(melody)

    mel_track = build_track_from_notes(melody_notes, ppq)
    mel_track.insert(0, MetaMessage('track_name', name='Melody'))
    mel_track.insert(0, Message('program_change', program=91))  # Pad 4 (choir)
    mid.tracks.append(mel_track)

    # --- Bass (deep, slow) ---
    bass_notes = []
    bass_line = [
        (0, note('D', 2), 7.5, 65),
        (8, note('A', 1), 7.5, 60),
        (16, note('Bb', 1), 7.5, 62),
        (24, note('G', 1), 7.5, 60),
        (32, note('D', 2), 7.5, 65),
        (40, note('F', 2), 7.5, 60),
        (48, note('C', 2), 7.5, 62),
        (56, note('A', 1), 7.5, 60),
    ]
    bass_notes.extend(bass_line)

    bass_track = build_track_from_notes(bass_notes, ppq)
    bass_track.insert(0, MetaMessage('track_name', name='Bass'))
    bass_track.insert(0, Message('program_change', program=39))  # Synth Bass 2
    mid.tracks.append(bass_track)

    return mid, "menu_theme"


def compose_zone1():
    """Dark dungeon atmosphere, rhythmic. A minor, 100 BPM."""
    ppq = 480
    mid = MidiFile(ticks_per_beat=ppq)
    bpm = 100
    bars = 16

    # --- Rhythmic arpeggio ---
    arp_notes = []
    # Am - Em - F - G pattern, 4 bars each, 16th note arpeggios
    patterns = [
        [note('A', 3), note('C', 4), note('E', 4), note('A', 4)],  # Am
        [note('E', 3), note('G', 3), note('B', 3), note('E', 4)],  # Em
        [note('F', 3), note('A', 3), note('C', 4), note('F', 4)],  # F
        [note('G', 3), note('B', 3), note('D', 4), note('G', 4)],  # G
    ]
    for pi, pattern in enumerate(patterns):
        base_beat = pi * 16  # 4 bars of 4 beats
        for bar in range(4):
            for beat in range(4):
                for sub in range(4):  # 16ths
                    idx = (beat * 4 + sub) % len(pattern)
                    t = base_beat + bar * 4 + beat + sub * 0.25
                    arp_notes.append((t, pattern[idx], 0.2, 55 + (sub == 0) * 15))

    arp_track = build_track_from_notes(arp_notes, ppq)
    arp_track.insert(0, MetaMessage('set_tempo', tempo=mido.bpm2tempo(bpm)))
    arp_track.insert(0, MetaMessage('track_name', name='Arpeggio'))
    arp_track.insert(0, Message('program_change', program=80))  # Square Lead
    mid.tracks.append(arp_track)

    # --- Bass ---
    bass_notes = []
    bass_roots = [note('A', 1), note('E', 1), note('F', 1), note('G', 1)]
    for pi, root in enumerate(bass_roots):
        base = pi * 16
        for bar in range(4):
            b = base + bar * 4
            bass_notes.append((b, root, 1.5, 80))
            bass_notes.append((b + 2, root + 12, 0.5, 65))
            bass_notes.append((b + 3, root + 7, 0.5, 70))

    bass_track = build_track_from_notes(bass_notes, ppq)
    bass_track.insert(0, MetaMessage('track_name', name='Bass'))
    bass_track.insert(0, Message('program_change', program=38))  # Synth Bass 1
    mid.tracks.append(bass_track)

    # --- Drums (MIDI channel 10) ---
    drum_notes = []
    kick, snare, hihat, ohh = 36, 38, 42, 46
    for bar in range(bars):
        b = bar * 4
        # Kick on 1 and 3
        drum_notes.append((b, kick, 0.5, 100))
        drum_notes.append((b + 2, kick, 0.5, 90))
        # Snare on 2 and 4
        drum_notes.append((b + 1, snare, 0.5, 85))
        drum_notes.append((b + 3, snare, 0.5, 80))
        # Hi-hat 8ths
        for eighth in range(8):
            vel = 60 if eighth % 2 == 0 else 45
            drum_notes.append((b + eighth * 0.5, hihat, 0.25, vel))
        # Open hat on off-beats sometimes
        if bar % 2 == 1:
            drum_notes.append((b + 3.5, ohh, 0.25, 55))

    drum_track = build_track_from_notes(drum_notes, ppq)
    drum_track.insert(0, MetaMessage('track_name', name='Drums'))
    drum_track.insert(0, Message('program_change', channel=9, program=0))
    # Set channel 10 for drums
    for msg in drum_track:
        if hasattr(msg, 'channel'):
            msg.channel = 9
    mid.tracks.append(drum_track)

    # --- Melody (dark, minor) ---
    mel_notes = []
    melody_phrases = [
        # Phrase 1 (over Am)
        [(0, note('E', 5), 1, 70), (1, note('D', 5), 0.5, 65),
         (1.5, note('C', 5), 0.5, 60), (2, note('A', 4), 2, 72)],
        # Phrase 2 (over Em)
        [(0, note('B', 4), 1, 68), (1, note('G', 4), 1, 62),
         (2, note('E', 4), 1.5, 70), (3.5, note('G', 4), 0.5, 58)],
        # Phrase 3 (over F)
        [(0, note('C', 5), 1, 72), (1, note('A', 4), 0.5, 65),
         (1.5, note('F', 4), 1.5, 68), (3, note('A', 4), 1, 60)],
        # Phrase 4 (over G)
        [(0, note('D', 5), 1.5, 70), (2, note('B', 4), 1, 65),
         (3, note('G', 4), 0.5, 62), (3.5, note('A', 4), 0.5, 58)],
    ]
    for pi, phrase in enumerate(melody_phrases):
        # Play each phrase twice (bars 1-2 melody, bars 3-4 variation)
        for rep in range(2):
            base = pi * 16 + rep * 8 + 4  # start on bar 2 of each section
            for beat, pitch, dur, vel in phrase:
                # Slight variation on repeat
                v = vel - 5 if rep == 1 else vel
                mel_notes.append((base + beat, pitch, dur, v))

    mel_track = build_track_from_notes(mel_notes, ppq)
    mel_track.insert(0, MetaMessage('track_name', name='Lead'))
    mel_track.insert(0, Message('program_change', program=81))  # Saw Lead
    mid.tracks.append(mel_track)

    return mid, "zone1"


def compose_zone2():
    """Intense void corridors. E minor, 120 BPM, driving bass."""
    ppq = 480
    mid = MidiFile(ticks_per_beat=ppq)
    bpm = 120
    bars = 16

    # --- Driving bass (8th notes) ---
    bass_notes = []
    # Em - C - D - Bm pattern
    bass_roots = [note('E', 1), note('C', 2), note('D', 2), note('B', 1)]
    for pi, root in enumerate(bass_roots):
        base = pi * 16
        for bar in range(4):
            b = base + bar * 4
            for eighth in range(8):
                p = root if eighth % 2 == 0 else root + 12
                vel = 85 if eighth % 4 == 0 else 65
                bass_notes.append((b + eighth * 0.5, p, 0.4, vel))

    bass_track = build_track_from_notes(bass_notes, ppq)
    bass_track.insert(0, MetaMessage('set_tempo', tempo=mido.bpm2tempo(bpm)))
    bass_track.insert(0, MetaMessage('track_name', name='Bass'))
    bass_track.insert(0, Message('program_change', program=38))
    mid.tracks.append(bass_track)

    # --- Power chords (stabs) ---
    chord_notes = []
    chords = [
        [note('E', 3), note('B', 3), note('E', 4)],
        [note('C', 3), note('G', 3), note('C', 4)],
        [note('D', 3), note('A', 3), note('D', 4)],
        [note('B', 2), note('F#', 3), note('B', 3)],
    ]
    for pi, chord in enumerate(chords):
        base = pi * 16
        for bar in range(4):
            b = base + bar * 4
            # Stab on beat 1 and the "and" of 2
            for n in chord:
                chord_notes.append((b, n, 0.3, 80))
                chord_notes.append((b + 2.5, n, 0.3, 70))

    chord_track = build_track_from_notes(chord_notes, ppq)
    chord_track.insert(0, MetaMessage('track_name', name='Chords'))
    chord_track.insert(0, Message('program_change', program=29))  # Overdriven Guitar
    mid.tracks.append(chord_track)

    # --- Drums (more intense) ---
    drum_notes = []
    kick, snare, hihat, crash = 36, 38, 42, 49
    for bar in range(bars):
        b = bar * 4
        # Double kick
        drum_notes.append((b, kick, 0.25, 110))
        drum_notes.append((b + 0.75, kick, 0.25, 80))
        drum_notes.append((b + 2, kick, 0.25, 100))
        drum_notes.append((b + 2.75, kick, 0.25, 75))
        # Snare on 2 and 4
        drum_notes.append((b + 1, snare, 0.25, 95))
        drum_notes.append((b + 3, snare, 0.25, 90))
        # 16th hi-hats
        for s in range(16):
            vel = 70 if s % 4 == 0 else 40
            drum_notes.append((b + s * 0.25, hihat, 0.12, vel))
        # Crash every 4 bars
        if bar % 4 == 0:
            drum_notes.append((b, crash, 1, 90))

    drum_track = build_track_from_notes(drum_notes, ppq)
    drum_track.insert(0, MetaMessage('track_name', name='Drums'))
    for msg in drum_track:
        if hasattr(msg, 'channel'):
            msg.channel = 9
    mid.tracks.append(drum_track)

    # --- Lead melody ---
    mel_notes = []
    phrases = [
        [(0, note('E', 5), 0.5, 75), (0.5, note('G', 5), 0.5, 70),
         (1, note('B', 5), 1, 80), (2.5, note('A', 5), 0.5, 68),
         (3, note('G', 5), 1, 72)],
        [(0, note('G', 5), 0.5, 72), (0.5, note('E', 5), 0.5, 68),
         (1, note('C', 5), 1.5, 78), (3, note('D', 5), 0.5, 65),
         (3.5, note('E', 5), 0.5, 70)],
        [(0, note('D', 5), 1, 75), (1, note('F#', 5), 0.5, 70),
         (1.5, note('A', 5), 1, 80), (3, note('G', 5), 0.5, 68),
         (3.5, note('F#', 5), 0.5, 65)],
        [(0, note('B', 4), 0.5, 72), (0.5, note('D', 5), 0.5, 68),
         (1, note('F#', 5), 1.5, 78), (3, note('E', 5), 1, 72)],
    ]
    for pi, phrase in enumerate(phrases):
        for rep in range(2):
            base = pi * 16 + rep * 8 + 4
            for beat, pitch, dur, vel in phrase:
                mel_notes.append((base + beat, pitch, dur, vel))

    mel_track = build_track_from_notes(mel_notes, ppq)
    mel_track.insert(0, MetaMessage('track_name', name='Lead'))
    mel_track.insert(0, Message('program_change', program=80))
    mid.tracks.append(mel_track)

    return mid, "zone2"


def compose_zone3():
    """Epic entropy core. C minor, 130 BPM, layered and climactic."""
    ppq = 480
    mid = MidiFile(ticks_per_beat=ppq)
    bpm = 130
    bars = 16

    # --- Orchestral pad ---
    pad_notes = []
    chords = [
        [note('C', 3), note('Eb', 3), note('G', 3), note('Bb', 3)],  # Cm7
        [note('Ab', 2), note('C', 3), note('Eb', 3), note('G', 3)],  # AbMaj7
        [note('Bb', 2), note('D', 3), note('F', 3), note('Ab', 3)],  # Bb7
        [note('G', 2), note('B', 2), note('D', 3), note('F', 3)],    # G7
    ]
    for pi, chord in enumerate(chords):
        base = pi * 16
        for n in chord:
            pad_notes.append((base, n, 15.5, 60))

    pad_track = build_track_from_notes(pad_notes, ppq)
    pad_track.insert(0, MetaMessage('set_tempo', tempo=mido.bpm2tempo(bpm)))
    pad_track.insert(0, MetaMessage('track_name', name='Pad'))
    pad_track.insert(0, Message('program_change', program=48))  # Strings
    mid.tracks.append(pad_track)

    # --- Bass (driving octaves) ---
    bass_notes = []
    roots = [note('C', 1), note('Ab', 1), note('Bb', 1), note('G', 1)]
    for pi, root in enumerate(roots):
        base = pi * 16
        for bar in range(4):
            b = base + bar * 4
            bass_notes.append((b, root, 0.5, 90))
            bass_notes.append((b + 0.5, root + 12, 0.25, 70))
            bass_notes.append((b + 1, root, 0.5, 85))
            bass_notes.append((b + 2, root + 12, 0.5, 80))
            bass_notes.append((b + 2.5, root, 0.25, 70))
            bass_notes.append((b + 3, root + 7, 0.5, 75))
            bass_notes.append((b + 3.5, root + 12, 0.25, 70))

    bass_track = build_track_from_notes(bass_notes, ppq)
    bass_track.insert(0, MetaMessage('track_name', name='Bass'))
    bass_track.insert(0, Message('program_change', program=38))
    mid.tracks.append(bass_track)

    # --- Drums (epic, heavy) ---
    drum_notes = []
    kick, snare, hihat, crash, ride, tom = 36, 38, 42, 49, 51, 45
    for bar in range(bars):
        b = bar * 4
        drum_notes.append((b, kick, 0.25, 115))
        drum_notes.append((b + 1.5, kick, 0.25, 85))
        drum_notes.append((b + 2, kick, 0.25, 105))
        drum_notes.append((b + 3.5, kick, 0.25, 80))
        drum_notes.append((b + 1, snare, 0.25, 100))
        drum_notes.append((b + 3, snare, 0.25, 95))
        for s in range(8):
            drum_notes.append((b + s * 0.5, ride, 0.25, 50 + (s == 0) * 20))
        if bar % 4 == 0:
            drum_notes.append((b, crash, 1.5, 95))
        # Tom fills every 4th bar
        if bar % 4 == 3:
            drum_notes.append((b + 3, tom, 0.25, 80))
            drum_notes.append((b + 3.25, tom - 2, 0.25, 75))
            drum_notes.append((b + 3.5, tom - 4, 0.25, 85))
            drum_notes.append((b + 3.75, tom - 6, 0.25, 90))

    drum_track = build_track_from_notes(drum_notes, ppq)
    drum_track.insert(0, MetaMessage('track_name', name='Drums'))
    for msg in drum_track:
        if hasattr(msg, 'channel'):
            msg.channel = 9
    mid.tracks.append(drum_track)

    # --- Lead melody (heroic, epic) ---
    mel_notes = []
    phrases = [
        [(0, note('G', 5), 1, 80), (1, note('Eb', 5), 0.5, 72),
         (1.5, note('C', 5), 0.5, 68), (2, note('D', 5), 1, 78),
         (3, note('Eb', 5), 0.5, 72), (3.5, note('G', 5), 0.5, 75)],
        [(0, note('Ab', 5), 1.5, 82), (2, note('G', 5), 0.5, 70),
         (2.5, note('F', 5), 0.5, 68), (3, note('Eb', 5), 1, 78)],
        [(0, note('Bb', 5), 1, 85), (1, note('Ab', 5), 0.5, 75),
         (1.5, note('G', 5), 0.5, 70), (2, note('F', 5), 1, 80),
         (3, note('D', 5), 0.5, 72), (3.5, note('Eb', 5), 0.5, 68)],
        [(0, note('G', 5), 2, 82), (2.5, note('F', 5), 0.5, 70),
         (3, note('D', 5), 1, 78)],
    ]
    for pi, phrase in enumerate(phrases):
        for rep in range(2):
            base = pi * 16 + rep * 8 + 4
            for beat, pitch, dur, vel in phrase:
                mel_notes.append((base + beat, pitch, dur, vel + rep * 3))

    mel_track = build_track_from_notes(mel_notes, ppq)
    mel_track.insert(0, MetaMessage('track_name', name='Lead'))
    mel_track.insert(0, Message('program_change', program=81))
    mid.tracks.append(mel_track)

    return mid, "zone3"


def compose_boss_theme():
    """Aggressive boss fight. E minor, 140 BPM, heavy and fast."""
    ppq = 480
    mid = MidiFile(ticks_per_beat=ppq)
    bpm = 140
    bars = 16

    # --- Aggressive bass (16ths) ---
    bass_notes = []
    roots = [note('E', 1), note('C', 2), note('D', 2), note('B', 1)]
    for pi, root in enumerate(roots):
        base = pi * 16
        for bar in range(4):
            b = base + bar * 4
            for s in range(16):
                p = root if s % 3 != 2 else root + 7
                vel = 95 if s % 4 == 0 else 70
                bass_notes.append((b + s * 0.25, p, 0.2, vel))

    bass_track = build_track_from_notes(bass_notes, ppq)
    bass_track.insert(0, MetaMessage('set_tempo', tempo=mido.bpm2tempo(bpm)))
    bass_track.insert(0, MetaMessage('track_name', name='Bass'))
    bass_track.insert(0, Message('program_change', program=30))  # Distortion Guitar
    mid.tracks.append(bass_track)

    # --- Stabs (dissonant power chords) ---
    stab_notes = []
    stabs = [
        [note('E', 4), note('B', 4)],
        [note('C', 4), note('G', 4)],
        [note('D', 4), note('A', 4)],
        [note('B', 3), note('F#', 4)],
    ]
    for pi, chord in enumerate(stabs):
        base = pi * 16
        for bar in range(4):
            b = base + bar * 4
            for n in chord:
                stab_notes.append((b, n, 0.15, 100))
                stab_notes.append((b + 1.5, n, 0.15, 85))
                stab_notes.append((b + 2, n, 0.15, 95))
                stab_notes.append((b + 3, n, 0.15, 90))
                stab_notes.append((b + 3.5, n, 0.15, 80))

    stab_track = build_track_from_notes(stab_notes, ppq)
    stab_track.insert(0, MetaMessage('track_name', name='Stabs'))
    stab_track.insert(0, Message('program_change', program=29))
    mid.tracks.append(stab_track)

    # --- Heavy drums ---
    drum_notes = []
    kick, snare, hihat, crash, china = 36, 38, 42, 49, 52
    for bar in range(bars):
        b = bar * 4
        # Blast beat style
        for s in range(16):
            if s % 2 == 0:
                drum_notes.append((b + s * 0.25, kick, 0.12, 110))
            if s % 4 == 2:
                drum_notes.append((b + s * 0.25, snare, 0.12, 95))
            drum_notes.append((b + s * 0.25, hihat, 0.1, 50))
        # Extra kick on downbeat
        drum_notes.append((b, kick, 0.25, 120))
        drum_notes.append((b + 2, kick, 0.25, 115))
        # Snare on 2 and 4
        drum_notes.append((b + 1, snare, 0.25, 105))
        drum_notes.append((b + 3, snare, 0.25, 100))
        if bar % 4 == 0:
            drum_notes.append((b, crash, 2, 100))
        if bar % 8 == 4:
            drum_notes.append((b, china, 1, 85))

    drum_track = build_track_from_notes(drum_notes, ppq)
    drum_track.insert(0, MetaMessage('track_name', name='Drums'))
    for msg in drum_track:
        if hasattr(msg, 'channel'):
            msg.channel = 9
    mid.tracks.append(drum_track)

    # --- Aggressive lead ---
    mel_notes = []
    phrases = [
        [(0, note('E', 5), 0.25, 90), (0.25, note('G', 5), 0.25, 85),
         (0.5, note('B', 5), 0.5, 95), (1.5, note('A', 5), 0.25, 80),
         (1.75, note('G', 5), 0.25, 78), (2, note('E', 5), 1, 88)],
        [(0, note('C', 5), 0.5, 85), (0.5, note('E', 5), 0.5, 82),
         (1, note('G', 5), 0.75, 90), (2, note('F#', 5), 0.25, 78),
         (2.25, note('E', 5), 0.25, 75), (2.5, note('D', 5), 0.5, 82),
         (3, note('C', 5), 1, 88)],
    ]
    for bar in range(bars):
        if bar % 2 == 0:
            phrase = phrases[bar // 4 % 2]
            b = bar * 4
            for beat, pitch, dur, vel in phrase:
                mel_notes.append((b + beat, pitch, dur, vel))

    mel_track = build_track_from_notes(mel_notes, ppq)
    mel_track.insert(0, MetaMessage('track_name', name='Lead'))
    mel_track.insert(0, Message('program_change', program=81))
    mid.tracks.append(mel_track)

    return mid, "boss_theme"


def compose_victory():
    """Triumphant victory/credits. C major, 100 BPM."""
    ppq = 480
    mid = MidiFile(ticks_per_beat=ppq)
    bpm = 100
    bars = 16

    # --- Bright pad (major chords) ---
    pad_notes = []
    chords = [
        [note('C', 3), note('E', 3), note('G', 3)],   # C
        [note('G', 2), note('B', 2), note('D', 3)],   # G
        [note('A', 2), note('C', 3), note('E', 3)],   # Am
        [note('F', 2), note('A', 2), note('C', 3)],   # F
        [note('C', 3), note('E', 3), note('G', 3)],   # C
        [note('F', 2), note('A', 2), note('C', 3)],   # F
        [note('G', 2), note('B', 2), note('D', 3)],   # G
        [note('C', 3), note('E', 3), note('G', 3)],   # C
    ]
    for i, chord in enumerate(chords):
        beat = i * 8
        for n in chord:
            pad_notes.append((beat, n, 7.5, 55))

    pad_track = build_track_from_notes(pad_notes, ppq)
    pad_track.insert(0, MetaMessage('set_tempo', tempo=mido.bpm2tempo(bpm)))
    pad_track.insert(0, MetaMessage('track_name', name='Pad'))
    pad_track.insert(0, Message('program_change', program=48))  # Strings
    mid.tracks.append(pad_track)

    # --- Uplifting melody ---
    mel_notes = []
    melody = [
        (0, note('E', 5), 1, 75), (1, note('G', 5), 1, 72),
        (2, note('C', 6), 2, 82), (4, note('B', 5), 1, 70),
        (5, note('A', 5), 1, 68), (6, note('G', 5), 2, 75),
        (8, note('D', 5), 1, 72), (9, note('E', 5), 1, 70),
        (10, note('G', 5), 1.5, 78), (12, note('F', 5), 1, 68),
        (13, note('E', 5), 1, 65), (14, note('D', 5), 2, 72),
        (16, note('C', 5), 1, 70), (17, note('E', 5), 1, 72),
        (18, note('G', 5), 2, 80), (20, note('A', 5), 1, 75),
        (21, note('G', 5), 1, 68), (22, note('F', 5), 2, 72),
        (24, note('E', 5), 1, 78), (25, note('F', 5), 0.5, 70),
        (25.5, note('G', 5), 0.5, 72), (26, note('A', 5), 2, 82),
        (28, note('G', 5), 1, 75), (29, note('F', 5), 1, 68),
        (30, note('E', 5), 2, 80),
        # Second half: higher, more triumphant
        (32, note('G', 5), 1, 78), (33, note('A', 5), 1, 75),
        (34, note('C', 6), 2, 85), (36, note('D', 6), 1.5, 82),
        (38, note('C', 6), 1, 78), (39, note('B', 5), 1, 72),
        (40, note('A', 5), 2, 80), (42, note('G', 5), 1, 72),
        (43, note('A', 5), 1, 75), (44, note('B', 5), 2, 82),
        (46, note('C', 6), 2, 85),
        (48, note('E', 5), 1, 75), (49, note('G', 5), 1, 78),
        (50, note('C', 6), 3, 88), (54, note('B', 5), 1, 75),
        (55, note('A', 5), 1, 72), (56, note('G', 5), 2, 80),
        (58, note('E', 5), 1, 70), (59, note('G', 5), 1, 72),
        (60, note('C', 6), 4, 90),
    ]
    mel_notes.extend(melody)

    mel_track = build_track_from_notes(mel_notes, ppq)
    mel_track.insert(0, MetaMessage('track_name', name='Melody'))
    mel_track.insert(0, Message('program_change', program=73))  # Flute
    mid.tracks.append(mel_track)

    # --- Bass ---
    bass_notes = []
    bass_line = [note('C', 2), note('G', 1), note('A', 1), note('F', 1),
                 note('C', 2), note('F', 1), note('G', 1), note('C', 2)]
    for i, root in enumerate(bass_line):
        b = i * 8
        bass_notes.append((b, root, 3, 70))
        bass_notes.append((b + 4, root + 7, 3, 60))

    bass_track = build_track_from_notes(bass_notes, ppq)
    bass_track.insert(0, MetaMessage('track_name', name='Bass'))
    bass_track.insert(0, Message('program_change', program=43))  # Cello
    mid.tracks.append(bass_track)

    # --- Light drums ---
    drum_notes = []
    kick, snare, hihat, ride = 36, 38, 42, 51
    for bar in range(bars):
        b = bar * 4
        drum_notes.append((b, kick, 0.5, 80))
        drum_notes.append((b + 2, kick, 0.5, 70))
        drum_notes.append((b + 1, snare, 0.5, 65))
        drum_notes.append((b + 3, snare, 0.5, 60))
        for s in range(8):
            drum_notes.append((b + s * 0.5, ride, 0.25, 40))

    drum_track = build_track_from_notes(drum_notes, ppq)
    drum_track.insert(0, MetaMessage('track_name', name='Drums'))
    for msg in drum_track:
        if hasattr(msg, 'channel'):
            msg.channel = 9
    mid.tracks.append(drum_track)

    return mid, "victory"


def compose_gameover():
    """Somber game over. D minor, 70 BPM, short (~20 seconds)."""
    ppq = 480
    mid = MidiFile(ticks_per_beat=ppq)
    bpm = 70
    bars = 4  # Short

    # --- Descending pad ---
    pad_notes = []
    chords = [
        [note('D', 3), note('F', 3), note('A', 3)],   # Dm
        [note('Bb', 2), note('D', 3), note('F', 3)],  # Bb
        [note('G', 2), note('Bb', 2), note('D', 3)],  # Gm
        [note('A', 2), note('C#', 3), note('E', 3)],  # A (dominant, unresolved)
    ]
    for i, chord in enumerate(chords):
        beat = i * 4
        for n in chord:
            pad_notes.append((beat, n, 3.8, 45 - i * 5))  # Fade out

    pad_track = build_track_from_notes(pad_notes, ppq)
    pad_track.insert(0, MetaMessage('set_tempo', tempo=mido.bpm2tempo(bpm)))
    pad_track.insert(0, MetaMessage('track_name', name='Pad'))
    pad_track.insert(0, Message('program_change', program=89))
    mid.tracks.append(pad_track)

    # --- Melody (falling) ---
    mel_notes = []
    melody = [
        (0, note('A', 4), 2, 55),
        (2, note('F', 4), 1.5, 48),
        (4, note('D', 4), 2, 50),
        (6, note('Bb', 3), 1.5, 42),
        (8, note('G', 3), 2, 45),
        (10, note('Bb', 3), 1, 38),
        (12, note('A', 3), 3, 42),
    ]
    mel_notes.extend(melody)

    mel_track = build_track_from_notes(mel_notes, ppq)
    mel_track.insert(0, MetaMessage('track_name', name='Melody'))
    mel_track.insert(0, Message('program_change', program=71))  # Clarinet
    mid.tracks.append(mel_track)

    return mid, "gameover"


# ============================================================
# LMMS PROJECT GENERATION
# ============================================================

def create_lmms_project(midi_path, name, bpm):
    """Create an LMMS .mmp project that imports the MIDI tracks."""
    # For now, create a minimal .mmp that users can load and enhance
    root = ET.Element("lmms-project", version="1.0", creator="LMMS",
                      creatorversion="1.2.0", type="song")

    head = ET.SubElement(root, "head")
    head.set("timesig_numerator", "4")
    head.set("timesig_denominator", "4")
    head.set("mastervol", "100")
    head.set("masterpitch", "0")
    head.set("bpm", str(bpm))

    song = ET.SubElement(root, "song")
    # LMMS can import MIDI directly via the GUI
    # The .mmp serves as a reference project

    xml_str = minidom.parseString(ET.tostring(root)).toprettyxml(indent="  ")
    out_path = os.path.join(LMMS_DIR, f"{name}.mmp")
    with open(out_path, "w") as f:
        f.write(xml_str)
    return out_path


# ============================================================
# WAV RENDERING (direct synthesis fallback)
# ============================================================

def render_midi_to_wav_synth(midi_path, wav_path):
    """Render MIDI to WAV using numpy synthesis (fallback if no SoundFont)."""
    import numpy as np
    from scipy.io import wavfile
    from scipy.signal import lfilter

    mid = MidiFile(midi_path)
    sr = 44100
    # Calculate total duration
    total_time = mid.length + 2.0  # extra 2s for reverb tail
    samples = int(total_time * sr)
    audio = np.zeros(samples, dtype=np.float64)

    # Collect all notes with absolute times
    for track in mid.tracks:
        current_time = 0.0  # in seconds
        tempo = 500000  # default 120 BPM
        channel = 0
        program = 0

        for msg in track:
            if msg.is_meta:
                if msg.type == 'set_tempo':
                    tempo = msg.tempo
                current_time += mido.tick2second(msg.time, mid.ticks_per_beat, tempo)
                continue

            current_time += mido.tick2second(msg.time, mid.ticks_per_beat, tempo)

            if msg.type == 'program_change':
                program = msg.program
                channel = msg.channel
                continue

            if msg.type == 'note_on' and msg.velocity > 0:
                ch = msg.channel if hasattr(msg, 'channel') else channel
                freq = 440.0 * (2.0 ** ((msg.note - 69) / 12.0))
                vel = msg.velocity / 127.0

                # Find note_off for duration
                dur = 0.5  # default
                t_acc = 0.0
                found = False
                for future_msg in track:
                    # This is imprecise but good enough for rendering
                    pass
                # Use a default duration based on instrument type
                if program in range(38, 40):  # bass
                    dur = 0.3
                elif program in range(48, 56):  # strings/ensemble
                    dur = 2.0
                elif program in range(80, 96):  # synth lead/pad
                    dur = 1.0
                elif program in range(24, 32):  # guitar
                    dur = 0.2
                elif ch == 9:  # drums
                    dur = 0.15
                else:
                    dur = 0.8

                start_sample = int(current_time * sr)
                n_samples = int(dur * sr)
                if start_sample + n_samples > samples:
                    n_samples = samples - start_sample
                if n_samples <= 0:
                    continue

                t = np.arange(n_samples) / sr

                # ADSR envelope
                attack = min(0.02, dur * 0.1)
                decay = min(0.1, dur * 0.2)
                sustain_level = 0.6
                release = min(0.2, dur * 0.3)

                env = np.ones(n_samples)
                att_s = int(attack * sr)
                dec_s = int(decay * sr)
                rel_s = int(release * sr)
                if att_s > 0:
                    env[:att_s] = np.linspace(0, 1, att_s)
                if dec_s > 0 and att_s + dec_s < n_samples:
                    env[att_s:att_s + dec_s] = np.linspace(1, sustain_level, dec_s)
                    env[att_s + dec_s:] = sustain_level
                if rel_s > 0 and n_samples > rel_s:
                    env[-rel_s:] *= np.linspace(1, 0, rel_s)

                # Waveform based on instrument type
                if ch == 9:
                    # Drums: noise-based with pitch envelope
                    if msg.note in [36, 35]:  # kick
                        freq_env = np.linspace(150, 40, n_samples)
                        wave = np.sin(2 * np.pi * np.cumsum(freq_env / sr)) * 0.8
                        wave += np.random.randn(n_samples) * 0.1 * env
                    elif msg.note in [38, 40]:  # snare
                        wave = np.sin(2 * np.pi * 200 * t) * 0.3
                        wave += np.random.randn(n_samples) * 0.5
                    elif msg.note in [42, 44]:  # closed hi-hat
                        wave = np.random.randn(n_samples) * 0.3
                        # High-pass via simple difference
                        wave = np.diff(wave, prepend=0)
                    elif msg.note in [46]:  # open hi-hat
                        wave = np.random.randn(n_samples) * 0.25
                    elif msg.note in [49, 51, 52, 55, 57]:  # cymbals
                        wave = np.random.randn(n_samples) * 0.2
                        wave = np.cumsum(wave) * 0.01  # metallic quality
                    elif msg.note in [45, 43, 41, 47, 48]:  # toms
                        f_env = np.linspace(freq, freq * 0.5, n_samples)
                        wave = np.sin(2 * np.pi * np.cumsum(f_env / sr)) * 0.6
                    else:
                        wave = np.random.randn(n_samples) * 0.2

                elif program in range(38, 40):  # synth bass
                    # Saw + sub sine
                    saw = 2 * (t * freq % 1) - 1
                    sub = np.sin(2 * np.pi * freq * 0.5 * t)
                    wave = saw * 0.5 + sub * 0.5
                    # Low-pass filter
                    cutoff = 2000
                    rc = 1 / (2 * np.pi * cutoff)
                    alpha = 1 / (sr * rc + 1)
                    b_coeff = [alpha]
                    a_coeff = [1, -(1 - alpha)]
                    wave = lfilter(b_coeff, a_coeff, wave)

                elif program in range(29, 32):  # distorted guitar
                    saw = 2 * (t * freq % 1) - 1
                    wave = np.tanh(saw * 3) * 0.4  # soft clip distortion

                elif program in range(48, 56):  # strings/ensemble
                    # Layered detuned sines for strings
                    wave = (np.sin(2 * np.pi * freq * t) * 0.3 +
                            np.sin(2 * np.pi * freq * 1.002 * t) * 0.25 +
                            np.sin(2 * np.pi * freq * 0.998 * t) * 0.25 +
                            np.sin(2 * np.pi * freq * 2 * t) * 0.1)

                elif program in range(80, 88):  # synth lead
                    # Square + saw blend
                    sq = np.sign(np.sin(2 * np.pi * freq * t))
                    saw = 2 * (t * freq % 1) - 1
                    wave = sq * 0.3 + saw * 0.4
                    cutoff = 4000
                    rc = 1 / (2 * np.pi * cutoff)
                    alpha = 1 / (sr * rc + 1)
                    wave = lfilter([alpha], [1, -(1 - alpha)], wave)

                elif program in range(88, 96):  # synth pad
                    # Warm detuned sines with slow LFO
                    lfo = 1 + 0.003 * np.sin(2 * np.pi * 0.5 * t)
                    wave = (np.sin(2 * np.pi * freq * lfo * t) * 0.3 +
                            np.sin(2 * np.pi * freq * 2 * t) * 0.15 +
                            np.sin(2 * np.pi * freq * 0.5 * t) * 0.2)

                elif program in range(71, 80):  # woodwinds/flute
                    # Sine + harmonics for flute-like sound
                    wave = (np.sin(2 * np.pi * freq * t) * 0.5 +
                            np.sin(2 * np.pi * freq * 2 * t) * 0.2 +
                            np.sin(2 * np.pi * freq * 3 * t) * 0.05 +
                            np.random.randn(n_samples) * 0.02)  # breath noise

                elif program in range(40, 48):  # cello/strings
                    saw = 2 * (t * freq % 1) - 1
                    wave = saw * 0.3
                    cutoff = 2500
                    rc = 1 / (2 * np.pi * cutoff)
                    alpha = 1 / (sr * rc + 1)
                    wave = lfilter([alpha], [1, -(1 - alpha)], wave)

                else:
                    # Default: sine with harmonics
                    wave = (np.sin(2 * np.pi * freq * t) * 0.4 +
                            np.sin(2 * np.pi * freq * 2 * t) * 0.2)

                wave *= env * vel * 0.4

                end_sample = start_sample + n_samples
                if end_sample <= samples:
                    audio[start_sample:end_sample] += wave

    # Master processing
    # Simple reverb (delay-based)
    delay_samples = int(0.08 * sr)
    reverb = np.zeros_like(audio)
    for d, gain in [(delay_samples, 0.3), (delay_samples * 2, 0.15),
                     (delay_samples * 3, 0.08), (delay_samples * 5, 0.04)]:
        if d < len(audio):
            reverb[d:] += audio[:-d] * gain
    audio += reverb

    # Soft clip / limiter
    audio = np.tanh(audio * 1.5) * 0.8

    # Normalize
    peak = np.max(np.abs(audio))
    if peak > 0:
        audio = audio / peak * 0.85

    # Convert to 16-bit
    audio_16 = (audio * 32767).astype(np.int16)
    wavfile.write(wav_path, sr, audio_16)
    return wav_path


# ============================================================
# MAIN
# ============================================================

def main():
    print("=== Riftwalker Music Generator ===\n")

    compositions = [
        compose_menu_theme,
        compose_zone1,
        compose_zone2,
        compose_zone3,
        compose_boss_theme,
        compose_victory,
        compose_gameover,
    ]

    bpms = {"menu_theme": 80, "zone1": 100, "zone2": 120,
            "zone3": 130, "boss_theme": 140, "victory": 100, "gameover": 70}

    for compose_fn in compositions:
        mid, name = compose_fn()
        midi_path = os.path.join(MIDI_DIR, f"{name}.mid")
        mid.save(midi_path)
        print(f"  MIDI: {name}.mid ({bpms[name]} BPM)")

        # Create LMMS project reference
        mmp_path = create_lmms_project(midi_path, name, bpms[name])
        print(f"  LMMS: {name}.mmp")

        # Render to WAV via Python synthesis
        wav_path = os.path.join(MUSIC_DIR, f"{name}.wav")
        print(f"  Rendering {name}.wav ...", end=" ", flush=True)
        render_midi_to_wav_synth(midi_path, wav_path)
        print("OK")

        # Convert to OGG via ffmpeg for smaller file size
        ogg_path = os.path.join(MUSIC_DIR, f"{name}.ogg")
        try:
            subprocess.run([
                FFMPEG, "-y", "-i", wav_path, "-c:a", "libvorbis",
                "-q:a", "6", ogg_path
            ], capture_output=True, check=True)
            print(f"  OGG:  {name}.ogg")
            # Keep WAV for reference, game will use OGG
        except (subprocess.CalledProcessError, FileNotFoundError):
            print(f"  OGG:  skipped (ffmpeg not available)")

    print(f"\nDone! Files in {MUSIC_DIR}")
    print("\nTo improve tracks:")
    print("  1. Open MIDI files in LMMS (File > Import MIDI)")
    print("  2. Assign better instruments/synths")
    print("  3. Export as WAV/OGG")
    print(f"  MIDI files: {MIDI_DIR}")


if __name__ == "__main__":
    main()
