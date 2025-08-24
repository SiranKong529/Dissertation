import re
import pandas as pd

# 文件路径
log_path = "database/receiver_drift.txt"

# 数据存储结构
data = []

with open(log_path, "r", encoding="utf-8") as file:
    current = {
        "round": None,
        "drift_ms": None,
        "vote": None,
        "chord_pair": None,
        "sent_time": None
    }
    for line in file:
        line = line.strip()

        # 匹配 Drift
        m = re.match(r"\[Drift\] Round (\d+): Drift = (\-?\d+) ms", line)
        if m:
            if current["round"] is not None:
                data.append(current)
                current = {
                    "round": None,
                    "drift_ms": None,
                    "vote": None,
                    "chord_pair": None,
                    "sent_time": None
                }
            current["round"] = int(m.group(1))
            current["drift_ms"] = int(m.group(2))
            continue

        # 匹配 Vote
        m = re.match(r"\[Receiver\] Sent vote: (VOTE_[A-Z]+[0-9]+)", line)
        if m:
            current["vote"] = m.group(1)
            continue

        # 匹配和弦对
        m = re.match(r"\[Receiver\].*chord pair: (.+)", line)
        if m:
            current["chord_pair"] = m.group(1)
            continue

        # 匹配发送时间
        m = re.match(r"🎵 Sent to M5 at: (\d+)", line)
        if m:
            current["sent_time"] = int(m.group(1))

    # 最后一组
    if current["round"] is not None:
        data.append(current)

# 转成DataFrame
df = pd.DataFrame(data)
print(df)

# 保存为Excel
df.to_excel("receiver_drift_analysis.xlsx", index=False)