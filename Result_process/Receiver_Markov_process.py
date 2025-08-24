import re
import matplotlib.pyplot as plt
import pandas as pd

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

# 饼状图
labels = ["VOTEYES", "VOTENO", "failed"]
sizes = [vote_yes, vote_no, failed]
plt.figure()
plt.pie(sizes, labels=labels, autopct='%1.1f%%', startangle=90)
plt.title("Vote Outcomes")
plt.axis("equal")
plt.tight_layout()
plt.savefig("result/vote_pie_chart_Markov.png")
plt.close()

# drift 折线图
plt.figure()
plt.plot(round_nums, drifts)
plt.xlabel("Round")
plt.ylabel("Drift (ms)")
plt.title("Drift Over Rounds")
plt.grid(True)
plt.tight_layout()
plt.savefig("result/drift_line_chart_Markov.png")
plt.close()

# DataFrame 输出（方便进一步分析）
df_drift = pd.DataFrame({"Round": round_nums, "Drift_ms": drifts})
df_votes = pd.DataFrame(votes, columns=["Round", "Vote"])
df_failed = pd.DataFrame({"Failed_Round": failed_rounds})

print("\nDrift by Round:")
print(df_drift.head(10))
print("\nVotes:")
print(df_votes.value_counts())
print("\nFailed Rounds:")
print(df_failed["Failed_Round"].value_counts())

# 你可以再根据需要保存这些表格，例如：
df_drift.to_csv("result/drift_by_round_Markov.csv", index=False)
#df_votes.to_csv("result/votes.csv", index=False)
#df_failed.to_csv("failed_rounds.csv", index=False)