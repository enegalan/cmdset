"""
CmdSet Python Wrapper

High-level Python interface for the CmdSet C extension.
"""

import os
import time
from typing import List, Dict, Optional, Tuple, Union
from dataclasses import dataclass
from enum import IntEnum

# Import the compiled C extension
try:
    import cmdset as _cmdset_lib
except ImportError:
    raise ImportError(
        "CmdSet C extension not found. Please install cmdset first:\n"
        "pip install -e ."
    )

class CmdSetError(Exception):
    """Base exception for CmdSet errors"""
    pass

class PresetError(CmdSetError):
    """Preset-related error"""
    pass

class EncryptionError(CmdSetError):
    """Encryption error"""
    pass

class FileError(CmdSetError):
    """File error"""
    pass

@dataclass
class Preset:
    """Represents a command preset"""
    name: str
    command: str
    encrypt: bool = False
    created_at: int = 0
    last_used: int = 0
    use_count: int = 0
    
    def __post_init__(self):
        if self.created_at == 0:
            self.created_at = int(time.time())
    
    @property
    def is_encrypted(self) -> bool:
        """Indicates if the command is encrypted"""
        return self.encrypt
    
    @property
    def age_days(self) -> int:
        """Days since it was created"""
        return int((time.time() - self.created_at) / 86400)
    
    @property
    def days_since_last_use(self) -> int:
        """Days since last use"""
        if self.last_used == 0:
            return -1
        return int((time.time() - self.last_used) / 86400)

class CmdSet:
    def __init__(self, working_dir: Optional[str] = None):
        self.working_dir = working_dir or os.getcwd()
        self._manager = None
        self._initialized = False
        # Change to working directory
        original_dir = os.getcwd()
        try:
            os.chdir(self.working_dir)
            self._manager = _cmdset_lib.init()
            if self._manager is None:
                raise CmdSetError("Failed to initialize CmdSet")
            self._initialized = True
        finally:
            os.chdir(original_dir)
    
    def __enter__(self):
        """Context manager entry"""
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit"""
        self.close()
    
    def close(self):
        """Close and clean resources"""
        if self._initialized and self._manager is not None:
            _cmdset_lib.cleanup(self._manager)
            self._initialized = False
    
    def add(self, name: str, command: str, encrypt: bool = False) -> bool:
        """
        Add a new preset
        
        Args:
            name: Name of the preset
            command: Command to execute
            encrypt: If encrypt the command
            
        Returns:
            True if added successfully
            
        Raises:
            PresetError: If the preset already exists or there is an error
        """
        if not self._initialized:
            raise CmdSetError("CmdSet not initialized")
        
        original_dir = os.getcwd()
        try:
            os.chdir(self.working_dir)
            result = _cmdset_lib.add_preset(self._manager, name, command, encrypt)
            if result is not None and result != 0:
                if result == -4:  # CMDSET_ERROR_EXISTS
                    raise PresetError(f"Preset '{name}' already exists")
                else:
                    raise PresetError(f"Failed to add preset: Error code {result}")
            return True
        finally:
            os.chdir(original_dir)
    
    def remove(self, name: str) -> bool:
        """
        Remove a preset
        
        Args:
            name: Name of the preset to remove
            
        Returns:
            True if removed successfully
            
        Raises:
            PresetError: If the preset does not exist
        """
        if not self._initialized:
            raise CmdSetError("CmdSet not initialized")
        
        original_dir = os.getcwd()
        try:
            os.chdir(self.working_dir)
            # Remove preset by finding and deleting it from the list
            presets = _cmdset_lib.list_presets(self._manager)
            for i, preset in enumerate(presets):
                if preset['name'] == name:
                    # For now, we'll just raise an error since we don't have a remove function
                    raise PresetError("Remove functionality not implemented in C extension")
            return True
        finally:
            os.chdir(original_dir)
    
    def execute(self, name: str, *args) -> int:
        """
        Execute a preset
        
        Args:
            name: Name of the preset
            *args: Additional arguments for the command
            
        Returns:
            Exit code of the command
            
        Raises:
            PresetError: If the preset does not exist
        """
        if not self._initialized:
            raise CmdSetError("CmdSet not initialized")
        
        original_dir = os.getcwd()
        try:
            os.chdir(self.working_dir)
            
            # Combine additional arguments
            additional_args = ' '.join(str(arg) for arg in args) if args else None
            
            result = _cmdset_lib.execute_preset(self._manager, name, additional_args)
            if result < 0:  # CmdSet error
                if result == -3:  # CMDSET_ERROR_NOT_FOUND
                    raise PresetError(f"Preset '{name}' not found")
                else:
                    raise PresetError(f"Failed to execute preset: Error code {result}")
            
            return result
        finally:
            os.chdir(original_dir)
    
    def list_presets(self) -> List[Preset]:
        """
        List all presets
        
        Returns:
            List of Preset objects
        """
        if not self._initialized:
            raise CmdSetError("CmdSet not initialized")
        
        # Use the list_presets function from the C extension
        preset_list = _cmdset_lib.list_presets(self._manager)
        presets = []
        
        for preset_dict in preset_list:
            preset = Preset(
                name=preset_dict['name'],
                command=preset_dict['command'],
                encrypt=bool(preset_dict['encrypt']),
                created_at=preset_dict['created_at'],
                last_used=preset_dict['last_used'],
                use_count=preset_dict['use_count']
            )
            presets.append(preset)
        
        return presets
    
    def find(self, name: str) -> Optional[Preset]:
        """
        Find a preset by name
        
        Args:
            name: Name of the preset
            
        Returns:
            Preset object if found, None otherwise
        """
        if not self._initialized:
            raise CmdSetError("CmdSet not initialized")
        
        # Use list_presets and find the preset by name
        presets = _cmdset_lib.list_presets(self._manager)
        for preset_dict in presets:
            if preset_dict['name'] == name:
                return Preset(
                    name=preset_dict['name'],
                    command=preset_dict['command'],
                    encrypt=bool(preset_dict['encrypt']),
                    created_at=preset_dict['created_at'],
                    last_used=preset_dict['last_used'],
                    use_count=preset_dict['use_count']
                )
        
        return None
    
    def save(self) -> bool:
        """
        Save presets to disk
        
        Returns:
            True if saved successfully
        """
        if not self._initialized:
            raise CmdSetError("CmdSet not initialized")
        
        original_dir = os.getcwd()
        try:
            os.chdir(self.working_dir)
            # Save functionality not implemented in C extension
            raise FileError("Save functionality not implemented in C extension")
            return True
        finally:
            os.chdir(original_dir)
    
    def load(self) -> bool:
        """
        Load presets from disk
        
        Returns:
            True if loaded successfully
        """
        if not self._initialized:
            raise CmdSetError("CmdSet not initialized")
        
        original_dir = os.getcwd()
        try:
            os.chdir(self.working_dir)
            # Load functionality not implemented in C extension
            raise FileError("Load functionality not implemented in C extension")
            return True
        finally:
            os.chdir(original_dir)
    
    def export(self, filename: str) -> bool:
        """
        Export presets to JSON file
        
        Args:
            filename: Name of the export file
            
        Returns:
            True if exported successfully
        """
        if not self._initialized:
            raise CmdSetError("CmdSet not initialized")
        
        original_dir = os.getcwd()
        try:
            os.chdir(self.working_dir)
            # Export functionality not implemented in C extension
            raise FileError("Export functionality not implemented in C extension")
            return True
        finally:
            os.chdir(original_dir)
    
    def import_presets(self, filename: str) -> bool:
        """
            Import presets from JSON file
        
        Args:
            filename: Name of the import file
            
        Returns:
            True if imported successfully
        """
        if not self._initialized:
            raise CmdSetError("CmdSet not initialized")
        
        original_dir = os.getcwd()
        try:
            os.chdir(self.working_dir)
            # Import functionality not implemented in C extension
            raise FileError("Import functionality not implemented in C extension")
            return True
        finally:
            os.chdir(original_dir)
    
    def encrypt_command(self, plaintext: str) -> str:
        """
        Encrypt a command
        
        Args:
            plaintext: Plaintext command
            
        Returns:
            Encrypted command
            
        Raises:
            EncryptionError: If encryption fails
        """
        if not self._initialized:
            raise CmdSetError("CmdSet not initialized")
        
        # Encryption functionality not implemented in C extension
        raise EncryptionError("Encryption functionality not implemented in C extension")
    
    def decrypt_command(self, encrypted: str) -> str:
        """
        Decrypt a command
        
        Args:
            encrypted: Encrypted command
            
        Returns:
            Plaintext command
            
        Raises:
            EncryptionError: If decryption fails
        """
        if not self._initialized:
            raise CmdSetError("CmdSet not initialized")
        
        # Decryption functionality not implemented in C extension
        raise EncryptionError("Decryption functionality not implemented in C extension")
    
    def __len__(self) -> int:
        """Number of active presets"""
        if not self._initialized:
            return 0
        return len(_cmdset_lib.list_presets(self._manager))
    
    def __contains__(self, name: str) -> bool:
        """Check if a preset exists"""
        return self.find(name) is not None
    
    def __iter__(self):
        """Iterate over all presets"""
        return iter(self.list_presets())
    
    def __repr__(self) -> str:
        """Object representation"""
        count = len(self) if self._initialized else 0
        return f"CmdSet(working_dir='{self.working_dir}', presets={count})"
