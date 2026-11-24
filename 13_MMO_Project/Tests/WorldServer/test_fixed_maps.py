import argparse
import os
from pathlib import Path
import re
import select
import socket
import struct
import subprocess
import tempfile
import time


def frame(opcode, fields, *values):
    payload = struct.pack('<H' + fields, opcode, *values)
    return struct.pack('<BHBB', 0x77, len(payload), 0, 0) + payload


def read_exact(peer, size):
    data = bytearray()
    while len(data) < size:
        chunk = peer.recv(size - len(data))
        assert chunk, 'unexpected EOF while reading a FixedMaps packet'
        data.extend(chunk)
    return data


def receive(peer):
    code, size, key, checksum = struct.unpack('<BHBB', read_exact(peer, 5))
    assert (code, key, checksum) == (0x77, 0, 0)
    payload = read_exact(peer, size)
    opcode, = struct.unpack_from('<H', payload)
    assert opcode in (2, 4, 6), f'unexpected response opcode {opcode}'
    return (opcode, *struct.unpack('<QQH', payload[2:]))


def migration_round_trip(port, wait, release, log):
    with socket.create_connection(('127.0.0.1', port), timeout=3) as peer:
        peer.sendall(frame(1, 'QI', 1, 1))
        assert receive(peer) == (2, 1, 1, 0)
        peer.sendall(frame(3, 'Q', 999) + frame(3, 'Q', 1) + frame(3, '') + frame(3, 'QB', 2, 0))
        assert [receive(peer) for _ in range(4)] == [(4, 1, 0, 1)] * 2 + [(4, 1, 0, 4)] * 2
        peer.sendall(frame(3, 'Q', 2) + frame(1, 'QI', 1, 100) + frame(1, 'QI', 2, 7))
        response = receive(peer)
        assert response == (4, 2, 0, 0), f'expected successful ResMove before buffered input; got {response}'
        assert receive(peer) == (2, 2, 0, 1)
        assert receive(peer) == (2, 2, 7, 0)
        peer.sendall(frame(3, 'Q', 1) + frame(1, 'QI', 1, 3))
        assert receive(peer) == (4, 1, 0, 0)
        assert receive(peer) == (2, 1, 4, 0)
    first = wait(rb'event=move_detached map=1 thread_id=(\d+) session_id=(\d+) character_id=(\d+)\b')
    second = wait(rb'event=move_attached map=2 thread_id=(\d+) session_id=' + first[2] + rb' character_id=' + first[3] + rb'\b')
    assert first[1] != second[1]
    wait(rb'event=move_attached map=1 thread_id=' + first[1] + rb' session_id=' + first[2] + rb' character_id=' + first[3] + rb'\b')
    wait(rb'event=map_connection_removed map=1 thread_id=' + first[1] + rb' session_id=' + first[2] + rb'\b')
    assert log().count(b'event=move_detached ') == log().count(b'event=move_attached ') == 2


def assert_no_response(peer):
    assert not select.select([peer], [], [], 0.05)[0], 'packet arrived before the test gate released'


def delayed_change(port, wait, release, log):
    with socket.create_connection(('127.0.0.1', port), timeout=3) as peer:
        peer.sendall(frame(5, 'QI', 1, 37) + frame(3, 'Q', 2) + frame(1, 'QI', 1, 99) + frame(1, 'QI', 2, 8))
        pending = wait(rb'event=move_waiting_for_change map=1 thread_id=(\d+) session_id=(\d+) character_id=(\d+)\b')
        assert_no_response(peer)
        assert b'event=move_detached' not in log()

        release('release-change')
        assert receive(peer) == (6, 1, 37, 0)
        completed = wait(rb'event=character_change_completed map=1 thread_id=' + pending[1]
                         + rb' session_id=' + pending[2] + rb' character_id=' + pending[3] + rb' value=37\b')
        detached = wait(rb'event=move_detached map=1 thread_id=' + pending[1]
                        + rb' session_id=' + pending[2] + rb' character_id=' + pending[3] + rb'\b')
        assert pending.start() < completed.start() < detached.start()
        assert_no_response(peer)
        assert b'event=move_attached' not in log()

        release('release-arrival')
        assert receive(peer) == (4, 2, 37, 0)
        assert receive(peer) == (2, 2, 0, 1)
        assert receive(peer) == (2, 2, 8, 0)
        attached = wait(rb'event=move_attached map=2 thread_id=(\d+) session_id='
                        + pending[2] + rb' character_id=' + pending[3] + rb'\b')
        assert attached[1] != pending[1]
    wait(rb'event=map_connection_removed map=2 thread_id=' + attached[1] + rb' session_id=' + pending[2] + rb'\b')
    assert log().count(b'event=character_change_completed ') == 1


def close_order(port, wait, release, log):
    with socket.create_connection(('127.0.0.1', port), timeout=3) as peer:
        peer.sendall(frame(3, 'Q', 2))
        wait(rb'event=move_detached map=1\b')
        release('close-map-2')
        wait(rb'event=move_close_deferred map=2\b')
        assert_no_response(peer)
        assert b'event=move_admission_closed' not in log()

        release('release-arrival')
        assert receive(peer) == (4, 2, 0, 0)
        peer.sendall(frame(3, 'Q', 1))
        assert receive(peer) == (4, 1, 0, 0)
        wait(rb'event=move_admission_closed map=2\b')

        peer.sendall(frame(3, 'Q', 2) + frame(1, 'QI', 2, 99) + frame(1, 'QI', 1, 2))
        assert receive(peer) == (4, 1, 0, 2)
        assert receive(peer) == (2, 1, 0, 1)
        assert receive(peer) == (2, 1, 2, 0)
        assert log().count(b'event=move_detached ') == 2


def disconnect_in_transit(port, wait, release, log):
    with socket.create_connection(('127.0.0.1', port), timeout=3) as peer:
        peer.sendall(frame(3, 'Q', 2) + frame(1, 'QI', 2, 99))
        detached = wait(rb'event=move_detached map=1 thread_id=(\d+) session_id=(\d+) character_id=(\d+)\b')
        wait(rb'event=move_published session_id=' + detached[2] + rb'\b')
        assert_no_response(peer)
    wait(rb'event=move_connection_left session_id=' + detached[2] + rb'\b')
    release('release-arrival')
    discarded = wait(rb'event=move_discarded map=2 thread_id=(\d+) session_id='
                     + detached[2] + rb' character_id=' + detached[3] + rb'\b')
    assert discarded[1] != detached[1]

    with socket.create_connection(('127.0.0.1', port), timeout=3) as peer:
        peer.sendall(frame(1, 'QI', 2, 1))
        assert receive(peer) == (2, 2, 1, 0)
        peer.sendall(frame(3, 'Q', 1))
        assert receive(peer) == (4, 1, 0, 0)
        peer.sendall(frame(3, 'Q', 2))
        assert receive(peer) == (4, 2, 0, 0)
    assert log().count(b'event=move_discarded ') == 1
    assert not re.search(rb'event=(?:move_attached|map_connection_removed) [^\r\n]*session_id=' + detached[2] + rb'\b', log())


def busy_destination(port, wait, release, log):
    with socket.create_connection(('127.0.0.1', port), timeout=3) as first:
        first.sendall(frame(3, 'Q', 2))
        wait(rb'event=move_published session_id=\d+\b')
        with socket.create_connection(('127.0.0.1', port), timeout=3) as resident:
            resident.sendall(frame(1, 'QI', 2, 4))
            assert receive(resident) == (2, 2, 4, 0)
            with socket.create_connection(('127.0.0.1', port), timeout=3) as contender:
                contender.sendall(frame(3, 'Q', 2) + frame(1, 'QI', 1, 8))
                assert receive(contender) == (4, 1, 0, 3)
                assert receive(contender) == (2, 1, 8, 0)
                assert log().count(b'event=move_detached ') == 1
                assert_no_response(first)
                release('release-arrival')
                assert receive(first) == (4, 2, 0, 0)
                contender.sendall(frame(3, 'Q', 2))
                assert receive(contender) == (4, 2, 0, 0)


def destroy_empty_map(port, wait, release, log):
    release('hold-before-move-publish')
    release('post-change-after-destroy')
    with socket.create_connection(('127.0.0.1', port), timeout=3) as source:
        source.sendall(frame(1, 'QI', 1, 1))
        assert receive(source) == (2, 1, 1, 0)
        with socket.create_connection(('127.0.0.1', port), timeout=3) as resident:
            resident.sendall(frame(1, 'QI', 2, 7))
            assert receive(resident) == (2, 2, 7, 0)
            entered = wait(rb'event=map_connection_entered map=2 thread_id=([1-9]\d*) session_id=([1-9]\d*)\b')
        wait(rb'event=map_connection_removed map=2 thread_id=' + entered[1] + rb' session_id=' + entered[2] + rb'\b')

        source.sendall(frame(3, 'Q', 2))
        reserved = wait(rb'event=move_publish_held target=2 session_id=([1-9]\d*)\b')
        deferred = wait(rb'event=map_destroy_deferred map=2\b')
        assert reserved.start() < deferred.start()
        assert b'event=move_published ' not in log()
        assert b'event=move_detached ' not in log()
        assert b'event=map_destroyed ' not in log()
        assert_no_response(source)

        release('release-move-publish')
        published = wait(rb'event=move_published session_id=' + reserved[1] + rb'\b')
        assert deferred.start() < published.start()
        release('release-arrival')
        assert receive(source) == (4, 2, 0, 0)
        source.sendall(frame(1, 'QI', 2, 3))
        assert receive(source) == (2, 2, 10, 0)
        attached = wait(rb'event=move_attached map=2 thread_id=' + entered[1] + rb' session_id=' + reserved[1] + rb'\b')
        assert deferred.start() < attached.start()
        assert b'event=map_destroyed ' not in log()

        departure_started = time.monotonic()
        source.sendall(frame(3, 'Q', 1))
        assert receive(source) == (4, 1, 0, 0)
        departed = wait(rb'event=move_detached map=2 thread_id=' + entered[1] + rb' session_id=' + reserved[1] + rb'\b')
        destroyed = wait(rb'event=map_destroyed map=2 thread_id=' + entered[1] + rb'\b')
        skipped = wait(rb'event=map_late_change_ignored map=2 thread_id=' + entered[1] + rb'\b')
        assert departed.start() < destroyed.start() < skipped.start()
        assert log().count(b'event=map_late_change_ignored ') == 1
        assert time.monotonic() - departure_started >= DESTROY_IDLE_MILLISECONDS / 1000

        source.sendall(frame(3, 'Q', 2) + frame(1, 'QI', 1, 2))
        assert receive(source) == (4, 1, 0, 2)
        assert receive(source) == (2, 1, 3, 0)

        with socket.create_connection(('127.0.0.1', port), timeout=3) as other:
            other.sendall(frame(1, 'QI', 1, 4))
            assert receive(other) == (2, 1, 7, 0)
            with socket.create_connection(('127.0.0.1', port), timeout=3) as rejected:
                rejection = wait(rb'event=map_admission_rejected map=2 session_id=([1-9]\d*)\b')
                try:
                    assert rejected.recv(1) == b'', 'retired map accepted an initial connection'
                except ConnectionResetError:
                    pass
                assert not re.search(rb'event=map_connection_entered [^\r\n]*session_id=' + rejection[1] + rb'\b', log())
        assert log().count(b'event=map_destroyed ') == 1
        assert log().count(b'event=move_detached ') == 2


DESTROY_IDLE_MILLISECONDS = 1000


def configure(directory, role, port, controls=False, destroy_idle_milliseconds=0):
    (directory / 'FixedMaps.conf').write_text(
        f'FixedMaps:\n-Role={role};\n-Port={port};\n-Controls={int(controls)};\n'
        f'-DestroyIdleMilliseconds={destroy_idle_milliseconds};\n', encoding='utf-16')


def wait_for_server_log(server, log_path, pattern, timeout_seconds):
    deadline = time.monotonic() + timeout_seconds
    while time.monotonic() < deadline:
        log = log_path.read_bytes()
        match = re.search(pattern, log)
        if match:
            return match
        if server.poll() is not None:
            raise AssertionError(f'missing {pattern!r}; server exit={server.returncode}; stdout={log!r}')
        time.sleep(0.02)
    raise AssertionError(f'missing {pattern!r} before deadline; stdout={log_path.read_bytes()!r}')


def run(executable, scenario=None):
    with tempfile.TemporaryDirectory(prefix='fixed-maps-', dir=os.environ.get('TMPDIR')) as temporary:
        root = Path(temporary)
        server_dir = root / 'server'
        client_dir = root / 'client'
        server_dir.mkdir()
        client_dir.mkdir()
        with socket.socket() as probe:
            probe.bind(('127.0.0.1', 0))
            port = probe.getsockname()[1]
        configure(server_dir, 'Server', port, controls=scenario not in (None, migration_round_trip),
                  destroy_idle_milliseconds=DESTROY_IDLE_MILLISECONDS if scenario is destroy_empty_map else 0)
        log_path = server_dir / 'stdout'
        with log_path.open('wb') as output, subprocess.Popen(
            [executable], cwd=server_dir, stdin=subprocess.DEVNULL,
            stdout=output, stderr=subprocess.STDOUT
        ) as server:
            try:
                wait_for_server_log(server, log_path, rb'event=fixed_maps_ready port=' + str(port).encode() + rb'\b', 10)
                if scenario:
                    scenario(port, lambda pattern: wait_for_server_log(server, log_path, pattern, 3),
                             lambda name: (server_dir / name).touch(), log_path.read_bytes)
                    assert server.poll() is None, (server.returncode, log_path.read_bytes())
                    print(f'event=fixed_maps_migration_pass case={scenario.__name__}')
                    return
                configure(client_dir, 'Client', port)
                result = subprocess.run([executable], cwd=client_dir, capture_output=True, timeout=20)
                expected = b'event=fixed_maps_pass first=1,2,7,7,10 second=10,10,14,14,14,14,14 reconnected=15'
                assert result.returncode == 0 and expected in result.stdout, (result, log_path.read_bytes())
                workers = re.findall(
                    rb'event=map_connection_entered map=([12]) thread_id=([1-9]\d*) session_id=([1-9]\d*)\b',
                    log_path.read_bytes(),
                )
                assert [map_id for map_id, _, _ in workers] == [b'1', b'2', b'1'], workers
                assert workers[0][1] != workers[1][1] and workers[0][1] == workers[2][1], workers
                for map_id, thread_id, session_id in workers:
                    wait_for_server_log(server, log_path, rb'event=map_connection_removed map=' + map_id
                                        + rb' thread_id=' + thread_id + rb' session_id=' + session_id + rb'\b', 3)
                assert server.poll() is None, (server.returncode, log_path.read_bytes())
                print(expected.decode())
                print('event=fixed_maps_worker_pass distinct_workers=2 reconnected_same_worker=1')
                print('event=fixed_maps_disconnect_pass removed_maps=1,2 removed_on_owner=3')
            finally:
                if server.poll() is None:
                    server.terminate()
                server.wait(timeout=5)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('executable', type=lambda path: Path(path).resolve(strict=True))
    scenarios = {case.__name__: case for case in (
        migration_round_trip, delayed_change, close_order, disconnect_in_transit, busy_destination, destroy_empty_map)}
    parser.add_argument('--case', choices=scenarios)
    args = parser.parse_args()
    if args.case:
        run(args.executable, scenarios[args.case])
    else:
        run(args.executable)
        for case in scenarios.values():
            run(args.executable, case)
