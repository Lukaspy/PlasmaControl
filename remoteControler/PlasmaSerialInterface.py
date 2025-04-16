from threading import Thread
import serial

import PlasmaException

class PlasmaSerialInterface:
    def __init__(self, serialPort):
        self.serial_port = serialPort
        self.initialized = False
        self.baud_rate = 8593750
        self.timeout = 0.1


    def _send(self, data):
        self.ser.write(data + "\r")

    def initialize(self):
        """Initializes communication with the microcontroller. Returns True if 
        device is connected, False otherwise"""
        self.ser = serial.Serial(self.serial_port, self.baud_rate, self.timeout)
        self.ser.reset_input_buffer()

        self._send("~")
        data = self.ser.readline()

        if not data:
            return False
        
        self.initialized = True
        return True
    

    def query_3_3_supply(self):
        """Queries whether the 3.3V supply is active
        returns True is active, False otherwise"""

        self._send("p3.3?")

        reply = self.ser.readline()

        if reply == "on":
            return True
        else:
            return False

    def query_15_supply(self):
        """Queries whether the 15V supply is active
        returns True is active, False otherwise"""

        self._send("p15?")

        reply = self.ser.readline()

        if reply == "on":
            return True
        else:
            return False

    def query_hv_supply(self):
        """Queries whether the high voltage supply is active
        returns True is active, False otherwise"""

        self._send("phv?")

        reply = self.ser.readline()

        if reply == "on":
            return True
        else:
            return False

    def toggle_low_voltage(self):
        """Toggles the low voltage (15v and 3.3V supplies)
        returns True if supplies are turrned on, False otherwise"""
        
        self._send("plv!")

        reply = self.ser.readline();

        if reply == "on":
            return True
        else:
            return False

    def toggle_high_voltage(self):
        """Toggles the high voltage (500V supply)
        returns True if supply are turrned on, False otherwise"""
        
        self._send("phv!")

        reply = self.ser.readline();

        if reply == "on":
            return True
        else:
            return False


    def start_plasma(self, datalog_flag, auto_voltage_flag, auto_freq_flag, voltage, manual_freq=30000, datalog_filepath=""):
        """Activates the plasma depending on the boolean flag parameters"""

        if not self.query_hv_supply or not self.query_15_supply or not self.query_3_3_supply:
            raise PlasmaException('Supplies not on!')
        
        if not datalog_flag:
            if not auto_freq_flag and not auto_voltage_flag:
               #TODO: Figure out what syntax should be for manual control
               raise PlasmaException("Not Implemented")
            if not auto_voltage_flag and auto_freq_flag:
                self._send("sp")
            
        else:
            try:
                file = open(datalog_filepath, 'wb')
            except:
                raise IOError

            self._send("spdl")

            self.ser.reset_input_buffer()

            #TODO: start a listner that montiors for a stop signal from the gui
            #figure out how to share a listener from to gui into this function

            while True:
                data = self.ser.readline()
                file.write(data)

        
        #now shutting down the plasma
        self._send("q")

    def set_freq(self, freq):
        self._send("f\r"+str(freq))

        
        
        


