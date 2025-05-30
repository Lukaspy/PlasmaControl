## Author Nolan Olaso
## Launches Plasma Control GUI

import sys
from PySide6.QtWidgets import QApplication, QMainWindow
from GUI_Logic import GUILogic

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = GUILogic()
    window.show()
    ret = app.exec()
    window.shutdown_system()
    sys.exit(ret)
