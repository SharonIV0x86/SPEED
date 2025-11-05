#!/usr/bin/env python3
"""
Debug SPEED chat with detailed logging
"""

import time
import sys
from pathlib import Path
from speed import SPEED, ThreadMode, PMessage

def message_callback(msg: PMessage):
    print(f"\n" + "="*50)
    print(f"📨 CALLBACK TRIGGERED!")
    print(f"   From: {msg.sender_name}")
    print(f"   Message: '{msg.message}'")
    print(f"   Timestamp: {msg.timestamp}")
    print(f"   Sequence: {msg.sequence_num}")
    print(f"   Message length: {len(msg.message)}")
    print("="*50)
    print("You: ", end="", flush=True)

def process_a():
    """Process A - Chat participant"""
    print("🚀 Starting Process A (Chat Terminal 1)...")
    print("=" * 50)
    
    config_key_path = Path(__file__).parent / "config.key"
    
    with SPEED("ProcessA", ThreadMode.MULTI) as speed_a:
        print("🔧 SPEED instance created")
        
        if speed_a.set_key_file(config_key_path):
            print("✅ Encryption enabled")
        else:
            print("❌ Encryption failed")
            return
        
        speed_a.set_callback(message_callback)
        print("🔧 Callback set")
        
        print("⏳ Adding ProcessB to access list...")
        if speed_a.add_process("ProcessB"):
            print("✅ ProcessB added successfully")
        else:
            print("⚠ ProcessB not in registry yet (will auto-connect)")
        
        # Print connection status
        print(f"🔗 Connected to ProcessB: {speed_a.access_registry.check_connection('ProcessB')}")
        
        print("🟢 Process A started! Type messages and press Enter")
        print("=" * 50)
        print("You: ", end="", flush=True)
        
        message_count = 0
        try:
            while True:
                user_input = input()
                if user_input.strip():
                    message_count += 1
                    print(f"📤 Sending message #{message_count}: '{user_input}'")
                    speed_a.send_message(user_input, "ProcessB")
                    print(f"✅ Message #{message_count} sent successfully")
                    print("You: ", end="", flush=True)
                    
        except KeyboardInterrupt:
            print("\n\n👋 Process A stopping...")
        except EOFError:
            print("\n\n👋 Process A stopping...")

def process_b():
    """Process B - Chat participant"""
    print("🚀 Starting Process B (Chat Terminal 2)...")
    print("=" * 50)
    
    config_key_path = Path(__file__).parent / "config.key"
    
    with SPEED("ProcessB", ThreadMode.MULTI) as speed_b:
        print("🔧 SPEED instance created")
        
        if speed_b.set_key_file(config_key_path):
            print("✅ Encryption enabled")
        else:
            print("❌ Encryption failed")
            return
        
        speed_b.set_callback(message_callback)
        print("🔧 Callback set")
        
        print("⏳ Adding ProcessA to access list...")
        if speed_b.add_process("ProcessA"):
            print("✅ ProcessA added successfully")
        else:
            print("⚠ ProcessA not in registry yet (will auto-connect)")
        
        # Print connection status
        print(f"🔗 Connected to ProcessA: {speed_b.access_registry.check_connection('ProcessA')}")
        
        print("🟢 Process B started! Type messages and press Enter")
        print("=" * 50)
        print("You: ", end="", flush=True)
        
        message_count = 0
        try:
            while True:
                user_input = input()
                if user_input.strip():
                    message_count += 1
                    print(f"📤 Sending message #{message_count}: '{user_input}'")
                    speed_b.send_message(user_input, "ProcessA")
                    print(f"✅ Message #{message_count} sent successfully")
                    print("You: ", end="", flush=True)
                    
        except KeyboardInterrupt:
            print("\n\n👋 Process B stopping...")
        except EOFError:
            print("\n\n👋 Process B stopping...")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python debug_chat.py <A|B>")
        print("  A - Run as Process A (Terminal 1)")
        print("  B - Run as Process B (Terminal 2)")
        sys.exit(1)
    
    if sys.argv[1].upper() == "A":
        process_a()
    elif sys.argv[1].upper() == "B":
        process_b()
    else:
        print("Invalid argument. Use 'A' or 'B'")