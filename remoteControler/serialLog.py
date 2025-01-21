#Provides a text based interface with the plasma controller and allows for logging data to a CSV

import serial
import sys
import time

ser = serial.Serial('/dev/ttyACM0', 115200, timeout=0.1)

#Check if device is connected
ser.write(b"\r")

#TODO check for device preamble####
data = ser.readline()
if not data:
    print("device not found... exiting")
    print("DEBUG: " + str(data))
    exit()

ser.reset_input_buffer()
#######################


#Wait for user to provide logging file name and start plasma

log_choice = input("Log plasma? (Y/n): ")

if not log_choice == "n":
    filename = input("Chose a filename to save as: ")
    file = open(filename, "wb")

print("press control+C to safely shut down")

start = input("press enter to start plasma")

if not start == "":
    print("exiting")
    exit()

#power on supplies
ser.write(b"p\r\n")
time.sleep(0.25)

#set deadtime to minimum
ser.write(b"d")
time.sleep(0.25)
ser.write(b"1\r")
time.sleep(0.25)

#start h-bridge
ser.write(b"s")
time.sleep(0.25)
ser.write(b"1\r")
time.sleep(0.25)


#start freq adjust
ser.write(b"y")

ser.reset_input_buffer()


try:
    while True:
        data = ser.readline()
        file.write(data)
        print("DEBUG: " + str(data))

except KeyboardInterrupt:
    ser.write(b"q\r")
    time.sleep(0.1)
    ser.write(b"s\r")
    time.sleep(0.1)
    ser.write(b"0\r")
    file.close()
    exit()
