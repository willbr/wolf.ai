#!/usr/bin/env python3
"""
Launch Wolf3D with procdump to capture crash dump.
"""

import subprocess
import time
import ctypes
from ctypes import wintypes

# Launch procdump to monitor for crashes
procdump_proc = subprocess.Popen([
    "procdump", "-ma", "-x", ".", "zig-out\\bin\\wolf3d.exe"
], cwd=r"C:\Users\wjbr\src\wolf.ai", stdout=subprocess.PIPE, stderr=subprocess.PIPE)

print(f"Launched procdump, PID={procdump_proc.pid}")

# Wait a bit for the game to start
time.sleep(2)

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
    procdump_proc.kill()
    exit(1)

print(f"Found window HWND={target_hwnd}")

# Bring to front
user32.SetForegroundWindow(target_hwnd)
time.sleep(0.5)

VK_RETURN = 0x0D

def send_key(hwnd, vk):
    user32.PostMessageW(hwnd, 0x100, vk, 0)
    time.sleep(0.1)
    user32.PostMessageW(hwnd, 0x101, vk, 0)
    time.sleep(0.1)

print("Sending Enter to select New Game...")
send_key(target_hwnd, VK_RETURN)
time.sleep(1)
print("Sending Enter to select Episode 1...")
send_key(target_hwnd, VK_RETURN)
time.sleep(1)
print("Sending Enter to select difficulty...")
send_key(target_hwnd, VK_RETURN)
time.sleep(3)

print("Waiting for procdump to finish...")
try:
    stdout, stderr = procdump_proc.communicate(timeout=10)
    print("STDOUT:", stdout.decode('utf-8', errors='replace') if stdout else "(none)")
    print("STDERR:", stderr.decode('utf-8', errors='replace') if stderr else "(none)")
except subprocess.TimeoutExpired:
    print("Timeout, killing procdump")
    procdump_proc.kill()

print("Done.")
