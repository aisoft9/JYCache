import numpy as np
import matplotlib.pyplot as plt

# 生成数据
x = np.linspace(0, 20000, 1000)

# Sigmoid 函数
def sigmoid(x, k=0.0003, x0=13000):
    return 1 / (1 + np.exp(-k * (x - x0)))

# Tanh 函数
def tanh(x, k=0.0005, x0=3000):
    return (np.tanh(k * (x - x0)) + 1) / 2

# 指数归一化函数
def exponential_normalize(x, alpha=0.5):
    return (x / 6000) ** alpha

# Arctan 函数
def arctan(x, k=0.001, x0=3000):
    return 0.5 + (1 / np.pi) * np.arctan(k * (x - x0))

# 映射到 0-1
y1 = sigmoid(x)
y2 = tanh(x)
y3 = exponential_normalize(x)
y4 = arctan(x)

# 绘制图像
plt.plot(x, y1, label='sigmoid')
plt.plot(x, y2, label='tanh')
plt.plot(x, y3, label='exponential_normalize')
plt.plot(x, y4, label='arctan')
plt.legend()
plt.title("Function Mapping")
plt.xlabel("Original Values")
plt.ylabel("Mapped Values (0-1)")
plt.savefig('../figures/activate_fun.png')
