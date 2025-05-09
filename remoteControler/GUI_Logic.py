## Author Nolan Olaso

import tempfile
import threading
import time
import operator
from PySide6.QtWidgets import QMainWindow, QMessageBox, QFileDialog
from plasma_control_GUI import Ui_MainWindow
from PlasmaSerialInterface import PlasmaSerialInterface

## This class extends QMainWindow and integrates the generated UI.
## It connects UI elements such as buttons, line edits, and checkboxes to functions
## Currently functions are limited to outputing text tne console
class GUILogic(QMainWindow, Ui_MainWindow):
    def __init__(self):
        super().__init__()
        self.setupUi(self)
        self.setup_connections()
        self.system_on = False # Track power status
        self.manual_voltage_allowed = False # Can auto control be turned off?
        self.manual_frequency_allowed = False # Can auto control be turned off?
        self.data_logging_allowed = False #Can data logging be turned on?
        self.save_location = ""
        self.plasma_thread = None
        self.logging_thread = None
        self.stop_event = threading.Event()
        self.serial_lock = threading.Lock()
        self.plasma_active_event = threading.Event()
        self.auto_freq_adjust_enabled = True

     # Initialize the PlasmaSerialInterface
        try:
            # Adjust the serial port as needed (e.g., "COM3" on Windows or "/dev/ttyACM0" on Linux)
            self.plasma_interface = PlasmaSerialInterface('/dev/ttyACM0', self.serial_lock, self.plasma_active_event)
            if not self.plasma_interface.initialize():
                self.show_warning_popup("Microcontroller not responding. Check connection.")
        except Exception as e:
            self.show_warning_popup("Error initializing plasma interface: " + str(e))

    ## Connects UI elements to respective event handlers
    def setup_connections(self):

        ## Push Buttons
        self.power_on.clicked.connect(self.handle_power_on)
        self.power_off.clicked.connect(self.handle_power_off)
        self.strike_plasma.clicked.connect(self.handle_strike_plasma)
        self.stop_plasma.clicked.connect(self.handle_plasma_off)
        self.data_logging_save.clicked.connect(self.handle_data_logging_save)
        
        ## Line Edits
        self.manual_voltage_selection.returnPressed.connect(self.handle_manual_voltage_selection)
        self.manual_frequency_selection.returnPressed.connect(self.handle_manual_frequency_selection)
        #self.duty_cycle_selection.returnPressed.connect(lambda: self.text_entered("% Duty Cycle", self.duty_cycle_selection.text())) #Removed control feature
        
        ## Check Boxes
        self.enable_auto_voltage_correction.toggled.connect(self.handle_enable_auto_voltage_correction)
        self.enable_auto_frequency_correction.toggled.connect(self.handle_enable_auto_frequency_correction)
        self.enable_data_logging.toggled.connect(self.handle_enable_data_logging)

## Defines functions for UI elements
 ## TODO Add dedicated methods instead of printing to console. 
    def handle_power_on(self):
        ## TODO Change system indicators to update on ADC measurment not button press
        if self.system_on:
            return
        
        if (not self.plasma_interface.toggle_low_voltage()):
            print("Power on unsucessful")
            return

        self.system_on = True
        self.led_system_status.setStyleSheet("background-color: green; border-radius: 40px;")
        self.label_system_status_value.setText("On")

        print("Power On button was pushed")
    
    def handle_power_off(self):
        ## TODO Change system indicators to update on ADC measurment not button press
        if not self.system_on:
            return
        self.handle_plasma_off()

        if (self.plasma_interface.toggle_low_voltage()):
            print("system is in undefined state. Power off unsuccessful")
            return


        self.led_system_status.setStyleSheet("background-color: red; border-radius: 40px;")
        self.label_system_status_value.setText("Off")


        self.system_on = False
        print("Power Off button was pushed")

    def update_freq_readout(self):
        self.plasma_interface.ser.reset_input_buffer()
        new_freq = self.plasma_interface.query_freq()
        new_freq = str(round(float(new_freq)/1000, 3))
        self.manual_frequency_selection.setText(new_freq)

    """Queries the current ADC3 supply voltages and updates the GUI accordingly.
        Assumes the following format: 3.3V,15V,HVDC
    """
    def update_supply_readout(self):
        self.plasma_interface.ser.reset_input_buffer()
        supply_update = self.plasma_interface.query_supply_voltages()

        voltages = supply_update.split()
        if len(voltages) != 3:
            return
        self._3_3V_supply_readout.setText(str(float(voltages[0].decode().replace(",", ""))/1000))
        self._15V_supply_readout.setText(str(float(voltages[1].decode().replace(",", ""))/1000))
        self.high_V_supply_readout.setText(str((float(voltages[2].decode()))/1000))


    """Updates the plot displaying the system parameters"""
    def update_plot(self, data):
        #parse the csv formatted data into usable lists
        rows = data.strip().split("\n\r") 
        rows = [row.split(",") for row in rows]
        rows = [[float(cell) for cell in row] for row in rows]


        columns = list(zip(*rows))
        
        #Subtract the initial time value from all subsequent values to get relative timing
        columns[0] = [x - columns[0][0] for x in columns[0]]
        
        #                  0          1              2             3         4          5         6        7            8           9        10
        #Columns layout: [Time], [Freq (Hz)], [Deadtime (%)], [Bridge I], [VplaL1], [VplaL2], [VbriS1], [VbriS2], [TIM1 status], [upper], [lower]


        
        time = columns[0]
        bridgeI = columns[3]
        #Subtract L1 and L2 to get differential voltage across array
        plasmaV = list(map(operator.sub, columns[4], columns[5]))
        
        upper = columns[9]
        lower = columns[10]



        self.canvas.ax1.clear()
        self.canvas.ax2.clear()
        #plot bridge current
        color = "tab:red"
        self.canvas.ax1.set_xlabel('Time (us)')
        self.canvas.ax1.set_ylabel('Bridge Current (mA)', color = color) 
        self.canvas.ax1.plot(time,bridgeI, color = color)
        
        if (self.auto_freq_adjust_enabled):
            uppercursorline = self.canvas.ax1.axhline(y=upper[1], color='green', linestyle='--')
            lowercursorline = self.canvas.ax1.axhline(y=lower[1], color='green', linestyle='--')
        
        #plot plasma voltage
        color = 'tab:blue'
        self.canvas.ax2.set_ylabel('Plasma Voltage', color = color) 
        self.canvas.ax2.plot(time, plasmaV, color = color) 
        self.canvas.ax2.yaxis.set_label_position("right")
        #self.canvas.ax2.tick_params(axis ='y', labelcolor = color) 

        self.canvas.draw()

        return

    

    """This function provides all of the updating, logging, and ploting that takes place while the plasma
    is active. The function continuously polls the serial buffer for data to log, checks supply voltages,
    and updates the frequency display"""
    def live_plasma_actions(self, datalog_filepath):
        supply_query_rate = 0.5 #defines how often ADC3 readings are queried in number of read cycles
        freq_query_rate = .1 #defines how often current freq is queried in number of read cycles
        logging_rate = 1/1000
        

        time.sleep(0.1)

        if (datalog_filepath == "temp"):
            file = tempfile.TemporaryFile(mode="w+b")
        else:
            try:
                file = open(datalog_filepath, 'wb')
            except:
                raise IOError


        #get header for csv file
        file.write(self.plasma_interface.query_log_header())

        next_supplies_time = time.time() + supply_query_rate
        next_freq_time = time.time() + freq_query_rate
        next_log_time = time.time() + logging_rate

        while not self.stop_event.is_set():
            current_time = time.time()

            if current_time >= next_supplies_time:
                next_supplies_time = current_time + supply_query_rate
                self.update_supply_readout()
            
            if current_time >= next_freq_time and self.auto_freq_adjust_enabled:
                next_freq_time = current_time + freq_query_rate
                self.update_freq_readout() 

            if current_time >= next_log_time:
                next_log_time = current_time + logging_rate
                try:
                    new_data = self.plasma_interface.query_log_data()
                    file.write(new_data)

                    self.update_plot(new_data.decode())

                except:
                    continue






                     

            #with self.serial_lock:
                #data = self.plasma_interface.ser.readline()

            ##ADC1/2 data recieved
            #if b"log" in data:
                #data = data[3:]
                #file.write(data)

            #extract period number
            #is this line the start of a new period?
            #if yes, read in all of the matching data points, plot, then continue

            #else, is this line describing adc3 readings? 
            #then update supply voltage readout


        file.close()
    

    def handle_strike_plasma(self):
        if not self.system_on:
            self.show_warning_popup("Please ensure system is powered on before attempting to strike a plasma.")
            return


        try:
            self.plasma_interface.start_plasma()
        
        except Exception as e:
            self.handle_plasma_off()
            self.show_warning_popup("Failed to start plasma: " + str(e))
            return

        ## TODO Change system indicators to update on ADC measurment not button press

        #start data plotting/supply voltage updates
        #if the user does not want to log data, tell live_plasma_actions to create a temp file for live plotting
        if not self.data_logging_allowed:
            self.save_location = "temp"

        self.stop_event.clear()
        self.plasma_active_event.clear()

        self.logging_thread = threading.Thread(target=self.live_plasma_actions, args=((self.save_location,)), daemon=True)
        self.logging_thread.start()

        self.led_plasma_status.setStyleSheet("background-color: green; border-radius: 40px;")
        self.label_plasma_status_value.setText("On")

    
    def handle_plasma_off(self):
        ## TODO Change system indicators to update on ADC measurment not button press
        if self.logging_thread is not None and self.logging_thread.is_alive():
            self.stop_event.set()
            self.plasma_active_event.clear()
            print("here")
            time.sleep(0.5)
            self.logging_thread.join()#timeout=3)
            print("hereher")
            
        self.plasma_interface.stop_plasma()
        self.led_plasma_status.setStyleSheet("background-color: red; border-radius: 40px;")
        self.label_plasma_status_value.setText("Off")

        print("Plasma Off Button was pushed")

    def handle_manual_voltage_selection(self):
        try:
            voltage = float(self.manual_voltage_selection.text())
            if not (200 <= voltage <= 400):  # Check if voltage is within the valid range
                self.manual_voltage_allowed = False
                raise ValueError
        except ValueError:
            self.show_warning_popup("Please enter a value within range. 200 - 400V.")
            self.manual_frequency_selection.clear()  # Clear invalid input
            return
        
        self.manual_voltage_allowed = True
        self.text_entered("V", self.manual_voltage_selection.text())
        self.plasma_interface.set_voltage(self.manual_voltage_selection.text())
    
    def handle_manual_frequency_selection(self):
        try:
            frequency = float(self.manual_frequency_selection.text())
            if not (20 <= frequency <= 65):  # Check if frequency is within the valid range
                self.manual_frequency_allowed = False
                raise ValueError        
        except ValueError:
            self.show_warning_popup("Please enter a value within range. 20 - 50kHz.")
            self.manual_frequency_selection.clear()  # Clear invalid input
            return

        self.manual_frequency_allowed = True
        self.text_entered("kHz", self.manual_frequency_selection.text())
        if not self.plasma_interface.set_freq(self.manual_frequency_selection.text()):
            self.handle_power_off()
            self.show_warning_popup("Error writing frequency. Shutting down system")
            
        self.manual_frequency_selection.setText(str(round(float(self.manual_frequency_selection.text()), 3))) #round to the nearest Hz

    
    def handle_enable_auto_voltage_correction(self,state):
        if state:  # If user is trying to enable auto control
            try:
                if not self.manual_voltage_allowed:
                    self.enable_auto_voltage_correction.setChecked(False)  # Re-enable checkbox
                    raise ValueError
            except ValueError:
                self.show_warning_popup("Please enter a set point before enabling auto control.")
                return
                
        self.checkbox_toggled("Voltage Auto Control", state)
        self.plasma_interface.set_auto_voltage(self.enable_auto_frequency_correction.checkState())
        #clear input box if enabling automatic control
        if not state:
            self.manual_frequency_selection.clear()

    def handle_enable_auto_frequency_correction(self,state):
        #This block forces user to manually select frequency before disabling autocontrol. Not strictly required
        #if not state:  # If user is trying to disable auto control
            #try:
                #if not self.manual_frequency_allowed:
                    #self.enable_auto_frequency_correction.setChecked(True)  # Re-enable checkbox
                    #raise ValueError
                

            #except ValueError:
                #self.show_warning_popup("Please enter a manual control value before disabling auto control.")
                #return

        self.checkbox_toggled("Frequency Auto Control", state)

        #send the command and verify the condition is set within the STM 
        if self.plasma_interface.set_auto_freq(state) != state: 
            self.enable_auto_frequency_correction.setChecked(not state)
            return

        #clear input box if enabling automatic control
        if state:
            self.manual_frequency_selection.clear()
        
        self.auto_freq_adjust_enabled = state

    def handle_data_logging_save(self):
        file_path, _ = QFileDialog.getSaveFileName(self, "Save File", "", "All Files (*);;Text Files (*.txt);;Python Files (*.py)")
        
        if file_path:
            self.save_location = file_path
            self.data_logging_save_location.setText(self.save_location)

    def handle_enable_data_logging(self, state):
        if state: # If user is trying to enable data logging
            try:
                ##TODO validate save location, not just that there's an input
                if (self.data_logging_save_location.text() == ""):
                    self.enable_data_logging.setChecked(False)  # Re-disable checkbox
                    self.data_logging_allowed = False
                    raise ValueError
            except ValueError:
                self.show_warning_popup("Please enter a valid save location before enabling data logging.")
                self.data_logging_save_location.clear()
                return

        self.data_logging_allowed = True
        self.checkbox_toggled("Data Logging", state)
        
    """Shuts down plasma and power supplies, leaving system in a known state on exit"""
    def shutdown_system(self):
        self.handle_power_off()
        self.handle_enable_auto_frequency_correction(True)
        self.handle_enable_auto_voltage_correction(False)

            
    
    ## Placeholder methods before interation layer control
    def button_pressed(self, name):
        print(f"{name} button was pushed")

    def text_entered(self, name, value):
        print(f"You entered {value} {name}")

    def checkbox_toggled(self, name, state):
        status = "enabled" if state else "disabled"
        print(f"{name} {status}")

    ## Defines warning popup
    def show_warning_popup(self, message):
        msg = QMessageBox(self)
        msg.setWindowTitle("Warning")
        msg.setText(message)
        msg.setIcon(QMessageBox.Warning)
        msg.addButton("Dismiss", QMessageBox.AcceptRole)
        msg.exec()