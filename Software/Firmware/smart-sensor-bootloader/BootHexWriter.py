import serial
import time

ser = serial.Serial('COM6')

lines = []
file = open('uploadtest.hex', 'r')
lines = file.readlines

ser.write(b'HEX')
time.sleep(10)
for line in lines:
    ser.write(b'{line}')
    time.sleep(1)
time.sleep(10)
ser.write(b'O')