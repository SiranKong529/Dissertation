import os
import subprocess

soundfont_path = "Saxophone.sf2"
output_dir = "output_saxophone_notes"
os.makedirs(output_dir, exist_ok=True)

# MIDI notes: C4(60) to B5(83) - total 24 notes, 顺序从低到高
midi_notes = list(range(48, 72))

for i, midi_note in enumerate(midi_notes):
    filename = f"{i}"
    midi_path = f"{filename}.mid"
    wav_path = os.path.join(output_dir, f"{filename}.wav")

    # MIDI header
    midi_header = [
        0x4d, 0x54, 0x68, 0x64,  # 'MThd'
        0x00, 0x00, 0x00, 0x06,  # header length
        0x00, 0x00,              # format 0
        0x00, 0x01,              # one track
        0x00, 0x60               # 96 ticks per quarter note
    ]

    # MIDI track with proper VLQ time to avoid "震动声"
    track_data = [0x00, 0xC0, 0x41]  # program change Alto Sax
    track_data += [0x00, 0x90, midi_note, 127]  # note on
    track_data += [0x81, 0x40, 0x80, midi_note, 0]  # note off after 192 ticks
    track_data += [0x00, 0xFF, 0x2F, 0x00]  # end of track

    # Track header
    length = len(track_data)
    track_header = [
        0x4d, 0x54, 0x72, 0x6b,  # 'MTrk'
        (length >> 24) & 0xFF,
        (length >> 16) & 0xFF,
        (length >> 8) & 0xFF,
        length & 0xFF
    ]

    # Write MIDI file
    with open(midi_path, "wb") as f:
        f.write(bytes(midi_header + track_header + track_data))

    # Render with fluidsynth
    cmd = [
        "fluidsynth",
        "-ni",
        "-g", "2.0",
        soundfont_path,
        midi_path,
        "-F", wav_path,
        "-T", "wav"
    ]
    print(f"🎷 渲染 {filename}.wav...")
    subprocess.run(cmd, check=True)

    os.remove(midi_path)

print("✅ 所有 sax 单音文件 (0~23) 已生成完毕，严格从低到高。")