import re
import pandas as pd
import matplotlib.pyplot as plt

def parse_sync_only(file_path, label):
    with open(file_path, "r", encoding="utf-8") as f:
        lines = f.readlines()

    sync_times = []
    round_number = 1
    for line in lines:
        line = line.strip()
        if line.startswith("Chord Sync time:"):
            m = re.search(r"Chord Sync time:\s*(\d+)", line)
            if m:
                sync_times.append({"Round": round_number, f"{label}_SyncTime": int(m.group(1))})
                round_number += 1

    return pd.DataFrame(sync_times)

# 解析两个日志
df_def  = parse_sync_only("database/Sender.txt",        "Default")
df_mark = parse_sync_only("database/Sender_Markov.txt", "Markov")

# ---------- 图：Sync Time 对比 ----------
plt.figure(figsize=(10,6))

plt.plot(df_def["Round"], df_def["Default_SyncTime"], label="Default Sync Time", linewidth=1)
plt.plot(df_mark["Round"], df_mark["Markov_SyncTime"], label="Markov Sync Time", linewidth=1)

plt.xlabel("Round")
plt.ylabel("Chord Sync Time (ms)")
plt.ylim(45,60)
plt.title("Chord Sync Time Comparison")
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig("result/sender_sync_time_comparison.png", dpi=300)
plt.close()

print("保存：result/sender_sync_time_comparison.png")

default_var = df_def["Default_SyncTime"].var()
markov_var  = df_mark["Markov_SyncTime"].var()

print(f"Default Sync Time Variation: {default_var:.4f}")
print(f"Markov  Sync Time Variation: {markov_var:.4f}")