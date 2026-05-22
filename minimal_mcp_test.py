#!/usr/bin/env python3
"""
Minimal MCP driver that prints the game PID, then drives menus.
If it hangs, the user can run lldb on the printed PID.
"""
import subprocess
import json
import sys
import os
import time

SC_RETURN = 0x1c
SC_ESCAPE = 0x01
SC_UP = 0x48
SC_DOWN = 0x50

exe = os.path.join(os.path.dirname(__file__), "zig-out", "bin", "wolf3d")
proc = subprocess.Popen(
    [exe],
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    text=True,
    bufsize=1,
)
print(f"GAME_PID={proc.pid}", flush=True)

req_id = 0
def send(msg):
    global req_id
    req_id += 1
    msg["id"] = req_id
    line = json.dumps(msg) + "\n"
    proc.stdin.write(line)
    proc.stdin.flush()
    return json.loads(proc.stdout.readline().strip())

# Initialize
send({"jsonrpc": "2.0", "method": "initialize"})
send({"jsonrpc": "2.0", "method": "notifications/initialized"})

# Poll state
def get_state():
    r = send({"jsonrpc": "2.0", "method": "get_state"})
    return json.loads(r["result"])

def press(sc):
    send({"jsonrpc": "2.0", "method": "send_key", "params": {"scancode": sc}})

# Break title screen
for i in range(20):
    press(SC_RETURN)
    time.sleep(0.4)
    s = get_state()
    print(f"state: menu={s.get('menu')} options={s.get('options')}", flush=True)
    if s.get("menu") and s.get("menu") != "null":
        break

# Enter New Game
press(SC_RETURN)
time.sleep(2)
s = get_state()
print(f"after_newgame: menu={s.get('menu')} options={s.get('options')}", flush=True)

# If we're still in main_menu, try again
if s.get("menu") == "main_menu":
    press(SC_RETURN)
    time.sleep(2)
    s = get_state()
    print(f"after_newgame2: menu={s.get('menu')} options={s.get('options')}", flush=True)

print("DONE", flush=True)
proc.terminate()
proc.wait(timeout=5)
