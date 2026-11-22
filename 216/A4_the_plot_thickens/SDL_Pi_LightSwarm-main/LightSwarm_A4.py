'''
    LightSwarm Raspberry Pi Logger 
    SwitchDoc Labs 
    December 2020
'''
from __future__ import print_function
from builtins import chr
from builtins import range
import time 
import datetime
from gpiozero import LED, Button
from netifaces import interfaces, ifaddresses, AF_INET
from socket import *
from collections import deque 
import matplotlib.pyplot as plt
import matplotlib.ticker as tkr
import numpy as np

VERSIONNUMBER = 7
# packet type definitions
LIGHT_UPDATE_PACKET = 0
RESET_SWARM_PACKET = 1
CHANGE_TEST_PACKET = 2   # Not Implemented
RESET_ME_PACKET = 3
DEFINE_SERVER_LOGGER_PACKET = 4
LOG_TO_SERVER_PACKET = 5
MASTER_CHANGE_PACKET = 6
BLINK_BRIGHT_LED = 7

# I decided to have the server and the swarms communicate on separate ports.
# With the previous setup, the server was receiving every light update packet
# from every swarm member, which seemed to defeat the purpose of using a
# master/slave architecture for logging to the server.
MYPORT = 2910
SWARMPORT = 2911
SWARMSIZE = 3
LOGLENGTH = 30

# GPIO outputs
reset_button = Button(4)
reset_led = LED(12)
swarm_led = (LED(16), LED(20), LED(21))
swarm_ips = []
master = -1

start_time = time.process_time()
data_points = deque([])
colors = ["red", "white", "green"]

# Setting up the 
def setupChart(fig, ax):
    plt.title("Light Sensor Readings")
    fig.patch.set_facecolor("#222222")
    ax.set_ylim(0, 4096)
    ax.xaxis.set_major_formatter(   
        tkr.FuncFormatter(lambda x, _: f'{round(x)}') # rounds them nicely back to 1, 2, 3
    )
    ax.set_facecolor("#222222")
    ax.title.set_color("#BBBBBB")
    ax.yaxis.label.set_color("#BBBBBB")
    ax.xaxis.label.set_color("#BBBBBB")
    ax.tick_params(axis="x", colors="#BBBBBB")
    ax.tick_params(axis="y", colors="#BBBBBB")
    ax.spines["top"].set_color("#BBBBBB")
    ax.spines["right"].set_color("#BBBBBB")
    ax.spines["bottom"].set_color("#BBBBBB")
    ax.spines["left"].set_color("#BBBBBB")

fig = plt.figure(1, figsize=(10,5))
ax = plt.gca()
setupChart(fig, ax)

# UDP Commands and packets
def SendDEFINE_SERVER_LOGGER_PACKET(s):
    print("DEFINE_SERVER_LOGGER_PACKET Sent") 
    s.setsockopt(SOL_SOCKET, SO_BROADCAST, 1)

	# get IP address
    for ifaceName in interfaces():
            addresses = [i['addr'] for i in ifaddresses(ifaceName).setdefault(AF_INET, [{'addr':'No IP addr'}] )]
            print('%s: %s' % (ifaceName, ', '.join(addresses)))
  
    # last interface (wlan0) grabbed 
    print(addresses)
    myIP = addresses[0].split('.')
    print(myIP) 

    data= ["" for i in range(8)]
    data[0] = int("F0", 16).to_bytes(1,'little') 
    data[1] = int(DEFINE_SERVER_LOGGER_PACKET).to_bytes(1,'little')
    data[2] = int("FF", 16).to_bytes(1,'little') # swarm id (FF means not part of swarm)
    data[3] = int(VERSIONNUMBER).to_bytes(1,'little')
    data[4] = int(myIP[0]).to_bytes(1,'little') # 1 octet of ip
    data[5] = int(myIP[1]).to_bytes(1,'little') # 2 octet of ip
    data[6] = int(myIP[2]).to_bytes(1,'little') # 3 octet of ip
    data[7] = int(myIP[3]).to_bytes(1,'little') # 4 octet of ip
    mymessage = ''.encode()  	
    s.sendto(mymessage.join(data), ('<broadcast>'.encode(), SWARMPORT))

def resetSwarm(s):
    print("Resetting swarm")
    s.setsockopt(SOL_SOCKET, SO_BROADCAST, 1)

    data= ["" for i in range(8)]
    data[0] = int("F0", 16).to_bytes(1,'little')
    data[1] = int(RESET_SWARM_PACKET).to_bytes(1,'little')
    data[2] = int("FF", 16).to_bytes(1,'little') # swarm id (FF means not part of swarm)
    data[3] = int(VERSIONNUMBER).to_bytes(1,'little')
    data[4] = int(0x00).to_bytes(1,'little')
    data[5] = int(0x00).to_bytes(1,'little')
    data[6] = int(0x00).to_bytes(1,'little')
    data[7] = int(0x00).to_bytes(1,'little')
      	
    mymessage = ''.encode()  	
    s.sendto(mymessage.join(data), ('<broadcast>'.encode(), SWARMPORT))
    print("RESET_SWARM_PACKET Sent") 
    
    # Once we've sent the reset instruction, turn the LED on and sleep for
    # three seconds (to give swarm time to reset)
    reset_led.on()
    time.sleep(3)
    reset_led.off()

    startLogFile()

# Calculates the speed of blinking as well as which LED to blink
def calcLogInfo(message, swarmSize, data_points):
    print("Log From SwarmID:",(message[2]))
    print("Swarm Software Version:", (message[4]))

    logString = ""
    for i in range(0,(message[3])):
        logString += chr((message[i+5]))

    swarmList = logString.split("|")
    # find the master and the average light value across the swarm
    master = -1
    light_val = 0
    for i in range(0, swarmSize):
        swarmElement = swarmList[i].split(",")
        print(swarmElement)
        if (swarmElement[4] == "PR"):
            # Which LED is associated with which IP will depend on their order
            # in the received packet
            if swarmElement[5] not in swarm_ips:
                swarm_ips.append(swarmElement[5])
                print(f"Found new IP: {swarmElement[5]}")
            
            if swarmElement[1] == "1":
                master = swarm_ips.index(swarmElement[5])
                light_val = int(swarmElement[3])
                data_points.append((light_val, i, time.process_time()))

                # update the graph
                if len(data_points) > LOGLENGTH:
                    data_points.popleft()

                    for i in np.arange(len(data_points)-1):
                        point = data_points[i]
                        next_point = data_points[i+1]
                        ax.plot([point[3],next_point[3]], [point[1],next_point[1]], c=colors[point[2]])

                    plt.show()

                f.write(f"{i}, {light_val}\n")
                swarm_led[i].on()
            else:
                swarm_led[i].off()

def startLogFile():
    global f
    f.close()
    filename = "lightswarm_log_" + datetime.datetime.now().strftime("%m_%d_%Y__%H_%M_%S") + ".csv"
    f.open(filename, "w")

# set up sockets for UDP
s=socket(AF_INET, SOCK_DGRAM)
host = 'localhost'
s.bind(('',MYPORT))
s.settimeout(0.1)

print("--------------")
print("LightSwarm Logger")
print("Version ", VERSIONNUMBER)
print("--------------")

# first send out DEFINE_SERVER_LOGGER_PACKET to tell swarm where to send logging information 
SendDEFINE_SERVER_LOGGER_PACKET(s)
time.sleep(3)
SendDEFINE_SERVER_LOGGER_PACKET(s)
# swarmStatus
swarmStatus = [[0 for x  in range(6)] for x in range(SWARMSIZE)]

# 6 items per swarm item

# 0 - NP  Not present, P = present, TO = time out
# 1 - timestamp of last LIGHT_UPDATE_PACKET received
# 2 - Master or slave status   M S
# 3 - Current Test Item - 0 - CC 1 - Lux 2 - Red 3 - Green  4 - Blue
# 4 - Current Test Direction  0 >=   1 <=
# 5 - IP Address of Swarm
for i in range(0,SWARMSIZE):
	swarmStatus[i][0] = "NP"
	swarmStatus[i][5] = 0

# 300 seconds round
seconds_300_round = time.time() + 300.0

reset_button.when_pressed = lambda: resetSwarm(s)
startLogFile()

while(1) :
    if (time.time() >  seconds_300_round):
        print(">>>>doing 300 second task")
        SendDEFINE_SERVER_LOGGER_PACKET(s)
        seconds_300_round = time.time() + 300.0	

    try:
        d =  s.recvfrom(1024)
    except TimeoutError:
        continue

    message = d[0]
    addr = d[1]
    if ((message[1]) == LOG_TO_SERVER_PACKET):
        print("Swarm LOG_TO_SERVER_PACKET Received")

        # process the Log Packet
        calcLogInfo(message, SWARMSIZE, data_points)

    else:
        print("error message length = ",len(message))
