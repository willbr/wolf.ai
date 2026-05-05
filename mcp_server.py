#!/usr/bin/env python3
"""
Simple MCP server for Wolf3D screenshot capture.
Communicates via stdio using JSON-RPC 2.0.
"""

import sys
import json
import base64
import subprocess
import os

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

def take_screenshot():
    """Take a screenshot of the Wolf3D window using PIL."""
    try:
        from PIL import ImageGrab
        import ctypes

        # Find the Wolf3D window
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

        EnumWindowsProc = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int))
        enum_windows(EnumWindowsProc(enum_callback), 0)

        if target_hwnd is None:
            return {"error": "Wolf3D window not found"}

        # Get window rect
        rect = ctypes.wintypes.RECT()
        user32.GetWindowRect(target_hwnd, ctypes.byref(rect))
        bbox = (rect.left, rect.top, rect.right, rect.bottom)

        # Capture screenshot
        screenshot = ImageGrab.grab(bbox)

        # Save to temp file
        temp_path = os.path.join(os.environ.get("TEMP", "/tmp"), "wolf3d_screenshot.png")
        screenshot.save(temp_path, "PNG")

        # Read and encode as base64
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
                        "version": "1.0.0"
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
                        }
                    ]
                }
            })

        elif method == "tools/call":
            params = msg.get("params", {})
            tool_name = params.get("name", "")

            if tool_name == "take_screenshot":
                result = take_screenshot()
                send_message({
                    "jsonrpc": "2.0",
                    "id": msg_id,
                    "result": {
                        "content": [
                            {
                                "type": "image",
                                "data": result.get("base64", ""),
                                "mimeType": "image/png"
                            }
                        ] if "base64" in result else [
                            {
                                "type": "text",
                                "text": json.dumps(result)
                            }
                        ]
                    }
                })
            else:
                send_message({
                    "jsonrpc": "2.0",
                    "id": msg_id,
                    "error": {"code": -32601, "message": f"Unknown tool: {tool_name}"}
                })

        elif method == "notifications/initialized":
            # No response needed for notifications
            pass

        else:
            send_message({
                "jsonrpc": "2.0",
                "id": msg_id,
                "error": {"code": -32601, "message": f"Unknown method: {method}"}
            })

if __name__ == "__main__":
    main()
