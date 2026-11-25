"""Run against the built x64 Chatting solution: python test_server_event_input.py bind|chat|log-interval|session-limit|server-options|metrics [Release|Debug]."""

from contextlib import ExitStack, contextmanager
import ctypes
from ctypes import wintypes
from math import isfinite
from pathlib import Path
import re
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time
from http.client import HTTPConnection


ROOT = Path(__file__).resolve().parents[2]


@contextmanager
def launch(base, configuration, side, label, port, levels=None, log_interval=None, nickname=None, *, session_limit=None,
           server_options=None, executable_name=None):
    folder = base / label
    folder.mkdir()
    source = ROOT / "Build" / configuration / f"Chat{side}"
    exe = folder / (executable_name or f"Chat{side}.exe")
    shutil.copyfile(source / f"Chat{side}.exe", exe)
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
    for key, value in (server_options or {}).items():
        text = re.sub(rf"(?m)^[ \t]*-[ \t]*{re.escape(key)}[ \t]*=[^\n]*\n?", "", text)
        text += f"\n\t- {key} = {value};"
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


def wait_text(child, needle, legacy=None, *, count=1):
    process, output = child
    deadline = time.monotonic() + 10
    while True:
        text = output.read_bytes().decode("utf-8")
        if text.count(needle) >= count or (legacy and text.count(legacy) >= count):
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


def server_options_config(base, configuration):
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
    options = {"ACCEPTEX_COUNT": 8, "IOCP_WORKER_COUNT": 3, "IOCP_CONCURRENCY": 2,
               "CONTENTS_WORKER_COUNT": 1, "LOG_QUEUE_CAPACITY": 2048}
    with launch(base, configuration, "Server", "custom-server", port, server_options=options) as server:
        wait_text(server, f"event=server_ready port={port}")
        wait_text(server, "event=server_config acceptex_count=8 iocp_worker_count=3 iocp_concurrency=2"
                  " contents_worker_count=1 log_queue_capacity=2048")
        wait_text(server, "event=worker_started worker_index=3")
        workers = event(server, "NetworkLib", "Info", "-", "Start", r"event=worker_started worker_index=\d+ \| .+")
        assert len(workers) == 3
        with launch(base, configuration, "Client", "custom-client", port) as client:
            wait_text(client, "READY account=")
            send(client, "custom server config")
            wait_text(client, ": custom server config")
            send(client, "/exit")
            assert client[0].wait(timeout=8) == 0
        assert "event=contents_worker_started" not in server[1].read_text(encoding="utf-8"), "legacy content worker started"
        send(server, "/exit")
        assert server[0].wait(timeout=8) == 0

    invalid = [("ACCEPTEX_COUNT", 0), ("ACCEPTEX_COUNT", 65536), ("IOCP_WORKER_COUNT", 65),
               ("IOCP_CONCURRENCY", 0), ("IOCP_CONCURRENCY", 4), ("CONTENTS_WORKER_COUNT", 0),
               ("LOG_QUEUE_CAPACITY", 0), ("LOG_QUEUE_CAPACITY", "1024items"), ("LOG_QUEUE_CAPACITY", 1048577),
               ("CONTENTS_FPS", 0), ("CONTENTS_FPS", 1001), ("CONTENTS_FPS", "25fps")]
    for index, (key, value) in enumerate(invalid):
        with launch(base, configuration, "Server", f"invalid-option-{index}", port,
                    server_options={**options, key: value}) as server:
            assert server[0].wait(timeout=5) == 2, f"{key}={value} was not rejected"
            wait_text(server, key)
            assert not (server[1].parent / "LogFile").exists()
    print("server options: custom worker counts and loopback echo verified; invalid settings rejected")


def chat(base, configuration, packet_mode=None):
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
    options = {"PACKET_MODE": packet_mode} if packet_mode is not None else None
    if packet_mode is not None:
        for side in ("Server", "Client"):
            with launch(base, configuration, side, f"invalid-{side}", port,
                        server_options={"PACKET_MODE": "Invalid"}) as invalid:
                wait_text(invalid, "PACKET_MODE must be Net or Lan")
                assert invalid[0].wait(timeout=8) == 2
    with ExitStack() as cleanup:
        server = cleanup.enter_context(launch(base, configuration, "Server", "server", port, server_options=options))
        wait_text(server, f"event=server_ready port={port}", legacy=f"READY chat test-only port={port}")
        nicknames = ("가", "나" * 17 + "😀")
        assert [len(name.encode("utf-16-le")) // 2 for name in nicknames] == [1, 19]
        with launch(base, configuration, "Client", "over-limit", port, nickname=nicknames[1] + "가") as invalid:
            assert invalid[0].wait(timeout=8) == 2
            wait_text(invalid, "NICKNAME is missing or invalid")
        clients = [cleanup.enter_context(launch(base, configuration, "Client", label, port,
                                               nickname=nickname, server_options=options))
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
        wait_text((server[0], server_log_path(server[1])), "event=session_leave", count=2)
        stdout = server[1].read_text(encoding="utf-8")
        assert "Server monitoring info" not in stdout, "legacy monitor output appeared during chat"
        assert "event=contents_worker_started" not in stdout, "legacy content worker started"
        ready = event(server, "ChatServer", "Info", "-", "OnServerReady",
                      rf"event=server_ready port={port} \| 테스트 채팅 서버가 준비되었습니다\.")
        workers = event(server, "NetworkLib", "Info", "-", "Start",
                        r"event=worker_started worker_index=\d+ \| 네트워크 워커를 시작했습니다\.")
        assert len(workers) == 2
        assert len(ready) == 1 and all(stdout.index(line) < stdout.index(ready[0]) for line in workers)
        def worker_id(line):
            return re.search(r"\[ChatServer\]\[(\d+)\]|\[NetworkLib\]\[(\d+)\]", line).group(0).split("[")[-1][:-1]

        chat_workers = set()
        accept_workers = set()
        for function, payload, count in (
            ("OnAccept", r"event=session_accepted session_id=\2 \| .+", 2),
            (r"\w+", r"event=session_enter \| 세션이 콘텐츠에 진입했습니다\.", 2),
            ("Handle", r"event=login_accepted account_no=\d+ \| 테스트 로그인을 승인했습니다\.", 2),
            ("Handle", r"event=sector_entered account_no=\d+ sector_x=10 sector_y=10 \| 섹터에 입장했습니다\.", 2),
            ("HandleReqMessage", r"event=chat_processed sector_x=10 sector_y=10 \| 채팅 요청을 처리했습니다\.", 2),
        ):
            lines = event(server, "ChatServer", "Debug", r"[1-9]\d*", function, payload, on_console=False)
            assert len(lines) == count
            if function == "OnAccept":
                accept_workers.update(map(worker_id, lines))
            else:
                chat_workers.update(map(worker_id, lines))
        leaves = event(server, "ChatServer", "Debug", r"[1-9]\d*", "OnLeave",
                       r"event=session_leave \| 세션이 콘텐츠를 떠났습니다\.", on_console=False)
        assert len(leaves) == 2 and {worker_id(line) for line in leaves} == chat_workers
        assert len(chat_workers) == 1 and chat_workers.isdisjoint(accept_workers)
        send(server, "/exit")
        assert server[0].wait(timeout=8) == 0
        print(f"chat: two clients READY, one self echo and one peer delivery per message, exits=0/0/0; {ready[0]}")
        print("chat: one Chatting owner worker; session_accepted/enter, login, sector and chat Debug events in file only")
        print("chat: 1/19-code-unit nicknames preserved, including Korean and emoji; 20-code-unit config rejected")


def scrape(address, port):
    connection = HTTPConnection(address, port, timeout=1)
    try:
        connection.request("GET", "/metrics")
        response = connection.getresponse()
        body = response.read().decode("utf-8")
        assert response.status in (200, 503), response.status
        if response.status == 503:
            return {}
        assert response.getheader("Content-Type").startswith("text/plain; version=0.0.4")
        lines = [sample.rsplit(" ", 1) for sample in body.splitlines() if sample and not sample.startswith("#")]
        samples = {name: float(value) for name, value in lines}
        assert len(samples) == len(lines), "duplicate metric series"
        return samples
    finally:
        connection.close()


def wait_metrics(address, port, predicate):
    deadline = time.monotonic() + 5
    samples = {}
    while time.monotonic() < deadline:
        samples = scrape(address, port)
        if predicate(samples):
            return samples
        time.sleep(0.04)
    raise AssertionError(f"metrics condition not met: {samples}")


NIC_PREFIX = "host_nic_total_bytes_per_second"
LABEL_ESCAPES = str.maketrans({"\\": "\\\\", '"': '\\"', "\n": "\\n"})


def windows_nic_series():
    class CounterItem(ctypes.Structure):
        _fields_ = [("name", wintypes.LPWSTR), ("status", wintypes.DWORD), ("value", ctypes.c_double)]

    pdh = ctypes.WinDLL("pdh.dll")
    pdh.PdhOpenQueryW.argtypes = (wintypes.LPCWSTR, ctypes.c_size_t, ctypes.POINTER(wintypes.HANDLE))
    pdh.PdhAddEnglishCounterW.argtypes = (wintypes.HANDLE, wintypes.LPCWSTR, ctypes.c_size_t, ctypes.POINTER(wintypes.HANDLE))
    pdh.PdhCollectQueryData.argtypes = (wintypes.HANDLE,)
    pdh.PdhGetFormattedCounterArrayW.argtypes = (
        wintypes.HANDLE, wintypes.DWORD, ctypes.POINTER(wintypes.DWORD), ctypes.POINTER(wintypes.DWORD), ctypes.c_void_p,
    )
    pdh.PdhCloseQuery.argtypes = (wintypes.HANDLE,)
    query = wintypes.HANDLE()
    counter = wintypes.HANDLE()
    assert pdh.PdhOpenQueryW(None, 0, ctypes.byref(query)) == 0
    try:
        status = pdh.PdhAddEnglishCounterW(query, r"\Network Interface(*)\Bytes Total/sec", 0, ctypes.byref(counter))
        assert status == 0, f"NIC counter registration: {status & 0xffffffff:#x}"
        assert pdh.PdhCollectQueryData(query) == 0
        time.sleep(1.1)
        assert pdh.PdhCollectQueryData(query) == 0
        size, count = wintypes.DWORD(), wintypes.DWORD()
        flags = 0x200 | 0x1000 | 0x8000  # DOUBLE | NOSCALE | NOCAP100
        status = pdh.PdhGetFormattedCounterArrayW(counter, flags, ctypes.byref(size), ctypes.byref(count), None)
        assert status & 0xffffffff == 0x800007D2, f"NIC array sizing: {status & 0xffffffff:#x}"
        buffer = ctypes.create_string_buffer(size.value)
        status = pdh.PdhGetFormattedCounterArrayW(counter, flags, ctypes.byref(size), ctypes.byref(count), buffer)
        assert status == 0, f"NIC array formatting: {status & 0xffffffff:#x}"
        items = ctypes.cast(buffer, ctypes.POINTER(CounterItem))
        return {items[i].name for i in range(count.value)
                if items[i].name and items[i].status in (0, 1) and isfinite(items[i].value) and items[i].value >= 0}
    finally:
        pdh.PdhCloseQuery(query)


def check_monitor_series(samples, process_series, *, has_rates=True, has_chat_rates=True):
    retired = {f"chat_work_{name}_{suffix}" for name in ("delegations", "steals") for suffix in ("total", "per_second")}
    assert retired.isdisjoint(samples), f"retired worker metrics: {retired & samples.keys()}"
    intervals = (
        "net_accept_completions", "net_received_packets", "net_sent_packets", "net_send_batch_limit_hits",
    )
    chat_intervals = ("chat_content_updates", "chat_login_requests", "chat_sector_move_requests",
                      "chat_message_requests", "chat_message_send_requests")
    rates = {f"{name}_per_second" for name in intervals}
    chat_rates = {f"{name}_per_second" for name in chat_intervals}
    required = {f"{name}_total" for name in intervals + chat_intervals}
    if has_rates:
        required |= rates
    else:
        assert rates.isdisjoint(samples), "first monitor snapshot published interval rates"
    if has_chat_rates:
        required |= chat_rates
    else:
        assert chat_rates.isdisjoint(samples), "first chat snapshot published interval rates"
    required |= {f"net_{name}" for name in ("sessions", "sessions_accepted_total", "wsasend_calls_total", "wsasend_pending_total")}
    required |= {f"process_{name}_bytes" for name in (
        "working_set", "peak_working_set", "paged_pool", "peak_paged_pool", "nonpaged_pool", "peak_nonpaged_pool",
        "commit", "peak_commit", "private",
    )}
    publish_time = "observability_snapshot_timestamp_seconds"
    required.update(("process_page_faults_total", process_series, publish_time))
    required.add("chat_content_frames_total")
    cpu = {f'{scope}_cpu_usage_ratio{{mode="{mode}"}}' for scope in ("host", "process")
           for mode in ("total", "user", "kernel")}
    pdh = {name for name in samples if re.fullmatch(rf'{NIC_PREFIX}\{{interface="(?:[^"\\]|\\["\\n])+"\}}', name)}
    if not has_rates:
        assert not pdh, "first monitor snapshot published NIC rates"
    pdh |= {"host_nonpaged_pool_bytes", "host_available_memory_bytes", "host_tcpv4_retransmitted_segments_per_second"}
    actual = set(samples)
    assert required <= actual <= required | cpu | pdh, (required - actual, actual - required - cpu - pdh)
    assert all(isfinite(value) and value >= 0 for value in samples.values()), samples
    assert samples[process_series] == 1
    assert abs(time.time() - samples[publish_time]) < 30, "expected a recent Unix publication time"
    print(f"metrics: {len(actual)} series; valid Windows samples: {sorted(actual & (cpu | pdh))}")


def metrics(base, configuration):
    address = "127.0.0.2"
    executable_name = "Monitor-검증-😀.exe"
    process_series = f'process_info{{process="{executable_name}"}}'
    with socket.socket() as chat_probe, socket.socket() as metrics_probe:
        chat_probe.bind(("127.0.0.1", 0))
        metrics_probe.bind(("0.0.0.0", 0))
        chat_port = chat_probe.getsockname()[1]
        metrics_port = metrics_probe.getsockname()[1]
    options = {"METRICS_BIND_ADDRESS": address, "METRICS_PORT": metrics_port}
    with launch(base, configuration, "Server", "metrics", chat_port, server_options={**options, "CONTENTS_FPS": 5},
                executable_name=executable_name) as server:
        wait_text(server, f"event=metrics_started port={metrics_port}")
        initial = wait_metrics(address, metrics_port, bool)
        expected_interfaces = windows_nic_series()
        assert expected_interfaces, "Windows PDH provides no valid NIC instances; discovery not verified"
        expected_nics = {f'{NIC_PREFIX}{{interface="{name.translate(LABEL_ESCAPES)}"}}'
                         for name in expected_interfaces}
        # The exporter only retains the latest sample, so the first HTTP read may miss warm-up.
        check_monitor_series(initial, process_series, has_rates="net_accept_completions_per_second" in initial,
                             has_chat_rates="chat_message_requests_per_second" in initial)
        wait_metrics(address, metrics_port, lambda values: expected_nics <= values.keys())
        print(f"metrics discovery: {process_series} = 1; Windows PDH `Bytes Total/sec`: {sorted(expected_interfaces)}")
        assert initial["net_sessions"] == 0
        assert initial["process_working_set_bytes"] > 0
        assert initial["process_nonpaged_pool_bytes"] > 0
        assert initial["net_accept_completions_total"] == 0
        assert initial["net_sessions_accepted_total"] == 0
        assert initial["net_wsasend_calls_total"] == initial["net_wsasend_pending_total"] == 0
        wait_text(server, f"event=server_ready port={chat_port}")
        idle_rates = wait_metrics(address, metrics_port, lambda values: "net_accept_completions_per_second" in values
                                  and "chat_message_requests_per_second" in values)
        check_monitor_series(idle_rates, process_series)
        assert idle_rates["net_received_packets_per_second"] == idle_rates["net_sent_packets_per_second"] == 0
        assert idle_rates["chat_message_requests_per_second"] == idle_rates["chat_message_send_requests_per_second"] == 0
        frame_count = idle_rates.get("chat_content_frames_total", 0)
        assert frame_count > 0, "no completed content frames while idle"
        publish_time = "observability_snapshot_timestamp_seconds"
        later = wait_metrics(address, metrics_port, lambda values: values.get(publish_time, 0) - idle_rates[publish_time] >= 1)
        measured_fps = (later["chat_content_frames_total"] - frame_count) / (later[publish_time] - idle_rates[publish_time])
        assert 2 <= measured_fps <= 10, f"configured 5 FPS, observed {measured_fps:.2f} FPS while idle"
        with socket.socket() as wrong_address:
            wrong_address.settimeout(3)
            assert wrong_address.connect_ex(("127.0.0.1", metrics_port)) in (10060, 10061), "unexpected metrics listener"
        with launch(base, configuration, "Client", "metrics-client", chat_port) as client:
            wait_text(client, "READY account=")
            pending_rates = {"net_received_packets_per_second", "net_sent_packets_per_second",
                             "chat_message_requests_per_second", "chat_message_send_requests_per_second"}

            def observed_directional_rates(values):
                # 수신과 송신은 서로 다른 모니터 구간에 집계될 수 있다.
                pending_rates.difference_update(name for name in tuple(pending_rates) if values.get(name, 0) > 0)
                return not pending_rates

            send(client, "metrics echo")
            try:
                wait_metrics(address, metrics_port, observed_directional_rates)
            except AssertionError as exc:
                raise AssertionError(f"positive network/chat rates not observed: {sorted(pending_rates)}") from exc
            wait_text(client, ": metrics echo")
            active = wait_metrics(address, metrics_port, lambda values: values.get("net_sessions") == 1
                                  and values.get("chat_message_send_requests_total", 0) >= 1
                                  and values.get("net_sent_packets_total", 0) >= 3)
            assert active["net_accept_completions_total"] == active["net_sessions_accepted_total"] == 1
            assert active["net_received_packets_total"] >= 3
            assert active["net_sent_packets_total"] >= 3
            assert active["net_wsasend_calls_total"] >= 1
            assert 0 <= active["net_wsasend_pending_total"] <= active["net_wsasend_calls_total"]
            assert active["chat_login_requests_total"] == 1
            assert active["chat_sector_move_requests_total"] == 1
            assert active["chat_content_updates_total"] >= 3
            assert active["chat_message_requests_total"] == active["chat_message_send_requests_total"] == 1
            check_monitor_series(active, process_series)
            send(client, "/exit")
            assert client[0].wait(timeout=8) == 0
        idle = wait_metrics(address, metrics_port, lambda values: values.get("net_sessions") == 0
                            and values.get("net_received_packets_per_second") == 0
                            and values.get("chat_message_requests_per_second") == 0
                            and values.get("chat_message_send_requests_per_second") == 0)
        assert idle["net_received_packets_total"] == active["net_received_packets_total"]
        assert idle["net_accept_completions_total"] == idle["net_sessions_accepted_total"] == 1
        assert idle["net_wsasend_calls_total"] >= active["net_wsasend_calls_total"]
        assert idle["net_wsasend_pending_total"] >= active["net_wsasend_pending_total"]
        assert idle["chat_message_requests_total"] == idle["chat_message_send_requests_total"] == 1
        send(server, "/exit")
        assert server[0].wait(timeout=8) == 0

    with socket.socket() as blocker:
        blocker.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        blocker.bind((address, metrics_port))
        blocker.listen()
        with launch(base, configuration, "Server", "metrics-conflict", chat_port, server_options=options) as server:
            wait_text(server, f"event=metrics_start_failed port={metrics_port}")
            wait_text(server, f"event=server_ready port={chat_port}")
            with launch(base, configuration, "Client", "conflict-client", chat_port) as client:
                wait_text(client, "READY account=")
                send(client, "metrics conflict echo")
                wait_text(client, ": metrics conflict echo")
                send(client, "/exit")
                assert client[0].wait(timeout=8) == 0
            send(server, "/exit")
            assert server[0].wait(timeout=8) == 0

    with launch(base, configuration, "Server", "metrics-disabled", chat_port,
                server_options={**options, "METRICS_PORT": 0}) as server:
        wait_text(server, f"event=server_ready port={chat_port}")
        assert "event=metrics_" not in server[1].read_text(encoding="utf-8")
        send(server, "/exit")
        assert server[0].wait(timeout=8) == 0

    for index, invalid in enumerate(({"METRICS_PORT": "65536"}, {"METRICS_PORT": "3ms"},
                                     {"METRICS_BIND_ADDRESS": "invalid"}, {"METRICS_BIND_ADDRESS": "0.0.0.0"})):
        with launch(base, configuration, "Server", f"metrics-invalid-{index}", chat_port,
                    server_options={**options, **invalid}) as server:
            assert server[0].wait(timeout=5) == 2
            wait_text(server, next(iter(invalid)))
    print("metrics: configured alternate loopback, live logger-collected snapshot, shutdown/port reuse, bind collision and config checks")


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    mode = sys.argv[1]
    configuration = sys.argv[2] if len(sys.argv) > 2 else "Release"
    case = {"bind": bind_failure, "chat": chat, "chat-lan": lambda base, config: chat(base, config, "Lan"),
            "log-interval": log_interval_config,
            "session-limit": session_limit_config, "server-options": server_options_config, "metrics": metrics}[mode]
    evidence = os.environ.get("MMO_SERVER_EVENT_EVIDENCE")
    if evidence:
        output = Path(evidence) / f"{configuration}-{mode}"
        output.mkdir(parents=True)
        case(output, configuration)
    else:
        with tempfile.TemporaryDirectory(prefix="mmo-server-events-", dir=ROOT / "Build") as directory:
            case(Path(directory), configuration)
