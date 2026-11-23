import ctypes
from ctypes import wintypes
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import unittest
import zlib


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
    def assert_rectangle_and_background(self, window, process, samples=None, moving_landmark=()):
        client = wintypes.RECT()
        self.assertTrue(USER32.GetClientRect(window, ctypes.byref(client)))
        samples = samples or (
            # 오른쪽 내부를 찍어 기존 프레임을 축소해 표시하는 경우를 구분한다.
            (wintypes.POINT(client.right // 2 + 73, client.bottom // 2), (255, 255, 255)),
            (wintypes.POINT(client.right // 2 + 83, client.bottom // 2), (25, 51, 102)),
            (wintypes.POINT(client.right // 2, client.bottom // 2), (255, 255, 255)),
        )
        for point, _ in samples:
            self.assertTrue(USER32.ClientToScreen(window, ctypes.byref(point)))
        for point in moving_landmark:
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
                ) and (not moving_landmark or any(
                    GDI32.GetPixel(desktop, point.x, point.y) == 0xFFFFFF for point in moving_landmark
                )):
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

    def test_camera_follows_and_clicks_resolve_in_world(self):
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
                client = wintypes.RECT()
                self.assertTrue(USER32.GetClientRect(window, ctypes.byref(client)))
                cx, cy = client.right // 2, client.bottom // 2

                def click(x, y):
                    position = x | (y << 16)
                    self.assertTrue(USER32.PostMessageW(window, 0x0201, 1, position))
                    self.assertTrue(USER32.PostMessageW(window, 0x0202, 0, position))

                white, blue = (255, 255, 255), (25, 51, 102)
                def check(*expected, moving_landmark=()):
                    self.assert_rectangle_and_background(window, process, tuple(
                        (wintypes.POINT(cx + x, cy + y), color) for x, y, color in expected
                    ), moving_landmark=tuple(wintypes.POINT(cx + x, cy + 100) for x in moving_landmark))

                # 월드 기준점 (460, 240)은 초기 카메라 위치 (160, 140)에서 (+300, +100)에 보인다.
                check((300, 100, white), (0, 100, blue))
                click(cx + 300, cy)
                # 사각형은 가운데에 있고 기준점은 이동 거리만큼 왼쪽으로 움직인다.
                check((0, 0, white), (300, 100, blue), moving_landmark=range(100, 201, 5))
                check((0, 0, white), (0, 100, white), (10, 100, blue))
                time.sleep(0.15)
                # 이동한 카메라 기준으로 두 번째 클릭을 해 월드 목적지 (300, 260)에 도착한다.
                click(cx - 160, cy + 120)
                check((0, 0, white), (160, -20, white), (170, -20, blue), (0, 100, blue))
                time.sleep(0.2)
                check((0, 0, white), (160, -20, white), (170, -20, blue))

                self.assertTrue(USER32.PostMessageW(window, 0x0112, 0xF060, 0))
                self.assertEqual(process.wait(timeout=5), 0)
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait(timeout=5)

    def test_camera_zoom_scales_drawing_and_clicks(self):
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
                client = wintypes.RECT()
                self.assertTrue(USER32.GetClientRect(window, ctypes.byref(client)))
                cx, cy = client.right // 2, client.bottom // 2
                white, blue = (255, 255, 255), (25, 51, 102)

                def check(*expected):
                    self.assert_rectangle_and_background(window, process, tuple(
                        (wintypes.POINT(cx + x, cy + y), color) for x, y, color in expected
                    ))

                def scroll(delta):
                    point = wintypes.POINT(cx + 25, cy + 25)
                    self.assertTrue(USER32.ClientToScreen(window, ctypes.byref(point)))
                    position = (point.x & 0xFFFF) | ((point.y & 0xFFFF) << 16)
                    self.assertTrue(USER32.PostMessageW(window, 0x020A, (delta & 0xFFFF) << 16, position))

                check((0, 0, white), (75, 0, white), (300, 100, white))
                scroll(-120)
                check((0, 0, white), (38, 0, white), (42, 0, blue), (150, 50, white), (300, 100, blue))

                # 반 배율에서 화면 +100px 클릭은 월드 +200만큼의 이동이다.
                position = (cx + 100) | (cy << 16)
                self.assertTrue(USER32.PostMessageW(window, 0x0201, 1, position))
                self.assertTrue(USER32.PostMessageW(window, 0x0202, 0, position))
                check((0, 0, white), (50, 50, white), (55, 50, blue), (150, 50, blue))
                time.sleep(0.2)
                check((0, 0, white), (50, 50, white), (55, 50, blue))

                scroll(120)
                check((75, 0, white), (100, 100, white), (50, 80, blue))
                scroll(120)
                check((155, 0, white), (165, 0, blue), (200, 200, white), (100, 155, blue))

                # 확대 상태에서 창 크기를 바꿔도 기준점과 클릭은 새 창 중심을 공유한다.
                self.assertTrue(USER32.SetWindowPos(window, -1, 40, 40, 691, 511, 0x0010))
                self.assertTrue(USER32.GetClientRect(window, ctypes.byref(client)))
                cx, cy = client.right // 2, client.bottom // 2
                check((0, 0, white), (200, 200, white), (170, 150, blue))
                position = (cx + 120) | (cy << 16)
                self.assertTrue(USER32.PostMessageW(window, 0x0201, 1, position))
                self.assertTrue(USER32.PostMessageW(window, 0x0202, 0, position))
                check((0, 0, white), (115, 200, blue), (80, 200, white), (68, 200, blue))

                scroll(-240)
                check((38, 0, white), (42, 0, blue), (20, 50, white))
                self.assertTrue(USER32.PostMessageW(window, 0x0112, 0xF060, 0))
                self.assertEqual(process.wait(timeout=5), 0)
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait(timeout=5)

    def test_png_alpha_and_reuse_through_camera_and_resize(self):
        def chunk(tag, data):
            return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            executable = shutil.copy2(EXECUTABLE, root)
            sprite = root / "BoyIdle.png"

            def write_sheet(path, rows):
                pixels = b"".join((b"\0" + row) * 16 for row in rows)
                path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 64, 16 * len(rows), 8, 6, 0, 0, 0))
                                 + chunk(b"IDAT", zlib.compress(pixels)) + chunk(b"IEND", b""))

            idle_colors = ((255, 0, 0), (180, 0, 0), (120, 0, 0), (60, 0, 0))
            alpha_samples = bytes((0, 255, 0, 128)) * 6 + bytes((0, 0, 255, 0)) * 5
            write_sheet(sprite, [b"".join(bytes((*color, 255)) * 5 + alpha_samples for color in idle_colors)])
            walk_sprite = root / "BoyWalk.png"
            walk_colors = ((0, 0, 255), (0, 255, 255), (255, 0, 255), (255, 255, 0))
            # Walk는 왼쪽부터 상·하·좌·우, 위에서 아래로 프레임이 진행된다.
            write_sheet(walk_sprite, [b"".join(bytes((*color, 255)) * 16 for color in
                        ((80, 0, 0), (40, 0, 0), (120, 0, 0), right_frame)) for right_frame in walk_colors])
            with subprocess.Popen([executable], cwd=EXECUTABLE.parent) as process:
                try:
                    deadline = time.monotonic() + 5
                    window = None
                    while time.monotonic() < deadline and process.poll() is None:
                        window = find_visible_window(process.pid)
                        if window:
                            break
                        time.sleep(0.02)
                    self.assertIsNotNone(window, f"Sandbox exited with {process.poll()}")

                    def resize(width, height):
                        self.assertTrue(USER32.SetWindowPos(window, -1, 40, 40, width, height, 0x0010))
                        client = wintypes.RECT()
                        self.assertTrue(USER32.GetClientRect(window, ctypes.byref(client)))
                        return client.right // 2, client.bottom // 2

                    def check(*expected):
                        self.assert_rectangle_and_background(window, process, tuple(
                            (wintypes.POINT(cx + x, cy + y), color) for x, y, color in expected
                        ))

                    down, right = idle_colors[0], idle_colors[3]
                    green, white, blue = (127, 255, 127), (255, 255, 255), (25, 51, 102)
                    cx, cy = resize(800, 600)
                    check((-12, -20, down), (0, -20, green), (12, -20, white), (300, 100, white))
                    idle_point = wintypes.POINT(cx - 12, cy - 20)
                    self.assertTrue(USER32.ClientToScreen(window, ctypes.byref(idle_point)))
                    desktop = USER32.GetDC(None)
                    self.assertTrue(desktop)
                    try:
                        deadline = time.monotonic() + 0.5
                        while time.monotonic() < deadline:
                            self.assertEqual(GDI32.GetPixel(desktop, idle_point.x, idle_point.y), down[0])
                            time.sleep(0.01)
                    finally:
                        USER32.ReleaseDC(None, desktop)
                    sprite.unlink()  # 이후 프레임과 리사이즈에 원본 파일이 필요 없어야 한다.
                    walk_sprite.unlink()
                    position = (cx + 240) | (cy << 16)
                    self.assertTrue(USER32.PostMessageW(window, 0x0201, 1, position))
                    self.assertTrue(USER32.PostMessageW(window, 0x0202, 0, position))
                    point = wintypes.POINT(cx, cy - 20)
                    self.assertTrue(USER32.ClientToScreen(window, ctypes.byref(point)))
                    walk_colorrefs = {r | (g << 8) | (b << 16) for r, g, b in walk_colors}
                    observed = set()
                    deadline = time.monotonic() + 1.5
                    desktop = USER32.GetDC(None)
                    self.assertTrue(desktop)
                    try:
                        while time.monotonic() < deadline and len(observed) < 2:
                            color = GDI32.GetPixel(desktop, point.x, point.y)
                            if color in walk_colorrefs:
                                observed.add(color)
                            time.sleep(0.01)
                    finally:
                        USER32.ReleaseDC(None, desktop)
                    self.assertGreaterEqual(len(observed), 2, "Walk should display more than one frame")
                    check((-12, -20, right), (60, 100, white), (70, 100, blue), (300, 100, blue))

                    self.assertTrue(USER32.PostMessageW(window, 0x020A, 120 << 16, 0))
                    check((-24, -40, right), (0, -40, green), (24, -40, white), (120, 200, white))
                    cx, cy = resize(691, 511)
                    check((-24, -40, right), (0, -40, green), (24, -40, white), (120, 200, white), (140, 200, blue))
                    for direction, offset in ((0, (0, 180)), (1, (0, -180)), (2, (-180, 0))):
                        position = (cx + offset[0]) | ((cy + offset[1]) << 16)
                        self.assertTrue(USER32.PostMessageW(window, 0x0201, 1, position))
                        self.assertTrue(USER32.PostMessageW(window, 0x0202, 0, position))
                        check((0, -40, (40 + direction * 40, 0, 0)))
                        check((-24, -40, idle_colors[direction]), (0, -40, green))
                    self.assertTrue(USER32.PostMessageW(window, 0x0112, 0xF060, 0))
                    self.assertEqual(process.wait(timeout=5), 0)
                finally:
                    if process.poll() is None:
                        process.kill()
                        process.wait(timeout=5)

            self.assertEqual(subprocess.run([executable], cwd=EXECUTABLE.parent, timeout=5).returncode, 2)
            sprite.write_bytes(b"not a PNG")
            self.assertEqual(subprocess.run([executable], cwd=EXECUTABLE.parent, timeout=5).returncode, 2)

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