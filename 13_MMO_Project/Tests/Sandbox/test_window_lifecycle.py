import ctypes
from ctypes import wintypes
from pathlib import Path
import re
import subprocess
import sys
import time
import unittest


USER32 = ctypes.WinDLL("user32", use_last_error=True)
ENUM_WINDOW = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
USER32.EnumWindows.argtypes = (ENUM_WINDOW, wintypes.LPARAM)
USER32.GetWindowThreadProcessId.argtypes = (wintypes.HWND, ctypes.POINTER(wintypes.DWORD))
USER32.IsWindowVisible.argtypes = (wintypes.HWND,)
USER32.GetWindowTextW.argtypes = (wintypes.HWND, wintypes.LPWSTR, ctypes.c_int)
USER32.PostMessageW.argtypes = (wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM)


def find_visible_window(process_id):
    result = []

    @ENUM_WINDOW
    def visit(window, _):
        owner = wintypes.DWORD()
        USER32.GetWindowThreadProcessId(window, ctypes.byref(owner))
        if owner.value == process_id and USER32.IsWindowVisible(window):
            result.append(window)
            return False
        return True

    USER32.EnumWindows(visit, 0)
    return result[0] if result else None


class SandboxWindowTest(unittest.TestCase):
    def test_frame_and_fixed_update_rates_are_reported_without_input(self):
        with subprocess.Popen([str(EXECUTABLE)]) as process:
            try:
                deadline = time.monotonic() + 5
                title = ctypes.create_unicode_buffer(64)
                rates = re.compile(r"Sandbox - FPS: ([0-9]+) \| UPS: ([0-9]+)")
                while time.monotonic() < deadline and process.poll() is None:
                    window = find_visible_window(process.pid)
                    if window:
                        USER32.GetWindowTextW(window, title, len(title))
                        sample = rates.fullmatch(title.value)
                        if sample and int(sample[1]) > 0 and 0 < int(sample[2]) <= 60:
                            self.assertTrue(USER32.PostMessageW(window, 0x0112, 0xF060, 0))
                            self.assertEqual(process.wait(timeout=5), 0)
                            return
                    time.sleep(0.02)
                self.fail(f"Sandbox did not report FPS and fixed UPS without input: {title.value!r}")
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait(timeout=5)

    def test_visible_window_closes_with_zero_exit_code(self):
        with subprocess.Popen([str(EXECUTABLE)]) as process:
            try:
                deadline = time.monotonic() + 5
                window = None
                while time.monotonic() < deadline and process.poll() is None:
                    window = find_visible_window(process.pid)
                    if window:
                        break
                    time.sleep(0.02)

                self.assertIsNotNone(window, f"Sandbox showed no visible window (exit={process.poll()})")
                wm_syscommand, sc_close = 0x0112, 0xF060
                self.assertTrue(USER32.PostMessageW(window, wm_syscommand, sc_close, 0))
                self.assertEqual(process.wait(timeout=5), 0)
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait(timeout=5)


if __name__ == "__main__":
    EXECUTABLE = Path(sys.argv[1]).resolve()
    unittest.main(argv=[sys.argv[0]], verbosity=2)