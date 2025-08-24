import pandas as pd
import matplotlib.pyplot as plt

# 读取文件
df = pd.read_excel("result/receiver_drift_analysis.xlsx")

# 计算 IQR
Q1 = df['drift_ms'].quantile(0.25)
Q3 = df['drift_ms'].quantile(0.75)
IQR = Q3 - Q1
lower_bound = Q1 - 1.5 * IQR
upper_bound = Q3 + 1.5 * IQR

# 标记异常值
df['is_outlier'] = (df['drift_ms'] < lower_bound) | (df['drift_ms'] > upper_bound)

# 去除异常值
df_filtered = df[~df['is_outlier']].copy()

# 重新计算去除异常值后的 Q1/Q3（可选，保持原始 IQR 可读性也合理）
# Q1_filtered = df_filtered['drift_ms'].quantile(0.25)
# Q3_filtered = df_filtered['drift_ms'].quantile(0.75)

# 绘图
plt.figure(figsize=(10, 6))
plt.plot(df_filtered.index, df_filtered['drift_ms'], label='Drift (ms)', marker='o', linestyle='-')

# 添加 IQR 范围线（原始 IQR）
plt.axhline(Q1, color='green', linestyle='--', label='Q1 (25th percentile)')
plt.axhline(Q3, color='blue', linestyle='--', label='Q3 (75th percentile)')
plt.axhline(lower_bound, color='red', linestyle=':', label='Lower Bound (Q1 - 1.5×IQR)')
plt.axhline(upper_bound, color='red', linestyle=':', label='Upper Bound (Q3 + 1.5×IQR)')

# 图表设置
plt.xlabel('Round')
plt.ylabel('Drift (ms)')
plt.title('Chord Sync Drift Without Outliers (With IQR Bounds)')
plt.legend()
plt.grid(True)

# 保存图像
plt.savefig("result/drift_no_outliers_with_IQR.png", dpi=300)
plt.show()