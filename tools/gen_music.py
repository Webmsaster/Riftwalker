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
    """Render MIDI to WAV using enhanced numpy synthesis.

    v2 improvements over original:
    - Proper note duration tracking from MIDI note_on/note_off pairs
    - Stereo output with per-track panning
    - Schroeder reverb (4 comb + 2 allpass filters)
    - Chorus effect for pads and strings
    - Per-instrument ADSR envelope profiles
    - Filter envelope modulation for bass and leads
    - Sub-oscillator and detuned unison for richness
    """
    import numpy as np
    from scipy.io import wavfile
    from scipy.signal import lfilter, butter

    mid = MidiFile(midi_path)
    sr = 44100
    total_time = mid.length + 3.0  # extra 3s for reverb tail
    samples = int(total_time * sr)
    # Stereo buffer: left and right channels
    audio_L = np.zeros(samples, dtype=np.float64)
    audio_R = np.zeros(samples, dtype=np.float64)

    # --- Helper: low-pass filter ---
    def lpf(signal, cutoff, sr=44100):
        if cutoff >= sr * 0.45:
            return signal
        b, a = butter(2, cutoff / (sr / 2), btype='low')
        return lfilter(b, a, signal)

    # --- Helper: high-pass filter ---
    def hpf(signal, cutoff, sr=44100):
        if cutoff <= 20:
            return signal
        b, a = butter(2, cutoff / (sr / 2), btype='high')
        return lfilter(b, a, signal)

    # --- Helper: band-limited saw wave (fewer aliasing artifacts) ---
    def saw_wave(freq, t, sr=44100):
        # freq can be scalar or array; ensure phase works for both
        freq_arr = np.broadcast_to(freq, t.shape)
        phase = np.cumsum(freq_arr / sr) % 1.0
        saw = 2.0 * phase - 1.0
        # Simple smoothing at discontinuities (no polyblep for array freq)
        return saw

    # --- Helper: ADSR envelope ---
    def adsr(n_samples, attack, decay, sustain, release, sr=44100):
        env = np.ones(n_samples)
        att_s = int(attack * sr)
        dec_s = int(decay * sr)
        rel_s = int(release * sr)
        sus_start = att_s + dec_s
        rel_start = max(0, n_samples - rel_s)
        if att_s > 0:
            end = min(att_s, n_samples)
            env[:end] = np.linspace(0, 1, end)
        if dec_s > 0 and sus_start < n_samples:
            end = min(sus_start, n_samples)
            env[att_s:end] = np.linspace(1, sustain, end - att_s)
        if sus_start < n_samples:
            env[sus_start:rel_start] = sustain
        if rel_s > 0 and rel_start < n_samples:
            env[rel_start:] *= np.linspace(1, 0, n_samples - rel_start)
        return env

    # --- Helper: chorus effect (subtle detuning + delay) ---
    def chorus(signal, depth=0.002, rate=1.5, sr=44100):
        n = len(signal)
        t = np.arange(n) / sr
        mod = (depth * sr) * (0.5 + 0.5 * np.sin(2 * np.pi * rate * t))
        out = np.zeros(n)
        for i in range(n):
            idx = i - mod[i]
            if idx >= 0:
                i0 = int(idx)
                frac = idx - i0
                if i0 + 1 < n:
                    out[i] = signal[i0] * (1 - frac) + signal[i0 + 1] * frac
        return signal * 0.7 + out * 0.3

    # --- Step 1: Parse MIDI into note events with proper durations ---
    all_notes = []  # (start_sec, duration_sec, note, velocity, channel, program, track_idx)

    for track_idx, track in enumerate(mid.tracks):
        # First pass: collect tempo changes
        tempo_map = []  # (tick, tempo)
        tick = 0
        for msg in track:
            tick += msg.time
            if msg.is_meta and msg.type == 'set_tempo':
                tempo_map.append((tick, msg.tempo))
        if not tempo_map:
            tempo_map.append((0, 500000))  # default 120 BPM

        # Second pass: pair note_on/note_off
        active_notes = {}  # (note, channel) -> (start_tick, velocity)
        tick = 0
        program = 0
        channel = 0

        def tick_to_sec(target_tick):
            """Convert MIDI tick to seconds using tempo map."""
            sec = 0.0
            prev_tick = 0
            prev_tempo = 500000
            for t_tick, t_tempo in tempo_map:
                if t_tick > target_tick:
                    break
                sec += mido.tick2second(t_tick - prev_tick, mid.ticks_per_beat, prev_tempo)
                prev_tick = t_tick
                prev_tempo = t_tempo
            sec += mido.tick2second(target_tick - prev_tick, mid.ticks_per_beat, prev_tempo)
            return sec

        for msg in track:
            tick += msg.time

            if msg.is_meta:
                continue

            if msg.type == 'program_change':
                program = msg.program
                channel = msg.channel
                continue

            ch = msg.channel if hasattr(msg, 'channel') else channel

            if msg.type == 'note_on' and msg.velocity > 0:
                key = (msg.note, ch)
                active_notes[key] = (tick, msg.velocity, program)

            elif msg.type == 'note_off' or (msg.type == 'note_on' and msg.velocity == 0):
                key = (msg.note, ch)
                if key in active_notes:
                    start_tick, vel, prog = active_notes.pop(key)
                    start_sec = tick_to_sec(start_tick)
                    end_sec = tick_to_sec(tick)
                    dur = max(0.01, end_sec - start_sec)
                    all_notes.append((start_sec, dur, msg.note, vel, ch, prog, track_idx))

        # Handle any notes still active at end of track (sustaining notes)
        for (note_num, ch), (start_tick, vel, prog) in active_notes.items():
            start_sec = tick_to_sec(start_tick)
            end_sec = tick_to_sec(tick)
            dur = max(0.05, end_sec - start_sec)
            all_notes.append((start_sec, dur, note_num, vel, ch, prog, track_idx))

    print(f"    Parsed {len(all_notes)} note events")

    # --- Per-track panning (spread instruments in stereo field) ---
    track_pans = {}  # track_idx -> pan (-1.0 left .. +1.0 right)
    num_tracks = len(mid.tracks)
    for i in range(num_tracks):
        # Spread tracks across stereo field
        if num_tracks <= 1:
            track_pans[i] = 0.0
        else:
            track_pans[i] = -0.4 + 0.8 * (i / max(1, num_tracks - 1))

    # --- Step 2: Render each note ---
    for start_sec, dur, note_num, velocity, ch, program, track_idx in all_notes:
        freq = 440.0 * (2.0 ** ((note_num - 69) / 12.0))
        vel = velocity / 127.0
        start_sample = int(start_sec * sr)
        n_samples = int(dur * sr)
        if start_sample + n_samples > samples:
            n_samples = samples - start_sample
        if n_samples <= 10:
            continue

        t = np.arange(n_samples) / sr

        # === DRUMS (channel 9) ===
        if ch == 9:
            if note_num in [36, 35]:  # kick drum
                n = min(n_samples, int(0.25 * sr))
                t_k = np.arange(n) / sr
                freq_env = np.linspace(180, 35, n)
                wave = np.sin(2 * np.pi * np.cumsum(freq_env / sr)) * 0.9
                wave += np.random.randn(n) * 0.08
                env = adsr(n, 0.001, 0.05, 0.2, 0.15)
                wave *= env
                # Pad to n_samples
                full = np.zeros(n_samples)
                full[:n] = wave

            elif note_num in [38, 40]:  # snare
                n = min(n_samples, int(0.2 * sr))
                t_s = np.arange(n) / sr
                tone = np.sin(2 * np.pi * 185 * t_s) * 0.35
                noise = np.random.randn(n) * 0.55
                noise = hpf(noise, 2000, sr)
                env = adsr(n, 0.001, 0.03, 0.15, 0.12)
                full = np.zeros(n_samples)
                full[:n] = (tone + noise) * env

            elif note_num in [42, 44]:  # closed hi-hat
                n = min(n_samples, int(0.08 * sr))
                noise = np.random.randn(n) * 0.35
                noise = hpf(noise, 6000, sr)
                env = adsr(n, 0.001, 0.01, 0.1, 0.04)
                full = np.zeros(n_samples)
                full[:n] = noise * env

            elif note_num in [46]:  # open hi-hat
                n = min(n_samples, int(0.3 * sr))
                noise = np.random.randn(n) * 0.28
                noise = hpf(noise, 5000, sr)
                env = adsr(n, 0.002, 0.05, 0.3, 0.2)
                full = np.zeros(n_samples)
                full[:n] = noise * env

            elif note_num in [49, 51, 52, 55, 57]:  # crash/ride cymbals
                n = min(n_samples, int(0.6 * sr))
                noise = np.random.randn(n) * 0.22
                noise = hpf(noise, 3000, sr)
                # Metallic shimmer: ring modulation
                t_c = np.arange(n) / sr
                shimmer = np.sin(2 * np.pi * 3750 * t_c) * 0.08
                env = adsr(n, 0.005, 0.1, 0.2, 0.4)
                full = np.zeros(n_samples)
                full[:n] = (noise + shimmer) * env

            elif note_num in [45, 43, 41, 47, 48]:  # toms
                n = min(n_samples, int(0.3 * sr))
                t_t = np.arange(n) / sr
                f_env = np.linspace(freq * 1.5, freq * 0.7, n)
                wave = np.sin(2 * np.pi * np.cumsum(f_env / sr)) * 0.6
                env = adsr(n, 0.001, 0.04, 0.2, 0.15)
                full = np.zeros(n_samples)
                full[:n] = wave * env

            else:  # generic percussion
                n = min(n_samples, int(0.15 * sr))
                full = np.zeros(n_samples)
                full[:n] = np.random.randn(n) * 0.2 * adsr(n, 0.001, 0.02, 0.1, 0.08)

            wave = full * vel * 0.5

        # === SYNTH BASS (programs 38-39) ===
        elif program in range(38, 40):
            saw = saw_wave(freq, t, sr)
            sub = np.sin(2 * np.pi * freq * 0.5 * t)
            # Filter envelope: cutoff sweeps down
            env = adsr(n_samples, 0.005, 0.15, 0.4, 0.1)
            filt_env = adsr(n_samples, 0.005, 0.2, 0.3, 0.15)
            raw = saw * 0.55 + sub * 0.45
            # Animate cutoff: high at attack, lower at sustain
            cutoff_base = 800
            cutoff_peak = 3500
            cutoffs = cutoff_base + (cutoff_peak - cutoff_base) * filt_env
            # Apply time-varying filter via blocks
            block_size = 512
            filtered = np.zeros(n_samples)
            for b in range(0, n_samples, block_size):
                end = min(b + block_size, n_samples)
                co = cutoffs[b]
                filtered[b:end] = lpf(raw[b:end], co, sr)
            wave = filtered * env * vel * 0.5

        # === DISTORTION GUITAR / STABS (programs 29-31) ===
        elif program in range(29, 32):
            saw = saw_wave(freq, t, sr)
            saw2 = saw_wave(freq * 1.003, t, sr)  # slight detune
            raw = (saw + saw2) * 0.5
            wave = np.tanh(raw * 4) * 0.35  # harder distortion
            env = adsr(n_samples, 0.005, 0.08, 0.6, 0.1)
            wave = lpf(wave, 5000, sr) * env * vel * 0.4

        # === STRINGS / ENSEMBLE (programs 48-55) ===
        elif program in range(48, 56):
            # 5-voice unison with slow LFO vibrato
            lfo = 1 + 0.004 * np.sin(2 * np.pi * 4.5 * t)
            detunes = [0.996, 0.998, 1.0, 1.002, 1.004]
            wave = np.zeros(n_samples)
            for d in detunes:
                wave += np.sin(2 * np.pi * freq * d * lfo * t) * 0.2
            wave += np.sin(2 * np.pi * freq * 2 * t) * 0.08  # octave harmonic
            wave = chorus(wave, depth=0.003, rate=1.2)
            env = adsr(n_samples, 0.08, 0.2, 0.7, 0.3)
            wave = lpf(wave, 6000, sr) * env * vel * 0.4

        # === SYNTH LEAD (programs 80-87) ===
        elif program in range(80, 88):
            saw1 = saw_wave(freq, t, sr)
            saw2 = saw_wave(freq * 1.005, t, sr)  # detune
            sq = np.sign(np.sin(2 * np.pi * freq * t))
            raw = saw1 * 0.35 + saw2 * 0.25 + sq * 0.2
            # Filter with envelope
            env = adsr(n_samples, 0.01, 0.1, 0.65, 0.15)
            filt_env = adsr(n_samples, 0.01, 0.15, 0.4, 0.2)
            cutoff = 2000 + 4000 * filt_env
            block_size = 512
            filtered = np.zeros(n_samples)
            for b in range(0, n_samples, block_size):
                end = min(b + block_size, n_samples)
                filtered[b:end] = lpf(raw[b:end], cutoff[b], sr)
            wave = filtered * env * vel * 0.45

        # === SYNTH PAD (programs 88-95) ===
        elif program in range(88, 96):
            lfo1 = 1 + 0.005 * np.sin(2 * np.pi * 0.3 * t)
            lfo2 = 1 + 0.004 * np.sin(2 * np.pi * 0.7 * t + 1.0)
            wave = (np.sin(2 * np.pi * freq * lfo1 * t) * 0.25 +
                    np.sin(2 * np.pi * freq * 2 * lfo2 * t) * 0.12 +
                    np.sin(2 * np.pi * freq * 0.5 * t) * 0.18 +
                    np.sin(2 * np.pi * freq * 1.002 * t) * 0.15)
            wave = chorus(wave, depth=0.004, rate=0.8)
            env = adsr(n_samples, 0.15, 0.3, 0.7, 0.5)
            wave = lpf(wave, 4000, sr) * env * vel * 0.4

        # === WOODWINDS / FLUTE (programs 71-79) ===
        elif program in range(71, 80):
            vibrato = 1 + 0.006 * np.sin(2 * np.pi * 5 * t)
            wave = (np.sin(2 * np.pi * freq * vibrato * t) * 0.5 +
                    np.sin(2 * np.pi * freq * 2 * t) * 0.15 +
                    np.sin(2 * np.pi * freq * 3 * t) * 0.04)
            # Breath noise
            breath = np.random.randn(n_samples) * 0.03
            breath = hpf(breath, 3000, sr)
            wave += breath
            env = adsr(n_samples, 0.05, 0.1, 0.7, 0.15)
            wave = wave * env * vel * 0.45

        # === CELLO / BOWED STRINGS (programs 40-47) ===
        elif program in range(40, 48):
            vibrato = 1 + 0.005 * np.sin(2 * np.pi * 5.5 * t)
            saw = saw_wave(freq * vibrato, t, sr)
            wave = saw * 0.5 + np.sin(2 * np.pi * freq * t) * 0.2
            env = adsr(n_samples, 0.06, 0.15, 0.65, 0.2)
            wave = lpf(wave, 3000, sr) * env * vel * 0.4

        # === DEFAULT: warm sine with harmonics ===
        else:
            wave = (np.sin(2 * np.pi * freq * t) * 0.4 +
                    np.sin(2 * np.pi * freq * 2 * t) * 0.15 +
                    np.sin(2 * np.pi * freq * 3 * t) * 0.05)
            env = adsr(n_samples, 0.02, 0.1, 0.6, 0.15)
            wave = wave * env * vel * 0.4

        # Apply panning and add to stereo buffer
        pan = track_pans.get(track_idx, 0.0)
        gain_L = np.sqrt(0.5 * (1.0 - pan))
        gain_R = np.sqrt(0.5 * (1.0 + pan))

        end_sample = start_sample + n_samples
        if end_sample <= samples:
            audio_L[start_sample:end_sample] += wave * gain_L
            audio_R[start_sample:end_sample] += wave * gain_R

    # --- Step 3: Master processing ---

    # Vectorized reverb using scipy IIR filter (comb + allpass, no per-sample loops)
    def comb_reverb(signal, delay_ms, feedback, sr=44100):
        """IIR comb filter: y[n] = x[n] + feedback * y[n-d]."""
        d = int(delay_ms * sr / 1000)
        b = np.zeros(d + 1)
        b[0] = 1.0
        a = np.zeros(d + 1)
        a[0] = 1.0
        a[d] = -feedback
        return lfilter(b, a, signal)

    def allpass_reverb(signal, delay_ms, gain, sr=44100):
        """IIR allpass: y[n] = -g*x[n] + x[n-d] + g*y[n-d]."""
        d = int(delay_ms * sr / 1000)
        b = np.zeros(d + 1)
        b[0] = -gain
        b[d] = 1.0
        a = np.zeros(d + 1)
        a[0] = 1.0
        a[d] = -gain
        return lfilter(b, a, signal)

    for ch_audio in [audio_L, audio_R]:
        reverb_sum = np.zeros(samples)
        comb_delays = [29.7, 37.1, 41.1, 43.7]  # ms (prime-ish for density)
        for delay_ms in comb_delays:
            reverb_sum += comb_reverb(ch_audio, delay_ms, 0.7) * 0.25
        reverb_sum = allpass_reverb(reverb_sum, 5.0, 0.7)
        reverb_sum = allpass_reverb(reverb_sum, 1.7, 0.7)
        ch_audio += reverb_sum * 0.20

    # Stereo width enhancement: slight mid/side processing
    mid_sig = (audio_L + audio_R) * 0.5
    side_sig = (audio_L - audio_R) * 0.5
    audio_L = mid_sig + side_sig * 1.2
    audio_R = mid_sig - side_sig * 1.2

    # Soft-knee limiter
    audio_L = np.tanh(audio_L * 1.3) * 0.85
    audio_R = np.tanh(audio_R * 1.3) * 0.85

    # Normalize stereo
    peak = max(np.max(np.abs(audio_L)), np.max(np.abs(audio_R)))
    if peak > 0:
        audio_L = audio_L / peak * 0.9
        audio_R = audio_R / peak * 0.9

    # Interleave to stereo 16-bit
    stereo = np.column_stack([audio_L, audio_R])
    audio_16 = (stereo * 32767).astype(np.int16)
    wavfile.write(wav_path, sr, audio_16)
    print(f"    -> Stereo WAV: {os.path.getsize(wav_path) // 1024} KB")
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
