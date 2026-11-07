from gpiozero import LED, Button
import time

gpios = [17, 27, 22, 5, 6, 13, 19, 26, 18, 23, 24, 25, 12, 16, 20, 21]
button = Button(4)

for i in gpios:
    led = LED(i)
    led.on()
    print(f"writing HIGH to pin {i}")
    button.wait_for_press()
    time.sleep(0.2)
    print(f"writing LOW to pin {i}")
    led.off()

