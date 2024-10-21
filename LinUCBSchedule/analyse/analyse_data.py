import matplotlib.pyplot as plt

if __name__ == '__main__':
    log_name = '../Logs/linucb.log'
    index = []
    reward = []
    count = 1
    # with open(log_name, 'r') as log_file:
    #     for line in log_file.readlines():
    #         # idx, rew = line.split(' ')
    #         rew = line
    #         idx = count
    #         index.append(idx)
    #         reward.append(rew)
    #         count += 1
    with open(log_name, 'r') as log_file:
        for line in log_file.readlines():
            if 'reward' in line:
                idx = count
                reward_str = line.split(':')[1].split('[')[0].strip()  # 提取 reward 数值部分
                reward.append(float(reward_str))
                index.append(idx)
                count += 1
    reward = [float(i) for i in reward]
    # best_score = 0.32       
    best_score = reward[0] * 1.2    
    target = [best_score] * len(index)
    init_score = [reward[0]] * len(index)

    plt.plot(index, reward)
    plt.plot(index, target)
    plt.plot(index, init_score)
    # 设置刻度
    plt.xticks(list(range(0, int(index[-1]), int(int(index[-1])/20))), rotation = 45)
    plt.yticks([i * 0.2 for i in range(0, int(best_score // 0.2) + 2, 1)])
    plt.title('OLUCB')
    # plt.show()
    plt.savefig('../figures/OLUCB_JYCache.png')

