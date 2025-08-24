import os
import re
import matplotlib.pyplot as plt
import pandas as pd

# 确保输出目录存在
os.makedirs("result", exist_ok=True)

# 路径根据实际调整
filepath = "database/Receiver_Markov.txt"

# 读取文件
with open(filepath, "r", encoding="utf-8") as f:
    lines = f.readlines()

# 解析 drift、vote、failed
drifts = []
round_nums = []
votes = []
failed_rounds = []

current_round = None
for line in lines:
    line = line.strip()
    # Drift 行
    m = re.match(r"\[Drift\] Round (\d+): Drift = (\d+) ms", line)
    if m:
        current_round = int(m.group(1))
        drift_val = int(m.group(2))
        round_nums.append(current_round)
        drifts.append(drift_val)
        continue
    # 标记 vote
    if "Sent vote (Markov):" in line:
        m2 = re.search(r"Sent vote \(Markov\):\s*(VOTE_YES2|VOTE_NO2)", line)
        if m2 and current_round is not None:
            vote = m2.group(1)
            votes.append((current_round, vote))
    # failed
    if "vote failed" in line.lower() and current_round is not None:
        failed_rounds.append(current_round)

# 统计
vote_yes = sum(1 for _, v in votes if "YES" in v)
vote_no = sum(1 for _, v in votes if "NO" in v)
failed = len(failed_rounds)

print(f"VOTE_YES2: {vote_yes}, VOTE_NO2: {vote_no}, failed: {failed}, total drift rounds: {len(drifts)}")

# --------- 饼状图 -------------
labels = ["VOTEYES", "VOTENO", "failed"]
sizes = [vote_yes, vote_no, failed]
plt.figure()
plt.pie(sizes, labels=labels, autopct='%1.1f%%', startangle=90)
plt.title("Vote Outcomes (Markov)")
plt.axis("equal")
plt.tight_layout()
plt.savefig("result/vote_pie_chart_Markov.png")
plt.close()

# --------- 构造 DataFrame -------------
df_drift = pd.DataFrame({"Round": round_nums, "drift_ms": drifts})
df_votes = pd.DataFrame(votes, columns=["Round", "Vote"])
df_failed = pd.DataFrame({"Failed_Round": failed_rounds})

# 按 Round 排序并重置 index，使后面绘图用 df.index 对齐参考风格
df_drift = df_drift.sort_values("Round").reset_index(drop=True)

# --------- IQR 异常值检测 -------------
Q1 = df_drift["drift_ms"].quantile(0.25)
Q3 = df_drift["drift_ms"].quantile(0.75)
IQR = Q3 - Q1
lower_bound = Q1 - 1.5 * IQR
upper_bound = Q3 + 1.5 * IQR

df_drift["is_outlier"] = (df_drift["drift_ms"] < lower_bound) | (df_drift["drift_ms"] > upper_bound)

# 保存带异常标记的结果（Markov 版本）
outlier_excel_path = "result/receiver_drift_with_outliers_Markov.xlsx"
df_drift.to_excel(outlier_excel_path, index=False)

# 打印统计
print("异常值数量:", df_drift["is_outlier"].sum())
print(f"保存为 {outlier_excel_path}")

# --------- 绘图: drift + IQR（风格对齐参考） -------------
plt.figure(figsize=(10, 6))
plt.plot(df_drift.index, df_drift["drift_ms"], label='Drift (ms)', marker='o', linestyle='-')

# IQR、bounds 线（颜色/线型参考原始代码）
plt.axhline(Q1, color='green', linestyle='--', label='Q1 (25th percentile)')
plt.axhline(Q3, color='blue', linestyle='--', label='Q3 (75th percentile)')
plt.axhline(lower_bound, color='red', linestyle=':', label='Lower Bound (Q1 - 1.5×IQR)')
plt.axhline(upper_bound, color='red', linestyle=':', label='Upper Bound (Q3 + 1.5×IQR)')

# 标注异常点（空心点）
#outliers = df_drift[df_drift["is_outlier"]]
#if not outliers.empty:
    #plt.scatter(outliers.index, outliers["drift_ms"], edgecolors='black', facecolors='none', s=100,
                #label="Outliers", linewidths=1.5)

# 美化
plt.xlabel("Round")
plt.ylabel("Drift (ms)")
plt.title("Chord Synchronisation Drift with IQR Bounds")
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig("result/drift_with_IQR_lines_Markov.png", dpi=300)
plt.close()

# --------- 额外可选: 去除异常后的折线（保留原逻辑） -------------
df_no_outliers = df_drift[~df_drift["is_outlier"]]
plt.figure()
plt.plot(df_no_outliers["Round"], df_no_outliers["drift_ms"], marker='o', linestyle='-')
# 添加 IQR 范围线
plt.axhline(Q1, color='green', linestyle='--', label='Q1 (25th percentile)')
plt.axhline(Q3, color='blue', linestyle='--', label='Q3 (75th percentile)')
plt.axhline(lower_bound, color='red', linestyle=':', label='Lower Bound (Q1 - 1.5×IQR)')
plt.axhline(upper_bound, color='red', linestyle=':', label='Upper Bound (Q3 + 1.5×IQR)')

plt.xlabel("Round")
plt.ylabel("Drift (ms)")
plt.title("Chord Synchronisation Drift with IQR Bounds(Markov process)")
plt.grid(True)
plt.legend()
#plt.tight_layout()
plt.savefig("result/drift_no_outliers_Markov.png", dpi=300)
plt.close()

# --------- 保存原始表格和其他数据 -------------
df_drift.to_csv("result/drift_by_round_Markov.csv", index=False)
df_votes.to_csv("result/votes_Markov.csv", index=False)
# df_failed.to_csv("result/failed_rounds_Markov.csv", index=False)

# --------- 控制台输出摘要 -------------
print("\n前10轮 Drift:")
print(df_drift.head(10))
print("\nVotes 分布:")
print(df_votes.value_counts())
print("\nFailed Rounds 频次:")
print(df_failed["Failed_Round"].value_counts())

print("\n图和数据都已保存到 result/ 目录下。")