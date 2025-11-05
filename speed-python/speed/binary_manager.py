import struct
from pathlib import Path
from typing import List, Tuple, Any
from .message_utils import Message, MessageHeader, MessageType

class BinaryManager:
    @staticmethod
    def write_binary(message: Message, file_path: Path, seq_number: int, proc_name: str) -> bool:
        """Write message to binary file"""
        try:
            with open(file_path, 'wb') as f:
                # Write header
                BinaryManager._write_header(f, message.header)
                # Write payload
                BinaryManager._write_bytes(f, message.payload)
            return True
        except Exception as e:
            print(f"Error writing binary: {e}")
            return False
    
    @staticmethod
    def read_binary(file_path: Path) -> Message:
        """Read message from binary file"""
        try:
            with open(file_path, 'rb') as f:
                # Read header
                header = BinaryManager._read_header(f)
                # Read payload
                payload = BinaryManager._read_bytes(f)
                
                return Message(header=header, payload=payload)
        except Exception as e:
            print(f"Error reading binary: {e}")
            return None
    
    @staticmethod
    def _write_header(f, header: MessageHeader):
        """Write message header to file"""
        # Version (1 byte)
        f.write(struct.pack('B', header.version))
        # MessageType (4 bytes as int)
        f.write(struct.pack('>I', header.type.value))
        # Sender PID (4 bytes)
        f.write(struct.pack('>I', header.sender_pid))
        # Timestamp (8 bytes)
        f.write(struct.pack('>Q', header.timestamp))
        # Sequence number (8 bytes)
        f.write(struct.pack('>Q', header.seq_num))
        # Sender (length + string)
        BinaryManager._write_string(f, header.sender)
        # Receiver (length + string)
        BinaryManager._write_string(f, header.reciever)
        # Nonce (24 bytes)
        f.write(bytes(header.nonce))
    
    @staticmethod
    def _read_header(f) -> MessageHeader:
        """Read message header from file"""
        header = MessageHeader()
        
        # Version
        header.version = struct.unpack('B', f.read(1))[0]
        # MessageType
        msg_type_val = struct.unpack('>I', f.read(4))[0]
        header.type = MessageType(msg_type_val)
        # Sender PID
        header.sender_pid = struct.unpack('>I', f.read(4))[0]
        # Timestamp
        header.timestamp = struct.unpack('>Q', f.read(8))[0]
        # Sequence number
        header.seq_num = struct.unpack('>Q', f.read(8))[0]
        # Sender
        header.sender = BinaryManager._read_string(f)
        # Receiver
        header.reciever = BinaryManager._read_string(f)
        # Nonce
        header.nonce = list(f.read(24))
        
        return header
    
    @staticmethod
    def _write_string(f, s: str):
        """Write string with length prefix"""
        encoded = s.encode('utf-8')
        f.write(struct.pack('>I', len(encoded)))
        f.write(encoded)
    
    @staticmethod
    def _read_string(f) -> str:
        """Read string with length prefix"""
        length = struct.unpack('>I', f.read(4))[0]
        return f.read(length).decode('utf-8')
    
    @staticmethod
    def _write_bytes(f, data: List[int]):
        """Write byte array with length prefix"""
        f.write(struct.pack('>I', len(data)))
        f.write(bytes(data))
    
    @staticmethod
    def _read_bytes(f) -> List[int]:
        """Read byte array with length prefix"""
        length = struct.unpack('>I', f.read(4))[0]
        return list(f.read(length))