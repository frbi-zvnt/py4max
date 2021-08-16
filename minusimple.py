import numpy as np
import random


def callist():
    return [1.123123123123123, 2, 3, 4, 5, 6, 7]


def casual():
    return random.randrange(100)


def random_matrix(n):
    return np.random.rand(n).tolist()


def add_list(lst):
    return (np.array(lst) * 0.5).tolist()
