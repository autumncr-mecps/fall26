import socket
from gpiozero import button
from gpiozero import LED
import time

UDP_PORT_IN = 6666
UDP_PORT_OUT = 6667
addr = ""

state_button = Button(21)
indicator_led = [LED(23), LED(24), LED(25)];
error_led = LED(8)

state = 0
thresholds = [50, 200, 400]

# surely there's a better way to do this -- this is just for getting our own IP
s1 = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s1.connect(("8.8.8.8", 80))
UDP_IP = s1.getsockname()[0]
print(UDP_IP)
s1.close

# now we're starting the server
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind((UDP_IP, UDP_PORT_IN))
print("begin udp server...")
print("waiting for initialization")


def input():
    data = s.recvfrom(40)
    print("received message: %s" % data)
    return data[0]

def process(data):
    print("processing data " + data)

def output():
    print("BUTTON PRESSED")
    if state == 0:
        s.sendto(b"INIT",  addr)
        state = 1
    if state == 1:
        s.sendto(b"STOP", addr)
        state = 0

button.whenpressed = output

while True:
    if state == 1:
        data = input()
        process(data)
