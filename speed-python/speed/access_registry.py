import threading
from pathlib import Path
from typing import Set, List
from .utils import Utils

class AccessRegistry:
    def __init__(self, speed_dir: Path, proc_name: str):
        self.speed_dir = speed_dir
        self.proc_name = proc_name
        self.access_registry_dir = speed_dir / "access_registry"
        self.allowed_processes: Set[str] = set()
        self.global_registry: Set[str] = set()
        self.connected_list: Set[str] = set()
        self.mutex = threading.Lock()
        
        # Initialize directories
        Utils.create_default_dir(speed_dir)
        Utils.create_access_registry_dir(self.access_registry_dir)
        
        # Build initial registry
        self.incremental_build_global_registry()
    
    def add_process_to_list(self, proc_name: str):
        """Add process to allowed list"""
        with self.mutex:
            self.allowed_processes.add(proc_name)
    
    def incremental_build_global_registry(self):
        """Build global registry from access registry files"""
        with self.mutex:
            self.global_registry.clear()
            
            if not self.access_registry_dir.exists():
                return
            
            for file_path in self.access_registry_dir.glob("*.oregistry"):
                try:
                    with open(file_path, 'r') as f:
                        proc_name = f.read().strip()
                        self.global_registry.add(proc_name)
                except Exception:
                    continue
    
    def remove_access_file(self):
        """Remove this process's access file"""
        access_file = self.access_registry_dir / f"{self.proc_name}.oregistry"
        if access_file.exists():
            access_file.unlink()
    
    def sync_access_registry(self):
        """Sync access registry with filesystem"""
        # Create/update our access file
        access_file = self.access_registry_dir / f"{self.proc_name}.iregistry"
        try:
            with open(access_file, 'w') as f:
                f.write(self.proc_name)
            # Rename to indicate it's ready
            ready_file = self.access_registry_dir / f"{self.proc_name}.oregistry"
            access_file.rename(ready_file)
        except Exception as e:
            print(f"Error syncing access registry: {e}")
    
    def remove_process_from_global_registry(self, proc_name: str) -> bool:
        """Remove process from global registry"""
        with self.mutex:
            if proc_name in self.global_registry:
                self.global_registry.remove(proc_name)
                return True
            return False
    
    def check_access(self, proc_name: str) -> bool:
        """Check if process is in allowed list"""
        with self.mutex:
            return proc_name in self.allowed_processes
    
    def remove_process_from_access_list(self, proc_name: str) -> bool:
        """Remove process from access list"""
        with self.mutex:
            if proc_name in self.allowed_processes:
                self.allowed_processes.remove(proc_name)
                return True
            return False
    
    def remove_process_from_connected_list(self, proc_name: str) -> bool:
        """Remove process from connected list"""
        with self.mutex:
            if proc_name in self.connected_list:
                self.connected_list.remove(proc_name)
                return True
            return False
    
    def connect_to(self, proc_name: str) -> bool:
        """Add process to connected list"""
        with self.mutex:
            if (proc_name in self.allowed_processes and 
                proc_name in self.global_registry):
                self.connected_list.add(proc_name)
                return True
            return False
    
    def check_global_registry(self, proc_name: str) -> bool:
        """Check if process is in global registry"""
        with self.mutex:
            return proc_name in self.global_registry
    
    def check_connection(self, proc_name: str) -> bool:
        """Check if process is connected"""
        with self.mutex:
            return proc_name in self.connected_list
    
    def get_global_registry(self) -> Set[str]:
        """Get copy of global registry"""
        with self.mutex:
            return self.global_registry.copy()
    
    def get_access_list(self) -> Set[str]:
        """Get copy of access list"""
        with self.mutex:
            return self.allowed_processes.copy()
    
    def get_connected_list(self) -> Set[str]:
        """Get copy of connected list"""
        with self.mutex:
            return self.connected_list.copy()