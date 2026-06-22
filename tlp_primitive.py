#=====================================================
# puzzle is generated like this
# phi and exp are kept secret
#-----------------------------------------------------

N = 13 * 17             # modulus
T_exp = 3
T = 2**T_exp            # T defines difficulty
phi = 12 * 16
exp = (2**T) % phi      # allows shortcut for verifier
x = 13


#=====================================================
# solver does this
# and knows only x, T and N
#-----------------------------------------------------

y = pow(x, 2**T, N)             # runs in O(T)


#=====================================================
# verifier does this
#-----------------------------------------------------
assert(y == pow(x, exp, N))     # runs in O(log(exp))   
