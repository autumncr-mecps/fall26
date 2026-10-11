import smbus
import time

# Power management registers
power_mgmt_1 = 0x6b

def read_byte(adr):
    return bus.read_byte_data(address, adr)

def read_word(adr):
    high = bus.read_byte_data(address, adr)
    low = bus.read_byte_data(address, adr+1)
    val = (high << 8) + low
    return val

def read_word_2c(adr):
    val = read_word(adr)
    if (val >= 0x8000):
        return -((65535 - val) + 1)
    else:
        return val

# let us write to 
def output(val):
    f.write(val)
    print(val, end="")

bus = smbus.SMBus(1) # or bus = smbus.SMBus(1) for Revision 2 boards
address = 0x68       # This is the address value read via the i2cdetect command

# Now wake the 6050 up as it starts in sleep mode
bus.write_byte_data(address, power_mgmt_1, 0)

# write file. if this file already exists, overwrite it
f = open("data.txt", "w")
start = time.time()
tick = time.time()

while time.time() - start <= 60.0:
    # non-blocking
    if time.time() - tick >= 0.1:
        gyro_xout = read_word_2c(0x43)
        gyro_yout = read_word_2c(0x45)
        gyro_zout = read_word_2c(0x47)

        output("raw gyro x y z:\n")
        output(f"{gyro_xout}\t{gyro_yout}\t{gyro_zout}\n")

        # I wasn't able to figure out where the 131 constant was coming from.
        # I think this is measured in degrees per second squared, and by default
        # the range is +/- 250 degrees per second squared
        # 
        # is this to do with converting it to radians or something??
        # or to rotations per second?
        output("scaled gyro x y z:\n")
        output(f"{(gyro_xout / 131):0.2}deg/s\t{(gyro_yout / 131):0.2}deg/s\t{(gyro_zout / 131):0.2}deg/s\n\n")

        output("raw accel x y z:\n")
        accel_xout = read_word_2c(0x3b)
        accel_yout = read_word_2c(0x3d)
        accel_zout = read_word_2c(0x3f)
        output(f"{accel_xout}\t{accel_yout}\t{accel_zout}\n")

        # each word is 16 bits, meaning a total range of 65536 possible values
        # this needs to be signed, so +g is 0 to 32767 and -g is -1 through -32768
        # then this is further divided by the resolution, which can be programmatically
        # set to measure +/-2g, 4g, 8g, or 16g
        #
        # this means the resolution goes down significantly when you're trying to
        # measure larger swings in acceleration
        # 
        # when the sensor is in +/-2g mode (default), a value of 16384 is
        # equivalent to 1g. Thereefore, dividing by 16384
        accel_xout_scaled = accel_xout / 16384.0
        accel_yout_scaled = accel_yout / 16384.0
        accel_zout_scaled = accel_zout / 16384.0

        output("scaled acceleration x y z:\n")
        output(f"{accel_xout_scaled:0.2}g\t{accel_yout_scaled:0.2}g\t{accel_zout_scaled:.04}g\n\n")
