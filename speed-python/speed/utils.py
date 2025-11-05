import os
import uuid
import time
import platform
from pathlib import Path
from typing import Optional

class Utils:
    @staticmethod
    def get_default_speed_dir() -> Path:
        """Get the default SPEED directory based on OS"""
        if platform.system() == "Windows":
            base_dir = Path(os.environ.get('LOCALAPPDATA', Path.home() / 'AppData' / 'Local'))
        else:
            base_dir = Path.home()
        
        return base_dir / ".speed"
    
    @staticmethod
    def get_current_timestamp() -> str:
        """Get current timestamp in ISO format"""
        return time.strftime("%Y%m%d_%H%M%S")
    
    @staticmethod
    def generate_uuid() -> str:
        """Generate a UUID string"""
        return str(uuid.uuid4())
    
    @staticmethod
    def get_timestamp_uuid() -> str:
        """Generate timestamp-based UUID"""
        return f"{Utils.get_current_timestamp()}_{Utils.generate_uuid()}"
    
    @staticmethod
    def get_process_id() -> int:
        """Get current process ID"""
        return os.getpid()
    
# In speed/utils.py, update the validate_key method:
    @staticmethod
    def validate_key(key: str) -> bool:
        try:
            import base64
            key_bytes = base64.b64decode(key)
            return len(key_bytes) == 32  # libsodium requires exactly 32 bytes
        except:
            return False    
    
    @staticmethod
    def file_exists(file_path: Path) -> bool:
        """Check if file exists"""
        return file_path.exists() and file_path.is_file()
    
    @staticmethod
    def directory_exists(dir_path: Path) -> bool:
        """Check if directory exists"""
        return dir_path.exists() and dir_path.is_dir()
    
    @staticmethod
    def create_default_dir(dir_path: Path) -> bool:
        """Create default directory"""
        try:
            dir_path.mkdir(parents=True, exist_ok=True)
            return True
        except Exception:
            return False
    
    @staticmethod
    def create_access_registry_dir(registry_path: Path) -> bool:
        """Create access registry directory"""
        return Utils.create_default_dir(registry_path)