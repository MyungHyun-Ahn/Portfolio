"""Run after the Development editor build: python -B test_ue_login.py ENGINE_DIR EVIDENCE_DIR."""
from pathlib import Path
import socket
import subprocess
import sys

from test_server_event_input import ROOT, launch, send, wait_text


def run(engine, evidence):
    evidence.mkdir(parents=True, exist_ok=False)
    project = ROOT / 'Client/MMO_Client/MMO_Client.uproject'
    with socket.socket() as probe:
        probe.bind(('127.0.0.1', 0))
        port = probe.getsockname()[1]
    with launch(evidence, 'Release', 'Server', 'server', port, levels=('Debug', 'Debug'),
                server_options={'IP': '127.0.0.1', 'METRICS_PORT': 0}) as server:
        wait_text(server, f'event=server_ready port={port}')
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
    run(*(Path(arg).resolve() for arg in sys.argv[1:]))
