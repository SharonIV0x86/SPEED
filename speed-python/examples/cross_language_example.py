#!/usr/bin/env python3
"""
Cross-language chat example
Python process communicating with C++ process interactively
"""

import sys
import time
from pathlib import Path

# Add parent directory to sys.path so Python can import consolidated_speed.py
sys.path.append(str(Path(__file__).resolve().parent.parent))

from consolidated_speed import SPEED, ThreadMode, PMessage  # Use the single-file implementation


def message_handler(msg: PMessage):
    """Handle incoming messages from C++"""
    print("\n[RECEIVED]")
    print(f"Message: {msg.message}")
    print(f"Sender:  {msg.sender_name}")
    print(f"TS:      {msg.timestamp}")


def python_process():
    """Interactive Python chat process (SPEED IPC with C++ side)"""
    # Initialize SPEED IPC
    spd = SPEED("Proc_1PY", ThreadMode.MULTI)
    spd.set_callback(message_handler)
    spd.add_process("Proc_1CPP")

    # Use the same key file as the C++ side
    # key_path = Path("/home/jasper/Development/finalSPEED/SPEED-PY/SPEED/speed-cpp/config.key")
    # if not key_path.exists():
    #     print(f"[ERROR] Key file not found: {key_path}")
    #     return

    # if not spd.set_key_file(key_path):
    #     print("[ERROR] Invalid or malformed key file.")
    #     return

    spd.start()

    print("==============================================")
    print(" Python ↔ C++ Chat (SPEED IPC) ")
    print("==============================================")
    print("Commands:")
    print("  --exit    Quit the chat")
    print("  --ping    Send a PING signal")
    print("  --pong    Send a PONG signal")
    print("  --getGR   Show Global Registry")
    print("  --getAL   Show Access List")
    print("  --getCL   Show Connected List")
    print("==============================================\n")

    try:
        while True:
            s = input("Enter a message to send: ").strip()
            if not s:
                continue

            if s == "--exit":
                break
            elif s == "--ping":
                spd.ping("Proc_1CPP")
            elif s == "--pong":
                spd.pong("Proc_1CPP")
            elif s == "--getGR":
                spd.print_global_registry()
            elif s == "--getAL":
                spd.print_access_list()
            elif s == "--getCL":
                spd.print_connected_list()
            else:
                spd.force_connect("Proc_1CPP")
                spd.send_message(s, "Proc_1CPP")

    except KeyboardInterrupt:
        print("\n[INFO] Interrupted by user.")
    finally:
        print("[INFO] Shutting down...")
        spd.stop()
        time.sleep(1)
        print("[INFO] Python SPEED process stopped.")


if __name__ == "__main__":
    python_process()
