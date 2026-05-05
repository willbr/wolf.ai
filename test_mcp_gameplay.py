#!/usr/bin/env python3
"""
MCP integration test for Wolf3D menu navigation into gameplay.

The game executable now embeds an MCP server thread that communicates
over stdio via JSON-RPC 2.0. This test launches the game and talks
directly to its stdin/stdout — no window handles, no PIL, no ctypes.

Prerequisites:
- zig-out\\bin\\wolf3d.exe must be built with MCP support
- wolf3d-data\\ shareware WL1 files must be present

Run:
    python test_mcp_gameplay.py
"""

import subprocess
import json
import sys
import os
import time


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
        """Call a method directly (bypass tools/call wrapper)."""
        resp = self._request(method, params, timeout_sec=timeout_sec)
        if "error" in resp:
            raise RuntimeError(f"Method {method} error: {resp['error']}")
        result = resp["result"]
        # C MCP returns raw result; old Python wrapper returned {"content": [...]}
        if isinstance(result, dict) and "content" in result:
            return result
        text = json.dumps(result) if not isinstance(result, str) else result
        return {"content": [{"type": "text", "text": text}]}


def poll_state(client, timeout_sec=30):
    """Poll get_state until it returns or timeout."""
    deadline = time.time() + timeout_sec
    while time.time() < deadline:
        result = client.call_method("get_state", timeout_sec=5)
        content = result["content"][0]
        return json.loads(content["text"])
    raise TimeoutError("State poll timed out")


def wait_for_menu(client, expected_menu, timeout_sec=30, step_sec=0.5, send_key=None):
    """Poll state until the expected menu is shown."""
    deadline = time.time() + timeout_sec
    while time.time() < deadline:
        if send_key:
            send_key()
        state = poll_state(client, timeout_sec=2)
        if state.get("menu") == expected_menu:
            return state
        time.sleep(step_sec)
    raise TimeoutError(f"Menu never reached {expected_menu}")


def test_navigate_to_gameplay():
    exe_path = os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "zig-out", "bin", "wolf3d.exe",
    )
    if not os.path.exists(exe_path):
        raise FileNotFoundError(f"Executable not found: {exe_path}")

    print("[test] Launching game with MCP...")
    proc = subprocess.Popen(
        [exe_path],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1,
    )

    client = None
    try:
        client = MCPClient(proc)

        # --- Step 1: break out of title screen / attract mode ---
        # Send Enter repeatedly until the main menu appears.
        # The game may still be initializing when the first key is sent.
        print("[test] Sending Enter to break title screen...")
        deadline = time.time() + 20
        while time.time() < deadline:
            client.call_method("send_key", {"scancode": 28}, timeout_sec=5)
            time.sleep(0.5)
            state = poll_state(client, timeout_sec=2)
            if state.get("menu") == "main_menu":
                break
        else:
            raise TimeoutError("Menu never reached main_menu")
        print(f"[test] Reached main menu: {state.get('options')}")

        # --- Step 2: Main Menu -> New Game ---
        print("[test] Sending Enter for New Game...")
        state = wait_for_menu(client, "episode_select", timeout_sec=15,
                              send_key=lambda: client.call_method("send_key", {"scancode": 28}, timeout_sec=5))
        print(f"[test] Reached episode select: {state.get('options')}")

        # --- Step 3: Episode select -> Episode 1 ---
        print("[test] Sending Enter for Episode 1...")
        state = wait_for_menu(client, "difficulty_select", timeout_sec=15,
                              send_key=lambda: client.call_method("send_key", {"scancode": 28}, timeout_sec=5))
        print(f"[test] Reached difficulty select: {state.get('options')}")

        # --- Step 4: Difficulty select -> first difficulty ---
        print("[test] Sending Enter for difficulty...")
        state = wait_for_menu(client, "gameplay", timeout_sec=20,
                              send_key=lambda: client.call_method("send_key", {"scancode": 28}, timeout_sec=5))
        print(f"[test] Reached gameplay: mapon={state.get('mapon')} episode={state.get('episode')} difficulty={state.get('difficulty')}")

        # --- Verify ---
        assert state.get("ingame") is True, f"Expected ingame=True, got {state}"
        assert state.get("startgame") is False, f"Expected startgame=False (cleared by GameLoop), got {state}"
        print(f"[test] Game state: {state}")

        # --- Take a screenshot ---
        print("[test] Taking screenshot...")
        result = client.call_method("take_screenshot", timeout_sec=10)
        content = result["content"][0]
        if content.get("type") == "text":
            data = json.loads(content["text"])
            print(f"[test] Screenshot saved to: {data.get('path')}")

        print("[test] PASSED: Game reached gameplay without crashing.")
        return True

    except Exception as e:
        print(f"[test] FAILED: {e}")
        # Drain stderr for diagnostics
        try:
            proc.stdin.close()
            _, stderr = proc.communicate(timeout=5)
            if stderr:
                print("[test] STDERR:")
                print(stderr)
        except Exception:
            pass
        return False

    finally:
        if proc.poll() is None:
            print("[test] Terminating game process...")
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()


if __name__ == "__main__":
    ok = test_navigate_to_gameplay()
    sys.exit(0 if ok else 1)
