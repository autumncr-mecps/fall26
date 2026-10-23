import socket
from gpiozero import Button
from gpiozero import LED
import time

class Talker:
    def __init__(self, ip, port, sock):
        self.addr = (ip, port)
        self.sock = sock
        self.val = -1

    def restart(self):
        self.timeout_clock = time.time()
        self.active = 0

    def sendMsg(self, msg):
        self.sock.sendto(msg,  self.addr)

    def resetTimeout(self):
        self.timeout_clock = time.time()
        self.active = 1

    def setVal(self, val):
        if val.isdigit():
            self.val = int(val)

    def errorState(self):
        self.active = 0

    def getTimeout(self):
        return self.timeout_clock

class Listener:
    def __init__(self, ip, in_port, out_port, btn_pin, indicator_led_pins, error_led_pin, esps):
        # establish GPIO variables
        self.state_button = Button(btn_pin)
        self.state_button.when_pressed = self.output
        self.indicator_leds = []
        for i in indicator_led_pins:
            self.indicator_leds.append(LED(i))
        self.error_led = LED(error_led_pin)

        # possible states:
        # 0: uninitialized
        # 1: working
        # 2: error
        self.state = 0
        self.thresholds = [2000, 3000, 4000]
        
        # establish UDP connection
        self.in_port = in_port
        self.out_port = out_port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind((ip, in_port))
        self.sock.settimeout(0.1)
        print("begin udp server...")
        print("waiting for initialization")

        # initialize the ESPs
        self.esps = {}
        for esp_ip in esps:
            self.esps[esp_ip] = (Talker(esp_ip, self.out_port, self.sock))

        # turn the LED on until we initialize
        self.setErrorLED(1)

    def input(self):
        try:
            (data, addr) = self.sock.recvfrom(8)
        except TimeoutError:
            return
        
        data = data.decode().replace('\00', '')
        addr = addr[0]
        #print("received data:", data, "from", addr)
        # if this is the first message we've received from this ESP, track it
        if self.esps[addr].active == 0:
            self.esps_connected += 1
            if self.esps_connected == len(self.esps):
                # turn off the error LED once we've recieved messages from all ESPs
                self.setErrorLED(0)

        # we got a new message, so reset the clock for this ESP
        self.esps[addr].resetTimeout()
        self.esps[addr].setVal(data)
        return data

    def process(self):
        avg = 0
        for esp in self.esps:
            v = self.esps[esp].val
            if v != -1:
                avg += v
                print(v, " ", end="")
        avg /= len(self.esps)
        print(f"\navg: {avg:0.2f}")

        for i, t in enumerate(self.thresholds):
            if avg >= t:
                self.indicator_leds[i].on()
            else:
                self.indicator_leds[i].off()

    def errorInput(self):
        try:
            (data, addr) = self.sock.recvfrom(8)
            data = data.decode().replace('\00', '')
        except TimeoutError:
            return
        if addr[0] == self.error_esp and data == "SORRY":
            print("received apology; returning to pre-INIT state")
            self.error_esp = -1
            self.state = 0
            self.setErrorLED(1)

    def output(self):
        if self.state == 0 or self.state == 2:
            for esp in self.esps:
                self.esps[esp].restart()
                self.esps[esp].sendMsg(b"INIT")
            print("INIT");
            self.state = 1
            self.esps_connected = 0
            # we're expecting to receive a message after five seconds
            # if we haven't, we need to be able to enter error state
            self.timeout_clock = time.time()
        elif self.state == 1:
            for esp in self.esps:
                self.esps[esp].sendMsg(b"STOP")
            print("STOP")
            self.state = 0
            for led in self.indicator_leds:
                led.off()
            self.setErrorLED(1)

    def update(self):
        if self.state == 1:
            # if it's been 10 seconds since we got a message, enter error state
            for esp in self.esps:
                curr_time = time.time()
                timeout = self.esps[esp].getTimeout()
                if (curr_time - timeout) >= 10:
                    self.state = 2
                    self.error_clock = curr_time
                    self.error_esp = esp
                    print("ERROR: have not received message from", esp)
                    for led in self.indicator_leds:
                        led.off()
                    # surely there's a better way to do this, but whatever
                    for esp1 in self.esps:
                        if esp1 == self.error_esp:
                            self.esps[esp1].sendMsg(b"WTFDUDE")
                        else:
                            self.esps[esp1].sendMsg(b"STOP")
            # if we're in the working state, check for input
            data = self.input()
            if data:
                self.process()
        elif self.state == 2:
            # check if we've gotten the correct message back
            data = self.errorInput()
            # blink the LED once every 0.5 seconds
            if time.time() - self.error_clock >= 0.5:
                self.setErrorLED(-1)
                self.error_clock = time.time()

    # just a helper method to make it easier to blink
    def setErrorLED(self, val):
        # if val is 0, turn LED off
        if val == 0:
            self.error_led.off()
            self.error_led_state = 0
        # if val is 1, turn LED on
        elif val == 1:
            self.error_led.on()
            self.error_led_state = 1
        # if val is -1, turn it to whatever the opposite state currently is
        elif val == -1:
            if self.error_led_state == 0:
                self.error_led.on()
                self.error_led_state = 1
            elif self.error_led_state == 1:
                self.error_led.off()
                self.error_led_state = 0

# surely there's a better way to do this -- this is just for getting our own IP
s1 = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s1.connect(("8.8.8.8", 80))
UDP_IP = s1.getsockname()[0]
print(UDP_IP)
s1.close

in_port = 6666
out_port = 6667
btn_pin = 21
leds = [23, 24, 25]
error_led = 12
esp_ips = ["172.31.206.163", "172.31.188.64", "172.31.184.255"]

l = Listener(UDP_IP, in_port, out_port, btn_pin, leds, error_led, esp_ips)

while True:
    l.update()
