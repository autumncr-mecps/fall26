import socket

UDP_PORT_IN = 6666
UDP_PORT_OUT = 6667

# surely there's a better way to do this -- this is just for getting our own IP
s1 = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s1.connect(("8.8.8.8", 80))
UDP_IP = s1.getsockname()[0]
print(UDP_IP)
s1.close

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind((UDP_IP, UDP_PORT_IN))
print("begin UDP server...")

while True:
    data, addr = s.recvfrom(1024)
    print("received message: %s" % data)

    s.sendto(b"HIIIII", addr);#[0], UDP_PORT_OUT))
