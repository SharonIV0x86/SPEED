from pathlib import Path

class KeyManager:
    @staticmethod
    def get_key_from_config_file(key_path: Path) -> str:
        """Read encryption key from config file"""
        try:
            with open(key_path, 'r') as f:
                key = f.read().strip()
            return key
        except Exception as e:
            print(f"Error reading key file: {e}")
            return ""