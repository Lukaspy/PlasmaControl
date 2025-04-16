## Created by: Qt User Interface Compiler version 6.8.2
## With significant edits from author Nolan Olaso

from PySide6.QtCore import (QCoreApplication, QDate, QDateTime, QLocale,
    QMetaObject, QObject, QPoint, QRect,
    QSize, QTime, QUrl, Qt)
from PySide6.QtGui import (QBrush, QColor, QConicalGradient, QCursor,
    QFont, QFontDatabase, QGradient, QIcon,
    QImage, QKeySequence, QLinearGradient, QPainter,
    QPalette, QPixmap, QRadialGradient, QTransform)
from PySide6.QtWidgets import (QApplication, QCheckBox, QFrame, QGroupBox,
    QLabel, QLineEdit, QMainWindow, QMenuBar,
    QPushButton, QSizePolicy, QStatusBar, QWidget)

# Ui_MainWindow defines the layout and components of the main window
class Ui_MainWindow(object):
    def setupUi(self, MainWindow):
        # Setup the main window properties (size, central widget, etc.)
        if not MainWindow.objectName():
            MainWindow.setObjectName(u"MainWindow")
        MainWindow.resize(800, 600) # Set window size
        self.centralwidget = QWidget(MainWindow) # Create central widget
        self.centralwidget.setObjectName(u"centralwidget")

        # Adding version history label at the bottom
        self.version_history = QLabel(self.centralwidget)
        self.version_history.setObjectName(u"version_history")
        self.version_history.setGeometry(QRect(318, 530, 201, 16)) # Position

        # Adding a horizontal line at the bottom of the window
        self.line_bottom_border = QFrame(self.centralwidget)
        self.line_bottom_border.setObjectName(u"line_bottom_border")
        self.line_bottom_border.setGeometry(QRect(0, 520, 791, 20)) # Position
        self.line_bottom_border.setFrameShape(QFrame.Shape.HLine)
        self.line_bottom_border.setFrameShadow(QFrame.Shadow.Sunken)

        # Frame containing system controls (Plasma and power buttons)
        self.frame_q1 = QFrame(self.centralwidget)
        self.frame_q1.setObjectName(u"frame_q1")
        self.frame_q1.setGeometry(QRect(0, 0, 381, 251)) # Frame size and location
        self.frame_q1.setFrameShape(QFrame.StyledPanel)
        self.frame_q1.setFrameShadow(QFrame.Raised)

        # Adding strike plasma and plasma off buttons
        self.stop_plasma = QPushButton(self.frame_q1)
        self.stop_plasma.setObjectName(u"stop_plasma")
        self.stop_plasma.setGeometry(QRect(230, 210, 111, 28)) # Position
        self.strike_plasma = QPushButton(self.frame_q1)

        self.strike_plasma.setObjectName(u"strike_plasma")
        self.strike_plasma.setGeometry(QRect(230, 170, 111, 28)) # Position
        
        # Adding power on and power off buttons
        self.power_on = QPushButton(self.frame_q1)
        self.power_on.setObjectName(u"power_on")
        self.power_on.setGeometry(QRect(40, 170, 111, 28)) # Position

        self.power_off = QPushButton(self.frame_q1)
        self.power_off.setObjectName(u"power_off")
        self.power_off.setGeometry(QRect(40, 210, 111, 28)) # Position
        
        # Labels for system and plasma status indicators
        self.label_system_status = QLabel(self.frame_q1)
        self.label_system_status.setObjectName(u"label_system_status") # Position
        self.label_system_status.setGeometry(QRect(41, 0, 91, 16))

        self.label_plasma_status = QLabel(self.frame_q1)
        self.label_plasma_status.setObjectName(u"label_plasma_status")
        self.label_plasma_status.setGeometry(QRect(230, 0, 91, 16)) # Position
        self.label_system_status_value = QLabel(self.frame_q1)

        # System and plasma status value labels (off by default)
        self.label_system_status_value.setObjectName(u"label_system_status_value") # Position
        self.label_system_status_value.setGeometry(QRect(131, 0, 21, 16))
        self.label_plasma_status_value = QLabel(self.frame_q1)

        self.label_plasma_status_value.setObjectName(u"label_plasma_status_value")
        self.label_plasma_status_value.setGeometry(QRect(320, 0, 21, 16)) # Position
        
        # LED indicators for system and plasma statuses (off by default)
        self.led_system_status = QLabel(self.frame_q1)
        self.led_system_status.setObjectName(u"led_system_status")
        self.led_system_status.setGeometry(QRect(55, 50, 80, 80)) # Position
        self.led_system_status.setStyleSheet("background-color: red; border-radius: 40px;")
        
        self.led_plasma_status = QLabel(self.frame_q1)
        self.led_plasma_status.setObjectName(u"led_plasma_status")
        self.led_plasma_status.setGeometry(QRect(245, 50, 80, 80)) # Position
        self.led_plasma_status.setStyleSheet("background-color: red; border-radius: 40px;")

        # Separator line system and controls
        self.line = QFrame(self.frame_q1)
        self.line.setObjectName(u"line")
        self.line.setGeometry(QRect(180, 10, 20, 231)) # Position
        self.line.setFrameShape(QFrame.Shape.VLine)
        self.line.setFrameShadow(QFrame.Shadow.Sunken)

        # Frame containing live data plotting
        self.frame_q2 = QFrame(self.centralwidget)
        self.frame_q2.setObjectName(u"frame_q2")
        self.frame_q2.setGeometry(QRect(420, 0, 381, 251)) # Position
        self.frame_q2.setFrameShape(QFrame.StyledPanel)
        self.frame_q2.setFrameShadow(QFrame.Raised)

        # Live data plotting label
        self.label_live_data_plotting = QLabel(self.frame_q2)
        self.label_live_data_plotting.setObjectName(u"label_live_data_plotting")
        self.label_live_data_plotting.setGeometry(QRect(140, 0, 101, 20)) # Position

        ##TODO Implement live date plotting
        
        # Frame containing settings
        self.frame_q3 = QFrame(self.centralwidget)
        self.frame_q3.setObjectName(u"frame_q3")
        self.frame_q3.setGeometry(QRect(0, 270, 381, 251)) # Position
        self.frame_q3.setFrameShape(QFrame.StyledPanel)
        self.frame_q3.setFrameShadow(QFrame.Raised)

        # Auto Controls
        self.enable_auto_frequency_correction = QCheckBox(self.frame_q3)
        self.enable_auto_frequency_correction.setObjectName(u"enable_auto_frequency_correction")
        self.enable_auto_frequency_correction.setGeometry(QRect(200, 10, 181, 20)) # Position
        self.enable_auto_frequency_correction.setChecked(True)

        self.enable_auto_voltage_correction = QCheckBox(self.frame_q3)
        self.enable_auto_voltage_correction.setObjectName(u"enable_auto_voltage_correction")
        self.enable_auto_voltage_correction.setGeometry(QRect(10, 10, 181, 20)) # Position
        self.enable_auto_voltage_correction.setChecked(True)

        # Groupbox for manual controls
        self.group_manual_controls = QGroupBox(self.frame_q3)
        self.group_manual_controls.setObjectName(u"group_manual_controls")
        self.group_manual_controls.setGeometry(QRect(10, 40, 361, 71)) # Position

        # Labels and inputs for manual voltage control
        self.manual_voltage_selection = QLineEdit(self.group_manual_controls)
        self.manual_voltage_selection.setObjectName(u"manual_voltage_selection")
        self.manual_voltage_selection.setGeometry(QRect(40, 40, 81, 22)) # Position
        
        self.label_voltage_value = QLabel(self.group_manual_controls)
        self.label_voltage_value.setObjectName(u"label_voltage_value")
        self.label_voltage_value.setGeometry(QRect(5, 20, 163, 16)) # Position

        self.label_voltage_unit = QLabel(self.group_manual_controls)
        self.label_voltage_unit.setObjectName(u"label_voltage_unit")
        self.label_voltage_unit.setGeometry(QRect(130, 40, 20, 20)) # Position
        
        # Labels and inputs for manual frequency control
        self.manual_frequency_selection = QLineEdit(self.group_manual_controls)
        self.manual_frequency_selection.setObjectName(u"manual_frequency_selection")
        self.manual_frequency_selection.setGeometry(QRect(220, 40, 81, 22)) # Position

        self.label_frequency_value = QLabel(self.group_manual_controls)
        self.label_frequency_value.setObjectName(u"label_frequency_value")
        self.label_frequency_value.setGeometry(QRect(180, 20, 176, 16)) #Position
        
        self.label_frequency_unit = QLabel(self.group_manual_controls)
        self.label_frequency_unit.setObjectName(u"label_frequency_unit")
        self.label_frequency_unit.setGeometry(QRect(310, 40, 21, 20)) #Position

        # Groupbox for data logging controls
        self.group_data_logging = QGroupBox(self.frame_q3)
        self.group_data_logging.setObjectName(u"group_data_logging")
        self.group_data_logging.setGeometry(QRect(10, 120, 191, 121)) # Position

        # Labels and inputs for data logging

        self.data_logging_save_location = QLineEdit(self.group_data_logging)
        self.data_logging_save_location.setObjectName(u"data_logging_save_location")
        self.data_logging_save_location.setGeometry(QRect(10, 90, 171, 22)) # Position
        self.data_logging_save_location.setReadOnly(True)  # Make it uneditable, just for displaying file path
        
        self.data_logging_save = QPushButton(self.group_data_logging)
        self.data_logging_save.setObjectName(u"label_data_logging_save")
        self.data_logging_save.setGeometry(QRect(25, 60, 141, 28)) # Position 
        
        self.enable_data_logging = QCheckBox(self.group_data_logging)
        self.enable_data_logging.setObjectName(u"enable_data_logging")
        self.enable_data_logging.setGeometry(QRect(10, 30, 101, 20)) # Position

        # Groupbox for other settings
        self.group_other = QGroupBox(self.frame_q3)
        self.group_other.setObjectName(u"group_other")
        self.group_other.setGeometry(QRect(210, 120, 161, 121)) # Position

        # Labels and inputs for duty cycle, disabled as voltage control is already a deadtime (dutycycle) adjustment 
        #self.label_duty_cycle_value = QLabel(self.centralwidget)
        #self.label_duty_cycle_value.setObjectName(u"label_duty_cycle_value")
        #self.label_duty_cycle_value.setGeometry(QRect(200, 410, 131, 16)) # Position

        #self.label_duty_cycle_unit = QLabel(self.centralwidget)
        #self.label_duty_cycle_unit.setObjectName(u"label_duty_cycle_unit")
        #self.label_duty_cycle_unit.setGeometry(QRect(300, 430, 20, 20)) # Position

        #self.duty_cycle_selection = QLineEdit(self.centralwidget)
        #self.duty_cycle_selection.setObjectName(u"duty_cycle_selection")
        #self.duty_cycle_selection.setGeometry(QRect(200, 430, 91, 22)) # Position

        # Frame for supply voltage/temperature readouts
        self.frame_q4 = QFrame(self.centralwidget)
        self.frame_q4.setObjectName(u"frame_q4")
        self.frame_q4.setGeometry(QRect(420, 270, 381, 251)) # Position
        self.frame_q4.setFrameShape(QFrame.StyledPanel)
        self.frame_q4.setFrameShadow(QFrame.Raised)

        self.line_2 = QFrame(self.frame_q4)
        self.line_2.setObjectName(u"line_5")
        self.line_2.setGeometry(QRect(180, 10, 20, 231)) # Position
        self.line_2.setFrameShape(QFrame.Shape.VLine)
        self.line_2.setFrameShadow(QFrame.Shadow.Sunken)

        # Labels for supply voltage/temperature readouts
        ##TODO Implement supply voltage readout
        ##TODO Implement temperature readout
        self.label_supply_voltage_readout = QLabel(self.frame_q4)
        self.label_supply_voltage_readout.setObjectName(u"label_supply_voltage_readout")
        self.label_supply_voltage_readout.setGeometry(QRect(30, 10, 141, 16)) # Position

        self.label_temperature_readout = QLabel(self.frame_q4)
        self.label_temperature_readout.setObjectName(u"label_temperature_readout")
        self.label_temperature_readout.setGeometry(QRect(230, 10, 131, 16)) # Position
        
        MainWindow.setCentralWidget(self.centralwidget)
        self.frame_q1.raise_()
        self.version_history.raise_()
        self.line_bottom_border.raise_()
        self.frame_q3.raise_()
        self.frame_q2.raise_()
        #self.label_duty_cycle_value.raise_()
        #self.label_duty_cycle_unit.raise_()
        #self.duty_cycle_selection.raise_()
        self.frame_q4.raise_()
        self.menubar = QMenuBar(MainWindow)
        self.menubar.setObjectName(u"menubar")
        self.menubar.setGeometry(QRect(0, 0, 800, 26))
        MainWindow.setMenuBar(self.menubar)
        self.statusbar = QStatusBar(MainWindow)
        self.statusbar.setObjectName(u"statusbar")
        MainWindow.setStatusBar(self.statusbar)

        self.retranslateUi(MainWindow)

        QMetaObject.connectSlotsByName(MainWindow)
    # setupUi

    def retranslateUi(self, MainWindow):
        MainWindow.setWindowTitle(QCoreApplication.translate("MainWindow", u"MainWindow", None))
        self.version_history.setText(QCoreApplication.translate("MainWindow", u"Plasma Control Team Version 0.2", None))
        self.stop_plasma.setText(QCoreApplication.translate("MainWindow", u"Stop Plasma", None))
        self.strike_plasma.setText(QCoreApplication.translate("MainWindow", u"Strike Plasma", None))
        self.power_on.setText(QCoreApplication.translate("MainWindow", u"Power On", None))
        self.power_off.setText(QCoreApplication.translate("MainWindow", u"Power Off", None))
        self.label_system_status.setText(QCoreApplication.translate("MainWindow", u"System Status:", None))
        self.label_plasma_status.setText(QCoreApplication.translate("MainWindow", u"Plasma Status:", None))
        self.label_system_status_value.setText(QCoreApplication.translate("MainWindow", u"Off", None))
        self.label_plasma_status_value.setText(QCoreApplication.translate("MainWindow", u"Off", None))
        self.group_manual_controls.setTitle(QCoreApplication.translate("MainWindow", u"Manual Control Values (Disable parameter auto-correction.)", None))
        self.label_voltage_value.setText(QCoreApplication.translate("MainWindow", u"Enter Voltage Value 200-500 V", None))
        self.label_frequency_value.setText(QCoreApplication.translate("MainWindow", u"Enter Frequency Value 20-50 kHz", None))
        self.label_voltage_unit.setText(QCoreApplication.translate("MainWindow", u"V", None))
        self.label_frequency_unit.setText(QCoreApplication.translate("MainWindow", u"kHz", None))
        self.group_data_logging.setTitle(QCoreApplication.translate("MainWindow", u"Data Logging Controls", None))
        self.data_logging_save.setText(QCoreApplication.translate("MainWindow", u"Enter Log Save Location", None))
        self.enable_data_logging.setText(QCoreApplication.translate("MainWindow", u"Data Logging", None))
        self.group_other.setTitle(QCoreApplication.translate("MainWindow", u"Other Settings", None))
        self.enable_auto_frequency_correction.setText(QCoreApplication.translate("MainWindow", u"Auto Frequency Correction", None))
        self.enable_auto_voltage_correction.setText(QCoreApplication.translate("MainWindow", u"Auto Voltage Correction", None))
        self.label_live_data_plotting.setText(QCoreApplication.translate("MainWindow", u"Live Data Plotting", None))
        #self.label_duty_cycle_value.setText(QCoreApplication.translate("MainWindow", u"Enter Duty Cycle Value", None))
        #self.label_duty_cycle_unit.setText(QCoreApplication.translate("MainWindow", u"%", None))
        self.label_supply_voltage_readout.setText(QCoreApplication.translate("MainWindow", u"Supply Voltage Readout", None))
        self.label_temperature_readout.setText(QCoreApplication.translate("MainWindow", u"Temperature Readout", None))
    # retranslateUi

