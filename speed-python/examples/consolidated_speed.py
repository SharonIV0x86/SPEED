#!/usr/bin/env python3
# consolidated_speed_fixed.py
# Fully corrected single-file Python SPEED implementation
# - filenames now match C++ regex: <seq>_<proc>_<seq>_<uuid>.ospeed
# - key file normalization (base64/hex/raw)
# - consistent seq_num assignment and increments
# - tolerant and clear behavior for connection handshake
# - no .lock side-effects (no filelock used)

import os
import time
import uuid
import platform
import struct
import threading
import queue
from enum import Enum
from pathlib import Path
from typing import Callable, Dict, List, Set, Optional, Any
from concurrent.futures import ThreadPoolExecutor

# Optional dependency for encryption
try:
    import nacl.secret
    import nacl.utils
    import base64
except Exception:
    nacl = None
    base64 = __import__("base64")


# -----------------------
# Utilities
# -----------------------
class Utils:
    @staticmethod
    def get_default_speed_dir() -> Path:
        """Get the default SPEED directory based on OS."""
        if platform.system() == "Windows":
            base_dir = Path(os.environ.get('LOCALAPPDATA', Path.home() / 'AppData' / 'Local'))
            speed_dir = base_dir / ".speed"
        else:
            # Use /tmp/speed and create it if it doesn't exist
            speed_dir = Path("/tmp/speed")
            speed_dir.mkdir(parents=True, exist_ok=True)

        return speed_dir

    @staticmethod
    def get_current_timestamp() -> str:
        return time.strftime("%Y%m%d_%H%M%S")

    @staticmethod
    def generate_uuid() -> str:
        return str(uuid.uuid4())

    @staticmethod
    def get_timestamp_uuid() -> str:
        return f"{Utils.get_current_timestamp()}_{Utils.generate_uuid()}"

    @staticmethod
    def get_process_id() -> int:
        return os.getpid()

    @staticmethod
    def validate_key(key: Any) -> bool:
        """Expect base64 encoded key that decodes to 32 bytes."""
        try:
            key_bytes = base64.b64decode(key)
            return len(key_bytes) == 32
        except Exception:
            return False

    @staticmethod
    def file_exists(file_path: Path) -> bool:
        return file_path.exists() and file_path.is_file()

    @staticmethod
    def directory_exists(dir_path: Path) -> bool:
        return dir_path.exists() and dir_path.is_dir()

    @staticmethod
    def create_default_dir(dir_path: Path) -> bool:
        try:
            dir_path.mkdir(parents=True, exist_ok=True)
            return True
        except Exception:
            return False

    @staticmethod
    def create_access_registry_dir(registry_path: Path) -> bool:
        return Utils.create_default_dir(registry_path)


# -----------------------
# Message definitions
# -----------------------
class MessageType(Enum):
    MSG = 0
    CON_REQ = 1
    CON_RES = 2
    INVOKE_METHOD = 3
    EXIT_NOTIF = 4
    PING = 5
    PONG = 6
    # If you add new types in C++, add them here with identical values.


class MessageHeader:
    def __init__(self):
        self.version: int = 1
        self.type: MessageType = MessageType.MSG
        self.sender_pid: int = os.getpid()
        # timestamp in milliseconds
        self.timestamp: int = int(time.time() * 1000)
        self.seq_num: int = 0
        self.sender: str = ""
        self.reciever: str = ""
        # 24-byte nonce for NaCl SecretBox
        self.nonce: List[int] = [0] * 24


class PMessage:
    def __init__(self, sender_name: str = "", message: str = "",
                 timestamp: int = 0, sequence_num: int = 0):
        self.sender_name = sender_name
        self.message = message
        self.timestamp = timestamp
        self.sequence_num = sequence_num


class Message:
    def __init__(self, header: MessageHeader = None, payload: List[int] = None):
        self.header: MessageHeader = header if header else MessageHeader()
        self.payload: List[int] = payload if payload else []


# -----------------------
# Message Utilities
# -----------------------
class MessageUtils:
    @staticmethod
    def construct_msg(msg: str, sender: str, receiver: str, seq_num: int = 0) -> Message:
        message = Message()
        message.header.type = MessageType.MSG
        message.header.sender = sender
        message.header.reciever = receiver
        message.header.seq_num = seq_num
        payload_bytes = msg.encode('utf-8', errors='replace')
        message.payload = list(payload_bytes)
        return message

    @staticmethod
    def construct_con_req(sender: str, receiver: str) -> Message:
        message = Message()
        message.header.type = MessageType.CON_REQ
        message.header.sender = sender
        message.header.reciever = receiver
        return message

    @staticmethod
    def construct_con_res(sender: str, receiver: str) -> Message:
        message = Message()
        message.header.type = MessageType.CON_RES
        message.header.sender = sender
        message.header.reciever = receiver
        return message

    @staticmethod
    def construct_invoke_method(method_name: str, sender: str, receiver: str, args: List[str] = None) -> Message:
        message = Message()
        message.header.type = MessageType.INVOKE_METHOD
        message.header.sender = sender
        message.header.reciever = receiver
        payload = f"{method_name}:{','.join(args) if args else ''}"
        message.payload = list(payload.encode('utf-8'))
        return message

    @staticmethod
    def construct_exit_notif(sender: str, receiver: str) -> Message:
        message = Message()
        message.header.type = MessageType.EXIT_NOTIF
        message.header.sender = sender
        message.header.reciever = receiver
        return message

    @staticmethod
    def construct_ping(sender: str, receiver: str) -> Message:
        message = Message()
        message.header.type = MessageType.PING
        message.header.sender = sender
        message.header.reciever = receiver
        return message

    @staticmethod
    def construct_pong(sender: str, receiver: str) -> Message:
        message = Message()
        message.header.type = MessageType.PONG
        message.header.sender = sender
        message.header.reciever = receiver
        return message

    @staticmethod
    def destruct_message(message: Message) -> PMessage:
        try:
            content = bytes(message.payload).decode('utf-8', errors='replace')
        except Exception:
            content = ""
        return PMessage(
            sender_name=message.header.sender,
            message=content,
            timestamp=message.header.timestamp,
            sequence_num=message.header.seq_num
        )

    @staticmethod
    def validate_message_sent(message: Message, self_proc_name: str, receiver_name: str) -> bool:
        return (message.header.sender == self_proc_name and
                message.header.reciever == receiver_name)

    @staticmethod
    def validate_message_received(message: Message, self_proc_name: str) -> bool:
        return message.header.reciever == self_proc_name

    @staticmethod
    def print_message(message: Message):
        print(f"Type: {message.header.type.name}")
        print(f"From: {message.header.sender} -> To: {message.header.reciever}")
        print(f"Seq: {message.header.seq_num}, Time: {message.header.timestamp}")
        if message.payload:
            try:
                content = bytes(message.payload).decode('utf-8')
                print(f"Content: {content}")
            except Exception:
                print(f"Binary content: {len(message.payload)} bytes")


# -----------------------
# Binary manager (FIXED header layout)
# -----------------------
class BinaryManager:
    """
    Binary format (Python side uses 1-byte MessageType to match C++ uint8 layout):
      - version: 1 byte (B)
      - message type: 1 byte (B)
      - sender_pid: 4 bytes (>I)
      - timestamp: 8 bytes (>Q)
      - seq_num: 8 bytes (>Q)
      - sender: length (4 bytes >I) + utf-8 bytes
      - reciever: length (4 bytes >I) + utf-8 bytes
      - nonce: 24 bytes
      - payload: length (4 bytes >I) + raw bytes
    """

    @staticmethod
    def write_binary(message: Message, file_path: Path, seq_number: int = 0, proc_name: str = "") -> bool:
        try:
            file_path.parent.mkdir(parents=True, exist_ok=True)
            with open(file_path, 'wb') as f:
                BinaryManager._write_header(f, message.header)
                BinaryManager._write_bytes(f, message.payload)
            return True
        except Exception as e:
            print(f"Error writing binary: {e}")
            return False

    @staticmethod
    def read_binary(file_path: Path) -> Optional[Message]:
        try:
            with open(file_path, 'rb') as f:
                header = BinaryManager._read_header(f)
                if header is None:
                    return None
                payload = BinaryManager._read_bytes(f)
                return Message(header=header, payload=payload)
        except Exception as e:
            print(f"Error reading binary: {e}")
            return None

    @staticmethod
    def _write_header(f, header: MessageHeader):
        f.write(struct.pack('B', header.version))
        f.write(struct.pack('B', header.type.value))
        f.write(struct.pack('>I', header.sender_pid))
        f.write(struct.pack('>Q', header.timestamp))
        f.write(struct.pack('>Q', header.seq_num))
        BinaryManager._write_string(f, header.sender)
        BinaryManager._write_string(f, header.reciever)
        nonce_bytes = bytes(header.nonce) if isinstance(header.nonce, (bytes, bytearray)) else bytes(header.nonce)
        if len(nonce_bytes) < 24:
            nonce_bytes = nonce_bytes.ljust(24, b'\x00')
        f.write(nonce_bytes[:24])

    @staticmethod
    def _read_header(f) -> Optional[MessageHeader]:
        header = MessageHeader()
        ver_b = f.read(1)
        if not ver_b or len(ver_b) < 1:
            return None
        header.version = struct.unpack('B', ver_b)[0]
        mt_b = f.read(1)
        if not mt_b or len(mt_b) < 1:
            return None
        msg_type_val = struct.unpack('B', mt_b)[0]
        try:
            header.type = MessageType(msg_type_val)
        except ValueError as e:
            raise ValueError(f"{msg_type_val} is not a valid MessageType") from e
        spid_b = f.read(4)
        if len(spid_b) < 4:
            return None
        header.sender_pid = struct.unpack('>I', spid_b)[0]
        ts_b = f.read(8)
        if len(ts_b) < 8:
            return None
        header.timestamp = struct.unpack('>Q', ts_b)[0]
        seq_b = f.read(8)
        if len(seq_b) < 8:
            return None
        header.seq_num = struct.unpack('>Q', seq_b)[0]
        header.sender = BinaryManager._read_string(f)
        header.reciever = BinaryManager._read_string(f)
        nonce_bytes = f.read(24)
        if len(nonce_bytes) < 24:
            nonce_bytes = nonce_bytes.ljust(24, b'\x00')
        header.nonce = list(nonce_bytes[:24])
        return header

    @staticmethod
    def _write_string(f, s: str):
        encoded = s.encode('utf-8')
        f.write(struct.pack('>I', len(encoded)))
        f.write(encoded)

    @staticmethod
    def _read_string(f) -> str:
        length_b = f.read(4)
        if not length_b or len(length_b) < 4:
            return ""
        length = struct.unpack('>I', length_b)[0]
        if length == 0:
            return ""
        s = f.read(length)
        if not s or len(s) < length:
            return ""
        return s.decode('utf-8', errors='replace')

    @staticmethod
    def _write_bytes(f, data: List[int]):
        f.write(struct.pack('>I', len(data)))
        f.write(bytes(data))

    @staticmethod
    def _read_bytes(f) -> List[int]:
        length_b = f.read(4)
        if not length_b or len(length_b) < 4:
            return []
        length = struct.unpack('>I', length_b)[0]
        if length == 0:
            return []
        data = f.read(length)
        if not data:
            return []
        return list(data)


# -----------------------
# Encryption manager
# -----------------------
class EncryptionManager:
    @staticmethod
    def encrypt(message: Message, key: Any) -> Message:
        """Encrypt message payload - accepts base64 key (str or bytes)"""
        if nacl is None:
            print("Encryption requested but pynacl not available.")
            return message
        try:
            key_bytes = base64.b64decode(key)
            if len(key_bytes) != nacl.secret.SecretBox.KEY_SIZE:
                raise ValueError("Invalid key length for SecretBox")
            nonce = nacl.utils.random(nacl.secret.SecretBox.NONCE_SIZE)
            message.header.nonce = list(nonce)
            box = nacl.secret.SecretBox(key_bytes)
            encrypted = box.encrypt(bytes(message.payload), nonce)
            # store ciphertext (PyNaCl's encrypted.ciphertext)
            message.payload = list(encrypted.ciphertext)
            return message
        except Exception as e:
            print(f"Encryption error: {e}")
            return message

    @staticmethod
    def decrypt(message: Message, key: Any) -> Message:
        if nacl is None:
            print("Decryption requested but pynacl not available.")
            return message
        try:
            key_bytes = base64.b64decode(key)
            box = nacl.secret.SecretBox(key_bytes)
            nonce = bytes(message.header.nonce)
            ciphertext = bytes(message.payload)
            combined = nonce + ciphertext
            decrypted = box.decrypt(combined)
            message.payload = list(decrypted)
            return message
        except Exception as e:
            print(f"Decryption error: {e}")
            return message


# -----------------------
# Key manager (robust)
# -----------------------
class KeyManager:
    @staticmethod
    def get_key_from_config_file(key_path: Path) -> str:
        """
        Read a key file and return a base64-encoded key string (32 bytes raw).
        Supports:
         - base64-encoded content (returns original)
         - hex-encoded content (converts to base64)
         - raw 32-byte binary file (converts to base64)
        """
        import base64 as _b64, binascii as _binascii
        try:
            raw = key_path.read_bytes()
        except Exception as e:
            print(f"Error reading key file: {e}")
            return ""
        s = raw.strip()
        # Try base64 first
        try:
            decoded = _b64.b64decode(s, validate=True)
            if len(decoded) == 32:
                return s.decode('utf-8') if isinstance(s, bytes) else s
        except Exception:
            pass
        # Try hex
        try:
            decoded = _binascii.unhexlify(s)
            if len(decoded) == 32:
                return _b64.b64encode(decoded).decode('utf-8')
        except Exception:
            pass
        # Try raw 32 bytes
        if len(raw) == 32:
            return _b64.b64encode(raw).decode('utf-8')
        print("Key file format not recognized or incorrect length (must represent 32 raw bytes).")
        return ""


# -----------------------
# Access registry
# -----------------------
class AccessRegistry:
    def __init__(self, speed_dir: Path, proc_name: str):
        self.speed_dir = speed_dir
        self.proc_name = proc_name
        self.access_registry_dir = speed_dir / "access_registry"
        self.allowed_processes: Set[str] = set()
        self.global_registry: Set[str] = set()
        self.connected_list: Set[str] = set()
        self.mutex = threading.Lock()
        Utils.create_default_dir(speed_dir)
        Utils.create_access_registry_dir(self.access_registry_dir)
        self.incremental_build_global_registry()

    def add_process_to_list(self, proc_name: str):
        with self.mutex:
            self.allowed_processes.add(proc_name)

    def incremental_build_global_registry(self):
        with self.mutex:
            self.global_registry.clear()
            if not self.access_registry_dir.exists():
                return
            for file_path in self.access_registry_dir.glob("*.oregistry"):
                try:
                    with open(file_path, 'r') as f:
                        proc = f.read().strip()
                        if proc:
                            self.global_registry.add(proc)
                except Exception:
                    continue

    def remove_access_file(self):
        access_file = self.access_registry_dir / f"{self.proc_name}.oregistry"
        if access_file.exists():
            try:
                access_file.unlink()
            except Exception:
                pass

    def sync_access_registry(self):
        access_file = self.access_registry_dir / f"{self.proc_name}.iregistry"
        try:
            with open(access_file, 'w') as f:
                f.write(self.proc_name)
            ready_file = self.access_registry_dir / f"{self.proc_name}.oregistry"
            access_file.rename(ready_file)
        except Exception as e:
            print(f"Error syncing access registry: {e}")

    def remove_process_from_global_registry(self, proc_name: str) -> bool:
        with self.mutex:
            if proc_name in self.global_registry:
                self.global_registry.remove(proc_name)
                return True
            return False

    def check_access(self, proc_name: str) -> bool:
        with self.mutex:
            return proc_name in self.allowed_processes

    def remove_process_from_access_list(self, proc_name: str) -> bool:
        with self.mutex:
            if proc_name in self.allowed_processes:
                self.allowed_processes.remove(proc_name)
                return True
            return False

    def remove_process_from_connected_list(self, proc_name: str) -> bool:
        with self.mutex:
            if proc_name in self.connected_list:
                self.connected_list.remove(proc_name)
                return True
            return False

    def connect_to(self, proc_name: str) -> bool:
        with self.mutex:
            if (proc_name in self.allowed_processes and
                    proc_name in self.global_registry):
                self.connected_list.add(proc_name)
                return True
            return False

    def check_global_registry(self, proc_name: str) -> bool:
        with self.mutex:
            return proc_name in self.global_registry

    def check_connection(self, proc_name: str) -> bool:
        with self.mutex:
            return proc_name in self.connected_list

    def get_global_registry(self) -> Set[str]:
        with self.mutex:
            return self.global_registry.copy()

    def get_access_list(self) -> Set[str]:
        with self.mutex:
            return self.allowed_processes.copy()

    def get_connected_list(self) -> Set[str]:
        with self.mutex:
            return self.connected_list.copy()


# -----------------------
# SPEED main class
# -----------------------
class ThreadMode(Enum):
    SINGLE = 0
    MULTI = 1


class SPEED:
    """
    SPEED IPC implementation (single-file corrected).
    """

    def __init__(self, proc_name: str, thread_mode: ThreadMode = ThreadMode.MULTI,
                 speed_dir: Optional[Path] = None):
        self.proc_name = proc_name
        self.thread_mode = thread_mode
        self.speed_dir = speed_dir or Utils.get_default_speed_dir()
        self.self_speed_dir = self.speed_dir / proc_name

        self.access_registry = AccessRegistry(self.speed_dir, proc_name)
        self.key: Any = ""
        self.key_path: Optional[Path] = None
        self.seq_number: int = 0

        self.callback: Optional[Callable[[PMessage], None]] = None
        self.function_registry: Dict[str, Callable] = {}
        self.running: bool = False
        self.watcher_thread: Optional[threading.Thread] = None

        self.mutex = threading.RLock()
        self.condition = threading.Condition(self.mutex)
        self.task_queue = queue.Queue()
        self.worker_thread: Optional[threading.Thread] = None

        # Create directories and register
        self._initialize_directories()

        # Start the system
        self.start()

    # -----------------------
    # filename generator matching C++ regex:
    # (<seq>)_<proc>_(<seq>)_<uuid>.ospeed
    # -----------------------
    def _make_ospeed_filename(self, msg: Message) -> str:
        seq = int(msg.header.seq_num if hasattr(msg.header, "seq_num") else 0)
        sender = msg.header.sender if getattr(msg.header, "sender", "") else self.proc_name
        # ensure sender has only allowed chars (C++ regex accepts A-Za-z0-9_)
        safe_sender = "".join(c if (c.isalnum() or c == '_') else '_' for c in sender)
        return f"{seq:04d}_{safe_sender}_{seq:04d}_{Utils.generate_uuid()}.ospeed"

    # -----------------------
    def _initialize_directories(self):
        if self.self_speed_dir.exists():
            import shutil
            try:
                shutil.rmtree(self.self_speed_dir)
            except Exception:
                pass
        Utils.create_default_dir(self.self_speed_dir)
        self.access_registry.sync_access_registry()

    def set_key_file(self, key_path: Path) -> bool:
        try:
            key = KeyManager.get_key_from_config_file(key_path)
            if not key:
                print("[ERROR] Could not read/normalize key file.")
                return False
            self.key = key
            self.key_path = key_path
            return Utils.validate_key(self.key)
        except Exception as e:
            print(f"Error setting key file: {e}")
            return False

    def set_callback(self, callback: Callable[[PMessage], None]):
        self.callback = callback

    # -----------------------
    # Sending helpers: ensure seq_num assignment BEFORE filename generation + increment AFTER successful write
    # -----------------------
    def send_message(self, message: str, receiver: str):
        with self.mutex:
            if not self.access_registry.check_connection(receiver):
                if not self._establish_connection(receiver):
                    print(f"Cannot send to {receiver}: not connected")
                    return

            msg = MessageUtils.construct_msg(message, self.proc_name, receiver, self.seq_number)
            # Prepare receiver dir & filename
            receiver_dir = self.speed_dir / receiver
            receiver_dir.mkdir(parents=True, exist_ok=True)
            filename = self._make_ospeed_filename(msg)
            file_path = receiver_dir / filename

            if self.key:
                msg = EncryptionManager.encrypt(msg, self.key)

            if BinaryManager.write_binary(msg, file_path, self.seq_number, self.proc_name):
                print(f"Message sent to {receiver}")
                self.seq_number += 1
            else:
                print(f"Failed to send message to {receiver}")

    def add_process(self, proc_name: str) -> bool:
        with self.mutex:
            self.access_registry.add_process_to_list(proc_name)
            return self._establish_connection(proc_name)

    def force_connect(self, proc_name: str):
        """Developer helper: forcefully mark a process as connected (not recommended for production)."""
        self.access_registry.add_process_to_list(proc_name)
        self.access_registry.connect_to(proc_name)

    def register_method(self, method_name: str, method: Callable):
        with self.mutex:
            self.function_registry[method_name] = method

    def invoke_method(self, method_name: str, receiver: str, args: List[str] = None):
        with self.mutex:
            if not self.access_registry.check_connection(receiver):
                if not self._establish_connection(receiver):
                    print(f"Cannot invoke method on {receiver}: not connected")
                    return
            msg = MessageUtils.construct_invoke_method(method_name, self.proc_name, receiver, args)
            msg.header.seq_num = self.seq_number
            receiver_dir = self.speed_dir / receiver
            receiver_dir.mkdir(parents=True, exist_ok=True)
            filename = self._make_ospeed_filename(msg)
            file_path = receiver_dir / filename
            if self.key:
                msg = EncryptionManager.encrypt(msg, self.key)
            if BinaryManager.write_binary(msg, file_path, self.seq_number, self.proc_name):
                self.seq_number += 1

    def ping(self, receiver: str):
        self._send_control_message(receiver, MessageType.PING)

    def pong(self, receiver: str):
        self._send_control_message(receiver, MessageType.PONG)

    def _send_control_message(self, receiver: str, msg_type: MessageType):
        with self.mutex:
            if not self.access_registry.check_connection(receiver):
                return
            if msg_type == MessageType.PING:
                msg = MessageUtils.construct_ping(self.proc_name, receiver)
            else:
                msg = MessageUtils.construct_pong(self.proc_name, receiver)
            msg.header.seq_num = self.seq_number
            receiver_dir = self.speed_dir / receiver
            receiver_dir.mkdir(parents=True, exist_ok=True)
            filename = self._make_ospeed_filename(msg)
            file_path = receiver_dir / filename
            if self.key:
                msg = EncryptionManager.encrypt(msg, self.key)
            if BinaryManager.write_binary(msg, file_path, self.seq_number, self.proc_name):
                self.seq_number += 1

    def _establish_connection(self, proc_name: str) -> bool:
        with self.mutex:
            # Already connected?
            if self.access_registry.check_connection(proc_name):
                return True

            # Wait briefly for global registry entries (race between processes)
            receiver_dir = self.speed_dir / proc_name
            for _ in range(30):  # up to ~3 seconds
                self.access_registry.incremental_build_global_registry()
                if self.access_registry.check_global_registry(proc_name):
                    break
                if receiver_dir.exists():
                    # might be there but not registered - re-scan registry files
                    self.access_registry.incremental_build_global_registry()
                time.sleep(0.1)

            if not self.access_registry.check_global_registry(proc_name):
                print(f"Process {proc_name} not found in global registry")
                return False

            # Send connection request
            con_req = MessageUtils.construct_con_req(self.proc_name, proc_name)
            con_req.header.seq_num = self.seq_number
            if self.key:
                con_req = EncryptionManager.encrypt(con_req, self.key)
            receiver_dir.mkdir(parents=True, exist_ok=True)
            filename = self._make_ospeed_filename(con_req)
            file_path = receiver_dir / filename
            if BinaryManager.write_binary(con_req, file_path, self.seq_number, self.proc_name):
                # increment seq after successful write and then wait for response
                self.seq_number += 1
                return self._wait_for_connection_response(proc_name)
            return False

    def _wait_for_connection_response(self, proc_name: str, timeout: float = 5.0) -> bool:
        start_time = time.time()
        while time.time() - start_time < timeout:
            if self.access_registry.check_connection(proc_name):
                return True
            # also refresh global registry (in case response created registry side effects)
            self.access_registry.incremental_build_global_registry()
            time.sleep(0.1)
        return False

    def start(self):
        with self.mutex:
            if self.running:
                return
            self.running = True
            if self.thread_mode == ThreadMode.SINGLE:
                self.watcher_thread = threading.Thread(target=self._watcher_single_thread, daemon=True)
            else:
                self.watcher_thread = threading.Thread(target=self._watcher_multi_thread, daemon=True)
            self.watcher_thread.start()
            self.worker_thread = threading.Thread(target=self._worker_loop, daemon=True)
            self.worker_thread.start()

    def stop(self):
        with self.mutex:
            self.running = False
            try:
                self.condition.notify_all()
            except Exception:
                pass
        self._send_exit_notifications()
        self.access_registry.remove_access_file()

    def _watcher_single_thread(self):
        while self.running:
            self._run_watcher_loop()
            time.sleep(0.1)

    def _watcher_multi_thread(self):
        with ThreadPoolExecutor(max_workers=4) as executor:
            while self.running:
                self._run_watcher_loop()
                time.sleep(0.05)

    def _run_watcher_loop(self):
        try:
            if not self.self_speed_dir.exists():
                return
            for file_path in self.self_speed_dir.glob("*.ospeed"):
                # only try files whose names match C++ expected pattern quickly
                try:
                    # quick validation: filename must have at least 3 underscores to resemble pattern
                    if file_path.name.count("_") < 3:
                        continue
                    self._process_file(file_path)
                except Exception:
                    continue
        except Exception as e:
            print(f"Watcher error: {e}")

    def _process_file(self, file_path: Path):
        """Process incoming message file"""
        try:
            message = BinaryManager.read_binary(file_path)
            if not message:
                try:
                    file_path.unlink()
                except Exception:
                    pass
                return

            if self.key:
                # decrypt in-place (will print decryption errors if any)
                message = EncryptionManager.decrypt(message, self.key)

            if not MessageUtils.validate_message_received(message, self.proc_name):
                try:
                    file_path.unlink()
                except Exception:
                    pass
                return

            self._handle_message(message)

            try:
                file_path.unlink()
            except Exception:
                pass

        except Exception as e:
            print(f"Error processing file {file_path}: {e}")

    def _handle_message(self, message: Message):
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
        else:
            # Unknown type - ignore
            pass

    def _handle_text_message(self, message: Message):
        pmsg = MessageUtils.destruct_message(message)
        if self.callback:
            try:
                self.callback(pmsg)
            except Exception as e:
                print(f"Callback error: {e}")
        else:
            print(f"Received from {pmsg.sender_name}: {pmsg.message}")

    def _handle_connection_request(self, message: Message):
        sender = message.header.sender
        # Accept connection only if sender is in our allowed list
        if self.access_registry.check_access(sender):
            self.access_registry.connect_to(sender)
            con_res = MessageUtils.construct_con_res(self.proc_name, sender)
            con_res.header.seq_num = self.seq_number
            if self.key:
                con_res = EncryptionManager.encrypt(con_res, self.key)
            sender_dir = self.speed_dir / sender
            sender_dir.mkdir(parents=True, exist_ok=True)
            filename = self._make_ospeed_filename(con_res)
            file_path = sender_dir / filename
            if BinaryManager.write_binary(con_res, file_path, self.seq_number, self.proc_name):
                self.seq_number += 1

    def _handle_connection_response(self, message: Message):
        sender = message.header.sender
        # Mark as connected
        self.access_registry.connect_to(sender)

    def _handle_method_invocation(self, message: Message):
        try:
            payload = bytes(message.payload).decode('utf-8', errors='replace')
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
        sender = message.header.sender
        self.pong(sender)

    def _handle_pong(self, message: Message):
        # currently no-op
        pass

    def _handle_exit_notification(self, message: Message):
        sender = message.header.sender
        self.access_registry.remove_process_from_connected_list(sender)

    def _send_exit_notifications(self):
        connected = self.access_registry.get_connected_list()
        for proc_name in connected:
            exit_msg = MessageUtils.construct_exit_notif(self.proc_name, proc_name)
            exit_msg.header.seq_num = self.seq_number
            if self.key:
                exit_msg = EncryptionManager.encrypt(exit_msg, self.key)
            receiver_dir = self.speed_dir / proc_name
            receiver_dir.mkdir(parents=True, exist_ok=True)
            filename = self._make_ospeed_filename(exit_msg)
            file_path = receiver_dir / filename
            if BinaryManager.write_binary(exit_msg, file_path, self.seq_number, self.proc_name):
                self.seq_number += 1

    def _worker_loop(self):
        while self.running:
            try:
                time.sleep(0.1)
            except Exception as e:
                print(f"Worker error: {e}")

    def print_global_registry(self):
        registry = self.access_registry.get_global_registry()
        print("----------Global Registry----------")
        for proc in sorted(registry):
            print(proc)
        print("----------Global Registry----------")

    def print_access_list(self):
        access_list = self.access_registry.get_access_list()
        print("----------Access List----------")
        for proc in sorted(access_list):
            print(proc)
        print("----------Access List----------")

    def print_connected_list(self):
        connected_list = self.access_registry.get_connected_list()
        print("----------Connected List----------")
        for proc in sorted(connected_list):
            print(proc)
        print("----------Connected List----------")

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.stop()


# Expose top-level names if used as a module
__all__ = ['SPEED', 'ThreadMode', 'PMessage', 'MessageType']
