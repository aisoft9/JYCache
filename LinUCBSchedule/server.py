import csv
import os
import sys
import threading
import socket
import time
import numpy as np
from LinUCB import *

curPath = os.path.abspath(os.path.dirname(__file__))
rootPath = os.path.split(curPath)[0]
sys.path.append(rootPath)
from server_util import *

NUM_RESOURCES_CACHE = 352  #total units of cache size 176*2

# initialize LinUCB parameters
alpha = 0.95
factor_alpha = 0.98
num_features = 1   # 缓冲池特征维度
# all_app = ['WritePool', 'ReadPool', 'FlushPool']
all_app = ['WritePool', 'ReadPool']

# 收到客户端发送的配置信息后，使用LinUCB,生成最新的配置信息，再发送给客户端
def handle_client(client_socket, ucb_cache, file_writer):
    try:
        # 从curve接收 pool_name, pool_size, pool_throughput, write read 函数的调用次数
        request = client_socket.recv(1024).decode("utf-8")
        print(f"Received: {request}")
        pool_and_size, pool_and_throughput, pool_and_invoke = process_request(request)
        pool_name = list(pool_and_size.keys())
        pool_size = list(pool_and_size.values())
        pool_throughput = list(pool_and_throughput.values())
        context_info = [list(pool_and_invoke.values())]

        if(np.sum(pool_throughput) > 0):
            start_time = time.time()
            chosen_arm = {}
            for j in range(len(all_app)):
                chosen_arm[all_app[j]] = pool_size[j]
            # reward
            th_reward = ucb_cache.get_now_reward(pool_throughput, context_info)

            ucb_cache.update(th_reward, chosen_arm)

            new_size = ucb_cache.select_arm()
            end_time = time.time()

            # write to log
            start_time_log = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(start_time))
            end_time_log = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(end_time))
            file_writer.write('start time : ' + start_time_log + '\n')
            run_time = end_time - start_time
            file_writer.write(f'run time : {run_time:.2f} s\n')
            file_writer.write('end time : ' + end_time_log + '\n')
            
            log_info =  'reward : ' + str(th_reward) + ' ' + str(new_size) + '\n'
            file_writer.write(log_info)
        else:
            new_size = pool_size

        # prepare new config
        new_config = [pool_name, new_size]
        print('new config:' + str(new_config))

        # server发送最新的配置信息给curve
        serialized_data = '\n'.join([' '.join(map(str, row)) for row in new_config])
        client_socket.send(serialized_data.encode())
        print("server发送最新配置给client:")
        print(serialized_data.encode())
    # except Exception as e:
    #     print(f"An error occurred: {e}")
    finally:
        client_socket.close()
    
# 启动server以监听curve发送的配置信息
def start_server():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.bind(("0.0.0.0", 2333))
    # 允许同时最多5个客户端排队等待连接
    server.listen(5)
    print("Server listening on port 2333")

    # 初始化LinUCB
    file_ = open('Logs/linucb.log', 'w', newline='')
    num_resources = NUM_RESOURCES_CACHE
    # 开启采样
    ucb_cache = LinUCB(all_app, num_resources, alpha, factor_alpha, num_features, True)

    while True:
        client_socket, addr = server.accept()
        print(f"Accepted connection from {addr}")
        client_handler = threading.Thread(
            target=handle_client, args=(client_socket, ucb_cache, file_)
        )
        client_handler.start()

    file_.close()

if __name__ == '__main__':
    start_server()