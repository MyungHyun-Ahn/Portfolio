"""Run after the Development editor build: python -B test_ue_login.py ENGINE_DIR EVIDENCE_DIR."""
from pathlib import Path
import socket
import subprocess
import sys

from test_server_event_input import ROOT, launch, send, wait_text


def run_pie(engine, project, evidence, port, server):
    import json
    import os
    import re

    log = evidence / 'pie.log'
    script = Path(__file__).with_name('ue_pie_cycles.py').resolve()
    args = [engine / 'Binaries/Win64/UnrealEditor-Cmd.exe', project, '/Game/TopDown/Lvl_TopDown',
            '-nullrhi', '-nosound', '-unattended', '-nop4', '-nosplash', '-NoRestoreOpenAssetTabs',
            '-EnablePlugins=PythonScriptPlugin', '-DisablePlugins=ModelContextProtocol,EditorToolset',
            '-forcelogflush', f'-UserDir={evidence / "user"}', f'-abslog={log}', f'-ExecutePythonScript={script}',
            '-MMOChatLogin', f'-MMOChatPort={port}', '-MMOChatAccount=710114']
    env = dict(os.environ, MMO_PIE_DIR=str(evidence), MMO_PIE_PORT=str(port), MMO_PIE_SERVER=str(server[0].pid))
    with subprocess.Popen(args, env=env, stdout=subprocess.DEVNULL) as editor:
        try:
            assert editor.wait(timeout=180) == 0, log
        finally:
            if editor.poll() is None:
                editor.kill()
    rows = json.loads((evidence / 'cycles.json').read_text())
    assert [row['cycle'] for row in rows] == [1, 2] and all(row['pid'] == editor.pid for row in rows), rows
    assert len({row['peer'] for row in rows}) == len({row['session'] for row in rows}) == 2, rows
    lines = log.read_text(encoding='utf-8-sig').splitlines()
    observed = [line.split('event=', 1)[1].split(' |')[0] for line in lines if 'LogChatLogin:' in line and 'event=' in line]
    pair = ['ue_login_submitted account_no=710114', 'ue_login_accepted account_no=710114 game_thread=1']
    assert observed == pair * 2, observed
    for row in rows:
        start = next(i for i, line in enumerate(lines) if f"PIE_BEGIN cycle={row['cycle']}" in line)
        stop = next(i for i, line in enumerate(lines) if f"PIE_STOP cycle={row['cycle']}" in line)
        assert [line.split('event=', 1)[1].split(' |')[0] for line in lines[start:stop]
                if 'LogChatLogin:' in line and 'event=' in line] == pair, row
    send(server, '/exit')
    assert server[0].wait(timeout=10) == 0
    peer_log = server[1].read_text(encoding='utf-8')
    for name in ('session_accepted', 'session_enter', 'login_accepted', 'session_leave'):
        ids = re.findall(r'\[(\d+)\]\[(?:void|bool) [^\n]*?\] event=' + name + r'\b', peer_log)
        assert ids == [row['session'] for row in rows], (name, ids, rows)
    print(f'PASS PIE editor={editor.pid} server={server[0].pid} port={port} cycles={rows} evidence={evidence}')


def run(engine, evidence, pie=False):
    evidence.mkdir(parents=True, exist_ok=False)
    project = ROOT / 'Client/MMO_Client/MMO_Client.uproject'
    with socket.socket() as probe:
        probe.bind(('127.0.0.1', 0))
        port = probe.getsockname()[1]
    with launch(evidence, 'Release', 'Server', 'server', port, levels=('Debug', 'Debug'),
                server_options={'IP': '127.0.0.1', 'METRICS_PORT': 0}) as server:
        wait_text(server, f'event=server_ready port={port}')
        if pie:
            return run_pie(engine, project, evidence, port, server)
        for enabled in (False, True):
            log = evidence / ('login.log' if enabled else 'offline.log')
            args = [engine / 'Binaries/Win64/UnrealEditor-Cmd.exe', project, '/Game/TopDown/Lvl_TopDown',
                    '-game', '-nullrhi', '-nosound', '-unattended', '-nop4', '-nosplash',
                    '-seconds=8', f'-abslog={log}', f'-MMOChatPort={port}', '-MMOChatAccount=710113']
            if enabled:
                args.append('-MMOChatLogin')
            subprocess.run(args, check=True, timeout=120, stdout=subprocess.DEVNULL)
            text = log.read_text(encoding='utf-8-sig')
            assert 'Bringing World /Game/TopDown/Lvl_TopDown' in text, log
            events = ['event=ue_login_submitted account_no=710113',
                      'event=ue_login_accepted account_no=710113 game_thread=1']
            for token in events:
                assert text.count(token) == int(enabled), (log, token, text.count(token))
            if enabled:
                wait_text(server, 'event=login_accepted account_no=710113')
            server_text = server[1].read_text(encoding='utf-8')
            for token in ('event=session_accepted', 'event=login_accepted account_no=710113'):
                assert server_text.count(token) == int(enabled), (server[1], token, server_text.count(token))
        send(server, '/exit')
        assert server[0].wait(timeout=10) == 0
    print(f'PASS offline=0 login=1 game_thread=1 account=710113 port={port} evidence={evidence}')


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("engine", type=Path)
    parser.add_argument("evidence", type=Path)
    parser.add_argument("--pie", action="store_true", help="같은 에디터에서 PIE 로그인과 Stop을 두 번 검증")
    args = parser.parse_args()
    run(args.engine.resolve(), args.evidence.resolve(), args.pie)
