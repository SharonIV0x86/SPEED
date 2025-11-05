import threading
import time
import queue
from enum import Enum
from pathlib import Path
from typing import Callable, Dict, List, Set, Optional
from concurrent.futures import ThreadPoolExecutor
import filelock

from .utils import Utils
from .access_registry import AccessRegistry
from .binary_manager import BinaryManager
from .encryption_manager import EncryptionManager
from .key_manager import KeyManager
from .message_utils import Message, PMessage, MessageType, MessageUtils

class ThreadMode(Enum):
    SINGLE = 0
    MULTI = 1

class SPEED:
    """
    SPEED IPC implementation for Python
    Compatible with C++ SPEED implementation
    """
    
    def __init__(self, proc_name: str, thread_mode: ThreadMode = ThreadMode.MULTI, 
                 speed_dir: Optional[Path] = None):
        self.proc_name = proc_name
        self.thread_mode = thread_mode
        self.speed_dir = speed_dir or Utils.get_default_speed_dir()
        self.self_speed_dir = self.speed_dir / proc_name
        
        # Initialize components
        self.access_registry = AccessRegistry(self.speed_dir, proc_name)
        self.key = ""
        self.key_path = None
        self.seq_number = 0
        
        # Callback and state management
        self.callback = None
        self.function_registry: Dict[str, Callable] = {}
        self.running = False
        self.watcher_thread = None
        
        # Threading
        self.mutex = threading.RLock()
        self.condition = threading.Condition(self.mutex)
        self.task_queue = queue.Queue()
        self.worker_thread = None
        
        # Create directories
        self._initialize_directories()
        
        # Start the system
        self.start()
    
    def _initialize_directories(self):
        """Initialize required directories"""
        if self.self_speed_dir.exists():
            # Clean up existing directory
            import shutil
            shutil.rmtree(self.self_speed_dir)
        
        Utils.create_default_dir(self.self_speed_dir)
        self.access_registry.sync_access_registry()
    
    def set_key_file(self, key_path: Path) -> bool:
        """Set encryption key from file"""
        try:
            self.key = KeyManager.get_key_from_config_file(key_path)
            self.key_path = key_path
            return Utils.validate_key(self.key)
        except Exception as e:
            print(f"Error setting key file: {e}")
            return False
    
    def set_callback(self, callback: Callable[[PMessage], None]):
        """Set message callback function"""
        self.callback = callback
    
    def send_message(self, message: str, receiver: str):
        """Send message to receiver"""
        with self.mutex:
            if not self.access_registry.check_connection(receiver):
                # Try to establish connection
                if not self._establish_connection(receiver):
                    print(f"Cannot send to {receiver}: not connected")
                    return
            
            # Construct and send message
            msg = MessageUtils.construct_msg(message, self.proc_name, receiver, self.seq_number)
            self.seq_number += 1
            
            # Encrypt if key is set
            if self.key:
                msg = EncryptionManager.encrypt(msg, self.key.encode('utf-8'))
            
            # Write to receiver's directory
            receiver_dir = self.speed_dir / receiver
            filename = f"{msg.header.seq_num:04d}_{Utils.generate_uuid()}.ospeed"
            file_path = receiver_dir / filename
            
            if BinaryManager.write_binary(msg, file_path, self.seq_number, self.proc_name):
                print(f"Message sent to {receiver}")
            else:
                print(f"Failed to send message to {receiver}")
    
    def add_process(self, proc_name: str) -> bool:
        """Add process to access list"""
        with self.mutex:
            self.access_registry.add_process_to_list(proc_name)
            return self._establish_connection(proc_name)
    
    def register_method(self, method_name: str, method: Callable):
        """Register a method for remote invocation"""
        with self.mutex:
            self.function_registry[method_name] = method
    
    def invoke_method(self, method_name: str, receiver: str, args: List[str] = None):
        """Invoke remote method on receiver"""
        with self.mutex:
            if not self.access_registry.check_connection(receiver):
                if not self._establish_connection(receiver):
                    print(f"Cannot invoke method on {receiver}: not connected")
                    return
            
            msg = MessageUtils.construct_invoke_method(method_name, self.proc_name, receiver, args)
            
            # Encrypt if key is set
            if self.key:
                msg = EncryptionManager.encrypt(msg, self.key.encode('utf-8'))
            
            # Send invocation message
            receiver_dir = self.speed_dir / receiver
            filename = f"{msg.header.seq_num:04d}_{Utils.generate_uuid()}.ospeed"
            file_path = receiver_dir / filename
            
            BinaryManager.write_binary(msg, file_path, self.seq_number, self.proc_name)
            self.seq_number += 1
    
    def ping(self, receiver: str):
        """Send ping to receiver"""
        self._send_control_message(receiver, MessageType.PING)
    
    def pong(self, receiver: str):
        """Send pong to receiver"""
        self._send_control_message(receiver, MessageType.PONG)
    
    def _send_control_message(self, receiver: str, msg_type: MessageType):
        """Send control message (PING/PONG)"""
        with self.mutex:
            if not self.access_registry.check_connection(receiver):
                return
            
            if msg_type == MessageType.PING:
                msg = MessageUtils.construct_ping(self.proc_name, receiver)
            else:
                msg = MessageUtils.construct_pong(self.proc_name, receiver)
            
            if self.key:
                msg = EncryptionManager.encrypt(msg, self.key.encode('utf-8'))
            
            receiver_dir = self.speed_dir / receiver
            filename = f"{msg.header.seq_num:04d}_{Utils.generate_uuid()}.ospeed"
            file_path = receiver_dir / filename
            
            BinaryManager.write_binary(msg, file_path, self.seq_number, self.proc_name)
            self.seq_number += 1
    
    def _establish_connection(self, proc_name: str) -> bool:
        """Establish connection with another process"""
        with self.mutex:
            # Check if already connected
            if self.access_registry.check_connection(proc_name):
                return True
            
            # Check if process exists in global registry
            if not self.access_registry.check_global_registry(proc_name):
                # Update registry and check again
                self.access_registry.incremental_build_global_registry()
                if not self.access_registry.check_global_registry(proc_name):
                    print(f"Process {proc_name} not found in global registry")
                    return False
            
            # Send connection request
            con_req = MessageUtils.construct_con_req(self.proc_name, proc_name)
            if self.key:
                con_req = EncryptionManager.encrypt(con_req, self.key.encode('utf-8'))
            
            receiver_dir = self.speed_dir / proc_name
            filename = f"{con_req.header.seq_num:04d}_{Utils.generate_uuid()}.ospeed"
            file_path = receiver_dir / filename
            
            if BinaryManager.write_binary(con_req, file_path, self.seq_number, self.proc_name):
                self.seq_number += 1
                # Wait for connection response
                return self._wait_for_connection_response(proc_name)
            return False
    
    def _wait_for_connection_response(self, proc_name: str, timeout: float = 5.0) -> bool:
        """Wait for connection response"""
        start_time = time.time()
        while time.time() - start_time < timeout:
            if self.access_registry.check_connection(proc_name):
                return True
            time.sleep(0.1)
        return False
    
    def start(self):
        """Start the SPEED system"""
        with self.mutex:
            if self.running:
                return
            
            self.running = True
            
            # Start watcher thread based on mode
            if self.thread_mode == ThreadMode.SINGLE:
                self.watcher_thread = threading.Thread(target=self._watcher_single_thread, daemon=True)
            else:
                self.watcher_thread = threading.Thread(target=self._watcher_multi_thread, daemon=True)
            
            self.watcher_thread.start()
            
            # Start worker thread for processing
            self.worker_thread = threading.Thread(target=self._worker_loop, daemon=True)
            self.worker_thread.start()
    
    def stop(self):
        """Stop the SPEED system"""
        with self.mutex:
            self.running = False
            self.condition.notify_all()
        
        # Send exit notification to connected processes
        self._send_exit_notifications()
        
        # Clean up access file
        self.access_registry.remove_access_file()
    
    def _watcher_single_thread(self):
        """Single-threaded watcher implementation"""
        while self.running:
            self._run_watcher_loop()
            time.sleep(0.1)
    
    def _watcher_multi_thread(self):
        """Multi-threaded watcher implementation"""
        with ThreadPoolExecutor(max_workers=4) as executor:
            while self.running:
                self._run_watcher_loop()
                time.sleep(0.05)
    
    def _run_watcher_loop(self):
        """Core watcher loop"""
        try:
            # Check for new files in our directory
            for file_path in self.self_speed_dir.glob("*.ospeed"):
                self._process_file(file_path)
        except Exception as e:
            print(f"Watcher error: {e}")
    
    def _process_file(self, file_path: Path):
        """Process incoming message file"""
        try:
            # Use file lock to prevent concurrent processing
            lock_file = file_path.with_suffix('.lock')
            with filelock.FileLock(lock_file):
                # Read and parse message
                message = BinaryManager.read_binary(file_path)
                if not message:
                    return
                
                # Decrypt if key is set
                if self.key:
                    message = EncryptionManager.decrypt(message, self.key.encode('utf-8'))
                
                # Validate message
                if not MessageUtils.validate_message_received(message, self.proc_name):
                    file_path.unlink()
                    return
                
                # Process based on message type
                self._handle_message(message)
                
                # Clean up processed file
                file_path.unlink()
                
        except Exception as e:
            print(f"Error processing file {file_path}: {e}")
    
    def _handle_message(self, message: Message):
        """Handle incoming message based on type"""
        if message.header.type == MessageType.MSG:
            self._handle_text_message(message)
        elif message.header.type == MessageType.CON_REQ:
            self._handle_connection_request(message)
        elif message.header.type == MessageType.CON_RES:
            self._handle_connection_response(message)
        elif message.header.type == MessageType.INVOKE_METHOD:
            self._handle_method_invocation(message)
        elif message.header.type == MessageType.PING:
            self._handle_ping(message)
        elif message.header.type == MessageType.PONG:
            self._handle_pong(message)
        elif message.header.type == MessageType.EXIT_NOTIF:
            self._handle_exit_notification(message)
    
    def _handle_text_message(self, message: Message):
        """Handle text message"""
        print(f"🔍 [DEBUG] _handle_text_message called")
        print(f"🔍 [DEBUG] Message type: {message.header.type}")
        print(f"🔍 [DEBUG] Payload length: {len(message.payload)}")
        print(f"🔍 [DEBUG] Payload bytes: {message.payload[:50]}...")  # First 50 bytes
    
        pmsg = MessageUtils.destruct_message(message)
    
        print(f"🔍 [DEBUG] PMessage created:")
        print(f"🔍 [DEBUG]   Sender: {pmsg.sender_name}")
        print(f"🔍 [DEBUG]   Message: '{pmsg.message}'")
        print(f"🔍 [DEBUG]   Message length: {len(pmsg.message)}")
    
        if self.callback:
            print(f"🔍 [DEBUG] Calling user callback...")
            self.callback(pmsg)
        else:
            print(f"Received from {pmsg.sender_name}: {pmsg.message}")    
    def _handle_connection_request(self, message: Message):
        """Handle connection request"""
        sender = message.header.sender
        
        # Check if sender is in our access list
        if self.access_registry.check_access(sender):
            # Accept connection
            self.access_registry.connect_to(sender)
            
            # Send connection response
            con_res = MessageUtils.construct_con_res(self.proc_name, sender)
            if self.key:
                con_res = EncryptionManager.encrypt(con_res, self.key.encode('utf-8'))
            
            sender_dir = self.speed_dir / sender
            filename = f"{con_res.header.seq_num:04d}_{Utils.generate_uuid()}.ospeed"
            file_path = sender_dir / filename
            
            BinaryManager.write_binary(con_res, file_path, self.seq_number, self.proc_name)
            self.seq_number += 1
    
    def _handle_connection_response(self, message: Message):
        """Handle connection response"""
        sender = message.header.sender
        self.access_registry.connect_to(sender)
    
    def _handle_method_invocation(self, message: Message):
        """Handle remote method invocation"""
        try:
            payload = bytes(message.payload).decode('utf-8')
            parts = payload.split(':', 1)
            if len(parts) == 2:
                method_name, args_str = parts
                args = args_str.split(',') if args_str else []
                
                if method_name in self.function_registry:
                    self.function_registry[method_name](args)
                else:
                    print(f"Unknown method: {method_name}")
        except Exception as e:
            print(f"Error invoking method: {e}")
    
    def _handle_ping(self, message: Message):
        """Handle ping message"""
        sender = message.header.sender
        self.pong(sender)
    
    def _handle_pong(self, message: Message):
        """Handle pong message"""
        # Currently just acknowledge
        pass
    
    def _handle_exit_notification(self, message: Message):
        """Handle exit notification"""
        sender = message.header.sender
        self.access_registry.remove_process_from_connected_list(sender)
    
    def _send_exit_notifications(self):
        """Send exit notifications to connected processes"""
        connected = self.access_registry.get_connected_list()
        for proc_name in connected:
            exit_msg = MessageUtils.construct_exit_notif(self.proc_name, proc_name)
            if self.key:
                exit_msg = EncryptionManager.encrypt(exit_msg, self.key.encode('utf-8'))
            
            receiver_dir = self.speed_dir / proc_name
            filename = f"{exit_msg.header.seq_num:04d}_{Utils.generate_uuid()}.ospeed"
            file_path = receiver_dir / filename
            
            BinaryManager.write_binary(exit_msg, file_path, self.seq_number, self.proc_name)
            self.seq_number += 1
    
    def _worker_loop(self):
        """Worker loop for processing tasks"""
        while self.running:
            try:
                # Process tasks from queue if needed
                time.sleep(0.1)
            except Exception as e:
                print(f"Worker error: {e}")
    
    def print_global_registry(self):
        """Print global registry"""
        registry = self.access_registry.get_global_registry()
        print("----------Global Registry----------")
        for proc in sorted(registry):
            print(proc)
        print("----------Global Registry----------")
    
    def print_access_list(self):
        """Print access list"""
        access_list = self.access_registry.get_access_list()
        print("----------Access List----------")
        for proc in sorted(access_list):
            print(proc)
        print("----------Access List----------")
    
    def print_connected_list(self):
        """Print connected list"""
        connected_list = self.access_registry.get_connected_list()
        print("----------Connected List----------")
        for proc in sorted(connected_list):
            print(proc)
        print("----------Connected List----------")
    
    def __enter__(self):
        """Context manager entry"""
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit"""
        self.stop()