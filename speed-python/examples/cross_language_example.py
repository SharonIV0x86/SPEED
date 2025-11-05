#!/usr/bin/env python3
"""
Cross-language communication example
Python process communicating with C++ process
"""

import time
from speed import SPEED, ThreadMode, PMessage

def python_process():
    """Python process that communicates with C++ process"""
    
    def message_handler(msg: PMessage):
        print(f"Python received from {msg.sender_name}: {msg.message}")
    
    with SPEED("PythonProcess", ThreadMode.MULTI) as speed:
        speed.set_callback(message_handler)
        speed.add_process("CppProcess")  # Connect to C++ process
        
        print("Python process started. Will communicate with C++ process.")
        print("Press Ctrl+C to stop.")
        
        try:
            counter = 0
            while True:
                message = f"Hello from Python! Message #{counter}"
                speed.send_message(message, "CppProcess")
                counter += 1
                time.sleep(2)
        except KeyboardInterrupt:
            print("Python process stopping...")

if __name__ == "__main__":
    python_process()