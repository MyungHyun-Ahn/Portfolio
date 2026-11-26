"""Build Chatting.sln x64 first; run python -B test_dummy_runner.py [Release|Debug]."""

from contextlib import contextmanager
from concurrent.futures import ThreadPoolExecutor
import json
from os import environ
from pathlib import Path
import select
import shutil
import socket
import subprocess
import sys
import tempfile
import time

from test_server_event_input import ROOT, launch, send, wait_text


@contextmanager
def dummy(base, configuration, label, port, *, has_config=True, encoding="utf-16", config_text=None, **overrides):
    folder = base / label
    folder.mkdir()
    exe = folder / "DummyClient.exe"
    shutil.copyfile(ROOT / "Build" / configuration / "DummyClient" / exe.name, exe)
    network = {"WORKER_COUNT": 1, "RECV_BUFFER_BYTES": 4096, "PACKET_MODE": "Net"}
    settings = {"IP": "127.0.0.1", "PORT": port, "ACCOUNT_BASE": 710001,
                "RUN_DURATION_MS": 1100, "ACTION_INTERVAL_MS": 40}
    for key, value in overrides.items():
        (network if key in network else settings)[key] = value
    if has_config:
        text = "DummyClient:\n" + "".join(f"    - {key} = {value};\n" for key, value in settings.items())
        text += "\nClientNetwork:\n" + "".join(f"    - {key} = {value};\n" for key, value in network.items() if value is not None)
        (folder / "DummyClientConfig.conf").write_text(text if config_text is None else config_text, encoding=encoding)
    stdout = folder / "stdout.txt"
    with stdout.open("wb") as output, subprocess.Popen(
        [str(exe)], cwd=base, stdin=subprocess.DEVNULL, stdout=output, stderr=subprocess.STDOUT
    ) as process:
        try:
            yield process, stdout
        finally:
            if process.poll() is None:
                process.kill()
            process.wait(timeout=8)


def summary(child, expected_exit, timeout=15):
    process, stdout = child
    exit_code = process.wait(timeout=timeout)
    assert exit_code == expected_exit, (exit_code, stdout.read_bytes().decode("utf-8", errors="replace"))
    reports = list(stdout.parent.glob("DummyReports/*/RunSummary.json"))
    assert len(reports) == 1, reports
    result = json.loads(reports[0].read_text(encoding="utf-8"))
    assert result["exit_code"] == exit_code, result
    return result


def relay_until_dummy_closes(front, server_port):
    with socket.create_connection(("127.0.0.1", server_port), timeout=5) as back:
        front.settimeout(5)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            readable, _, _ = select.select([front, back], [], [], deadline - time.monotonic())
            for origin in readable:
                try:
                    data = origin.recv(4096)
                except ConnectionResetError:
                    data = b""
                if not data:
                    assert origin is front, "test server closed before DummyClient"
                    return
                (back if origin is front else front).sendall(data)
        raise AssertionError("DummyClient did not close the relayed connection")


def paced_connect(base, configuration, server_port, server):
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        listener.listen(5)
        listener.settimeout(4)
        with dummy(base, configuration, "paced-connect", listener.getsockname()[1], CLIENT_COUNT=5,
                   APP_WORKER_COUNT=3, CONNECT_INTERVAL_MS=700, CONNECT_ATTEMPTS_PER_INTERVAL=2,
                   RUN_DURATION_MS=2300, ACTION_INTERVAL_MS=5000, HEARTBEAT_INTERVAL_MS=600,
                   SECTOR_MOVE_WEIGHT=0, MESSAGE_WEIGHT=1) as child:
            def relay_client(front):
                with front:
                    relay_until_dummy_closes(front, server_port)

            with ThreadPoolExecutor(max_workers=5) as relay:
                accepted = []
                for count in (2, 2, 1):
                    for _ in range(count):
                        accepted.append(relay.submit(relay_client, listener.accept()[0]))
                    if len(accepted) < 5:
                        assert not select.select([listener], [], [], 0.35)[0], "too many connects in one interval"
                    if len(accepted) == 2:
                        # 후속 접속은 아직 서버에 전달하지 않았다. 첫 배치만으로 채팅이 처리되어야 한다.
                        wait_text(server, "event=chat_processed")
                for operation in accepted:
                    operation.result(timeout=8)
            result = summary(child, 0)
    assert result["settings"]["connect_interval_ms"] == 700, result
    assert result["settings"]["connect_attempts_per_interval"] == 2, result
    assert result["stages"]["connect_requested"] == result["stages"]["ready"] == result["stages"]["terminal"] == 5, result
    actions = result["actions"]
    assert actions["message_submitted"] == actions["message_completed"] == 5 and actions["heartbeat_submitted"] > 0, result
    assert result["end_unix_ms"] - result["start_unix_ms"] >= 2250, result
    print(f"{configuration}/paced-connect: 2/2/1 TCP batches; first chat before all Ready, five self echoes")


def cutoff_during_initial_connect(base, configuration, server_port):
    with dummy(base, configuration, "startup-cutoff", server_port, CLIENT_COUNT=2, APP_WORKER_COUNT=2,
               CONNECT_INTERVAL_MS=5000, CONNECT_ATTEMPTS_PER_INTERVAL=1, RUN_DURATION_MS=700,
               ACTION_INTERVAL_MS=5000, SECTOR_MOVE_WEIGHT=0, MESSAGE_WEIGHT=1) as child:
        result = summary(child, 0, timeout=3)
    stages = result["stages"]
    assert stages["connect_requested"] == stages["ready"] == stages["terminal"] == 1, result
    assert result["end_unix_ms"] - result["start_unix_ms"] >= 650, result
    assert result["actions"]["message_completed"] == 1, result
    print(f"{configuration}/startup-cutoff: stopped after one Ready, before the next connection batch")


def drain_mid_reconnect(base, configuration, server_port):
    # 두 워커의 두 번째 TCP 접속을 받아 두되, cutoff가 관측될 때까지 로그인은 서버에 전달하지 않는다.
    with socket.socket() as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        listener.bind(("127.0.0.1", 0))
        listener.listen(2)
        listener.settimeout(5)
        with dummy(base, configuration, "drain-reconnect", listener.getsockname()[1], CLIENT_COUNT=2,
                   APP_WORKER_COUNT=2, WORKER_COUNT=2, ACCOUNT_BASE=9223372036854775806,
                   CONNECT_INTERVAL_MS=700, CONNECT_ATTEMPTS_PER_INTERVAL=2,
                   RUN_DURATION_MS=1500, ACTION_INTERVAL_MS=100,
                   SECTOR_MOVE_WEIGHT=1, MESSAGE_WEIGHT=0, RECONNECT_PROBABILITY_BPS=10000) as child:
            with ThreadPoolExecutor(max_workers=2) as relay:
                for is_reconnect in (False, True):
                    if is_reconnect:
                        assert not select.select([listener], [], [], 0.2)[0], "reconnect bypassed the shared connect budget"
                    with listener.accept()[0] as front_a, listener.accept()[0] as front_b:
                        if is_reconnect:
                            for front in (front_a, front_b):
                                front.settimeout(2)
                                assert front.recv(1, socket.MSG_PEEK), "no buffered reconnect login traffic"
                            wait_text(child, "event=dummy_draining")
                            assert child[0].poll() is None
                        pending = [relay.submit(relay_until_dummy_closes, front, server_port) for front in (front_a, front_b)]
                        for operation in pending:
                            operation.result(timeout=8)
            result = summary(child, 0)
            listener.setblocking(False)
            assert not select.select([listener], [], [], 0)[0], "additional connection submitted after cutoff"
    stages = result["stages"]
    assert stages["connect_requested"] == stages["ready"] == stages["terminal"] == 4, result
    assert result["reconnects"] == {"requested": 2, "started": 2, "ready": 2}, result
    assert result["actions"]["move_submitted"] == result["actions"]["move_completed"] == 2, result
    assert len(result["app_workers"]) == 2, result
    assert all(worker["clients"] == worker["first_ready"] == worker["actions_completed"] == 1 and worker["terminals"] == 2
               for worker in result["app_workers"]), result
    assert result["failure_count"] == 0, result
    print(f"{configuration}/drain-reconnect: two workers, two buffered logins released after cutoff, four terminals, no new work")


def loopback(base, configuration, long_heartbeat):
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
    with launch(base, configuration, "Server", "server", port, levels=("Debug", "Debug")) as server:
        wait_text(server, f"event=server_ready port={port}")
        paced_connect(base, configuration, port, server)
        cutoff_during_initial_connect(base, configuration, port)
        start = time.monotonic()
        with dummy(base, configuration, "message-only", port, WORKER_COUNT=2, APP_WORKER_COUNT=2,
                   SECTOR_MOVE_WEIGHT=0, MESSAGE_WEIGHT=1) as child:
            result = summary(child, 0)
        assert time.monotonic() - start >= 1.1, result
        stages, actions = result["stages"], result["actions"]
        assert result["settings"]["client_count"] == 2 and stages["ready"] == stages["terminal"] == 2, result
        assert result["settings"]["workers"] == result["settings"]["app_worker_count"] == 2, result
        workers = result["app_workers"]
        assert len(workers) == len({worker["thread_id"] for worker in workers}) == 2, result
        assert all(worker["clients"] == worker["first_ready"] == worker["terminals"] == 1
                   and worker["actions_completed"] > 0 for worker in workers), result
        assert actions["message_submitted"] == actions["message_completed"] > 2 and actions["peer_received"] > 0, result
        rtt = result["message_rtt_ms"]
        assert rtt["count"] == actions["message_completed"], result
        assert 0 <= rtt["min"] <= rtt["mean"] <= rtt["max"] > 0, result
        assert rtt["min"] <= rtt["p50"] <= rtt["p95"] <= rtt["p99"] <= rtt["max"], result
        assert actions["move_submitted"] == actions["move_completed"] == result["reconnects"]["requested"] == 0, result
        assert result["failure_count"] == 0, result
        print(f"{configuration}/message-only: {actions['message_completed']} self echoes, {actions['peer_received']} peer receives")

        with dummy(base, configuration, "move-reconnect", port, CLIENT_COUNT=4, APP_WORKER_COUNT=2, WORKER_COUNT=3, SECTOR_MOVE_WEIGHT=1, MESSAGE_WEIGHT=0,
                   RECONNECT_PROBABILITY_BPS=10000) as child:
            result = summary(child, 0)
        stages, actions, reconnects = result["stages"], result["actions"], result["reconnects"]
        assert result["settings"]["client_count"] == 4 and reconnects["ready"] >= 4, result
        assert stages["ready"] == 4 + reconnects["ready"] and stages["connect_requested"] == stages["terminal"], result
        assert reconnects["ready"] <= reconnects["started"] <= reconnects["requested"] <= actions["move_completed"], result
        assert actions["move_submitted"] == actions["move_completed"] and actions["message_submitted"] == 0, result
        assert result["message_rtt_ms"] == dict(count=0, min=None, mean=None, p50=None, p95=None, p99=None, max=None), result
        assert result["failure_count"] == 0, result
        print(f"{configuration}/move-reconnect: {actions['move_completed']} moves, {reconnects['ready']} reconnects Ready")

        drain_mid_reconnect(base, configuration, port)

        if long_heartbeat:
            with dummy(base, configuration, "heartbeat", port, CLIENT_COUNT=1, APP_WORKER_COUNT=2, RUN_DURATION_MS=42000,
                       ACTION_INTERVAL_MS=41000, HEARTBEAT_INTERVAL_MS=1000,
                       SECTOR_MOVE_WEIGHT=0, MESSAGE_WEIGHT=1) as child:
                result = summary(child, 0, timeout=55)
            assert result["actions"]["heartbeat_submitted"] >= 35 and result["actions"]["message_completed"] == 2, result
            assert result["stages"]["ready"] == result["stages"]["terminal"] == 1, result
            assert len(result["app_workers"]) == 1, result
            print(f"{configuration}/heartbeat: {result['actions']['heartbeat_submitted']} submissions; action after 41 s")

        assert server[0].poll() is None
        send(server, "/exit")
        assert server[0].wait(timeout=8) == 0


def failures(base, configuration):
    with socket.socket() as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        listener.bind(("127.0.0.2", 0))
        port = listener.getsockname()[1]
        with dummy(base, configuration, "refused", port, IP="127.0.0.2", CLIENT_COUNT=257, APP_WORKER_COUNT=2,
                   CONNECT_INTERVAL_MS=10000, CONNECT_ATTEMPTS_PER_INTERVAL=32, RUN_DURATION_MS=7000) as child:
            result = summary(child, 1)
        assert 1 <= result["stages"]["connect_requested"] == result["stages"]["terminal"] <= 32, result
        assert any(f["reason"] == "connect_failed" and f["error"] in (1225, 10061) for f in result["failures"]), result
        assert len(result["failures"]) == min(result["failure_count"], 16), result
        listener.listen(4)
        with dummy(base, configuration, "login-timeout", port, IP="127.0.0.2", LOGIN_TIMEOUT_MS=150) as child:
            result = summary(child, 1)
        assert result["stages"]["login_pending"] == result["stages"]["terminal"] == 2, result
        assert result["failures"][0]["reason"] == "login_timeout", result
        print(f"{configuration}/failure: native refusal stopped initial batches; alternate IP login timeout")

    with dummy(base, configuration, "unavailable-storage", port, CLIENT_COUNT=9223372036854775807,
               ACCOUNT_BASE=1) as child:
        result = summary(child, 1)
    assert result["stages"]["connect_requested"] == 0, result
    assert [failure["reason"] for failure in result["failures"]] == ["client_storage_size_unavailable"], result

    with dummy(base, configuration, "parse-failed", port, config_text="// No entries\n") as child:
        result = summary(child, 2)
    assert result["stages"]["connect_requested"] == 0, result
    assert [failure["reason"] for failure in result["failures"]] == ["config_parse_failed"], result

    for index, (key, value, reason) in enumerate((
        ("WORKER_COUNT", None, "config_network_limits_invalid"),
        ("RECV_BUFFER_BYTES", None, "config_network_limits_invalid"),
        ("PACKET_MODE", None, "config_packet_mode_invalid"),
        ("PACKET_MODE", "Invalid", "config_packet_mode_invalid"),
    )):
        with dummy(base, configuration, f"network-invalid-{index}", port, **{key: value}) as child:
            wait_text(child, f"ClientNetwork: {key} ")
            result = summary(child, 2)
        assert result["stages"]["connect_requested"] == 0, result
        assert [failure["reason"] for failure in result["failures"]] == [reason], result

    for label, overrides in (
        ("missing", {}), ("wrong-encoding", {"encoding": "utf-8"}), ("legacy", {"SCENARIO": "smoke"}),
        ("external", {"IP": "192.0.2.1"}),
        ("invalid-ip", {"IP": "127.0.0.256"}), ("zero-count", {"CLIENT_COUNT": 0}),
        ("negative-count", {"CLIENT_COUNT": -1}), ("malformed-count", {"CLIENT_COUNT": "4clients"}),
        ("count-overflow", {"CLIENT_COUNT": 9223372036854775808}),
        ("account-overflow", {"ACCOUNT_BASE": 9223372036854775807}),
        ("time", {"RUN_DURATION_MS": 0}), ("interval", {"ACTION_INTERVAL_MS": "4ms"}),
        ("connect-interval-zero", {"CONNECT_INTERVAL_MS": 0}), ("connect-interval-text", {"CONNECT_INTERVAL_MS": "1ms"}),
        ("connect-attempts-zero", {"CONNECT_ATTEMPTS_PER_INTERVAL": 0}),
        ("connect-attempts-overflow", {"CONNECT_ATTEMPTS_PER_INTERVAL": 4294967296}),
        ("weight", {"MESSAGE_WEIGHT": 0, "SECTOR_MOVE_WEIGHT": 0}), ("negative-weight", {"MESSAGE_WEIGHT": -1}),
        ("app-workers-zero", {"APP_WORKER_COUNT": 0}), ("app-workers-limit", {"APP_WORKER_COUNT": 17}),
        ("probability", {"RECONNECT_PROBABILITY_BPS": 10001}), ("heartbeat-limit", {"HEARTBEAT_INTERVAL_MS": 40000}),
    ):
        with dummy(base, configuration, label, port, has_config=label != "missing", **overrides) as child:
            result = summary(child, 2)
        assert result["stages"]["connect_requested"] == 0 and result["failure_count"] == 1, result
        if label.startswith("connect-"):
            assert result["failures"][0]["reason"] == "config_connect_schedule_invalid", result
    print(f"{configuration}/config: invalid or missing inputs rejected before connect")


def run(base, configuration, long_heartbeat):
    loopback(base, configuration, long_heartbeat)
    failures(base, configuration)


if __name__ == "__main__":
    configuration = sys.argv[1] if len(sys.argv) > 1 else "Release"
    assert configuration in ("Release", "Debug")
    evidence = environ.get("MMO_DUMMY_RUNNER_EVIDENCE")
    if evidence:
        base = Path(evidence).resolve() / configuration
        base.mkdir(parents=True, exist_ok=True)
        run(base, configuration, "--long-heartbeat" in sys.argv)
    else:
        with tempfile.TemporaryDirectory(prefix="mmo-dummy-runner-", dir=ROOT / "Build") as directory:
            run(Path(directory), configuration, "--long-heartbeat" in sys.argv)
