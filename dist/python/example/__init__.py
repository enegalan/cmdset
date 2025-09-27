"""
CmdSet Python Extension

A Python extension for the CmdSet command preset manager.
Allows you to save, manage, and execute command presets directly from Python.
"""

from .cmdset import CmdSet, Preset, CmdSetError
from .cli import main

__version__ = '1.0.0'
__author__ = 'Eneko Galan'
__email__ = 'enekogalanelorza@gmail.com'

__all__ = ['CmdSet', 'Preset', 'CmdSetError', 'main']
