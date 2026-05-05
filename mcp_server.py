#!/usr/bin/env python3
"""
MCP server for Wolf3D automation and screenshot capture.
Communicates via stdio using JSON-RPC 2.0.

Tools:
- take_screenshot: Capture the Wolf3D window
- launch_game: Start wolf3d.exe
- send_key: Send a virtual keypress to the game window
- navigate_to_gameplay: Automate menu navigation (New Game -> Episode 1 -> Difficulty)
- is_process_alive: Check if the game process is still running
"""

import sys
import json
import base64
import subprocess
import os
import time

game_process = None

def send_message(msg):
    """Send a JSON-RPC message to stdout."""
    data = json.dumps(msg)
    sys.stdout.write(data + "\n")
    sys.stdout.flush()

def read_message():
    """Read a JSON-RPC message from stdin."""
    line = sys.stdin.readline()
    if not line:
        return None
    return json.loads(line.strip())

def find_window():
    """Find the Wolf3D window HWND."""
    try:
        import ctypes
        import ctypes.wintypes
        user32 = ctypes.windll.user32
        enum_windows = user32.EnumWindows
        get_window_text = user32.GetWindowTextW
        get_window_text_length = user32.GetWindowTextLengthW
        is_window_visible = user32.IsWindowVisible

        target_hwnd = None

        def enum_callback(hwnd, extra):
            nonlocal target_hwnd
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

        EnumWindowsProc = ctypes.WINFUNCTYPE(
            ctypes.c_bool, ctypes.c_void_p, ctypes.c_longlong
        )
        enum_windows(EnumWindowsProc(enum_callback), 0)
        return target_hwnd
    except Exception:
        return None

def take_screenshot():
    """Take a screenshot of the Wolf3D window using PIL."""
    try:
        from PIL import ImageGrab
        import ctypes
        import ctypes.wintypes

        target_hwnd = find_window()
        if target_hwnd is None:
            return {"error": "Wolf3D window not found"}

        user32 = ctypes.windll.user32
        rect = ctypes.wintypes.RECT()
        user32.GetWindowRect(target_hwnd, ctypes.byref(rect))
        bbox = (rect.left, rect.top, rect.right, rect.bottom)

        screenshot = ImageGrab.grab(bbox)
        temp_path = os.path.join(os.environ.get("TEMP", "/tmp"), "wolf3d_screenshot.png")
        screenshot.save(temp_path, "PNG")

        with open(temp_path, "rb") as f:
            img_data = base64.b64encode(f.read()).decode("utf-8")

        return {
            "path": temp_path,
            "width": screenshot.width,
            "height": screenshot.height,
            "base64": img_data
        }
    except Exception as e:
        return {"error": str(e)}

def launch_game():
    """Launch the Wolf3D executable."""
    global game_process
    try:
        if game_process is not None and game_process.poll() is None:
            return {"status": "already_running", "pid": game_process.pid}

        exe_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "zig-out", "bin", "wolf3d.exe")
        if not os.path.exists(exe_path):
            return {"error": f"Executable not found: {exe_path}"}

        game_process = subprocess.Popen([exe_path], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return {"status": "launched", "pid": game_process.pid}
    except Exception as e:
        return {"error": str(e)}

def send_key(vk_code):
    """Send a virtual keypress to the Wolf3D window."""
    try:
        import ctypes
        user32 = ctypes.windll.user32

        target_hwnd = find_window()
        if target_hwnd is None:
            return {"error": "Wolf3D window not found"}

        user32.SetForegroundWindow(target_hwnd)
        time.sleep(0.2)

        user32.PostMessageW(target_hwnd, 0x100, vk_code, 0)  # WM_KEYDOWN
        time.sleep(0.15)
        user32.PostMessageW(target_hwnd, 0x101, vk_code, 0)  # WM_KEYUP
        time.sleep(0.15)

        return {"status": "sent", "vk": vk_code}
    except Exception as e:
        return {"error": str(e)}

def navigate_to_gameplay():
    """Automate menu navigation into gameplay.

    Menu flow:
      Title screen / attract mode -> Enter
      Main Menu (New Game selected) -> Enter
      Episode select (Episode 1 selected) -> Enter
      Difficulty select (1st difficulty selected) -> Enter
      -> Gameplay / level loading screen
    """
    global game_process
    try:
        if game_process is None or game_process.poll() is not None:
            launch_game()
            time.sleep(4)          # let title screen fully appear
        else:
            # game already running – give it a moment to come to foreground
            time.sleep(1)

        VK_RETURN = 0x0D

        # make sure the window exists
        for _ in range(30):
            if find_window() is not None:
                break
            time.sleep(0.5)
        else:
            return {"error": "Window did not appear"}

        # --- Step 1: break out of title-screen / attract-mode ---
        send_key(VK_RETURN)
        time.sleep(3)              # fade-out + main menu fade-in

        # --- Step 2: Main Menu -> New Game ---
        send_key(VK_RETURN)
        time.sleep(3)              # fade-out + episode-select fade-in

        # --- Step 3: Episode select -> Episode 1 ---
        send_key(VK_RETURN)
        time.sleep(3)              # fade-out + difficulty-select fade-in

        # --- Step 4: Difficulty select -> first difficulty ---
        send_key(VK_RETURN)
        time.sleep(5)              # level loading + gameplay start

        if game_process.poll() is not None:
            return {"error": f"Process crashed with code {game_process.returncode}"}

        return {"status": "in_gameplay", "pid": game_process.pid}
    except Exception as e:
        return {"error": str(e)}

def is_process_alive():
    """Check if the game process is still alive."""
    global game_process
    if game_process is None:
        return {"alive": False, "status": "never_started"}
    poll = game_process.poll()
    if poll is None:
        return {"alive": True, "pid": game_process.pid}
    return {"alive": False, "returncode": poll}

def main():
    while True:
        msg = read_message()
        if msg is None:
            break

        method = msg.get("method", "")
        msg_id = msg.get("id")

        if method == "initialize":
            send_message({
                "jsonrpc": "2.0",
                "id": msg_id,
                "result": {
                    "protocolVersion": "2024-11-05",
                    "capabilities": {},
                    "serverInfo": {
                        "name": "wolf3d-mcp",
                        "version": "1.1.0"
                    }
                }
            })

        elif method == "tools/list":
            send_message({
                "jsonrpc": "2.0",
                "id": msg_id,
                "result": {
                    "tools": [
                        {
                            "name": "take_screenshot",
                            "description": "Take a screenshot of the Wolf3D game window",
                            "inputSchema": {
                                "type": "object",
                                "properties": {}
                            }
                        },
                        {
                            "name": "launch_game",
                            "description": "Launch the Wolf3D executable",
                            "inputSchema": {
                                "type": "object",
                                "properties": {}
                            }
                        },
                        {
                            "name": "send_key",
                            "description": "Send a virtual keypress to the game window (vk_code: int)",
                            "inputSchema": {
                                "type": "object",
                                "properties": {
                                    "vk_code": {
                                        "type": "integer",
                                        "description": "Virtual key code (e.g. 0x0D for Enter, 0x28 for Down)"
                                    }
                                },
                                "required": ["vk_code"]
                            }
                        },
                        {
                            "name": "navigate_to_gameplay",
                            "description": "Automate menu navigation into gameplay (New Game -> Episode 1 -> Difficulty)",
                            "inputSchema": {
                                "type": "object",
                                "properties": {}
                            }
                        },
                        {
                            "name": "is_process_alive",
                            "description": "Check if the Wolf3D process is still running",
                            "inputSchema": {
                                "type": "object",
                                "properties": {}
                            }
                        }
                    ]
                }
            })

        elif method == "tools/call":
            params = msg.get("params", {})
            tool_name = params.get("name", "")
            tool_args = params.get("arguments", {})

            if tool_name == "take_screenshot":
                result = take_screenshot()
            elif tool_name == "launch_game":
                result = launch_game()
            elif tool_name == "send_key":
                result = send_key(tool_args.get("vk_code", 0))
            elif tool_name == "navigate_to_gameplay":
                result = navigate_to_gameplay()
            elif tool_name == "is_process_alive":
                result = is_process_alive()
            else:
                send_message({
                    "jsonrpc": "2.0",
                    "id": msg_id,
                    "error": {"code": -32601, "message": f"Unknown tool: {tool_name}"}
                })
                continue

            if "base64" in result:
                send_message({
                    "jsonrpc": "2.0",
                    "id": msg_id,
                    "result": {
                        "content": [
                            {
                                "type": "image",
                                "data": result["base64"],
                                "mimeType": "image/png"
                            }
                        ]
                    }
                })
            else:
                send_message({
                    "jsonrpc": "2.0",
                    "id": msg_id,
                    "result": {
                        "content": [
                            {
                                "type": "text",
                                "text": json.dumps(result)
                            }
                        ]
                    }
                })

        elif method == "notifications/initialized":
            pass

        else:
            send_message({
                "jsonrpc": "2.0",
                "id": msg_id,
                "error": {"code": -32601, "message": f"Unknown method: {method}"}
            })

if __name__ == "__main__":
    main()
