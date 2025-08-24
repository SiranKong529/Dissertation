import pandas as pd
import matplotlib.pyplot as plt

# 读取 Excel 文件
df = pd.read_excel("result/receiver_drift_with_outliers.xlsx")

# 只保留非异常值的数据
df_clean = df[df['is_outlier'] == False]

# 绘制折线图
plt.figure(figsize=(10, 6))
plt.plot(df_clean['round'], df_clean['drift_ms'], marker='o', linestyle='-')
plt.title('Drift over Rounds (Outliers Removed)')
plt.xlabel('Round')
plt.ylabel('Drift (ms)')
plt.ylim(0, 50)
plt.yticks(range(0, 51, 10))
plt.grid(True)
plt.tight_layout()
plt.show()