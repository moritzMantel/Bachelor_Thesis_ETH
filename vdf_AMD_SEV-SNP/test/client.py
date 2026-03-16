import requests

request_URL = "http://localhost:8080/request_vdf"
solve_URL = "http://localhost:8080/solve"

def send_request(url, elem):
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

def solve_vdf(x, T, N):
	return pow(x, 2**T, N)


def send_solve(url, elem, y):
	result = requests.post(url + f"?element={elem}&y={y}")
	return result.text


if __name__ == "__main__":
	elem = 10
	x, T, N = send_request(request_URL, elem)
	y = solve_vdf(x, T, N)
	res = send_solve(solve_URL, elem, y)
	print(res)