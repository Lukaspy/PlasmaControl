import threading
import serial
import time

import PlasmaException

class PlasmaSerialInterface:
    def __init__(self, serialPort, serial_lock, plasma_active_event):
        self.serial_port = serialPort
        self.initialized = False
        self.baud_rate = 8593750
        self.timeout = 0.01
        self.serial_lock = serial_lock
        self.plasma_active_event = plasma_active_event


    """The microcontroller does not have a uart buffer. 
    Must send each char with a slight delay to allow stm to process the command
    """
    def _send(self, data):
        self.ser.reset_input_buffer()
        with self.serial_lock:
            for char in data:
                self.ser.write(char.encode())
                time.sleep(10/1000)
            #self.ser.write(data.encode() + b"\r")
            self.ser.write(b"\r")
            return self.ser.readline()


    def initialize(self):
        """Initializes communication with the microcontroller. Returns True if 
        device is connected, False otherwise"""
        self.ser = serial.Serial(self.serial_port, self.baud_rate, timeout=self.timeout)
        self.ser.reset_input_buffer()

        data = self._send("~")

        if not data:
            return False
        
        #put system in known state
        self.set_auto_freq(True)
        self.set_auto_voltage(False)
        self.set_datalogging(False)
        
        self.initialized = True
        return True
    

    """Queries whether the 3.3V supply is active
    returns True is active, False otherwise"""
    def query_3_3_supply(self):

        reply = self._send("p?3.3")

        if reply == b"on":
            return True
        else:
            return False

    """Queries whether the 15V supply is active
    returns True is active, False otherwise"""
    def query_15_supply(self):

        reply = self._send("p?15")

        if reply == b"on":
            return True
        else:
            return False

    """Queries whether the high voltage supply is active
    returns True is active, False otherwise"""
    def query_hv_supply(self):

        reply = self._send("p?hv")

        if reply == b"on":
            return True
        else:
            return False

    """Toggles the low voltage (15v and 3.3V supplies)
    returns True if supplies are turrned on, False otherwise"""
    def toggle_low_voltage(self):
        
        reply = self._send("p!lv")

        if reply.strip() == b"on":
            return True
        else:
            return False
        

    """Toggles the high voltage (500V supply)
    returns True if supply are turrned on, False otherwise"""
    def toggle_high_voltage(self):
        reply = self._send("p!hv")

        if reply.strip() == b"on":
            return True
        else:
            return False
        

    """Sets the frequency based on a input in kHz
    returns True if freq was set, False otherwise
    """
    def set_freq(self, freq):

        reply = self._send("f!"+str(round(float(freq)*1000)))

        if reply == b"ok":
            return True
        else:
            return False
        

    """Queries the current frequency. Returns answer in Hz as 
    a str
    """
    def query_freq(self):
        return self._send("f?").decode()



    """Sets voltage setpoint"""
    def set_voltage(self, voltage):
        self._send("v!".encode + voltage.encode())

    def query_voltage(self):
        return self._send("v?").decode()
    
    def set_auto_freq(self, new_setting):
        send_flag = 0

        if new_setting:
            send_flag = 1

        reply = self._send("mf"+str(send_flag))

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


    """Queries the microcontroller for the csv log header
    describing the logged parameters
    """
    def query_log_header(self):
        return self._send("lh")


    """Queries the microcontroller for the newest available ADC1/2 data
    """
    def query_log_data(self):
        self.ser.reset_input_buffer()
        with self.serial_lock:
            for char in "l?":
                self.ser.write(char.encode())
                time.sleep(10/1000)
            #self.ser.write(data.encode() + b"\r")
            self.ser.write(b"\r")
            response = b""
            while True:
                response += self.ser.read()
                if b"#" in response:
                    return response[0:-1]


    """ Queries the ADC3 to read the current supply voltages
    returns the voltages in the following format: 3.3V, 15V, HVDC
    """
    def query_supply_voltages(self):
        self.ser.reset_input_buffer()
        return self._send("p?a")
        

    """Send the system shutdown command. Stops plasma (if running) shutsdown all supplies"""
    def system_shutdown(self):
        self._send("z")




    def start_plasma(self, auto_voltage_flag, auto_freq_flag, stop_event, voltage=-1, manual_freq=30000):
        """Activates the plasma depending on the boolean flag parameters"""
        if not self.query_15_supply() or not self.query_3_3_supply():
            raise PlasmaException.PlasmaException('Low Voltage Supplies not on!')
        
        if not self.toggle_high_voltage():
            self.system_shutdown()
            raise PlasmaException.PlasmaException("High voltage in unknown state!")
        
        self._send("s!")

        time.sleep(0.1)
        self.plasma_active_event.set()

        #Do nothing until stop signal is triggered
        while not stop_event.is_set():
            time.sleep(0.01)


        self.plasma_active_event.clear()
        
        #now shutting down the plasma
        self._send("q")


        
        
        


