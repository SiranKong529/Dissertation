import os
import subprocess

soundfont_path = "Drum.sf2"
output_dir = "output_swing_drums"
os.makedirs(output_dir, exist_ok=True)

# 乐器对应的 midi note
instruments = {
    "ride": 51,
    "snare": 38,
    "hihat": 44,
    "kick": 36
}

for name, note in instruments.items():
    midi_path = f"{name}.mid"
    wav_path = os.path.join(output_dir, f"{name}.wav")

    # MIDI header
    midi_header = [
        0x4d, 0x54, 0x68, 0x64,
        0x00, 0x00, 0x00, 0x06,
        0x00, 0x00,
        0x00, 0x01,
        0x00, 0x60
    ]

    track_data = []
    track_data += [0x00, 0xB9, 0x07, 0x64]  # set volume

    # 只打一下
    track_data += [0x00, 0x99, note, 100]  # Note on
    track_data += [0x60, 0x89, note, 0]    # Note off after 96 ticks

    track_data += [0x00, 0xFF, 0x2F, 0x00]  # End of track

    length = len(track_data)
    track_header = [
        0x4d, 0x54, 0x72, 0x6b,
        (length >> 24) & 0xFF,
        (length >> 16) & 0xFF,
        (length >> 8) & 0xFF,
        length & 0xFF
    ]

    # 写入 MIDI 文件
    with open(midi_path, "wb") as f:
        f.write(bytes(midi_header + track_header + track_data))

    # 使用 fluidsynth 渲染
    cmd = [
        "fluidsynth",
        "-ni",
        "-g", "2.0",
        soundfont_path,
        midi_path,
        "-F", wav_path,
        "-T", "wav"
    ]
    print(f"🥁 渲染 {name}.wav...")
    subprocess.run(cmd, check=True)
    os.remove(midi_path)

print("✅ 所有单击鼓 wav 已生成！（每个只打一下）")