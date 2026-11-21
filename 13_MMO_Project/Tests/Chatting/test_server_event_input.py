"""Run against the built x64 Chatting solution: python test_server_event_input.py bind|chat|log-interval|session-limit [Release|Debug]."""

from contextlib import ExitStack, contextmanager
from pathlib import Path
import re
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time


ROOT = Path(__file__).resolve().parents[2]


@contextmanager
def launch(base, configuration, side, label, port, levels=None, log_interval=None, nickname=None, *, session_limit=None):
    folder = base / label
    folder.mkdir()
    source = ROOT / "Build" / configuration / f"Chat{side}"
    exe = folder / f"Chat{side}.exe"
    shutil.copyfile(source / exe.name, exe)
    config = f"{side}Config.conf"
    text = (source / config).read_text(encoding="utf-16")
    text, count = re.subn(r"(\bPORT\s*=\s*)\d+", rf"\g<1>{port}", text)
    assert count == 1
    if nickname is not None:
        text, count = re.subn(r"(\bNICKNAME\s*=\s*)[^;]*", lambda match: match[1] + nickname, text)
        assert count == 1
    if levels is not None:
        for key, value in zip(("CONSOLE_LOG_LEVEL", "FILE_LOG_LEVEL"), levels):
            text, count = re.subn(rf"(\b{key}\s*=\s*)\w+", rf"\g<1>{value}", text)
            if not count:
                text += f"\n\t- {key} = {value};"
    if log_interval is not None:
        text, count = re.subn(r"\bLOG_POLL_INTERVAL_MS\s*=\s*[^;]*;", f"LOG_POLL_INTERVAL_MS = {log_interval};", text)
        if not count:
            text += f"\n\t- LOG_POLL_INTERVAL_MS = {log_interval};"
    if session_limit is not None:
        text = re.sub(r"(?m)^[ \t]*-[ \t]*MAX_SESSION_COUNT[ \t]*=[^\n]*\n?", "", text)
        text += f"\n\t- MAX_SESSION_COUNT = {session_limit};"
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


def server_log_path(stdout):
    paths = list((stdout.parent / "LogFile").glob("run-*/*.log"))
    assert len(paths) == 1, f"expected one async run file, found {paths}"
    assert re.fullmatch(r"[0-9]{6}-[0-9]{6}-ChatServer\.log", paths[0].name), paths[0]
    return paths[0]


def event(child, project, level, session_pattern, function, payload_pattern, *, on_console=True, in_file=True):
    stdout = child[1]
    path = server_log_path(stdout)
    raw = path.read_bytes()
    assert not raw.startswith((b"\xff\xfe", b"\xef\xbb\xbf")), path
    pattern = (
        r"\[\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z\]"
        rf"\[{level}\]\[{project}\]\[([1-9]\d*)\]\[({session_pattern})\]"
        rf"\[[^\]\r\n]*{function}[^\]\r\n]* : [1-9]\d*\] {payload_pattern}"
    )
    file_lines = [line for line in raw.decode("utf-8").splitlines() if re.fullmatch(pattern, line)]
    console_lines = [line for line in stdout.read_bytes().decode("utf-8").splitlines() if re.fullmatch(pattern, line)]
    assert (bool(file_lines), bool(console_lines)) == (in_file, on_console) and (
        not (in_file and on_console) or sorted(file_lines) == sorted(console_lines)
    ), (
        f"{project}/{function}: expected file/stdout={in_file}/{on_console}; "
        f"matches file/stdout={len(file_lines)}/{len(console_lines)}; file tail={raw.decode('utf-8')[-1000:]!r}"
    )
    return file_lines if in_file else console_lines


def bind_failure(base, configuration, levels=("Error", "Info"), log_interval=None):
    with socket.socket() as blocker:
        blocker.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        blocker.bind(("127.0.0.1", 0))
        blocker.listen()
        port = blocker.getsockname()[1]
        with launch(base, configuration, "Server", "bind", port, levels, log_interval) as server:
            assert server[0].wait(timeout=10) == 1
            lines = event(server, "NetworkLib", "Error", "-", "Start",
                          r"event=bind_failed error_code=10048 \| 서버 소켓 바인딩에 실패했습니다\.")
            assert len(lines) == 1
            summary = event(server, "ChatServer", "Error", "-", "main",
                            r"event=server_start_failed \| 서버 시작에 실패했습니다\.")
            assert len(summary) == 1
            event(server, "ChatServer", "Info", "-", "main",
                  r"event=server_local_test \| .+", on_console=False, in_file=False)
            assert list((server[1].parent / "LogFile").rglob("*.txt")) == []
            print(f"bind: exclusive port={port}, exit=1; identical Error stdout/file: {lines[0]}")


def log_interval_config(base, configuration):
    for index, value in enumerate(("0", "1001", "abc", "20ms")):
        with launch(base, configuration, "Server", f"invalid-{index}", 6000, log_interval=value) as server:
            assert server[0].wait(timeout=5) == 2, f"LOG_POLL_INTERVAL_MS={value} was not rejected"
            assert "LOG_POLL_INTERVAL_MS" in server[1].read_text(encoding="utf-8")
            assert not (server[1].parent / "LogFile").exists()
    bind_failure(base, configuration, log_interval="25")
    print("log interval: rejected invalid values before logger startup; accepted 25 ms")


def session_limit_config(base, configuration):
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]

    with launch(base, configuration, "Server", "one-slot", port, levels=("Debug", "Debug"), session_limit="1") as server:
        wait_text(server, f"event=server_ready port={port}")
        with socket.create_connection(("127.0.0.1", port), timeout=5):
            wait_text(server, "event=session_accepted")
            with socket.create_connection(("127.0.0.1", port), timeout=5) as excess:
                try:
                    assert excess.recv(1) == b"", "excess connection was not closed"
                except ConnectionResetError:
                    pass
                except TimeoutError as exc:
                    raise AssertionError("MAX_SESSION_COUNT=1 did not close the second connection") from exc
            wait_text(server, "event=session_slot_pop_failed")
        send(server, "/exit")
        assert server[0].wait(timeout=8) == 0

    for index, value in enumerate(("0", "-1", "65536", "1slot")):
        with launch(base, configuration, "Server", f"invalid-limit-{index}", port, session_limit=value) as server:
            assert server[0].wait(timeout=5) == 2, f"MAX_SESSION_COUNT={value} was not rejected"
            wait_text(server, "MAX_SESSION_COUNT")
            assert not (server[1].parent / "LogFile").exists()
    print("session limit: one connection admitted, second closed; invalid values rejected before startup")


def chat(base, configuration):
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
    with ExitStack() as cleanup:
        server = cleanup.enter_context(launch(base, configuration, "Server", "server", port))
        wait_text(server, f"event=server_ready port={port}", legacy=f"READY chat test-only port={port}")
        nicknames = ("가", "나" * 17 + "😀")
        assert [len(name.encode("utf-16-le")) // 2 for name in nicknames] == [1, 19]
        with launch(base, configuration, "Client", "over-limit", port, nickname=nicknames[1] + "가") as invalid:
            assert invalid[0].wait(timeout=8) == 2
            wait_text(invalid, "NICKNAME is missing or invalid")
        clients = [cleanup.enter_context(launch(base, configuration, "Client", label, port, nickname=nickname))
                   for label, nickname in zip(("a", "b"), nicknames)]
        for client in clients:
            wait_text(client, "READY account=")
        messages = ("로그 A→B 😀 100%", "로그 B→A 😀 %ls")
        for index, message in enumerate(messages):
            sender, receiver = clients[index], clients[1 - index]
            send(sender, message)
            for client in (sender, receiver):
                wait_text(client, f"name={nicknames[index]}: {message}")
        for client in clients:
            send(client, "/exit")
            assert client[0].wait(timeout=8) == 0
            text = client[1].read_text(encoding="utf-8")
            assert [line.split(" name=", 1)[1] for line in text.splitlines() if "CHAT account=" in line] == [
                f"{name}: {message}" for name, message in zip(nicknames, messages)
            ], text
        wait_text((server[0], server_log_path(server[1])), "event=session_leave")
        ready = event(server, "ChatServer", "Info", "-", "RegisterContentTimerEvent",
                      rf"event=server_ready port={port} \| 테스트 채팅 서버가 준비되었습니다\.")
        workers = event(server, "NetworkLib", "Info", "-", "Start",
                        r"event=worker_started worker_index=\d+ \| 네트워크 워커를 시작했습니다\.")
        assert len(workers) == 2
        contents_workers = event(server, "NetworkLib", "Info", "-", "threadFunc",
                                 r"event=contents_worker_started \| .+")
        assert len(contents_workers) == 2
        for function, payload, count in (
            ("OnAccept", r"event=session_accepted session_id=\2 \| .+", 2),
            (r"\w+", r"event=session_enter \| 세션이 콘텐츠에 진입했습니다\.", 2),
            ("Handle", r"event=login_accepted account_no=\d+ \| 테스트 로그인을 승인했습니다\.", 2),
            ("Handle", r"event=sector_entered account_no=\d+ sector_x=10 sector_y=10 \| 섹터에 입장했습니다\.", 2),
            ("HandleReqMessage", r"event=chat_processed sector_x=10 sector_y=10 \| 채팅 요청을 처리했습니다\.", 2),
        ):
            lines = event(server, "ChatServer", "Debug", r"[1-9]\d*", function, payload, on_console=False)
            assert len(lines) == count
        event(server, "ChatServer", "Debug", r"[1-9]\d*", "OnLeave",
              r"event=session_leave \| 세션이 콘텐츠를 떠났습니다\.", on_console=False)
        assert len(ready) == 1
        send(server, "/exit")
        assert server[0].wait(timeout=8) == 0
        print(f"chat: two clients READY, one self echo and one peer delivery per message, exits=0/0/0; {ready[0]}")
        print("chat: contents_worker_started=2; session_accepted/enter, login, sector and chat Debug events in file only")
        print("chat: 1/19-code-unit nicknames preserved, including Korean and emoji; 20-code-unit config rejected")


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    mode = sys.argv[1]
    configuration = sys.argv[2] if len(sys.argv) > 2 else "Release"
    case = {"bind": bind_failure, "chat": chat, "log-interval": log_interval_config, "session-limit": session_limit_config}[mode]
    evidence = os.environ.get("MMO_SERVER_EVENT_EVIDENCE")
    if evidence:
        output = Path(evidence) / f"{configuration}-{mode}"
        output.mkdir(parents=True)
        case(output, configuration)
    else:
        with tempfile.TemporaryDirectory(prefix="mmo-server-events-", dir=ROOT / "Build") as directory:
            case(Path(directory), configuration)
