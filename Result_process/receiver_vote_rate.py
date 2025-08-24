import re
import pandas as pd
import matplotlib.pyplot as plt

# 读取 Receiver 日志文件
with open("database/receiver_drift.txt", "r", encoding="utf-8") as f:
    lines = f.readlines()

# 初始化
vote_results = []
current_vote = None

for line in lines:
    line = line.strip()
    if "Sent vote:" in line:
        if "VOTE_YES2" in line:
            current_vote = "VOTE_YES2"
        elif "VOTE_NO2" in line:
            current_vote = "VOTE_NO2"
    elif "[Receiver] Next basic chord pair" in line:
        vote_results.append(current_vote if current_vote else "Blank")
        current_vote = None  # 重置

# 统计
vote_counts = pd.Series(vote_results).value_counts()
print("投票统计：")
print(vote_counts)

# 保存为 Excel
df = vote_counts.rename_axis("Vote Result").reset_index(name="Count")
df.to_excel("result/vote_result_piechart.xlsx", index=False)

# 绘制饼状图
plt.figure(figsize=(6, 6))
plt.pie(df["Count"], labels=df["Vote Result"], autopct='%1.1f%%', startangle=90)
plt.title("Vote Result Distribution")
plt.tight_layout()
plt.savefig("result/vote_piechart.png")
plt.show()