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
import matplotlib
matplotlib.use("TkAgg")
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

start_time = time.time()
data_points = deque([])
master_counts = [0, 0, 0]
master_counts_total = [0, 0, 0]
colors = ["white", "green", "red"]
muted_colors = ["#880000", "#888888", "#005500"]

# Setting up the chart(s). Using a dark gray background so that the white shows up
def setupChart(fig, ax_light, ax_master, ax_totals):
    fig.patch.set_facecolor("#222222")

    ax_light.set_title("Light Sensor Readings")
    ax_light.set_xlabel("Seconds")
    ax_light.set_ylabel("Photoresistor Reading")
    ax_light.set_ylim(0, 4096)
    ax_light.set_facecolor("#222222")
    ax_light.xaxis.set_major_formatter(   
        tkr.FuncFormatter(lambda x, _: f"{round(x)}") # rounds them nicely back to 1, 2, 3
    )
    ax_light.title.set_color("#BBBBBB")
    ax_light.yaxis.label.set_color("#BBBBBB")
    ax_light.xaxis.label.set_color("#BBBBBB")
    ax_light.tick_params(axis="x", colors="#BBBBBB")
    ax_light.tick_params(axis="y", colors="#BBBBBB")
    ax_light.spines["top"].set_color("#BBBBBB")
    ax_light.spines["right"].set_color("#BBBBBB")
    ax_light.spines["bottom"].set_color("#BBBBBB")
    ax_light.spines["left"].set_color("#BBBBBB")

    ax_master.set_title("Masters Last 30 Sec")
    ax_master.set_xlabel("Last section of IP")
    ax_master.set_ylabel("Seconds as master")
    ax_master.set_facecolor("#222222")
    ax_master.yaxis.set_major_formatter(   
        tkr.FuncFormatter(lambda y, _: f"{round(y)}") # rounds them nicely back to 1, 2, 3
    )
    ax_master.title.set_color("#BBBBBB")
    ax_master.yaxis.label.set_color("#BBBBBB")
    ax_master.xaxis.label.set_color("#BBBBBB")
    ax_master.tick_params(axis="x", colors="#BBBBBB")
    ax_master.tick_params(axis="y", colors="#BBBBBB")
    ax_master.spines["top"].set_color("#BBBBBB")
    ax_master.spines["right"].set_color("#BBBBBB")
    ax_master.spines["bottom"].set_color("#BBBBBB")
    ax_master.spines["left"].set_color("#BBBBBB")

    ax_totals.set_title("Masters All Time")
    ax_totals.set_xlabel("Last section of IP")
    ax_totals.set_ylabel("Percent time master")
    ax_totals.set_facecolor("#222222")
    ax_totals.yaxis.set_major_formatter(   
        tkr.FuncFormatter(lambda y, _: f"{round(y)}") # rounds them nicely back to 1, 2, 3
    )
    ax_totals.title.set_color("#BBBBBB")
    ax_totals.yaxis.label.set_color("#BBBBBB")
    ax_totals.xaxis.label.set_color("#BBBBBB")
    ax_totals.tick_params(axis="x", colors="#BBBBBB")
    ax_totals.tick_params(axis="y", colors="#BBBBBB")
    ax_totals.spines["top"].set_color("#BBBBBB")
    ax_totals.spines["right"].set_color("#BBBBBB")
    ax_totals.spines["bottom"].set_color("#BBBBBB")
    ax_totals.spines["left"].set_color("#BBBBBB")

fig, (ax_light, ax_master, ax_totals) = plt.subplots(1, 3, figsize=(12,5), width_ratios=[4, 1, 1])
setupChart(fig, ax_light, ax_master, ax_totals)
plt.ion()
plt.show()

# UDP Commands and packets
def defineServer(s):
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

    data = ["" for i in range(8)]
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

    data = ["" for i in range(8)]
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
def calcLogInfo(message, swarmSize, data_points, master_counts, master_counts_total):
    print("Log From SwarmID:",(message[2]))

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
                data_points.append((light_val, master, time.time() - start_time))
                master_counts[master] += 1
                master_counts_total[master] += 1

    # update the graph
    if len(data_points) > LOGLENGTH:
        removed_point = data_points.popleft()
        master_counts[removed_point[1]] -= 1

        plt.cla()
        for j in np.arange(len(data_points)-1):
            point = data_points[j]
            next_point = data_points[j+1]
            # this if statement provides discontinuity between different masters' segments
            if point[1] == next_point[1]:
                ax_light.plot([point[2],next_point[2]], [point[0],next_point[0]], c=colors[next_point[1]])
        # apparently have to do this here too
        ax_light.set_ylim(0, 4096)
        # plot the counts of who has been the master the past 30 seconds
        bars = ax_master.bar(swarm_ips, master_counts, color=colors)
        ax_master.bar_label(bars, label_type="center")
        # report who has been the master over the lifetime of the server
        sum = sum(master_counts_total)
        for k in range(3):
            bar = ax_totals.bar(swarm_ips[k], round(master_counts_total[k]/sum*100),  color=muted_colors[k])
            ax_totals.bar_label(bar, label_type="center")
        plt.draw()
        plt.pause(0.001)

        f.write(f"{swarm_ips[master]}, {light_val}, {master_counts_total[master]}\n")
        swarm_led[master].on()
    else:
        swarm_led[master].off()

def startLogFile():
    global f
    if "f" in globals():
        f.close()
    filename = "logs/lightswarm_log_" + datetime.datetime.now().strftime("%m_%d_%Y__%H_%M_%S") + ".csv"
    f = open(filename, "w")
    f.write("master, sensor_data, time_as_master")

# set up sockets for UDP
s=socket(AF_INET, SOCK_DGRAM)
host = "localhost"
s.bind(("",MYPORT))
s.settimeout(0.1)

print("--------------")
print("LightSwarm Logger")
print("Version ", VERSIONNUMBER)
print("--------------")

# first send out DEFINE_SERVER_LOGGER_PACKET to tell swarm where to send logging information 
defineServer(s)
time.sleep(3)
defineServer(s)
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
        defineServer(s)
        seconds_300_round = time.time() + 300.0	

    try:
        d =  s.recvfrom(1024)
    except TimeoutError:
        continue

    message = d[0]
    addr = d[1]
    if ((message[1]) == LOG_TO_SERVER_PACKET):
        # process the Log Packet
        calcLogInfo(message, SWARMSIZE, data_points, master_counts, master_counts_total)

    else:
        print("error message length = ",len(message))
