#!/usr/bin/env python3
"""
Systematically explore all Wolf3D menus via the built-in MCP server.
"""

import subprocess
import json
import sys
import os
import time

# Scancodes from id_in.h
SC_RETURN = 0x1c
SC_ESCAPE = 0x01
SC_UP = 0x48
SC_DOWN = 0x50
SC_LEFT = 0x4b
SC_RIGHT = 0x4d


class MCPClient:
    """JSON-RPC 2.0 client over stdio pipes."""

    def __init__(self, proc):
        self.proc = proc
        self._request_id = 0
        self._initialize()

    def _send(self, msg):
        line = json.dumps(msg) + "\n"
        self.proc.stdin.write(line)
        self.proc.stdin.flush()

    def _recv(self, timeout_sec=30):
        import threading
        import queue

        q = queue.Queue()

        def reader():
            try:
                line = self.proc.stdout.readline()
                q.put(line)
            except Exception as e:
                q.put(e)

        t = threading.Thread(target=reader, daemon=True)
        t.start()
        try:
            item = q.get(timeout=timeout_sec)
        except queue.Empty:
            raise TimeoutError(f"No response within {timeout_sec}s")

        if isinstance(item, Exception):
            raise item
        if not item:
            raise ConnectionError("Game closed stdout")
        return json.loads(item.strip())

    def _request(self, method, params=None, timeout_sec=30):
        self._request_id += 1
        req = {"jsonrpc": "2.0", "id": self._request_id, "method": method}
        if params is not None:
            req["params"] = params
        self._send(req)
        return self._recv(timeout_sec)

    def _initialize(self):
        resp = self._request("initialize")
        assert "result" in resp, f"initialize failed: {resp}"
        self._send({"jsonrpc": "2.0", "method": "notifications/initialized"})

    def call_method(self, method, params=None, timeout_sec=60):
        resp = self._request(method, params, timeout_sec=timeout_sec)
        if "error" in resp:
            raise RuntimeError(f"Method {method} error: {resp['error']}")
        result = resp["result"]
        if isinstance(result, dict) and "content" in result:
            return result
        text = json.dumps(result) if not isinstance(result, str) else result
        return {"content": [{"type": "text", "text": text}]}


def poll_state(client, timeout_sec=10):
    result = client.call_method("get_state", timeout_sec=timeout_sec)
    content = result["content"][0]
    return json.loads(content["text"])


def send_key(client, scancode):
    client.call_method("send_key", {"scancode": scancode}, timeout_sec=5)


def wait_for_stable_menu(client, expected_menu=None, timeout_sec=15, poll_interval=0.3):
    """Poll until menu stabilizes (doesn't change between polls)."""
    deadline = time.time() + timeout_sec
    last_menu = None
    stable_count = 0
    while time.time() < deadline:
        state = poll_state(client, timeout_sec=2)
        menu = state.get("menu")
        if menu == last_menu:
            stable_count += 1
            if stable_count >= 2:
                if expected_menu is None or menu == expected_menu:
                    return state
        else:
            stable_count = 0
            last_menu = menu
        time.sleep(poll_interval)
    raise TimeoutError(f"Menu did not stabilize")


def explore_menus():
    exe_path = os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "zig-out", "bin", "wolf3d",
    )
    if not os.path.exists(exe_path):
        raise FileNotFoundError(f"Executable not found: {exe_path}")

    print("[explore] Launching game with MCP...")
    proc = subprocess.Popen(
        [exe_path],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1,
    )

    client = None
    discovered = {}

    try:
        client = MCPClient(proc)

        # --- Break out of title screen ---
        print("[explore] Breaking title screen...")
        for _ in range(30):
            send_key(client, SC_RETURN)
            time.sleep(0.5)
            state = poll_state(client, timeout_sec=2)
            menu = state.get("menu")
            if menu and menu != "null":
                break
        else:
            raise TimeoutError("Could not reach any menu")

        # --- Explore Main Menu ---
        print(f"[explore] Reached menu: {state.get('menu')}")
        print(f"[explore] Options: {state.get('options')}")
        discovered[state.get('menu')] = {
            "selected": state.get("selected"),
            "options": list(state.get("options", [])),
        }

        main_options = state.get("options", [])
        for idx in range(len(main_options)):
            # Reset to top of main menu
            for _ in range(15):
                send_key(client, SC_UP)
                time.sleep(0.15)

            # Navigate to option idx
            for _ in range(idx):
                send_key(client, SC_DOWN)
                time.sleep(0.3)

            state = wait_for_stable_menu(client, expected_menu="main_menu", timeout_sec=10)
            selected = state.get("selected", 0)
            option_name = state.get("options", [])[selected] if selected < len(state.get("options", [])) else "?"
            print(f"[explore] Main menu item [{selected}]: '{option_name}'")

            lower = option_name.lower()
            if "quit" in lower or "back to demo" in lower or "view scores" in lower:
                print(f"[explore]   SKIPPING (dangerous/boring): {option_name}")
                continue

            # Enter this menu item
            send_key(client, SC_RETURN)
            time.sleep(1.5)

            # Wait for submenu to appear
            try:
                sub_state = wait_for_stable_menu(client, timeout_sec=15)
            except TimeoutError:
                print(f"[explore]   No submenu appeared for '{option_name}'")
                continue

            sub_menu = sub_state.get("menu")
            if sub_menu == "main_menu":
                print(f"[explore]   Stayed in main_menu (toggle/action)")
                continue
            if sub_menu == "gameplay":
                print(f"[explore]   Reached gameplay!")
                discovered["gameplay"] = {
                    "selected": sub_state.get("selected"),
                    "options": list(sub_state.get("options", [])),
                }
                break  # Don't go further into gameplay

            print(f"[explore]   Entered submenu: {sub_menu}")
            print(f"[explore]   Options: {sub_state.get('options')}")
            if sub_menu not in discovered:
                discovered[sub_menu] = {
                    "selected": sub_state.get("selected"),
                    "options": list(sub_state.get("options", [])),
                }

            # If submenu has options, try navigating through them
            sub_options = sub_state.get("options", [])
            if sub_options:
                for sidx in range(len(sub_options)):
                    for _ in range(15):
                        send_key(client, SC_UP)
                        time.sleep(0.1)
                    for _ in range(sidx):
                        send_key(client, SC_DOWN)
                        time.sleep(0.2)
                    ss = wait_for_stable_menu(client, expected_menu=sub_menu, timeout_sec=5)
                    sel = ss.get("selected", 0)
                    oname = ss.get("options", [])[sel] if sel < len(ss.get("options", [])) else "?"
                    print(f"[explore]     Sub-menu '{sub_menu}' item [{sel}]: '{oname}'")

            # Escape back to main menu
            for _ in range(5):
                send_key(client, SC_ESCAPE)
                time.sleep(0.8)
                state = poll_state(client, timeout_sec=2)
                if state.get("menu") == "main_menu":
                    break
            else:
                print(f"[explore]   WARNING: Could not escape back to main menu!")
                break

        print("\n[explore] ===== DISCOVERED MENUS =====")
        for name, info in discovered.items():
            print(f"\nMenu: {name}")
            print(f"  Options ({len(info['options'])}):")
            for i, opt in enumerate(info['options']):
                marker = " <-- default" if i == info['selected'] else ""
                print(f"    [{i}] {opt}{marker}")

        return discovered

    except Exception as e:
        print(f"[explore] FAILED: {e}")
        import traceback
        traceback.print_exc()
        return discovered

    finally:
        if proc.poll() is None:
            print("[explore] Terminating game process...")
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()


if __name__ == "__main__":
    discovered = explore_menus()
    print("\n[explore] Done.")
