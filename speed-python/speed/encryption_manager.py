# speed/encryption_manager.py
import nacl.secret
import nacl.utils
import base64
from typing import List
from .message_utils import Message

class EncryptionManager:
    @staticmethod
    def encrypt(message: Message, key: str) -> Message:
        """Encrypt message payload - accepts base64 string key"""
        try:
            # Decode base64 key to raw bytes
            key_bytes = base64.b64decode(key)
            
            # Generate random nonce
            nonce = nacl.utils.random(nacl.secret.SecretBox.NONCE_SIZE)
            message.header.nonce = list(nonce)
            
            # Create secret box
            box = nacl.secret.SecretBox(key_bytes)
            
            # Encrypt payload
            encrypted = box.encrypt(bytes(message.payload), nonce)
            
            # Update message with encrypted payload (excluding nonce)
            message.payload = list(encrypted.ciphertext)
            return message
            
        except Exception as e:
            print(f"Encryption error: {e}")
            return message
    
    @staticmethod
    def decrypt(message: Message, key: str) -> Message:
        """Decrypt message payload - accepts base64 string key"""
        try:
            # Decode base64 key to raw bytes
            key_bytes = base64.b64decode(key)
            
            # Create secret box
            box = nacl.secret.SecretBox(key_bytes)
            
            # Decrypt payload
            nonce = bytes(message.header.nonce)
            decrypted = box.decrypt(bytes(message.payload), nonce)
            
            # Update message with decrypted payload
            message.payload = list(decrypted)
            return message
            
        except Exception as e:
            print(f"Decryption error: {e}")
            return message