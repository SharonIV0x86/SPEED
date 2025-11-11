#!/usr/bin/env python3
"""
Cross-language chat example
Python process communicating with C++ process interactively
"""

import sys
import time
from pathlib import Path

from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QTextEdit, QLineEdit, QPushButton, QLabel, QGroupBox, QFrame)
from PyQt5.QtCore import QTimer, pyqtSignal, QObject, Qt
from PyQt5.QtGui import QFont, QPalette, QColor

# Add parent directory to sys.path so Python can import consolidated_speed.py
sys.path.append(str(Path(__file__).resolve().parent.parent))

from consolidated_speed import SPEED, ThreadMode, PMessage  # Use the single-file implementation

class MessageSignal(QObject):
    received = pyqtSignal(PMessage)

class SpeedChatWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("SPEED IPC Chat - Python ↔ C++")
        self.setGeometry(100, 100, 900, 650)
        
        self.spd = None
        self.signal = MessageSignal()
        self.signal.received.connect(self.on_message_received)
        
        # Font size settings
        self.chat_font_size = 12
        self.input_font_size = 11
        
        self.setup_ui()
        self.apply_styles()
        self.start_speed()
    
    def setup_ui(self):
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        layout = QVBoxLayout(central_widget)
        layout.setContentsMargins(20, 20, 20, 20)
        layout.setSpacing(15)
        
        # Header with status
        header_frame = QFrame()
        header_frame.setObjectName("headerFrame")
        header_layout = QHBoxLayout(header_frame)
        header_layout.setContentsMargins(15, 10, 15, 10)
        
        status_label = QLabel("Process: Proc_1PY")
        status_label.setObjectName("statusLabel")
        status_label.setFont(QFont("Segoe UI", 11, QFont.Bold))
        header_layout.addWidget(status_label)
        
        connection_label = QLabel("● Connected")
        connection_label.setObjectName("connectionLabel")
        connection_label.setFont(QFont("Segoe UI", 10))
        header_layout.addStretch()
        header_layout.addWidget(connection_label)
        
        # Font size controls
        font_label = QLabel("Font:")
        font_label.setFont(QFont("Segoe UI", 9))
        header_layout.addWidget(font_label)
        
        decrease_font_btn = QPushButton("A-")
        decrease_font_btn.setObjectName("fontButton")
        decrease_font_btn.setFont(QFont("Segoe UI", 9, QFont.Bold))
        decrease_font_btn.clicked.connect(self.decrease_font)
        decrease_font_btn.setCursor(Qt.PointingHandCursor)
        decrease_font_btn.setToolTip("Decrease font size")
        decrease_font_btn.setMaximumWidth(40)
        header_layout.addWidget(decrease_font_btn)
        
        increase_font_btn = QPushButton("A+")
        increase_font_btn.setObjectName("fontButton")
        increase_font_btn.setFont(QFont("Segoe UI", 10, QFont.Bold))
        increase_font_btn.clicked.connect(self.increase_font)
        increase_font_btn.setCursor(Qt.PointingHandCursor)
        increase_font_btn.setToolTip("Increase font size")
        increase_font_btn.setMaximumWidth(40)
        header_layout.addWidget(increase_font_btn)
        
        layout.addWidget(header_frame)
        
        # Chat display
        self.chat_display = QTextEdit()
        self.chat_display.setReadOnly(True)
        self.chat_display.setObjectName("chatDisplay")
        self.chat_display.setFont(QFont("Consolas", self.chat_font_size))
        layout.addWidget(self.chat_display, stretch=1)
        
        # Input area
        input_frame = QFrame()
        input_frame.setObjectName("inputFrame")
        input_layout = QHBoxLayout(input_frame)
        input_layout.setContentsMargins(15, 10, 15, 10)
        input_layout.setSpacing(10)
        
        msg_label = QLabel("💬")
        msg_label.setFont(QFont("Segoe UI", 14))
        input_layout.addWidget(msg_label)
        
        self.message_input = QLineEdit()
        self.message_input.setObjectName("messageInput")
        self.message_input.setPlaceholderText("Type your message here...")
        self.message_input.setFont(QFont("Segoe UI", self.input_font_size))
        self.message_input.returnPressed.connect(self.send_message)
        input_layout.addWidget(self.message_input)
        
        send_btn = QPushButton("Send ➤")
        send_btn.setObjectName("sendButton")
        send_btn.setFont(QFont("Segoe UI", 10, QFont.Bold))
        send_btn.clicked.connect(self.send_message)
        send_btn.setCursor(Qt.PointingHandCursor)
        input_layout.addWidget(send_btn)
        
        layout.addWidget(input_frame)
        
        # Commands
        cmd_group = QGroupBox("Quick Actions")
        cmd_group.setObjectName("commandGroup")
        cmd_group.setFont(QFont("Segoe UI", 10, QFont.Bold))
        cmd_layout = QHBoxLayout()
        cmd_layout.setSpacing(8)
        cmd_layout.setContentsMargins(10, 15, 10, 10)
        
        commands = [
            ("📡 PING", self.send_ping, "#6366f1"),
            ("🔄 PONG", self.send_pong, "#8b5cf6"),
            ("📋 Registry", lambda: self.spd.print_global_registry(), "#10b981"),
            ("🔐 Access", lambda: self.spd.print_access_list(), "#f59e0b"),
            ("🔗 Connected", lambda: self.spd.print_connected_list(), "#06b6d4"),
            ("🗑️ Clear", self.chat_display.clear, "#ef4444")
        ]
        
        for label, func, color in commands:
            btn = QPushButton(label)
            btn.setObjectName("commandButton")
            btn.setProperty("color", color)
            btn.setFont(QFont("Segoe UI", 9))
            btn.clicked.connect(func)
            btn.setCursor(Qt.PointingHandCursor)
            btn.setMinimumHeight(35)
            cmd_layout.addWidget(btn)
        
        cmd_group.setLayout(cmd_layout)
        layout.addWidget(cmd_group)
    
    def apply_styles(self):
        self.setStyleSheet("""
            QMainWindow {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                    stop:0 #0f172a, stop:1 #1e293b);
            }
            
            QWidget {
                background-color: transparent;
                color: #e2e8f0;
            }
            
            #headerFrame {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                    stop:0 #6366f1, stop:0.5 #8b5cf6, stop:1 #ec4899);
                border-radius: 12px;
                padding: 5px;
            }
            
            #statusLabel {
                color: white;
                font-weight: bold;
            }
            
            #connectionLabel {
                color: #10b981;
                font-weight: bold;
            }
            
            #chatDisplay {
                background-color: #1e293b;
                border: 2px solid #334155;
                border-radius: 12px;
                padding: 15px;
                color: #e2e8f0;
                selection-background-color: #6366f1;
            }
            
            #inputFrame {
                background-color: #1e293b;
                border: 2px solid #334155;
                border-radius: 12px;
            }
            
            #messageInput {
                background-color: #0f172a;
                border: 2px solid #475569;
                border-radius: 8px;
                padding: 8px 12px;
                color: #e2e8f0;
                font-size: 10pt;
            }
            
            #messageInput:focus {
                border: 2px solid #6366f1;
                background-color: #1e293b;
            }
            
            #sendButton {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                    stop:0 #6366f1, stop:1 #8b5cf6);
                color: white;
                border: none;
                border-radius: 8px;
                padding: 8px 20px;
                font-weight: bold;
                min-width: 80px;
            }
            
            #sendButton:hover {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                    stop:0 #4f46e5, stop:1 #7c3aed);
            }
            
            #sendButton:pressed {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                    stop:0 #4338ca, stop:1 #6d28d9);
            }
            
            #commandGroup {
                background-color: #1e293b;
                border: 2px solid #334155;
                border-radius: 12px;
                padding: 10px;
                font-weight: bold;
                color: #e2e8f0;
            }
            
            #commandGroup::title {
                subcontrol-origin: margin;
                subcontrol-position: top left;
                padding: 5px 10px;
                color: #e2e8f0;
            }
            
            #commandButton {
                background-color: #334155;
                color: white;
                border: none;
                border-radius: 8px;
                padding: 8px 12px;
                font-weight: 600;
            }
            
            #commandButton:hover {
                background-color: #475569;
            }
            
            #commandButton:pressed {
                background-color: #1e293b;
            }
            
            QPushButton#commandButton[color="#6366f1"]:hover {
                background-color: #6366f1;
            }
            
            QPushButton#commandButton[color="#8b5cf6"]:hover {
                background-color: #8b5cf6;
            }
            
            QPushButton#commandButton[color="#10b981"]:hover {
                background-color: #10b981;
            }
            
            QPushButton#commandButton[color="#f59e0b"]:hover {
                background-color: #f59e0b;
            }
            
            QPushButton#commandButton[color="#06b6d4"]:hover {
                background-color: #06b6d4;
            }
            
            QPushButton#commandButton[color="#ef4444"]:hover {
                background-color: #ef4444;
            }
            
            QScrollBar:vertical {
                background-color: #1e293b;
                width: 12px;
                border-radius: 6px;
            }
            
            QScrollBar::handle:vertical {
                background-color: #475569;
                border-radius: 6px;
                min-height: 20px;
            }
            
            QScrollBar::handle:vertical:hover {
                background-color: #6366f1;
            }
            
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
                height: 0px;
            }
            
            #fontButton {
                background-color: #334155;
                color: white;
                border: none;
                border-radius: 6px;
                padding: 5px 8px;
                font-weight: bold;
            }
            
            #fontButton:hover {
                background-color: #6366f1;
            }
            
            #fontButton:pressed {
                background-color: #4338ca;
            }
        """)
    
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
    
    def increase_font(self):
        """Increase font size for chat and input"""
        if self.chat_font_size < 24:  # Max font size
            self.chat_font_size += 1
            self.input_font_size += 1
            self.chat_display.setFont(QFont("Consolas", self.chat_font_size))
            self.message_input.setFont(QFont("Segoe UI", self.input_font_size))
    
    def decrease_font(self):
        """Decrease font size for chat and input"""
        if self.chat_font_size > 8:  # Min font size
            self.chat_font_size -= 1
            self.input_font_size -= 1
            self.chat_display.setFont(QFont("Consolas", self.chat_font_size))
            self.message_input.setFont(QFont("Segoe UI", self.input_font_size))
    
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
