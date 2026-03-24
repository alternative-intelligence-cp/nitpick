"""Call Aria shared library from Python via ctypes.

Build the shared library first:
    ariac mathlib.aria --shared -o libmathlib.so

Then run:
    python3 use_from_python.py
"""
import ctypes
import os

# Load Aria shared library
lib_path = os.path.join(os.path.dirname(__file__), "libmathlib.so")
lib = ctypes.CDLL(lib_path)

# Declare function signatures
lib.add.restype = ctypes.c_int
lib.add.argtypes = [ctypes.c_int, ctypes.c_int]

lib.subtract.restype = ctypes.c_int
lib.subtract.argtypes = [ctypes.c_int, ctypes.c_int]

lib.multiply.restype = ctypes.c_int
lib.multiply.argtypes = [ctypes.c_int, ctypes.c_int]

lib.square.restype = ctypes.c_int
lib.square.argtypes = [ctypes.c_int]

lib.is_even.restype = ctypes.c_int
lib.is_even.argtypes = [ctypes.c_int]

# Call Aria functions
print("=== Calling Aria from Python ===")
print(f"add(10, 20) = {lib.add(10, 20)}")
print(f"subtract(100, 42) = {lib.subtract(100, 42)}")
print(f"multiply(7, 8) = {lib.multiply(7, 8)}")
print(f"square(12) = {lib.square(12)}")
print(f"is_even(4) = {bool(lib.is_even(4))}")
print(f"is_even(7) = {bool(lib.is_even(7))}")
