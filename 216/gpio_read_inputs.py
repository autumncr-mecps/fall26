from gpiozero import Button

gpios = [4, 17, 27, 22, 6, 13, 19, 26, 18, 23, 24, 25, 12, 16, 20, 21]

for i in gpios:
    button = Button(i)
    print(f"waiting for button press {i}")
    button.wait_for_press()
    print(f"registered press for button {i}")

