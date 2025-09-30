import os
import sys
import ctypes
from ctypes import c_int, c_char_p, c_long, c_void_p, Structure


class CmdsetPreset(Structure):
    _fields_ = [
        ("name", ctypes.c_char * 50),
        ("command", ctypes.c_char * 500),
        ("active", c_int),
        ("encrypt", c_int),
        ("created_at", c_long),
        ("last_used", c_long),
        ("use_count", c_int),
    ]


class CmdsetManager(Structure):
    _fields_ = [
        ("presets", CmdsetPreset * 100),
        ("count", c_int),
    ]


class Preset:
    """Python wrapper for a command preset with convenient properties"""
    def __init__(self, preset_data: dict):
        self.name = preset_data["name"]
        self.command = preset_data["command"]
        self.encrypt = preset_data["encrypt"]
        self.created_at = preset_data["created_at"]
        self.last_used = preset_data["last_used"]
        self.use_count = preset_data["use_count"]
    
    @property
    def is_encrypted(self) -> bool:
        return self.encrypt
    
    @property
    def age_days(self) -> int:
        import time
        return int((time.time() - self.created_at) / 86400)
    
    @property
    def days_since_last_use(self) -> int:
        import time
        if self.last_used == 0:
            return -1 # Never used
        return int((time.time() - self.last_used) / 86400)
    
    def __repr__(self) -> str:
        return f"Preset(name='{self.name}', command='{self.command[:30]}...', encrypted={self.encrypt})"


class CmdSetError(Exception):
    pass


class PresetError(CmdSetError):
    pass


class FileError(CmdSetError):
    pass


def _find_library_path():
    lib_name_candidates = ["libcmdset.so"]
    here = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    search_paths = [
        here,
        os.getcwd(),
    ]
    for base in search_paths:
        for name in lib_name_candidates:
            candidate = os.path.join(base, name)
            if os.path.exists(candidate):
                return candidate
    # Fallback to loader search
    for name in lib_name_candidates:
        try:
            return ctypes.util.find_library(name) # type: ignore[attr-defined]
        except Exception:
            pass
    raise FileNotFoundError("Could not locate libcmdset shared library")


_lib_path = _find_library_path()
_lib = ctypes.CDLL(_lib_path)


# Function prototypes
_lib.cmdset_init.argtypes = [ctypes.POINTER(CmdsetManager)]
_lib.cmdset_init.restype = c_int

_lib.cmdset_add_preset.argtypes = [ctypes.POINTER(CmdsetManager), c_char_p, c_char_p, c_int]
_lib.cmdset_add_preset.restype = c_int

_lib.cmdset_remove_preset.argtypes = [ctypes.POINTER(CmdsetManager), c_char_p]
_lib.cmdset_remove_preset.restype = c_int

_lib.cmdset_execute_preset.argtypes = [ctypes.POINTER(CmdsetManager), c_char_p, c_char_p]
_lib.cmdset_execute_preset.restype = c_int

_lib.cmdset_get_error_message.argtypes = [c_int]
_lib.cmdset_get_error_message.restype = c_char_p

_lib.cmdset_cleanup.argtypes = [ctypes.POINTER(CmdsetManager)]
_lib.cmdset_cleanup.restype = None

_lib.cmdset_get_preset_count.argtypes = [ctypes.POINTER(CmdsetManager)]
_lib.cmdset_get_preset_count.restype = c_int

_lib.cmdset_get_preset_by_index.argtypes = [ctypes.POINTER(CmdsetManager), c_int, ctypes.POINTER(CmdsetPreset)]
_lib.cmdset_get_preset_by_index.restype = c_int

class CmdSet:
    def __init__(self):
        self._manager = CmdsetManager()
        rc = _lib.cmdset_init(ctypes.byref(self._manager))
        if rc != 0:
            msg = _lib.cmdset_get_error_message(rc)
            raise RuntimeError(msg.decode("utf-8") if msg else "cmdset_init failed")

    def add(self, name: str, command: str, encrypt: bool = False) -> None:
        rc = _lib.cmdset_add_preset(
            ctypes.byref(self._manager),
            name.encode("utf-8"),
            command.encode("utf-8"),
            1 if encrypt else 0,
        )
        if rc != 0:
            msg = _lib.cmdset_get_error_message(rc)
            raise RuntimeError(msg.decode("utf-8") if msg else "add_preset failed")

    def list(self):
        """List all presets as Preset objects"""
        count = _lib.cmdset_get_preset_count(ctypes.byref(self._manager))
        result = []
        for i in range(count):
            preset = CmdsetPreset()
            rc = _lib.cmdset_get_preset_by_index(ctypes.byref(self._manager), i, ctypes.byref(preset))
            if rc == 0:
                preset_data = {
                    "name": preset.name.decode("utf-8").rstrip("\x00"),
                    "command": preset.command.decode("utf-8").rstrip("\x00"),
                    "encrypt": bool(preset.encrypt),
                    "created_at": int(preset.created_at),
                    "last_used": int(preset.last_used),
                    "use_count": int(preset.use_count),
                }
                result.append(Preset(preset_data))
        return result

    def exec(self, name: str, additional_args: str = None) -> int:
        rc = _lib.cmdset_execute_preset(
            ctypes.byref(self._manager),
            name.encode("utf-8"),
            None if additional_args is None else additional_args.encode("utf-8"),
        )
        if rc < 0:
            msg = _lib.cmdset_get_error_message(rc)
            raise RuntimeError(msg.decode("utf-8") if msg else "execute_preset failed")
        return int(rc)

    def remove(self, name: str) -> None:
        rc = _lib.cmdset_remove_preset(ctypes.byref(self._manager), name.encode("utf-8"))
        if rc != 0:
            msg = _lib.cmdset_get_error_message(rc)
            raise RuntimeError(msg.decode("utf-8") if msg else "remove_preset failed")

    def close(self) -> None:
        _lib.cmdset_cleanup(ctypes.byref(self._manager))

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass

__all__ = ["CmdSet"]
