import pybind11
from setuptools import setup, Extension
import sys

# Phân biệt hệ điều hành để dùng đúng cờ (flags) biên dịch
if sys.platform == "win32":
    # Cờ cho MSVC (Windows)
    compile_args = ['/std:c++20', '/O2', '/arch:AVX2']
else:
    # Cờ cho GCC/Clang (Linux/Mac)
    compile_args = ['-std=c++20', '-O3', '-march=native']

ext_modules = [
    Extension(
        'tft_core_engine',
        ['tft_core_engine.cpp'],
        include_dirs=[pybind11.get_include()],
        language='c++',
        extra_compile_args=compile_args,
    ),
]

setup(
    name='tft_core_engine', 
    version='1.0.0', 
    ext_modules=ext_modules
)