#!/usr/bin/env python3
"""Compose the Zone 4 and Zone 5 MIDI loops for Riftwalker."""

from __future__ import annotations

from collections.abc import Sequence
from dataclasses import dataclass
from pathlib import Path

import mido
from mido import Message, MetaMessage, MidiFile, MidiTrack

PPQ = 480
BEATS_PER_BAR = 4

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_DIR = SCRIPT_DIR.parent
MIDI_DIR = PROJECT_DIR / "assets" / "music" / "midi"

NOTE_MAP = {
    "C": 0,
    "C#": 1,
    "Db": 1,
    "D": 2,
    "D#": 3,
    "Eb": 3,
    "E": 4,
    "F": 5,
    "F#": 6,
    "Gb": 6,
    "G": 7,
    "G#": 8,
    "Ab": 8,
    "A": 9,
    "A#": 10,
    "Bb": 10,
    "B": 11,
}


@dataclass(frozen=True)
class NoteEvent:
    start: float
    pitch: int
    duration: float
    velocity: int


@dataclass(frozen=True)
class ChordBlock:
    chord: Sequence[int]
    root: int
    bars: int


def note(name: str, octave: int) -> int:
    """Convert a note name and octave to a MIDI pitch."""
    return NOTE_MAP[name] + (octave + 1) * 12


def beats_to_ticks(beats: float) -> int:
    return round(beats * PPQ)


def add_chord(
    notes: list[NoteEvent],
    pitches: Sequence[int],
    start: float,
    duration: float,
    velocity: int,
) -> None:
    for pitch in pitches:
        notes.append(NoteEvent(start, pitch, duration, velocity))


def expand_roots(blocks: Sequence[ChordBlock]) -> list[int]:
    roots: list[int] = []
    for block in blocks:
        roots.extend([block.root] * block.bars)
    return roots


def append_note_events(
    track: MidiTrack,
    notes: Sequence[NoteEvent],
    channel: int,
) -> None:
    events: list[tuple[int, int, int, Message]] = []
    for index, event in enumerate(notes):
        on_tick = beats_to_ticks(event.start)
        off_tick = beats_to_ticks(event.start + event.duration)
        velocity = max(1, min(127, event.velocity))
        events.append(
            (
                on_tick,
                1,
                index,
                Message(
                    "note_on",
                    note=event.pitch,
                    velocity=velocity,
                    channel=channel,
                    time=0,
                ),
            )
        )
        events.append(
            (
                off_tick,
                0,
                index,
                Message(
                    "note_off",
                    note=event.pitch,
                    velocity=0,
                    channel=channel,
                    time=0,
                ),
            )
        )

    current_tick = 0
    for tick, _, _, message in sorted(events, key=lambda item: item[:3]):
        message.time = tick - current_tick
        track.append(message)
        current_tick = tick


def make_track(
    name: str,
    program: int,
    channel: int,
    notes: Sequence[NoteEvent],
    *,
    tempo_bpm: int | None = None,
    volume: int = 100,
    pan: int = 64,
    reverb: int = 70,
    chorus: int = 24,
) -> MidiTrack:
    track = MidiTrack()
    track.append(MetaMessage("track_name", name=name, time=0))
    if tempo_bpm is not None:
        track.append(MetaMessage("set_tempo", tempo=mido.bpm2tempo(tempo_bpm), time=0))
        track.append(
            MetaMessage(
                "time_signature",
                numerator=4,
                denominator=4,
                clocks_per_click=24,
                notated_32nd_notes_per_beat=8,
                time=0,
            )
        )
    track.append(Message("program_change", program=program, channel=channel, time=0))
    track.append(
        Message(
            "control_change",
            control=7,
            value=max(0, min(127, volume)),
            channel=channel,
            time=0,
        )
    )
    track.append(
        Message(
            "control_change",
            control=10,
            value=max(0, min(127, pan)),
            channel=channel,
            time=0,
        )
    )
    track.append(
        Message(
            "control_change",
            control=91,
            value=max(0, min(127, reverb)),
            channel=channel,
            time=0,
        )
    )
    track.append(
        Message(
            "control_change",
            control=93,
            value=max(0, min(127, chorus)),
            channel=channel,
            time=0,
        )
    )
    append_note_events(track, notes, channel)
    return track


def compose_zone4() -> MidiFile:
    """Entropy Cascade: dark F minor loop with unstable pad figuration."""
    bpm = 95
    mid = MidiFile(ticks_per_beat=PPQ)
    blocks = [
        ChordBlock(
            [note("F", 2), note("C", 3), note("F", 3), note("Ab", 3), note("Bb", 3)],
            note("F", 2),
            2,
        ),
        ChordBlock(
            [note("Db", 2), note("Ab", 2), note("Db", 3), note("F", 3), note("G", 3)],
            note("Db", 2),
            2,
        ),
        ChordBlock(
            [note("B", 1), note("F", 2), note("B", 2), note("D", 3)],
            note("B", 1),
            2,
        ),
        ChordBlock(
            [note("C", 2), note("G", 2), note("Bb", 2), note("E", 3), note("Gb", 3)],
            note("C", 2),
            2,
        ),
        ChordBlock(
            [note("F", 2), note("C", 3), note("F", 3), note("Ab", 3)],
            note("F", 2),
            2,
        ),
        ChordBlock(
            [note("Eb", 2), note("Bb", 2), note("Eb", 3), note("Gb", 3), note("Ab", 3)],
            note("Eb", 2),
            2,
        ),
        ChordBlock(
            [note("Db", 2), note("Ab", 2), note("Db", 3), note("F", 3), note("B", 3)],
            note("Db", 2),
            2,
        ),
        ChordBlock(
            [note("C", 2), note("G", 2), note("Bb", 2), note("Db", 3), note("E", 3)],
            note("C", 2),
            2,
        ),
        ChordBlock(
            [note("F", 2), note("C", 3), note("F", 3), note("Ab", 3), note("B", 3)],
            note("F", 2),
            2,
        ),
        ChordBlock(
            [note("Ab", 1), note("Eb", 2), note("Ab", 2), note("B", 2), note("Eb", 3)],
            note("Ab", 1),
            2,
        ),
        ChordBlock(
            [note("C", 2), note("F", 2), note("Gb", 2), note("Bb", 2), note("E", 3)],
            note("C", 2),
            2,
        ),
        ChordBlock(
            [note("F", 2), note("C", 3), note("F", 3), note("Ab", 3), note("C", 4)],
            note("F", 2),
            2,
        ),
    ]

    pad_notes: list[NoteEvent] = []
    start = 0.0
    for block in blocks:
        duration = block.bars * BEATS_PER_BAR
        add_chord(pad_notes, block.chord, start, duration - 0.15, 46)
        start += duration

    roots = expand_roots(blocks)
    cascade_notes: list[NoteEvent] = []
    pulse_notes: list[NoteEvent] = []
    for bar, root in enumerate(roots):
        bar_start = bar * BEATS_PER_BAR
        base = root + 24
        intervals = [0, 7, 3, 10, 6, 7, 12, 6]
        if bar % 8 >= 4:
            intervals = [12, 6, 7, 3, 0, 10, 6, 7]
        if bar % 6 in (2, 3):
            intervals = [0, 6, 7, 3, 10, 6, 12, 7]

        for step in range(16):
            pitch = base + intervals[(step + bar) % len(intervals)]
            if pitch > note("C", 6):
                pitch -= 12
            start_beat = bar_start + step * 0.25
            accent = step % 4 == 0
            velocity = 58 if accent else 42
            if step in (7, 15):
                velocity += 8
            cascade_notes.append(NoteEvent(start_beat, pitch, 0.16, velocity))

            if step in (3, 11) and bar % 4 == 2:
                tension = pitch + 6 if pitch <= note("F", 5) else pitch - 6
                cascade_notes.append(NoteEvent(start_beat, tension, 0.12, 34))

        pulse_notes.append(NoteEvent(bar_start, root, 1.45, 62))
        pulse_notes.append(NoteEvent(bar_start + 2.0, root + 12, 0.65, 52))
        if bar % 4 == 3:
            pulse_notes.append(NoteEvent(bar_start + 3.5, root + 6, 0.25, 46))

    mid.tracks.append(
        make_track(
            "Entropy Cascade Pad",
            95,
            0,
            pad_notes,
            tempo_bpm=bpm,
            volume=88,
            pan=52,
            reverb=92,
            chorus=42,
        )
    )
    mid.tracks.append(
        make_track(
            "Entropy Cascade Figuration",
            88,
            1,
            cascade_notes,
            volume=76,
            pan=82,
            reverb=82,
            chorus=56,
        )
    )
    mid.tracks.append(
        make_track(
            "Entropy Cascade Pulse",
            89,
            2,
            pulse_notes,
            volume=82,
            pan=64,
            reverb=76,
            chorus=36,
        )
    )
    return mid


def compose_zone5() -> MidiFile:
    """Sovereign's Domain: slow imperial D minor loop with a B minor detour."""
    bpm = 78
    mid = MidiFile(ticks_per_beat=PPQ)
    blocks = [
        ChordBlock(
            [note("D", 2), note("A", 2), note("D", 3), note("F", 3), note("A", 3)],
            note("D", 2),
            2,
        ),
        ChordBlock(
            [note("Bb", 1), note("F", 2), note("Bb", 2), note("D", 3), note("F", 3)],
            note("Bb", 1),
            2,
        ),
        ChordBlock(
            [note("G", 1), note("D", 2), note("G", 2), note("Bb", 2), note("D", 3)],
            note("G", 1),
            2,
        ),
        ChordBlock(
            [note("A", 1), note("E", 2), note("G", 2), note("C#", 3), note("E", 3)],
            note("A", 1),
            2,
        ),
        ChordBlock(
            [note("B", 1), note("F#", 2), note("B", 2), note("D", 3), note("F#", 3)],
            note("B", 1),
            2,
        ),
        ChordBlock(
            [note("E", 2), note("B", 2), note("E", 3), note("G", 3), note("B", 3)],
            note("E", 2),
            2,
        ),
        ChordBlock(
            [note("F#", 1), note("C#", 2), note("E", 2), note("A#", 2), note("C#", 3)],
            note("F#", 1),
            2,
        ),
        ChordBlock(
            [note("B", 1), note("F#", 2), note("B", 2), note("D", 3), note("F#", 3)],
            note("B", 1),
            2,
        ),
        ChordBlock(
            [note("D", 2), note("A", 2), note("D", 3), note("F", 3), note("A", 3)],
            note("D", 2),
            2,
        ),
        ChordBlock(
            [note("Bb", 1), note("F", 2), note("Bb", 2), note("D", 3), note("F", 3)],
            note("Bb", 1),
            2,
        ),
        ChordBlock(
            [note("A", 1), note("E", 2), note("G", 2), note("C#", 3), note("E", 3)],
            note("A", 1),
            2,
        ),
        ChordBlock(
            [note("D", 2), note("A", 2), note("D", 3), note("F", 3), note("A", 3)],
            note("D", 2),
            2,
        ),
    ]

    pad_notes: list[NoteEvent] = []
    start = 0.0
    for block in blocks:
        duration = block.bars * BEATS_PER_BAR
        add_chord(pad_notes, block.chord, start, duration - 0.2, 52)
        start += duration

    melody_notes = [
        NoteEvent(0.0, note("A", 4), 1.5, 76),
        NoteEvent(2.0, note("D", 5), 1.0, 72),
        NoteEvent(3.0, note("C", 5), 0.75, 66),
        NoteEvent(8.0, note("Bb", 4), 1.5, 72),
        NoteEvent(10.0, note("A", 4), 1.0, 68),
        NoteEvent(12.0, note("G", 4), 2.0, 70),
        NoteEvent(16.0, note("D", 5), 1.25, 78),
        NoteEvent(18.0, note("F", 5), 0.75, 72),
        NoteEvent(20.0, note("E", 5), 1.5, 70),
        NoteEvent(24.0, note("C#", 5), 1.0, 76),
        NoteEvent(26.0, note("E", 5), 1.0, 70),
        NoteEvent(28.0, note("A", 4), 2.0, 72),
        NoteEvent(32.0, note("F#", 4), 1.5, 74),
        NoteEvent(34.0, note("B", 4), 1.0, 72),
        NoteEvent(36.0, note("D", 5), 1.5, 76),
        NoteEvent(40.0, note("E", 5), 1.0, 70),
        NoteEvent(42.0, note("G", 5), 1.0, 72),
        NoteEvent(44.0, note("F#", 5), 1.75, 76),
        NoteEvent(48.0, note("C#", 5), 1.0, 72),
        NoteEvent(50.0, note("A#", 4), 1.0, 70),
        NoteEvent(52.0, note("F#", 4), 2.0, 74),
        NoteEvent(56.0, note("B", 4), 1.5, 76),
        NoteEvent(58.0, note("A", 4), 0.75, 68),
        NoteEvent(60.0, note("F#", 4), 2.0, 72),
        NoteEvent(64.0, note("A", 4), 1.0, 76),
        NoteEvent(66.0, note("D", 5), 1.25, 78),
        NoteEvent(68.0, note("F", 5), 1.0, 72),
        NoteEvent(72.0, note("Bb", 4), 1.5, 70),
        NoteEvent(74.0, note("D", 5), 1.0, 74),
        NoteEvent(76.0, note("F", 5), 1.0, 76),
        NoteEvent(80.0, note("C#", 5), 1.0, 76),
        NoteEvent(82.0, note("E", 5), 0.75, 72),
        NoteEvent(84.0, note("A", 4), 1.5, 70),
        NoteEvent(88.0, note("D", 5), 2.0, 80),
        NoteEvent(92.0, note("A", 4), 1.0, 72),
        NoteEvent(94.0, note("D", 5), 1.75, 78),
    ]

    drum_notes: list[NoteEvent] = []
    kick = 36
    snare = 38
    closed_hat = 42
    low_tom = 41
    for bar in range(24):
        bar_start = bar * BEATS_PER_BAR
        drum_notes.append(NoteEvent(bar_start, kick, 0.18, 92))
        drum_notes.append(NoteEvent(bar_start + 1.0, snare, 0.18, 72))
        drum_notes.append(NoteEvent(bar_start + 2.0, kick, 0.18, 86))
        drum_notes.append(NoteEvent(bar_start + 3.0, snare, 0.18, 76))
        for beat in range(4):
            drum_notes.append(NoteEvent(bar_start + beat, closed_hat, 0.08, 34))
        if bar % 4 == 3:
            drum_notes.append(NoteEvent(bar_start + 3.5, low_tom, 0.18, 74))

    mid.tracks.append(
        make_track(
            "Sovereign Domain Pad",
            95,
            0,
            pad_notes,
            tempo_bpm=bpm,
            volume=92,
            pan=56,
            reverb=90,
            chorus=38,
        )
    )
    mid.tracks.append(
        make_track(
            "Sovereign Domain Brass",
            61,
            1,
            melody_notes,
            volume=102,
            pan=68,
            reverb=74,
            chorus=20,
        )
    )
    mid.tracks.append(
        make_track(
            "Sovereign Domain March",
            0,
            9,
            drum_notes,
            volume=92,
            pan=64,
            reverb=58,
            chorus=0,
        )
    )
    return mid


def main() -> None:
    MIDI_DIR.mkdir(parents=True, exist_ok=True)
    outputs = {
        "zone4.mid": compose_zone4(),
        "zone5.mid": compose_zone5(),
    }
    for filename, midi in outputs.items():
        path = MIDI_DIR / filename
        midi.save(path)
        print(f"Wrote {path}")


if __name__ == "__main__":
    main()
