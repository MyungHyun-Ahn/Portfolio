import argparse
import os
from pathlib import Path
import re
import socket
import subprocess
import tempfile
import time


def configure(directory, role, port):
    (directory / 'FixedMaps.conf').write_text(
        f'FixedMaps:\n-Role={role};\n-Port={port};\n', encoding='utf-16')


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


def run(executable):
    with tempfile.TemporaryDirectory(prefix='fixed-maps-', dir=os.environ.get('TMPDIR')) as temporary:
        root = Path(temporary)
        server_dir = root / 'server'
        client_dir = root / 'client'
        server_dir.mkdir()
        client_dir.mkdir()
        with socket.socket() as probe:
            probe.bind(('127.0.0.1', 0))
            port = probe.getsockname()[1]
        configure(server_dir, 'Server', port)
        log_path = server_dir / 'stdout'
        with log_path.open('wb') as output, subprocess.Popen(
            [executable], cwd=server_dir, stdin=subprocess.DEVNULL,
            stdout=output, stderr=subprocess.STDOUT
        ) as server:
            try:
                wait_for_server_log(server, log_path, rb'event=fixed_maps_ready port=' + str(port).encode() + rb'\b', 10)
                configure(client_dir, 'Client', port)
                result = subprocess.run([executable], cwd=client_dir, capture_output=True, timeout=20)
                expected = b'event=fixed_maps_pass first=1,2,7,7,10 second=10,10,14,14 reconnected=15'
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
    run(parser.parse_args().executable)
