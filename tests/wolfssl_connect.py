import ussl as ssl
import usocket as socket

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.settimeout(10) 
sockaddr = socket.getaddrinfo('127.0.0.1', 11111)[0][-1]
sock.connect(sockaddr)

wsock = ssl.wrap_socket(sock, cert_reqs=0)

testmsg = "foo, bar, noo, baz"

wsock.write(testmsg.encode('utf-8'))

print("Sent: \"{}\"".format(testmsg))

resp = wsock.read(len(testmsg.encode('utf-8')))

assert resp.decode('utf-8') == testmsg

print("Recv: \"{}\"".format(resp.decode('utf-8')))


