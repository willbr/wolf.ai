#!/usr/bin/env python3
"""
Quick script to launch Wolf3D and navigate to New Game to reproduce the crash.
"""

import subprocess
import time
import ctypes
from ctypes import wintypes
import pathlib

CRASH_LOG = pathlib.Path("crash_log.txt")
if CRASH_LOG.exists():
    CRASH_LOG.unlink()

# Launch the game with stderr to file
stderr_file = open("repro_stderr.txt", "w")
proc = subprocess.Popen([r"zig-out\bin\wolf3d.exe"], cwd=r"C:\Users\wjbr\src\wolf.ai", stderr=stderr_file, stdout=subprocess.DEVNULL)
print(f"Launched game, PID={proc.pid}")

# Wait for window to appear
time.sleep(3)

# Find window
user32 = ctypes.windll.user32
enum_windows = user32.EnumWindows
get_window_text = user32.GetWindowTextW
get_window_text_length = user32.GetWindowTextLengthW
is_window_visible = user32.IsWindowVisible

target_hwnd = None

def enum_callback(hwnd, extra):
    global target_hwnd
    if not is_window_visible(hwnd):
        return True
    length = get_window_text_length(hwnd)
    if length > 0:
        buffer = ctypes.create_unicode_buffer(length + 1)
        get_window_text(hwnd, buffer, length + 1)
        title = buffer.value
        if "Wolfenstein" in title or "wolf3d" in title.lower():
            target_hwnd = hwnd
            return False
    return True

EnumWindowsProc = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int))
enum_windows(EnumWindowsProc(enum_callback), 0)

if target_hwnd is None:
    print("Window not found!")
    proc.kill()
    exit(1)

print(f"Found window HWND={target_hwnd}")

# Bring to front
user32.SetForegroundWindow(target_hwnd)
time.sleep(0.5)

# Send keys: Down arrow to get to New Game (it's usually first item), then Enter
# Actually New Game is usually the first item, so just press Enter
# But let's send down/up to make sure we're on New Game, then Enter

VK_RETURN = 0x0D
VK_DOWN = 0x28

def send_key(hwnd, vk):
    user32.PostMessageW(hwnd, 0x100, vk, 0)  # WM_KEYDOWN
    time.sleep(0.1)
    user32.PostMessageW(hwnd, 0x101, vk, 0)  # WM_KEYUP
    time.sleep(0.1)

# The menu starts with "New Game" selected, so just press Enter
print("Sending Enter to select New Game...")
send_key(target_hwnd, VK_RETURN)

time.sleep(1)

# If episode select appears, select Episode 1 (Enter again)
print("Sending Enter to select Episode 1...")
send_key(target_hwnd, VK_RETURN)

time.sleep(1)

# If difficulty select appears, select first difficulty (Enter again)
print("Sending Enter to select difficulty...")
send_key(target_hwnd, VK_RETURN)

time.sleep(3)

print("Checking if process is still alive...")
if proc.poll() is None:
    print("Process still running, killing it...")
    proc.kill()
    proc.wait()
else:
    print(f"Process exited with code {proc.returncode}")
    if proc.returncode != 0:
        print("FAILED: Process crashed or exited with error.")
        exit(1)

if CRASH_LOG.exists():
    print("FAILED: crash_log.txt was written during test.")
    exit(1)

stderr_file.close()

try:
    with open("repro_stderr.txt", "r") as f:
        contents = f.read()
        if contents:
            print("STDERR:")
            print(contents)
        else:
            print("(no stderr output)")
except Exception as e:
    print(f"Error reading stderr file: {e}")

print("Done.")
