import threading
import serial
import time

import PlasmaException

class PlasmaSerialInterface:
    def __init__(self, serialPort):
        self.serial_port = serialPort
        self.initialized = False
        self.baud_rate = 8593750
        self.timeout = 0.1


    """The microcontroller does not have a uart buffer. 
    Must send each char with a slight delay to allow stm to process the command
    """
    def _send(self, data):
        for char in data:
            self.ser.write(char.encode())
            time.sleep(10/1000)
        #self.ser.write(data.encode() + b"\r")
        self.ser.write(b"\r")

    def initialize(self):
        """Initializes communication with the microcontroller. Returns True if 
        device is connected, False otherwise"""
        self.ser = serial.Serial(self.serial_port, self.baud_rate, timeout=self.timeout)
        self.ser.reset_input_buffer()

        self._send("~")
        data = self.ser.readline()

        if not data:
            return False
        
        #put system in known state
        self.set_auto_freq(True)
        self.set_auto_voltage(False)
        self.set_datalogging(False)
        
        self.initialized = True
        return True
    

    def query_3_3_supply(self):
        """Queries whether the 3.3V supply is active
        returns True is active, False otherwise"""

        self._send("p?3.3")

        reply = self.ser.readline()

        if reply == b"on":
            return True
        else:
            return False

    def query_15_supply(self):
        """Queries whether the 15V supply is active
        returns True is active, False otherwise"""

        self._send("p?15")

        reply = self.ser.readline()

        if reply == b"on":
            return True
        else:
            return False

    def query_hv_supply(self):
        """Queries whether the high voltage supply is active
        returns True is active, False otherwise"""

        self._send("p?hv")

        reply = self.ser.readline()

        if reply == b"on":
            return True
        else:
            return False

    def toggle_low_voltage(self):
        """Toggles the low voltage (15v and 3.3V supplies)
        returns True if supplies are turrned on, False otherwise"""
        
        self.ser.reset_input_buffer()
        self._send("p!lv")

        reply = self.ser.readline()

        if reply.strip() == b"on":
            return True
        else:
            return False

    def toggle_high_voltage(self):
        """Toggles the high voltage (500V supply)
        returns True if supply are turrned on, False otherwise"""
        
        self._send("p!hv")

        reply = self.ser.readline()

        if reply.strip() == b"on":
            return True
        else:
            return False

    def set_freq(self, freq):
        self._send("f!"+str(round(float(freq)*1000)))
        reply = self.ser.readline().strip().decode()
        if reply == "ok":
            return True
        else:
            return False

    def query_freq(self):
        self._send("f?")
        return self.ser.readline().decode()

    def set_voltage(self, voltage):
        self._send("v!".encode + voltage.encode())

    def query_voltage(self):
        self._send("v?")

        return self.ser.readline()
    
    def set_auto_freq(self, new_setting):
        send_flag = 0

        if new_setting:
            send_flag = 1

        self._send("mf"+str(send_flag))
        reply = self.ser.readline()
        if reply.strip() == b"1":
            return True
        elif reply.strip() == b"0":
            return False
    
    def set_auto_voltage(self, new_setting):
        send_flag = 0

        if new_setting:
            send_flag = 1

        self._send("mv"+str(send_flag))
        

    def set_datalogging(self, new_setting):
        send_flag = 0

        if new_setting:
            send_flag = 1

        self._send("l"+str(send_flag))

    """Send the system shutdown command. Stops plasma (if running) shutsdown all supplies"""
    def system_shutdown(self):
        self._send("z")




    def start_plasma(self, datalog_flag, auto_voltage_flag, auto_freq_flag, stop_event, voltage=-1, manual_freq=30000,  datalog_filepath=""):
        """Activates the plasma depending on the boolean flag parameters"""

        if not self.query_15_supply() or not self.query_3_3_supply():
            raise PlasmaException.PlasmaException('Low Voltage Supplies not on!')
        
        if not self.toggle_high_voltage():
            self.system_shutdown()
            raise PlasmaException.PlasmaException("High voltage in unknown state!")
        
        if not datalog_flag:
            self._send("s!")
            while not stop_event.is_set():
                continue
            
        else:
            try:
                file = open(datalog_filepath, 'wb')
            except:
                raise IOError

            self.ser.reset_input_buffer()
            self._send("s!")

            with open(datalog_filepath, 'wb') as file:
                while not stop_event.is_set():
                    data = self.ser.readline()
                    file.write(data)
                    file.flush()
        
        #now shutting down the plasma
        self._send("q")


        
        
        


