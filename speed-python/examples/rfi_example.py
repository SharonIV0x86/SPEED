#!/usr/bin/env python3
"""
SPEED RFI (Remote Function Invocation) example
"""

import time
from speed import SPEED, ThreadMode

class Calculator:
    def add(self, args):
        if len(args) >= 2:
            a, b = float(args[0]), float(args[1])
            result = a + b
            print(f"Calculator: {a} + {b} = {result}")
        else:
            print("Calculator: Need 2 arguments for add")
    
    def multiply(self, args):
        if len(args) >= 2:
            a, b = float(args[0]), float(args[1])
            result = a * b
            print(f"Calculator: {a} * {b} = {result}")
        else:
            print("Calculator: Need 2 arguments for multiply")

def server_process():
    """Server process that provides calculator functions"""
    calculator = Calculator()
    
    with SPEED("CalculatorServer", ThreadMode.MULTI) as speed:
        # Register methods for remote invocation
        speed.register_method("add", calculator.add)
        speed.register_method("multiply", calculator.multiply)
        
        print("Calculator server started. Press Ctrl+C to stop.")
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            print("Server stopping...")

def client_process():
    """Client process that invokes remote methods"""
    with SPEED("CalculatorClient", ThreadMode.MULTI) as speed:
        speed.add_process("CalculatorServer")
        
        print("Calculator client started. Press Ctrl+C to stop.")
        try:
            # Wait for connection
            time.sleep(2)
            
            # Invoke remote methods
            print("Invoking remote methods...")
            speed.invoke_method("add", "CalculatorServer", ["5", "3"])
            time.sleep(1)
            speed.invoke_method("multiply", "CalculatorServer", ["4", "7"])
            time.sleep(1)
            speed.invoke_method("add", "CalculatorServer", ["10", "20"])
            
            # Keep running to see responses
            time.sleep(5)
            
        except KeyboardInterrupt:
            print("Client stopping...")

if __name__ == "__main__":
    import sys
    if len(sys.argv) != 2:
        print("Usage: python rfi_example.py <server|client>")
        sys.exit(1)
    
    if sys.argv[1] == "server":
        server_process()
    elif sys.argv[1] == "client":
        client_process()
    else:
        print("Invalid argument. Use 'server' or 'client'")