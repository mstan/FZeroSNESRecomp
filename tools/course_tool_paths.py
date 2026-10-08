"""Locate course-converter resources in a checkout or standalone helper."""
from pathlib import Path
import sys

ROOT = Path(getattr(sys, '_MEIPASS', Path(__file__).resolve().parents[1]))


def native_tool(name):
    folder = Path(sys.executable).resolve().parent if getattr(sys, 'frozen', False) else ROOT/'build'
    return folder/name
