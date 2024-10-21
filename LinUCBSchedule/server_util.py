import numpy as np

# unet3d x0 设置为 3000，对于 resnet50 x0 设置为 30000
# s3fs x0 设置为13000, k = 0.0003
def sigmoid(x, k=0.0003, x0=13000):
    return 1 / (1 + np.exp(-k * (x - x0)))
# 接收curve发送的request，解析为pool_and_size, pool_and_throughput
def process_request(request):
    pairs = request.split(';')[:-1]
    pool_and_size = {}
    pool_and_throughput = {}
    pool_and_invoke = {}
    pair1 = pairs[:2]
    pair2 = pairs[2:4]
    pair3 = pairs[4:]
    for pair in pair1:
        key,value = pair.split(':')
        pool_and_size[key] = int(value)
    for pair in pair2:
        key,value = pair.split(':')
        pool_and_throughput[key] = float(value)
    for pair in pair3:
        key,value = pair.split(':')
        pool_and_invoke[key] = sigmoid(float(value))
    return pool_and_size, pool_and_throughput, pool_and_invoke