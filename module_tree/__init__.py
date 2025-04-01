import os, sys

extra_dll_dir = os.path.join(os.path.dirname(__file__), '.lib')
# On windows, rpath doesn't exist so we need to dynamically specify the SDL libs
print(f"extra dlls in {extra_dll_dir}")
print(os.path.isdir(extra_dll_dir))
print(sys.platform)
print(os.path.isdir(extra_dll_dir))
print(os.listdir(os.path.dirname(__file__)))
if os.path.isdir(extra_dll_dir):
    print(os.listdir(extra_dll_dir))
if sys.platform == "win32" and os.path.isdir(extra_dll_dir):
    print("Adding dll dir to os libraries")
    os.add_dll_directory(extra_dll_dir)

from . import _city_of_gold # exports everything to this namespace
del os, sys, _city_of_gold

