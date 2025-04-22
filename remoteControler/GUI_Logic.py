## Author Nolan Olaso

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

     # Initialize the PlasmaSerialInterface
        try:
            # Adjust the serial port as needed (e.g., "COM3" on Windows or "/dev/ttyACM0" on Linux)
            self.plasma_interface = PlasmaSerialInterface('/dev/ttyACM0')
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
        
        if (not self.plasma_interface.toggle_low_voltage() or not self.plasma_interface.toggle_high_voltage()):
            print("Power on unsucessful")
            return

        self.system_on = True
        self.led_system_status.setStyleSheet("background-color: green; border-radius: 40px;")
        self.label_system_status_value.setText("On")

        print("Power On button was pushed")
    
    def handle_power_off(self):
        ## TODO Change system indicators to update on ADC measurment not button press
        self.handle_plasma_off()

        if (self.plasma_interface.toggle_low_voltage() or self.plasma_interface.toggle_high_voltage()):
            print("system is in undefined state. Power off unsuccessful")
            return


        self.led_system_status.setStyleSheet("background-color: red; border-radius: 40px;")
        self.label_system_status_value.setText("Off")


        self.system_on = False
        print("Power Off button was pushed")
    
    def handle_strike_plasma(self):
        ## TODO Change system indicators to update on ADC measurment not button press
        
        if not self.system_on:
            self.show_warning_popup("Please ensure system is powered on before attempting to strike a plasma.")
            return

        self.led_plasma_status.setStyleSheet("background-color: green; border-radius: 40px;")
        self.label_plasma_status_value.setText("On")

        print("Strike Plasma button was pushed")

    def handle_strike_plasma(self):
        if not self.system_on:
            self.show_warning_popup("Please ensure system is powered on before attempting to strike a plasma.")
            return
    
        # I think these need to be flipped, I'm not sure
        auto_voltage_flag = not self.manual_voltage_allowed
        auto_freq_flag = not self.manual_frequency_allowed

        try:
            # Directly calling start_plasma on an existing instance
            self.plasma_interface.start_plasma(self.data_logging_allowed, auto_voltage_flag, auto_freq_flag, self.manual_voltage_selection, self.manual_frequency_selection,self.save_location)
        
        except Exception as e:
            self.show_warning_popup("Failed to start plasma: " + str(e))
            self.handle_plasma_off()
            return

        ## TODO Change system indicators to update on ADC measurment not button press
        self.led_plasma_status.setStyleSheet("background-color: green; border-radius: 40px;")
        self.label_plasma_status_value.setText("On")

    
    def handle_plasma_off(self):
        ## TODO Change system indicators to update on ADC measurment not button press
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
        self.plasma_interface.set_freq(self.manual_frequency_selection.text())
    
    def handle_enable_auto_voltage_correction(self,state):
        if not state:  # If user is trying to disable auto control
            try:
                if not self.manual_voltage_allowed:
                    self.enable_auto_voltage_correction.setChecked(True)  # Re-enable checkbox
                    raise ValueError
            except ValueError:
                self.show_warning_popup("Please enter a manual control value before disabling auto control.")
                return
                
        self.checkbox_toggled("Voltage Auto Control", state)
        self.plasma_interface.set_auto_voltage(self.enable_auto_frequency_correction.checkState())
        #clear input box if enabling automatic control
        if state:
            self.manual_frequency_selection.clear()

    def handle_enable_auto_frequency_correction(self,state):
        if not state:  # If user is trying to disable auto control
            try:
                if not self.manual_frequency_allowed:
                    self.enable_auto_frequency_correction.setChecked(True)  # Re-enable checkbox
                    raise ValueError
                

            except ValueError:
                self.show_warning_popup("Please enter a manual control value before disabling auto control.")
                return

        self.checkbox_toggled("Frequency Auto Control", state)
        self.plasma_interface.set_auto_freq(self.enable_auto_frequency_correction.checkState())
        #clear input box if enabling automatic control
        if state:
            self.manual_frequency_selection.clear()

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