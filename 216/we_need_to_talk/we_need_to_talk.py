import socket
from gpiozero import Button
from gpiozero import LED
import time

UDP_PORT_IN = 6666
UDP_PORT_OUT = 6667
UDP_ADDR_OUT = ""
addr = ("192.168.1.71", UDP_PORT_OUT)

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
    data = s.recv(8).decode()
    print("received data: " + data)
    return data

def process(data):
    indicator_led[0].on()
    time.sleep(0.1)
    indicator_led[0].off()

def output():
    global state
    if state == 0:
        s.sendto(b"INIT",  addr)
        print("INIT");
        state = 1
    elif state == 1:
        s.sendto(b"STOP", addr)
        print("STOP")
        state = 0

state_button.when_pressed = output

while True:
    if state == 1:
        data = input()
        process(data)
