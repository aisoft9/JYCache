# The number of hypercube samples in the initial stage
SAMPLE_TIMES = 20

# Ratio of sample, if ratio is 10 and num_cache is 100, valid solution in sample is 0, 10, 20, 30 ...... 100
SAMPLE_RATIO = 10

# If no better config is found within threshold,
# it is determined that the model has entered a state of approximate convergence
APPROXIMATE_CONVERGENCE_THRESHOLD = 150

# In approximate convergence state,There is a certain probability to directly choose the optimal solution
# that has been explored, otherwise continue to explore
PROBABILITY_THRESHOLD = 80

# It takes time for rewards to level off, only after this threshold, detection begins for load changes
LOAD_CHANGE_THRESHOLD = 50

# The size of history reward sliding window
HISTORY_REWARD_WINDOW = 50

# Basis for workload changes
WORKLOAD_CHANGE = 0.30

