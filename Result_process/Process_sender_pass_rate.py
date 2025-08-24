import re
import pandas as pd
import matplotlib.pyplot as plt

def parse_sender_log(file_path, label):
    with open(file_path, "r", encoding="utf-8") as f:
        lines = f.readlines()

    data = []
    round_number = 1

    for line in lines:
        line = line.strip()
        if line.startswith("pass rate:"):
            match = re.search(r"pass rate: ([0-9.]+)", line)
            if match:
                pass_rate = float(match.group(1))
                data.append({
                    "Round": round_number,
                    label: pass_rate
                })
                round_number += 1

    return pd.DataFrame(data)

# 解析两个文件
df_default = parse_sender_log("database/Sender.txt", "Default")
df_markov = parse_sender_log("database/Sender_Markov.txt", "Markov")

# 合并，按 Round 对齐
df = pd.merge(df_default, df_markov, on="Round", how="outer").sort_values("Round")

# 补齐到 101 轮
df = df.set_index("Round").reindex(range(1, 102))

# 缺失值用前一轮的值填充（forward fill）
df = df.ffill()

# 如果第一行还缺失（比如日志从 round 2 开始），用 0 填
df = df.fillna(0)

# 保存结果 Excel
df.to_excel("result/sender_passrate_compare_filled.xlsx")
print("保存完成：sender_passrate_compare_filled.xlsx")

# 画折线图
plt.figure(figsize=(10,6))
plt.plot(df.index, df["Default"], marker="o", label="Default")
plt.plot(df.index, df["Markov"], marker="s", label="Markov")

# 理论通过率参考线
plt.axhline(y=0.437, color="blue", linestyle="--", linewidth=1, label="Default Theoretical (43.7%)")
plt.axhline(y=0.55, color="orange", linestyle="--", linewidth=1, label="Markov Theoretical (55%)")

plt.xlabel("Round")
plt.ylabel("Pass Rate")
plt.title("Pass Rate Comparison (Default vs Markov)")
plt.legend()
plt.grid(True)
plt.tight_layout()

# 保存图片
plt.savefig("result/sender_passrate_compare.png", dpi=300)
print("图片保存完成：sender_passrate_compare.png")

plt.show()