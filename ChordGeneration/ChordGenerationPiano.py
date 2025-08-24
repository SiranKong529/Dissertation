import os
import subprocess

# 替换为你真实的 SoundFont 文件路径
soundfont_path = "Piano.sf2"
output_dir = "output_chords_piano"
os.makedirs(output_dir, exist_ok=True)

# 和弦类型与半音间隔
chord_types = {
    "maj7": [0, 4, 7, 11],
    "min7": [0, 3, 7, 10],
    "7": [0, 4, 7, 10],
    "minMaj7": [0, 3, 7, 11],
    "dim7": [0, 3, 6, 9],
}

note_names = ['C', 'C#', 'D', 'D#', 'E', 'F',
              'F#', 'G', 'G#', 'A', 'A#', 'B']

base_midi = 60  # C4

for i, root in enumerate(note_names):
    root_midi = base_midi + i
    for chord_name, intervals in chord_types.items():
        notes = [root_midi + interval for interval in intervals]

        # 计算相对于 C 的半音序列（0-11）
        relative_intervals = [(i + intervals[j]) % 12 for j in range(4)]
        interval_name = "_".join(str(x) for x in relative_intervals)

        midi_path = f"{interval_name}.mid"
        wav_path = os.path.join(output_dir, f"{interval_name}.wav")

        # MIDI 文件头
        midi_header = [
            0x4d, 0x54, 0x68, 0x64,
            0x00, 0x00, 0x00, 0x06,
            0x00, 0x00,
            0x00, 0x01,
            0x00, 0x60
        ]

        # MIDI 轨道
        track_data = [0x00, 0xC0, 0x00]  # Program Change
        for n in notes:
            track_data += [0x00, 0x90, n, 100]
        for n in notes:
            track_data += [0x60, 0x80, n, 0]
        track_data += [0x00, 0xFF, 0x2F, 0x00]

        # Track Header
        track_header = [0x4d, 0x54, 0x72, 0x6b]
        length = len(track_data)
        track_header += [
            (length >> 24) & 0xFF,
            (length >> 16) & 0xFF,
            (length >> 8) & 0xFF,
            length & 0xFF
        ]

        # 写入 .mid
        with open(midi_path, "wb") as f:
            f.write(bytes(midi_header + track_header + track_data))

        # 使用 fluidsynth 渲染
        cmd = [
            "fluidsynth",
            "-ni",
            "-g", "3.0",
            soundfont_path,
            midi_path,
            "-F", wav_path,
            "-T", "wav"
        ]
        print(f"🎼 渲染 {interval_name}.wav ...")
        subprocess.run(cmd, check=True)

        os.remove(midi_path)

print("✅ 所有和弦文件生成完毕！")