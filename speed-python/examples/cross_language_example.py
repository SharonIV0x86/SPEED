#!/usr/bin/env python3
"""
Cross-language chat example
Python process communicating with C++ process interactively
"""

import sys
import time
from pathlib import Path

from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QTextEdit, QLineEdit, QPushButton, QLabel, QGroupBox)
from PyQt5.QtCore import QTimer, pyqtSignal, QObject

# Add parent directory to sys.path so Python can import consolidated_speed.py
sys.path.append(str(Path(__file__).resolve().parent.parent))

from consolidated_speed import SPEED, ThreadMode, PMessage  # Use the single-file implementation

class MessageSignal(QObject):
    received = pyqtSignal(PMessage)

class SpeedChatWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("SPEED IPC Chat - Python ↔ C++")
        self.setGeometry(100, 100, 800, 600)
        
        self.spd = None
        self.signal = MessageSignal()
        self.signal.received.connect(self.on_message_received)
        
        self.setup_ui()
        self.start_speed()
    
    def setup_ui(self):
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        layout = QVBoxLayout(central_widget)
        
        # Status
        status_label = QLabel("Process: <b style='color:green'>Proc_1PY</b>")
        layout.addWidget(status_label)
        
        # Chat display
        self.chat_display = QTextEdit()
        self.chat_display.setReadOnly(True)
        layout.addWidget(self.chat_display)
        
        # Input area
        input_layout = QHBoxLayout()
        self.message_input = QLineEdit()
        self.message_input.returnPressed.connect(self.send_message)
        input_layout.addWidget(QLabel("Message:"))
        input_layout.addWidget(self.message_input)
        
        send_btn = QPushButton("Send")
        send_btn.clicked.connect(self.send_message)
        input_layout.addWidget(send_btn)
        layout.addLayout(input_layout)
        
        # Commands
        cmd_group = QGroupBox("Commands")
        cmd_layout = QHBoxLayout()
        
        for label, func in [("PING", self.send_ping), ("PONG", self.send_pong), ("Global Registry", lambda: self.spd.print_global_registry()), ("Access List", lambda: self.spd.print_access_list()), ("Connected List", lambda: self.spd.print_connected_list()), ("Clear", self.chat_display.clear)]:
            btn = QPushButton(label)
            btn.clicked.connect(func)
            cmd_layout.addWidget(btn)
        
        cmd_group.setLayout(cmd_layout)
        layout.addWidget(cmd_group)
    
    def start_speed(self):
        self.spd = SPEED("Proc_1PY", ThreadMode.MULTI)
        self.spd.set_callback(self.message_handler)
        self.spd.add_process("Proc_1CPP")
        self.spd.start()
        self.append_chat("=== SPEED IPC Started ===", "SYSTEM")
    
    def message_handler(self, msg: PMessage):
        self.signal.received.emit(msg)
    
    def on_message_received(self, msg: PMessage):
        ts = time.strftime("%H:%M:%S", time.localtime(msg.timestamp))
        self.append_chat(msg.message, msg.sender_name, ts)
    
    def append_chat(self, message, sender="", timestamp=None):
        prefix = f"[{timestamp}] {sender}: " if timestamp else f"[{sender}] " if sender else ""
        self.chat_display.append(f"{prefix}{message}")
    
    def send_message(self):
        message = self.message_input.text().strip()
        if not message:
            return
        
        self.spd.force_connect("Proc_1CPP")
        self.spd.send_message(message, "Proc_1CPP")
        self.append_chat(message, "YOU", time.strftime("%H:%M:%S"))
        self.message_input.clear()
    
    def send_ping(self):
        self.spd.ping("Proc_1CPP")
        self.append_chat("PING sent", "SYSTEM")
    
    def send_pong(self):
        self.spd.pong("Proc_1CPP")
        self.append_chat("PONG sent", "SYSTEM")
    
    def closeEvent(self, event):
        if self.spd:
            self.spd.stop()
        event.accept()

def main():
    app = QApplication(sys.argv)
    window = SpeedChatWindow()
    window.show()
    sys.exit(app.exec_())

if __name__ == "__main__":
    main()
