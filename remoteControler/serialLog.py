#Provides a text based interface with the plasma controller and allows for logging data to a CSV

import serial
import sys

ser = serial.Serial('/dev/ttyACM0', 115200, timeout=1)

#Check if device is connected
ser.write(b"\n")

#TODO check for device preamble####
if not ser.readline():
    print("device not found... exiting")
    exit()

ser.reset_input_buffer()
#######################


#Wait for user to provide logging file name and start plasma

log_choice = input("Log plasma? (Y/n): ")
log = False

if not log_choice == "n":
    filename = input("Chose a filename to save as: ")
    file = open(filename, "x")

print("press any key to safely shut down")

start = input("press enter to start plasma")

if not start == "":
    print("exiting")
    exit()

#power on supplies
ser.write("p\n")

#set deadtime to minimum
ser.write("d\n")
ser.write("1\n")

#start h-bridge
ser.write("s\n")
ser.write("1\n")

#start freq adjust
ser.write("y\n")

file.write("insert column names here\n")

while true:
    if sys.stdin:
        ser.write("q\n")
        ser.write("s\n")
        ser.write("0\n")
        exit()

    data = ser.readline()
    f.write(data)


    










