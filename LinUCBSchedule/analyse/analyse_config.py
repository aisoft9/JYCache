import re
import matplotlib.pyplot as plt

with open('../Logs/linucb.log', 'r', encoding='utf-8') as file:
    text = file.read()

result = [[float(x) for x in match] for match in re.findall(r'reward : ([\d\.]+) \[(\d+), (\d+)\]', text)]

rewards =  [item[0] for item in result]
write_pool_sizes = [item[1] for item in result]
read_pool_sizes = [item[2] for item in result]
# flush_pool_sizes = [item[3] for item in result]

def putAll():
    plt.figure(figsize=(15, 6))

    plt.plot(write_pool_sizes, label='WritePool Size', marker='o', markersize=5, alpha=0.5)
    plt.plot(read_pool_sizes, label='ReadPool Size', marker='s', markersize=5, alpha=0.8)
    # plt.plot(flush_pool_sizes, label='FlushPool Size', marker='^', markersize=5, alpha=0.8)

    plt.title('Pool Sizes Over Time')
    plt.xlabel('Time (Epoch)')
    plt.ylabel('Pool Size')

    plt.legend()

    plt.grid(True)

    # plt.show()
    plt.savefig('../figures/newPoolSize.png', dpi=300)

def separate():
    # Plotting the data in a 2x2 grid
    fig, axs = plt.subplots(2, 2, figsize=(18, 10))

    # Write Pool size plot
    axs[0, 0].plot(write_pool_sizes, marker='o', linestyle='-', color='blue')
    axs[0, 0].set_title('Write Pool Size Over Time')
    axs[0, 0].set_xlabel('Iteration')
    axs[0, 0].set_ylabel('Write Pool Size')
    axs[0, 0].grid(True)

    # Read Pool size plot
    axs[0, 1].plot(read_pool_sizes, marker='^', linestyle='-', color='green')
    axs[0, 1].set_title('Read Pool Size Over Time')
    axs[0, 1].set_xlabel('Iteration')
    axs[0, 1].set_ylabel('Read Pool Size')
    axs[0, 1].grid(True)

    # Flush Pool size plot
    # axs[1, 0].plot(flush_pool_sizes, marker='+', linestyle='-', color='red')
    # axs[1, 0].set_title('Flush Pool Size Over Time')
    # axs[1, 0].set_xlabel('Iteration')
    # axs[1, 0].set_ylabel('Flush Pool Size')
    # axs[1, 0].grid(True)

    # Reward plot
    axs[1, 1].plot(rewards, marker='*', linestyle='-', color='purple')
    axs[1, 1].set_title('Reward Over Time')
    axs[1, 1].set_xlabel('Iteration')
    axs[1, 1].set_ylabel('Reward')
    axs[1, 1].grid(True)

    plt.tight_layout()

    plt.savefig('../figures/pool_sizes_separate.png', dpi=300)

def calculate_greater_than_ratio(pool_sizes, average_size=80):
    count_greater= sum(1 for size in pool_sizes if size > average_size)
    total_count = len(pool_sizes)
    ratio = count_greater / total_count
    return ratio

def calculate_all_ratio():
    average_size = 120 # 单机unet3d专属
    train_point = 20 # 训练开始的节点
    write_pool_ratio = calculate_greater_than_ratio(write_pool_sizes[train_point:], average_size)
    read_pool_ratio = calculate_greater_than_ratio(read_pool_sizes[train_point:], average_size)
    # flush_pool_ratio = calculate_greater_than_ratio(flush_pool_sizes[train_point:], average_size)

    print(f'WritePool > 120 Ratio: {write_pool_ratio:.2%}')
    print(f'ReadPool > 120 Ratio: {read_pool_ratio:.2%}')
    # print(f'FlushPool > 120 Ratio: {flush_pool_ratio:.2%}')

if __name__ == '__main__':
    separate()
    for i in result:
        print(i)