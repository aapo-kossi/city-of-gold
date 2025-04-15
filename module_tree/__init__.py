import os, sys

# On windows, rpath doesn't exist so we need to dynamically specify the SDL libs
extra_dll_dir = os.path.join(os.path.dirname(__file__), '.lib')
if sys.platform == "win32" and os.path.isdir(extra_dll_dir):
    os.add_dll_directory(extra_dll_dir)

from . import _city_of_gold # exports everything to this namespace
del os, sys, _city_of_gold

