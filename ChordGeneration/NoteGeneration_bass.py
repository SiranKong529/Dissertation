import os
import subprocess

# SoundFont 文件路径，可以换成 bass sf2，比如 "Bass.sf2"
soundfont_path = "Bass Guitars.sf2"

# 输出目录
output_dir = "output_bass_notes"
os.makedirs(output_dir, exist_ok=True)

# Note 名称与 MIDI 对应（C = 0）
note_names = ['C', 'C#', 'D', 'D#', 'E', 'F',
              'F#', 'G', 'G#', 'A', 'A#', 'B']

# 生成从 C3(48) 到 B3(59)
for octave in range(3, 4):  # 只生成 C3 到 B3
    for idx, note in enumerate(note_names):
        midi_note = 12 * octave + idx
        filename = f"{idx}"      # 用 0,1,2,3...
        midi_path = f"{filename}.mid"
        wav_path = os.path.join(output_dir, f"{filename}.wav")

        # MIDI 文件头
        midi_header = [
            0x4d, 0x54, 0x68, 0x64,
            0x00, 0x00, 0x00, 0x06,
            0x00, 0x00,
            0x00, 0x01,
            0x00, 0x60
        ]

        # MIDI 轨道
        track_data = [0x00, 0xC0, 0x32]  # program change 50 = Acoustic Bass
        track_data += [0x00, 0x90, midi_note, 100]  # note on
        track_data += [0x60, 0x80, midi_note, 0]    # note off
        track_data += [0x00, 0xFF, 0x2F, 0x00]      # end of track

        # Track Header
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

        # 使用 fluidsynth 渲染为 WAV
        cmd = [
            "fluidsynth",
            "-ni",
            "-g", "2.0",
            soundfont_path,
            midi_path,
            "-F", wav_path,
            "-T", "wav"
        ]
        print(f"🎵 渲染 {filename}.wav...")
        subprocess.run(cmd, check=True)

        os.remove(midi_path)

print("✅ 所有 bass 单音文件生成完毕！")