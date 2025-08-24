import pandas as pd
import matplotlib.pyplot as plt

# 读取 Excel 文件
df = pd.read_excel("result/receiver_drift_analysis.xlsx")

# 计算 IQR
Q1 = df['drift_ms'].quantile(0.25)
Q3 = df['drift_ms'].quantile(0.75)
IQR = Q3 - Q1

# 计算下界和上界
lower_bound = Q1 - 1.5 * IQR
upper_bound = Q3 + 1.5 * IQR

# 标记是否为异常值
df['is_outlier'] = (df['drift_ms'] < lower_bound) | (df['drift_ms'] > upper_bound)

# 保存标记后的结果为新文件
df.to_excel("result/receiver_drift_with_outliers.xlsx", index=False)

# 打印统计信息
print("异常值数量:", df['is_outlier'].sum())
print("保存为 receiver_drift_with_outliers.xlsx")

# ----------- 绘图部分 ------------
plt.figure(figsize=(10, 6))
plt.plot(df.index, df['drift_ms'], label='Drift (ms)', marker='o', linestyle='-')

# 添加 IQR 范围线
plt.axhline(Q1, color='green', linestyle='--', label='Q1 (25th percentile)')
plt.axhline(Q3, color='blue', linestyle='--', label='Q3 (75th percentile)')
plt.axhline(lower_bound, color='red', linestyle=':', label='Lower Bound (Q1 - 1.5×IQR)')
plt.axhline(upper_bound, color='red', linestyle=':', label='Upper Bound (Q3 + 1.5×IQR)')

# 图表美化
plt.xlabel('Round')
plt.ylabel('Drift (ms)')
plt.title('Chord Synchronisation Drift with IQR Bounds')
plt.legend()
plt.grid(True)

# 保存图像
plt.savefig("result/drift_with_IQR_lines.png", dpi=300)
plt.show()