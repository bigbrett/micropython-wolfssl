import ussl as ssl
import usocket as socket

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.settimeout(10) 
sockaddr = socket.getaddrinfo('127.0.0.1', 11111)[0][-1]
sock.connect(sockaddr)

wsock = ssl.wrap_socket(sock, cert_reqs=0)

print("socket wrapped!")

wsock.write(b"bar") 
assert wsock.read(3) == b"bar"

print("SUCCESS!")

