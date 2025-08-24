import re
import pandas as pd

# 打开 Sender.txt 文件
with open("database/Sender_Markov.txt", "r", encoding="utf-8") as f:
    lines = f.readlines()

# 初始化变量
data = []
round_number = 0
current_sync_time = None
current_pass_rate = None

for line in lines:
    line = line.strip()

    # 捕获 Chord Sync time
    if line.startswith("Chord Sync time:"):
        match = re.search(r"Chord Sync time: (\d+)", line)
        if match:
            current_sync_time = int(match.group(1))

    # 捕获 pass rate
    elif line.startswith("pass rate:"):
        match = re.search(r"pass rate: ([0-9.]+)", line)
        if match:
            current_pass_rate = float(match.group(1))

    # 每当两个都出现时，记录并重置
    if current_sync_time is not None and current_pass_rate is not None:
        data.append({
            "Round": round_number,
            "Chord Sync Time (ms)": current_sync_time,
            "Pass Rate": current_pass_rate
        })
        round_number += 1
        current_sync_time = None
        current_pass_rate = None

# 转换为 DataFrame
df = pd.DataFrame(data)

# 保存为 Excel（可选）
df.to_excel("result/sender_sync_and_passrate_Markov.xlsx", index=False)
print("统计完成，保存为 sender_sync_and_passrate.xlsx")