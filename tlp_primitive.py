import random
import math

# puzzle is generated like this
N = 13 * 17
T_Base = 3
T = 2**T_Base
phi = 12 * 16
exp = (2**T) % phi
x = 13

# solver does this
y1 = x
for i in range(T):
    y1 = (y1 * y1) % N

# verifier does this
assert(y1 == pow(x, exp, N))