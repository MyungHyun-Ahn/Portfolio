import ctypes
from ctypes import wintypes
from pathlib import Path
import re
import subprocess
import sys
import time
import unittest


USER32 = ctypes.WinDLL("user32", use_last_error=True)
GDI32 = ctypes.WinDLL("gdi32", use_last_error=True)
ENUM_WINDOW = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
USER32.EnumWindows.argtypes = (ENUM_WINDOW, wintypes.LPARAM)
USER32.GetWindowThreadProcessId.argtypes = (wintypes.HWND, ctypes.POINTER(wintypes.DWORD))
USER32.IsWindowVisible.argtypes = (wintypes.HWND,)
USER32.IsIconic.argtypes = (wintypes.HWND,)
USER32.GetWindowTextW.argtypes = (wintypes.HWND, wintypes.LPWSTR, ctypes.c_int)
USER32.PostMessageW.argtypes = (wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM)
USER32.GetClientRect.argtypes = (wintypes.HWND, ctypes.POINTER(wintypes.RECT))
USER32.ClientToScreen.argtypes = (wintypes.HWND, ctypes.POINTER(wintypes.POINT))
USER32.SetWindowPos.argtypes = (
    wintypes.HWND, wintypes.HWND, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, wintypes.UINT
)
USER32.GetDC.argtypes = (wintypes.HWND,)
USER32.GetDC.restype = wintypes.HDC
USER32.ReleaseDC.argtypes = (wintypes.HWND, wintypes.HDC)
GDI32.GetPixel.argtypes = (wintypes.HDC, ctypes.c_int, ctypes.c_int)
GDI32.GetPixel.restype = wintypes.DWORD


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
    def assert_rectangle_and_background(self, window, process, samples=None):
        client = wintypes.RECT()
        self.assertTrue(USER32.GetClientRect(window, ctypes.byref(client)))
        samples = samples or (
            # 오른쪽 내부를 찍어 기존 프레임을 축소해 표시하는 경우를 구분한다.
            (wintypes.POINT(220, 160), (255, 255, 255)),
            (wintypes.POINT(client.right // 2, client.bottom // 2), (25, 51, 102)),
        )
        for point, _ in samples:
            self.assertTrue(USER32.ClientToScreen(window, ctypes.byref(point)))

        colors = []
        desktop = USER32.GetDC(None)
        self.assertTrue(desktop)
        try:
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline and process.poll() is None:
                colors = [GDI32.GetPixel(desktop, point.x, point.y) for point, _ in samples]
                if all(
                    color != 0xFFFFFFFF and all(
                        abs(((color >> shift) & 255) - target) <= 2
                        for shift, target in zip((0, 8, 16), expected)
                    )
                    for color, (_, expected) in zip(colors, samples)
                ):
                    return
                time.sleep(0.02)
            self.fail(f"Sandbox did not present expected colors: COLORREFs={colors!r}, exit={process.poll()}")
        finally:
            USER32.ReleaseDC(None, desktop)

    def assert_window_rates(self, window, process, *, minimized):
        deadline = time.monotonic() + 5
        title = ctypes.create_unicode_buffer(64)
        while time.monotonic() < deadline and process.poll() is None:
            USER32.GetWindowTextW(window, title, len(title))
            sample = re.fullmatch(r"Sandbox - FPS: ([0-9]+) \| UPS: ([0-9]+)", title.value)
            if bool(USER32.IsIconic(window)) == minimized and sample:
                fps, ups = map(int, sample.groups())
                if (fps == 0 if minimized else fps > 0) and ups > 0:
                    return
            time.sleep(0.02)
        self.fail(f"Sandbox did not report expected rates (minimized={minimized}): {title.value!r}")

    def test_direct2d_survives_resize_minimize_and_restore(self):
        with subprocess.Popen([str(EXECUTABLE)]) as process:
            try:
                deadline = time.monotonic() + 5
                window = None
                while time.monotonic() < deadline and process.poll() is None:
                    window = find_visible_window(process.pid)
                    if window:
                        break
                    time.sleep(0.02)
                self.assertIsNotNone(window, f"Sandbox showed no window (exit={process.poll()})")

                hwnd_topmost, swp_no_activate = -1, 0x0010
                wm_syscommand, sc_minimize, sc_restore, sc_close = 0x0112, 0xF020, 0xF120, 0xF060
                for width, height in ((800, 600), (570, 460), (740, 540)):
                    with self.subTest(width=width, height=height):
                        self.assertTrue(USER32.SetWindowPos(window, hwnd_topmost, 40, 40, width, height, swp_no_activate))
                        self.assert_rectangle_and_background(window, process)

                self.assertTrue(USER32.PostMessageW(window, wm_syscommand, sc_minimize, 0))
                self.assert_window_rates(window, process, minimized=True)
                self.assertTrue(USER32.PostMessageW(window, wm_syscommand, sc_restore, 0))
                self.assert_window_rates(window, process, minimized=False)
                self.assert_rectangle_and_background(window, process)

                self.assertTrue(USER32.PostMessageW(window, wm_syscommand, sc_close, 0))
                self.assertEqual(process.wait(timeout=5), 0)
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait(timeout=5)

    def test_rectangle_moves_and_stops_at_latest_click(self):
        with subprocess.Popen([str(EXECUTABLE)]) as process:
            try:
                deadline = time.monotonic() + 5
                window = None
                while time.monotonic() < deadline and process.poll() is None:
                    window = find_visible_window(process.pid)
                    if window:
                        break
                    time.sleep(0.02)
                self.assertIsNotNone(window)
                self.assertTrue(USER32.SetWindowPos(window, -1, 40, 40, 800, 600, 0x0010))
                self.assert_rectangle_and_background(window, process)

                def click(x, y):
                    position = x | (y << 16)
                    self.assertTrue(USER32.PostMessageW(window, 0x0201, 1, position))
                    self.assertTrue(USER32.PostMessageW(window, 0x0202, 0, position))

                white, blue = (255, 255, 255), (25, 51, 102)
                click(610, 140)
                # 첫 목적지에 도착하기 전에 지나가는 위치를 관찰한다.
                self.assert_rectangle_and_background(window, process, (
                    (wintypes.POINT(310, 140), white), (wintypes.POINT(610, 140), white), (wintypes.POINT(610, 160), blue),
                ))
                click(460, 350)
                expected = ((381, 350, white), (538, 350, white), (460, 291, white), (460, 408, white),
                            (378, 350, blue), (542, 350, blue), (460, 288, blue), (460, 412, blue), (610, 140, blue))
                for _ in range(2):
                    self.assert_rectangle_and_background(window, process, tuple(
                        (wintypes.POINT(x, y), color) for x, y, color in expected
                    ))
                    time.sleep(0.2)

                self.assertTrue(USER32.PostMessageW(window, 0x0112, 0xF060, 0))
                self.assertEqual(process.wait(timeout=5), 0)
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait(timeout=5)

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
    unittest.main(argv=[sys.argv[0], *sys.argv[2:]], verbosity=2)