import random
import numpy as np
from itertools import zip_longest
from ScheduleFrame import *


def gen_feasible_configs(num_of_cache, cache_top_k):
    # TODO：根据各个应用的top_k整理出的所有可行的结果
    num_app = len(cache_top_k)
    top_k = len(cache_top_k[0])

    def gen_side(tmp, k, n=1):
        """
        :param root: root node, first app node
        :param n: n_th in top_k
        :return:
        """
        if n == num_app:
            return [[]]
        for k in range(top_k):
            t1 = k * top_k ** (num_app - n - 1)
            t2 = top_k ** (num_app - 1) - (top_k - k - 1) * top_k ** (num_app - n - 1)
            delta2 = top_k ** (num_app - 1 - n)
            delta1 = top_k ** (num_app - n)
            for i in range(t1, t2, delta1):
                for j in range(i, i + delta2):
                    app_core = cache_top_k[n][k]
                    feasible_core = num_of_cache - sum(tmp[j])
                    if app_core > feasible_core:
                        tmp[j].append(feasible_core)
                    else:
                        tmp[j].append(app_core)

        gen_side(n=n + 1, tmp=tmp, k=k)
        return tmp

    all_feasible_config = []
    for i in range(len(cache_top_k)):
        cache_top_k.append(cache_top_k.pop(0))
        for j in range(top_k):
            tmp = [[cache_top_k[0][j]] for _ in range(top_k ** (num_app - 1))]
            res = gen_side(tmp, k=0)
            for k in range(i + 1):
                res = [[row[-1]] + row[:-1] for row in res]
            all_feasible_config.extend(res)

    # 先去重
    unique_tuples = set(tuple(x) for x in all_feasible_config)
    all_feasible_config = [list(x) for x in unique_tuples]
    # 对未利用的资源重新分配
    for config in all_feasible_config:
        assert sum(config) <= num_of_cache, 'The allocated cache exceeds the limit'
        if sum(config) < num_of_cache:
            left_cache = num_of_cache - sum(config)
            for i in range(left_cache):
                min_value = min(config)
                min_index = config.index(min_value)
                config[min_index] += 1
    return all_feasible_config


def get_top_k(arr, k, times):
    if times < 5 or random.randint(1, 10) > 8:
        arr_top_k_id = [random.randint(0, len(arr) - 1) for _ in range(k)]
    else:
        arr_top_k_id = np.argsort(arr)[-k:]
    return list(arr_top_k_id)


def beam_search(num_of_cache, all_apps, p_c_t, times, end_condition=30):
    # TODO：从各个子bandit中选出top_k的配置，并进行组合，形成全局的配置
    action = {}.fromkeys(all_apps)
    num_app = len(all_apps)
    top_k = int(10 ** (np.log10(end_condition) / num_app))

    cache_top_k = [get_top_k(p_c_t[all_apps[i]], top_k, times) for i in range(num_app)]
    feasible_configs = gen_feasible_configs(num_of_cache=num_of_cache, cache_top_k=cache_top_k)
    sum_p_list = []
    for config in feasible_configs:
        try:
            config_p = [p_c_t[all_apps[i]][config[i]] for i in range(num_app)]
            sum_p = sum(config_p)
            sum_p_list.append(sum_p)
        except IndexError as e:
            print(f"Caught an IndexError: {e}")
            print(config)
            print(cache_top_k)
    cache_act = feasible_configs[np.argmax(sum_p_list)]
    for i in range(num_app):
        action[all_apps[i]] = cache_act[i]
    return action


def latin_hypercube_sampling(n, d, m, M, ratio):
    # Initialize the samples array
    samples = np.zeros((n, d))
    for i in range(n):
        # Generate d random numbers and normalize them
        while True:
            x = np.random.uniform(m, M, d)
            if np.sum(x) >= M:
                break
        x = x / np.sum(x) * M

        # Check if all elements are in the range [m, M]
        if np.all(x >= m) and np.all(x <= M):
            samples[i] = x
        else:
            # Re-generate the sample if it doesn't satisfy the constraints
            i -= 1
    sample_config = []
    for i in range(len(samples)):
        tmp = [int(ele) for ele in samples[i]]
        curr_sum = sum(tmp)
        tmp[-1] += M - curr_sum
        sample_config.append([ele * ratio for ele in tmp])
    return sample_config


class LayerUCB(ScheduleFrame):
    def __init__(self, all_apps, n_cache, alpha, factor_alpha, n_features):
        super().__init__()
        self.all_apps = all_apps
        self.num_apps = len(all_apps)
        self.arm_dict = {}
        for app in self.all_apps:
            self.arm_dict[app] = list(range(0, n_cache, 5))
        self.n_features = n_features

        self.A_c = {}
        self.b_c = {}
        self.p_c_t = {}

        for app in self.all_apps:
            self.A_c[app] = np.zeros((len(self.arm_dict[app]), self.n_features * 2, self.n_features * 2))
            self.b_c[app] = np.zeros((len(self.arm_dict[app]), self.n_features * 2, 1))
            self.p_c_t[app] = np.zeros(len(self.arm_dict[app]))
            for arm in range(len(self.arm_dict[app])):
                self.A_c[app][arm] = np.eye(self.n_features * 2)

        # contexts
        self.context = {}
        self.other_context = {}
        sum = np.zeros(n_features)
        for app in self.all_apps:
            self.context[app] = [1.0 for _ in range(self.n_features)]
            sum += np.array(self.context[app])
        for app in self.all_apps:
            self.other_context[app] = list((sum - np.array(self.context[app])) / (len(self.all_apps) - 1))

        return

    def select_arm(self):
        if self.sampling_model:
            if self.times == len(self.sample_config):
                # sample end
                self.times += 1
                self.sampling_model = False
                # print(max(self.sample_result))
                return self.sample_config[self.sample_result.index(max(self.sample_result))]
            # initial phase
            cache_allocation = self.sample_config[self.times]
            self.times += 1
            return cache_allocation

        self.times += 1
        if self.duration_period > self.threshold and random.randint(1, 10) < 8:
            cache_allocation = []
            for app in self.all_apps:
                cache_allocation.append(self.curr_best_config[app])
            return cache_allocation
        else:
            contexts = {}
            for app in self.all_apps:
                A = self.A_c[app]
                b = self.b_c[app]
                contexts[app] = np.hstack((self.context[app], self.other_context[app]))

                for i in range(self.n_arms):
                    theta = np.linalg.inv(A[i]).dot(b[i])
                    cntx = np.array(contexts[app])
                    self.p_c_t[app][i] = theta.T.dot(cntx) + self.alpha * np.sqrt(
                        cntx.dot(np.linalg.inv(A[i]).dot(cntx)))
            cache_action = beam_search(self.n_arms - 1, self.all_apps, self.p_c_t, self.times, end_condition=30)

            # cache_action is a dict
            cache_allocation = []
            for app in self.all_apps:
                cache_allocation.append(cache_action[app])

            self.alpha *= self.factor_alpha
            return cache_allocation

    def update(self, reward, chosen_arm):
        if self.sampling_model:
            # initial phase
            self.sample_result.append(float(reward))
        else:
            if self.curr_best_config is None:
                self.curr_best_config = chosen_arm
                self.curr_best_reward = reward
                self.duration_period = 1
                # self.history_reward = []
            else:
                if reward > self.curr_best_reward:
                    self.curr_best_reward = reward
                    self.curr_best_config = chosen_arm
                    self.duration_period = 1
                    # self.history_reward = []
                else:
                    self.duration_period += 1

            contexts = {}
            for app in self.all_apps:
                arm = chosen_arm[app]
                contexts[app] = np.hstack((self.context[app], self.other_context[app]))
                self.A_c[app][arm] += np.outer(np.array(contexts[app]), np.array(contexts[app]))
                self.b_c[app][arm] = np.add(self.b_c[app][arm].T, np.array(contexts[app]) * reward).reshape(self.n_features * 2, 1)

            if self.times > 200:
                if len(self.history_reward) < 60:
                    self.history_reward.append(round(float(reward), 3))
                else:
                    first_half = self.history_reward[:30]
                    second_half = self.history_reward[30:]
                    first_aver = sum(first_half) / len(first_half)
                    second_aver = sum(second_half) / len(second_half)
                    # print('{} --> {},  {}'.format(first_aver, second_aver, abs(second_aver - first_aver) / first_aver))
                    if abs(second_aver - first_aver) / first_aver > 0.20:
                        # workload change
                        print('----- test workload change -----')
                        self.reset()
                    else:
                        self.history_reward = second_half

    def get_now_reward(self, performance, context_info=None):
        # update the context
        tmp = [list(row) for row in zip_longest(*context_info, fillvalue=None)]
        sum_context = np.zeros(self.n_features)
        for i, app in enumerate(self.all_apps):
            self.context[app] = tmp[i]
            sum_context += np.array(self.context[app])
        for app in self.all_apps:
            self.other_context[app] = list((sum_context - np.array(self.context[app])) / (len(self.all_apps) - 1))
        # calculate the reward
        th_reward = sum(float(x) for x in performance) / len(performance)
        return th_reward

    def reset(self):
        self.alpha = self.init_factor[0]
        self.times = 0
        self.sampling_model = True
        self.sample_config = latin_hypercube_sampling(20, self.num_apps, 0, (self.n_arms - 1)//self.ratio, self.ratio)
        self.sample_result = []

        self.curr_best_config = None
        self.curr_best_reward = None
        self.duration_period = 0

        self.history_reward = []

        for app in self.all_apps:
            self.A_c[app] = np.zeros((self.n_arms, self.n_features * 2, self.n_features * 2))
            self.b_c[app] = np.zeros((self.n_arms, self.n_features * 2, 1))
            self.p_c_t[app] = np.zeros(self.n_arms)
            for arm in range(self.n_arms):
                self.A_c[app][arm] = np.eye(self.n_features * 2)


if __name__ == '__main__':
    pass
