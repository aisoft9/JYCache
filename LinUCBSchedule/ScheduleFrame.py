from abc import ABC, abstractmethod
import numpy as np
# from scipy.special import gamma

class ScheduleFrame:
    def __init__(self):
        pass

    @abstractmethod
    def select_arm(self):
        pass

    @abstractmethod
    def update(self, reward, chosen_arm):
        pass

    @abstractmethod
    def get_now_reward(self, performance, context_info=None):
        pass


def latin_hypercube_sampling(n, d, m, M):
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
    for i in range(len(samples)):
        samples[i] = [int(ele) for ele in samples[i]]
        curr_sum = sum(samples[i])
        samples[i][-1] += M - curr_sum

    return samples


if __name__ == '__main__':
    # Parameters
    num_samples = 20  # Number of samples
    num_dimensions = 3  # Number of dimensions
    min_value = 0  # Minimum value of each element
    total_sum = 240  # Total sum of elements in each sample

    # Generate samples
    samples = latin_hypercube_sampling(num_samples, num_dimensions, min_value, total_sum)
    print(samples)

    pass
