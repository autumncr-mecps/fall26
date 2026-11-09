'''
    LightSwarm Raspberry Pi Logger 
    SwitchDoc Labs 
    December 2020
'''
from __future__ import print_function
 
from builtins import chr
from builtins import range
import time 
from gpiozero import LED, Button

from netifaces import interfaces, ifaddresses, AF_INET

from socket import *

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

MYPORT = 2910
SWARMPORT = 2911
SWARMSIZE = 3

# GPIO outputs
reset_button = Button(4)
swarm_led = (LED(16), LED(20), LED(21))
white_led = LED(12)
master = -1
blink_clock = time.time()
blink_led_val = False

logString = ""

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
    data= ["" for i in range(14)]

    data[0] = int("F0", 16).to_bytes(1,'little') 
    data[1] = int(DEFINE_SERVER_LOGGER_PACKET).to_bytes(1,'little')
    data[2] = int("FF", 16).to_bytes(1,'little') # swarm id (FF means not part of swarm)
    data[3] = int(VERSIONNUMBER).to_bytes(1,'little')
    data[4] = int(myIP[0]).to_bytes(1,'little') # 1 octet of ip
    data[5] = int(myIP[1]).to_bytes(1,'little') # 2 octet of ip
    data[6] = int(myIP[2]).to_bytes(1,'little') # 3 octet of ip
    data[7] = int(myIP[3]).to_bytes(1,'little') # 4 octet of ip
    data[8] = int(0x00).to_bytes(1,'little')
    data[9] = int(0x00).to_bytes(1,'little')
    data[10] = int(0x00).to_bytes(1,'little')
    data[11] = int(0x00).to_bytes(1,'little')
    data[12] = int(0x00).to_bytes(1,'little')
    data[13] = int(0x0F).to_bytes(1,'little')
    mymessage = ''.encode()  	
    s.sendto(mymessage.join(data), ('<broadcast>'.encode(), SWARMPORT))

def SendRESET_SWARM_PACKET(s):
    print("RESET_SWARM_PACKET Sent") 
    s.setsockopt(SOL_SOCKET, SO_BROADCAST, 1)

    data= ["" for i in range(14)]

    data[0] = int("F0", 16).to_bytes(1,'little')
    
    data[1] = int(RESET_SWARM_PACKET).to_bytes(1,'little')
    data[2] = int("FF", 16).to_bytes(1,'little') # swarm id (FF means not part of swarm)
    data[3] = int(VERSIONNUMBER).to_bytes(1,'little')
    data[4] = int(0x00).to_bytes(1,'little')
    data[5] = int(0x00).to_bytes(1,'little')
    data[6] = int(0x00).to_bytes(1,'little')
    data[7] = int(0x00).to_bytes(1,'little')
    data[8] = int(0x00).to_bytes(1,'little')
    data[9] = int(0x00).to_bytes(1,'little')
    data[10] = int(0x00).to_bytes(1,'little')
    data[11] = int(0x00).to_bytes(1,'little')
    data[12] = int(0x00).to_bytes(1,'little')
    data[13] = int(0x0F).to_bytes(1,'little')
      	
    mymessage = ''.encode()  	
    s.sendto(mymessage.join(data), ('<broadcast>'.encode(), SWARMPORT))
    
    # Once we've sent the reset instruction, turn the LED on and sleep for
    # three seconds (to give swarm time to reset)
    white_led.on()
    time.sleep(3)
    white_led.off()

reset_button.when_pressed = SendRESET_SWARM_PACKET

def parseLogPacket(message):
	print("Log From SwarmID:",(message[2]))
	print("Swarm Software Version:", (message[4]))
	
	print("StringLength:",(message[3]))
	logString = ""
	for i in range(0,(message[3])):
		logString = logString + chr((message[i+5]))

	#print("logString:", logString)	
	return logString	

def calcBlinkInfo(logString, swarmSize):
    swarmList = logString.split("|")
    # find the master and the average light value across the swarm
    master = -1
    light_val = 0
    for i in range(0,swarmSize):
        swarmElement = swarmList[i].split(",")
        if (swarmElement[4] == "PR"):
            light_val += swarmElement[3]
            if (swarmElement[1] == "1"):
                master = i

    light_val /= swarmSize
    # should do the same thing as Arduino map(val, 0, 4096, 50, 2000)
    blink_speed = (((4096 - light_val) / 4096) * 1950) + 50
    return (master, blink_speed)

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

#300 seconds round
seconds_300_round = time.time() + 300.0

while(1) :
    # receive datclient (data, addr)
    #d = s.recvfrom(1024)
    try:
        d =  s.recvfrom(1024)
    except TimeoutError:
        break

    message = d[0]
    addr = d[1]

    if ((message[1]) == LOG_TO_SERVER_PACKET):
        print("Swarm LOG_TO_SERVER_PACKET Received")

        # process the Log Packet
        logString = parseLogPacket(message)
        (master, blink_speed) = calcBlinkInfo(logString, SWARMSIZE)

    else:
        print("error message length = ",len(message))

    # blink the master's LED at a speed proportional to swarm brightness
    if (time.time() - blink_clock >= blink_speed):
        for i in range(SWARMSIZE):
            # only the master blinks
            if i != master:
                swarm_led[i].off()
            else:
                # if the led is currently on, turn it off
                if blink_led_val:
                    swarm_led[i].off()
                # if it's currently off, turn it on
                else:
                    swarm_led[i].on()
                blink_led_val = not blink_led_val
        blink_clock = time.time()

    if (time.time() >  seconds_300_round):
        print(">>>>doing 300 second task")
        SendDEFINE_SERVER_LOGGER_PACKET(s)
        seconds_300_round = time.time() + 300.0	
