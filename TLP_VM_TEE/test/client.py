#
# Example usage of the server
#

import requests

request_URL = "http://localhost:8080/request_TLP"
solve_URL = "http://localhost:8080/solve"

def send_request(url, elem):
	"""
	Sends a request for a puzzle.
	
	Params:
		* url:			the url / endpoint to make the request to
		* elem:			the requested element
	
	Returns:
		* (x, T, N)		the puzzle
	"""

	r = requests.get(url + f"?element={elem}")
	puzzle = r.text


	fields = {}
	for part in puzzle.split(";"):
		if part:
			k, v = part.split("=")
			fields[k] = v
	x = int(fields["x"], 16)
	T = int(fields["T"], 16)
	N = int(fields["N"], 16)
	return (x, T, N)

def solve_TLP(x, T, N):
	"""
	Solves the puzzle.
	
	Params:
		* x: 			the base
		* T:			the number of squarings
		* N: 			the modulos
		
	Returns:
		* y = x^2^T mod N
	"""

	return pow(x, 2**T, N)


def send_solve(url, elem, y):
	"""
	Sends a request including the solution to the puzzle.
	
	Params:
		* url:			the url / endpoint to make the request to
		* elem:			the requested element
		* y:			the solution to the puzzle
		
	Returns:
		* the response of the server
	"""

	result = requests.get(url + f"?element={elem}&y={y:x}")
	return result.text


if __name__ == "__main__":
	# sample usecase: the full protocol for requesting the element 10
	elem = 10
	x, T, N = send_request(request_URL, elem)
	y = solve_TLP(x, T, N)
	res = send_solve(solve_URL, elem, y)
	print(res)