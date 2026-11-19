"""Run against the built x64 Chatting solution: python test_server_event_input.py bind|chat [Release|Debug]."""

from contextlib import ExitStack, contextmanager
from pathlib import Path
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import time


ROOT = Path(__file__).resolve().parents[2]


@contextmanager
def launch(base, configuration, side, label, port):
    folder = base / label
    folder.mkdir()
    source = ROOT / "Build" / configuration / f"Chat{side}"
    exe = folder / f"Chat{side}.exe"
    shutil.copyfile(source / exe.name, exe)
    config = f"{side}Config.conf"
    text = (source / config).read_text(encoding="utf-16")
    text, count = re.subn(r"(\bPORT\s*=\s*)\d+", rf"\g<1>{port}", text)
    assert count == 1
    (folder / config).write_text(text, encoding="utf-16")
    stdout = folder / "stdout.txt"
    with stdout.open("wb") as output, subprocess.Popen(
        [str(exe)], cwd=folder, stdin=subprocess.PIPE, stdout=output, stderr=subprocess.STDOUT
    ) as process:
        try:
            yield process, stdout
        finally:
            if process.poll() is None:
                process.kill()
            process.wait(timeout=8)


def wait_text(child, needle, legacy=None):
    process, output = child
    deadline = time.monotonic() + 10
    while True:
        text = output.read_bytes().decode("utf-8")
        if needle in text or (legacy and legacy in text):
            return
        if process.poll() is not None or time.monotonic() >= deadline:
            raise AssertionError(f"{output}: missing {needle!r}; exit={process.poll()}; tail={text[-1500:]!r}")
        time.sleep(0.04)


def send(child, text):
    child[0].stdin.write((text + "\n").encode("utf-8"))
    child[0].stdin.flush()


def event(child, project, level, session_pattern, function, payload_pattern):
    stdout = child[1]
    paths = list((stdout.parent / "LogFile" / "SYSTEM").glob(f"*_{project}.txt"))
    assert len(paths) == 1, f"{project}: expected one UTF-16LE event file, found {paths}"
    raw = paths[0].read_bytes()
    assert raw.startswith(b"\xff\xfe"), paths[0]
    pattern = (
        r"\[\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z\]"
        rf"\[{level}\]\[{project}\]\[([1-9]\d*)\]\[({session_pattern})\]"
        rf"\[[^\]\r\n]*{function}[^\]\r\n]* : [1-9]\d*\] {payload_pattern}"
    )
    file_lines = [line for line in raw.decode("utf-16").splitlines() if re.fullmatch(pattern, line)]
    console_lines = [line for line in stdout.read_bytes().decode("utf-8").splitlines() if re.fullmatch(pattern, line)]
    assert sorted(file_lines) == sorted(console_lines) and file_lines, (
        f"{project}/{function}: expected identical structured event in file and stdout; "
        f"matches file/stdout={len(file_lines)}/{len(console_lines)}; file tail={raw.decode('utf-16')[-1000:]!r}"
    )
    return file_lines


def bind_failure(base, configuration):
    with socket.socket() as blocker:
        blocker.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        blocker.bind(("127.0.0.1", 0))
        blocker.listen()
        with launch(base, configuration, "Server", "bind", blocker.getsockname()[1]) as server:
            assert server[0].wait(timeout=10) == 1
            lines = event(server, "NetworkLib", "Error", "-", "Start",
                          r"event=bind_failed error_code=10048 \| 서버 소켓 바인딩에 실패했습니다\.")
            assert len(lines) == 1
            summary = event(server, "ChatServer", "Error", "-", "main",
                            r"event=server_start_failed \| 서버 시작에 실패했습니다\.")
            assert len(summary) == 1
            print(f"bind: exit=1, identical stdout/file: {lines[0]}")


def chat(base, configuration):
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
    with ExitStack() as cleanup:
        server = cleanup.enter_context(launch(base, configuration, "Server", "server", port))
        wait_text(server, f"event=server_ready port={port}", legacy=f"READY chat test-only port={port}")
        clients = [cleanup.enter_context(launch(base, configuration, "Client", label, port)) for label in ("a", "b")]
        for client in clients:
            wait_text(client, "READY account=")
        for sender, receiver, message in ((clients[0], clients[1], "로그 A→B 😀 100%"),
                                          (clients[1], clients[0], "로그 B→A 😀 %ls")):
            send(sender, message)
            wait_text(receiver, f"name=player: {message}")
        for client in clients:
            send(client, "/exit")
            assert client[0].wait(timeout=8) == 0
        wait_text(server, "event=session_leave")
        send(server, "/exit")
        assert server[0].wait(timeout=8) == 0
        ready = event(server, "ChatServer", "Info", "-", "RegisterContentTimerEvent",
                      rf"event=server_ready port={port} \| 테스트 채팅 서버가 준비되었습니다\.")
        workers = event(server, "NetworkLib", "Info", "-", "Start",
                        r"event=worker_started worker_index=\d+ \| 네트워크 워커를 시작했습니다\.")
        assert len(workers) == 2
        for function, payload, count in (
            (r"\w+", r"event=session_enter \| 세션이 콘텐츠에 진입했습니다\.", 2),
            ("Handle", r"event=login_accepted account_no=\d+ \| 테스트 로그인을 승인했습니다\.", 2),
            ("Handle", r"event=sector_entered account_no=\d+ sector_x=10 sector_y=10 \| 섹터에 입장했습니다\.", 2),
        ):
            lines = event(server, "ChatServer", "Debug", r"[1-9]\d*", function, payload)
            assert len(lines) == count
        event(server, "ChatServer", "Debug", r"[1-9]\d*", "OnLeave",
              r"event=session_leave \| 세션이 콘텐츠를 떠났습니다\.")
        assert len(ready) == 1
        print(f"chat: two client READY, A→B and B→A exact payloads, exits=0/0/0; {ready[0]}")
        print("chat: session_enter=2, login_accepted=2, sector_entered=2; identical stdout/file")


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    mode = sys.argv[1]
    configuration = sys.argv[2] if len(sys.argv) > 2 else "Release"
    with tempfile.TemporaryDirectory(prefix="mmo-server-events-", dir=ROOT / "Build") as directory:
        {"bind": bind_failure, "chat": chat}[mode](Path(directory), configuration)
