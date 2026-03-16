from client import send_request, send_solve, solve_vdf, request_URL, solve_URL
import time

elem = 1000
num_trials = 10 
x, T, N = send_request(request_URL, elem)

t1 = time.time_ns()
for i in range(num_trials):
    solve_vdf(x, T, N)
t2 = time.time_ns()

delta_time = (t2 - t1) / (num_trials * 1_000_000)

print("Time for T =", T,
      delta_time, "ms",
      "(", delta_time / 1_000,  "s)")