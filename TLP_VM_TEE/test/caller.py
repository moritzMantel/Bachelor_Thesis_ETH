#
# example usage of rate_limiting.so
#
import ctypes
import json

#
# we first need to load the library and define they c types
#
lib = ctypes.CDLL("/Users/moritzmantel/00_code/00_ethz/06_FS26/00_BT/BT-Implementation/TLP_VM_TEE/build/rate_limiter.so")

lib.request.argtypes     = [ctypes.c_char_p, ctypes.c_ulong]
lib.request.restype      = None
lib.request_sol.argtypes = [ctypes.c_char_p, ctypes.c_ulong, ctypes.c_char_p]
lib.request_sol.restype  = None
lib.buf_size.argtypes = []
lib.buf_size.restype  = ctypes.c_size_t

init = False
def init_lib():
    """
    Initialises the library (needs to be called first)
    """
    lib.init()
    global init
    init = True

def teardown_lib():
    """
    Cleans up the library (needs to be called last)
    """
    lib.teardown()

def request(e):
    """
    Requests the puzzle for an element.
    Initialises the lib if not done already.

    Params:
        * e:            requested element
    
    Returns:
        * dict          containing the puzzle (x, T, N)
    """

    if not init:
        init_lib()
    puzzle_buf = ctypes.create_string_buffer(lib.buf_size())
    lib.request(puzzle_buf, e)
    return json.loads(puzzle_buf.value.decode())

def solve(puzzle):
    """
    Solves the puzzle.

    Params:
        * puzzle:           dict containing x, T, N
    
    Returns:
        * solution y = x^2^T mod N
    """

    return pow(int(puzzle["x"], 16), 2**int(puzzle["T"], 16), int(puzzle["N"], 16))

def submit(e, Y):
    """
    Submit a request for an element including the solution.
    Initialises the lib if not done already.
    
    Params:
        * e:		        requested element
        * Y:		        solution y = e^2^T in hex

    Returs:
        * "1"               if e is in S
        * "0"               if e is not in S
        * "NO MEMORY"       if malloc fails
        * "NOT ACCEPTED"    if Y is not a valid solution
    """

    if not init:
        init_lib()
    result_buf = ctypes.create_string_buffer(lib.buf_size())
    lib.request_sol(result_buf, e, Y.encode())
    return result_buf.value.decode()


if __name__ == "__main__":
    # sample usecase: one full protocol requesting an element

    init_lib()
    print("Choose element...\n>")
    e = int(input())
    p = request(e)
    print(f"Puzzle:\n{p}")
    y = solve(p)
    print(f"Solution: {hex(y)}")
    res = submit(e, hex(y))
    if res == "1":
        print(f"{e} is in S")
    elif res == "0":
        print(f"{e} is not in S")
    else:
        print(res)
    teardown_lib()