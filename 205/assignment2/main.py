import numpy as np
import pywt
import cv2
import matplotlib.pyplot as plt
from matplotlib.colors import LogNorm

# part 1
dataStr=np.loadtxt("./fall26/205/assignment2/gyroData.txt", delimiter="," ,dtype='str' )
data=dataStr.astype(int)

def Cnt(data):
    count=np.zeros((1,8))
    for i in data:
        for j in range(8):
            if(i+4==j):
                count[0,j]=count[0,j]+1
    return count

count = Cnt(data)
print("count: ", count);

#part 2
# tbh there's probably an easier way to do this in Python
def calcEnergy(samp):
    sum = 0
    for i in samp:
        sum += i**2 # square the data
    return sum

print("raw data energy: ", calcEnergy(data))
# decomposition levels 1-3
for i in range(1, 4):
    a = pywt.downcoef("a", data, "haar", level=i)
    d = pywt.downcoef("d", data, "haar", level=i)
    print("decomposition" , i, "approximation:", calcEnergy(a))
    print("decomposition" , i, "detail:", calcEnergy(d))
    print()

# part 3
emma = cv2.imread("./fall26/205/assignment2/emma.png")
#plt.imshow(emma)
#plt.show()

red_emma = np.float32(np.array(emma[:, :, 2]))
red_emma_coeffs = cv2.dct(red_emma)

#plt.imshow(red_emma_coeffs, norm=LogNorm())
#plt.show()
small_coeffs = 0
def reduceCoeffs(val):
    global small_coeffs
    if abs(val) >= 10:
        return val
    
    small_coeffs += 1
    return 0

for idx, row in enumerate(red_emma_coeffs):
    red_emma_coeffs[idx] = list(map(reduceCoeffs, row))

imsize = 500**2
print("Coefficients kept:", (imsize - small_coeffs))
print("Compression ratio:", (small_coeffs/imsize))

red_emma_again = cv2.idct(red_emma_coeffs)
#plt.imshow(red_emma_again)
#plt.show()

def row_mse(original, compressed):
    err = 0
    for idx, val in enumerate(original):
        err += (val - compressed[idx]) ** 2
    err /= len(original)
    return err

mse = 0
for idx, original_row in enumerate(red_emma):
    mse += row_mse(original_row, red_emma_again[idx])
mse /= len(red_emma)

print("Mean squared error:", mse)
    
