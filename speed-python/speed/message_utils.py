from enum import Enum
from typing import List
import time
import os

class MessageType(Enum):
    MSG = 0
    CON_REQ = 1
    CON_RES = 2
    INVOKE_METHOD = 3
    EXIT_NOTIF = 4
    PING = 5
    PONG = 6

class MessageHeader:
    def __init__(self):
        self.version = 1
        self.type = MessageType.MSG
        self.sender_pid = os.getpid()
        self.timestamp = int(time.time() * 1000)
        self.seq_num = 0
        self.sender = ""
        self.reciever = ""
        self.nonce = [0] * 24

class PMessage:
    def __init__(self, sender_name: str = "", message: str = "", 
                 timestamp: int = 0, sequence_num: int = 0):
        self.sender_name = sender_name
        self.message = message
        self.timestamp = timestamp
        self.sequence_num = sequence_num

class Message:
    def __init__(self, header: MessageHeader = None, payload: List[int] = None):
        self.header = header if header else MessageHeader()
        self.payload = payload if payload else []

class MessageUtils:
    @staticmethod
    def construct_msg(msg: str, sender: str, receiver: str, seq_num: int = 0) -> Message:
        """Construct a MSG type message"""
        print(f"📝 [CONSTRUCT_MSG] Creating message:")
        print(f"📝 [CONSTRUCT_MSG]   Content: '{msg}'")
        print(f"📝 [CONSTRUCT_MSG]   Sender: {sender}")
        print(f"📝 [CONSTRUCT_MSG]   Receiver: {receiver}")
        print(f"📝 [CONSTRUCT_MSG]   Seq: {seq_num}")
    
        message = Message()
        message.header.type = MessageType.MSG
        message.header.sender = sender
        message.header.reciever = receiver
        message.header.seq_num = seq_num

        # Convert message to bytes
        payload_bytes = msg.encode('utf-8')
        message.payload = list(payload_bytes)
    
        print(f"📝 [CONSTRUCT_MSG]   Final payload: {message.payload[:20]}... (length: {len(message.payload)})")
        print(f"📝 [CONSTRUCT_MSG]   Payload as string: '{bytes(message.payload).decode('utf-8', errors='replace')}'")

        return message    
    @staticmethod
    def construct_con_req(sender: str, receiver: str) -> Message:
        """Construct CON_REQ message"""
        message = Message()
        message.header.type = MessageType.CON_REQ
        message.header.sender = sender
        message.header.reciever = receiver
        return message
    
    @staticmethod
    def construct_con_res(sender: str, receiver: str) -> Message:
        """Construct CON_RES message"""
        message = Message()
        message.header.type = MessageType.CON_RES
        message.header.sender = sender
        message.header.reciever = receiver
        return message
    
    @staticmethod
    def construct_invoke_method(method_name: str, sender: str, receiver: str, args: List[str] = None) -> Message:
        """Construct INVOKE_METHOD message"""
        message = Message()
        message.header.type = MessageType.INVOKE_METHOD
        message.header.sender = sender
        message.header.reciever = receiver
        
        # Serialize method invocation
        payload = f"{method_name}:{','.join(args) if args else ''}"
        message.payload = list(payload.encode('utf-8'))
        return message
    
    @staticmethod
    def construct_exit_notif(sender: str, receiver: str) -> Message:
        """Construct EXIT_NOTIF message"""
        message = Message()
        message.header.type = MessageType.EXIT_NOTIF
        message.header.sender = sender
        message.header.reciever = receiver
        return message
    
    @staticmethod
    def construct_ping(sender: str, receiver: str) -> Message:
        """Construct PING message"""
        message = Message()
        message.header.type = MessageType.PING
        message.header.sender = sender
        message.header.reciever = receiver
        return message
    
    @staticmethod
    def construct_pong(sender: str, receiver: str) -> Message:
        """Construct PONG message"""
        message = Message()
        message.header.type = MessageType.PONG
        message.header.sender = sender
        message.header.reciever = receiver
        return message
    
    @staticmethod
    def destruct_message(message: Message) -> PMessage:
        """Convert Message to PMessage"""
        return PMessage(
            sender_name=message.header.sender,
            message=bytes(message.payload).decode('utf-8'),
            timestamp=message.header.timestamp,
            sequence_num=message.header.seq_num
        )
    
    @staticmethod
    def validate_message_sent(message: Message, self_proc_name: str, receiver_name: str) -> bool:
        """Validate outgoing message"""
        return (message.header.sender == self_proc_name and 
                message.header.reciever == receiver_name)
    
    @staticmethod
    def validate_message_received(message: Message, self_proc_name: str) -> bool:
        """Validate incoming message"""
        return message.header.reciever == self_proc_name
    
    @staticmethod
    def print_message(message: Message):
        """Print message details"""
        print(f"Type: {message.header.type.name}")
        print(f"From: {message.header.sender} -> To: {message.header.reciever}")
        print(f"Seq: {message.header.seq_num}, Time: {message.header.timestamp}")
        if message.payload:
            try:
                content = bytes(message.payload).decode('utf-8')
                print(f"Content: {content}")
            except:
                print(f"Binary content: {len(message.payload)} bytes")