import csv
import os
import sys
import time

import numpy as np
from LinUCB import *
from EpsilonGreedy import *

curPath = os.path.abspath(os.path.dirname(__file__))
rootPath = os.path.split(curPath)[0]
sys.path.append(rootPath)
from util import *

NUM_RESOURCES_CACHE = 60  #total units of cache size


def for_epsilon_greedy():
    cm = simulation_config_management()
    curr_config = cm.receive_config()

    all_app = curr_config[0]
    num_resources = int(np.sum(curr_config[1]))
    chosen_arm = {}
    curr_allocate = curr_config[1]
    for j in range(len(all_app)):
        chosen_arm[all_app[j]] = curr_allocate[j]

    # initialize parameters
    epsilon = 0.5
    factor_alpha = 0.98
    epochs = 3000
    ucb_cache = EpsilonGreedy(epsilon, factor_alpha, len(all_app), num_resources)

    file_ = open('linucb.log', 'w', newline='')
    start_time = time.time()
    for i in range(epochs):
        performance = curr_config[2]
        th_reward = ucb_cache.get_now_reward(performance)
        ucb_cache.update(th_reward, chosen_arm)
        # choose an arm
        chosen_arm = ucb_cache.select_arm()
        # prepare new config
        new_config = [curr_config[0], chosen_arm]
        cm.send_config(new_config)

        # waiting for result
        curr_config = cm.receive_config()

        # write to log
        log_info = str(i) + ' ' + str(th_reward) + '\n'
        file_.write(log_info)
        if (i + 1) % 100 == 0:
            print('epoch [{} / {}]'.format(i + 1, epochs))
    end_time = time.time()
    print('used time :{}'.format(end_time - start_time))
    file_.close()


def for_linucb():
    cm = simulation_config_management()
    curr_config = cm.receive_config()

    all_app = curr_config[0]
    num_resources = int(np.sum(curr_config[1]))

    alpha = 0.90
    factor_alpha = 0.98
    n_features = 10
    epochs = 1000
    ucb_cache = LinUCB(all_app, num_resources, alpha, factor_alpha, n_features)

    file_ = open('linucb.log', 'w', newline='')
    start_time = time.time()
    for i in range(epochs):
        curr_allocate = curr_config[1]
        performance = curr_config[2]
        context_info = curr_config[3:]
        chosen_arm = {}
        for j in range(len(all_app)):
            chosen_arm[all_app[j]] = curr_allocate[j]

        th_reward = ucb_cache.get_now_reward(performance, context_info)
        ucb_cache.update(th_reward, chosen_arm)

        new_arm = ucb_cache.select_arm()
        # prepare new config
        new_config = [curr_config[0], new_arm]
        cm.send_config(new_config)

        # waiting for result
        curr_config = cm.receive_config()
        # write to log
        log_info = str(i) + ' ' + str(th_reward) + '\n'
        file_.write(log_info)
        if (i + 1) % 100 == 0:
            print('epoch [{} / {}]'.format(i + 1, epochs))


    end_time = time.time()
    print('used time :{}'.format(end_time - start_time))
    file_.close()


def for_linucb_sample():
    cm = simulation_config_management()
    curr_config = cm.receive_config()

    all_app = curr_config[0]
    num_resources = int(np.sum(curr_config[1]))

    alpha = 0.95
    factor_alpha = 0.98
    n_features = 10
    epochs = 3000
    ucb_cache = LinUCB(all_app, num_resources, alpha, factor_alpha, n_features, True)

    file_ = open('linucb.log', 'w', newline='')
    start_time = time.time()
    for i in range(epochs):
        new_arm = ucb_cache.select_arm()
        # prepare new config
        new_config = [curr_config[0], new_arm]
        cm.send_config(new_config)

        # waiting for result
        curr_config = cm.receive_config()

        curr_allocate = curr_config[1]
        performance = curr_config[2]
        context_info = curr_config[3:]
        chosen_arm = {}
        for j in range(len(all_app)):
            chosen_arm[all_app[j]] = curr_allocate[j]

        th_reward = ucb_cache.get_now_reward(performance, context_info)
        ucb_cache.update(th_reward, chosen_arm)

        # write to log
        log_info = str(i) + ' ' + str(th_reward) + '\n'
        file_.write(log_info)
        if (i + 1) % 100 == 0:
            print('epoch [{} / {}]'.format(i + 1, epochs))

    end_time = time.time()
    print('used time :{}'.format(end_time - start_time))
    file_.close()


if __name__ == '__main__':
    # for_epsilon_greedy()
    for_linucb_sample()
    # cm = simulation_config_management()
    # curr_config = cm.receive_config()
    #
    # num_apps = len(curr_config[0])
    # num_resources = [NUM_RESOURCES_CACHE]
    # all_cache_config = gen_all_config(num_apps, num_resources[0])
    # num_config = len(all_cache_config)
    #
    # epochs = 1000
    # file_ = open('linucb.log', 'w', newline='')
    #
    # app_name = curr_config[0]
    # cache_config = curr_config[1]
    # cache_hit = curr_config[2]
    # cache_reward = curr_config[3]
    # init_cache_index = all_cache_config.index(cache_config)
    #
    # # context infomation and reward
    # context, th_reward = get_now_reward(cache_hit)
    #
    # # initialize parameters
    # alpha = 0.999
    # factor_alpha = 0.997
    #
    # n_features_cache = len(context)
    # context_cache = np.array(context)
    #
    # num_arm = num_config
    # lin_ucb_cache = LinUCB(alpha, num_arm, n_features_cache)
    # # lin_ucb_cache = EpsilonGreedy(alpha, num_arm, n_features_cache)
    #
    # lin_ucb_cache.update(init_cache_index, th_reward, context_cache)
    # start_time = time.time()
    # for i in range(epochs):
    #     context, th_reward = get_now_reward(cache_hit)
    #     # print('----- reward is: ' + str(th_reward))
    #     context_cache = np.array(context)
    #     # choose a arm
    #     chosen_arm_cache = lin_ucb_cache.select_arm(context_cache, factor_alpha)
    #     cache_index = chosen_arm_cache
    #     # prepare new config
    #     new_config = []
    #     new_config.append(curr_config[0])
    #     new_partition = all_cache_config[cache_index]
    #     new_config.append(new_partition)
    #
    #     # print('new config:' + str(new_config))
    #     cm.send_config(new_config)
    #
    #     # waiting for result
    #     curr_config = cm.receive_config()
    #     # print('recv_config is: ' + str(curr_config))
    #     app_name = curr_config[0]
    #     cache_config = curr_config[1]
    #     cache_hit = curr_config[2]
    #     cache_reward = curr_config[3]
    #
    #     context, th_reward = get_now_reward(cache_hit)
    #     context_cache = np.array(context)
    #     lin_ucb_cache.update(chosen_arm_cache, th_reward, context_cache)
    #     # write to log
    #     log_info = str(i) + ' ' + str(th_reward) + '\n'
    #     file_.write(log_info)
    #     if (i + 1) % 100 == 0:
    #         print('epoch [{} / {}]'.format(i + 1, epochs))
    # end_time = time.time()
    # print('used time :{}'.format(end_time-start_time))
    # file_.close()
    
