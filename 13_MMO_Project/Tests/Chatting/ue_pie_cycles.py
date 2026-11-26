"""test_ue_login.py --pie가 소유한 에디터에서만 실행한다."""
import json
import os
from pathlib import Path
import re
import subprocess
import time
import traceback

import unreal


base = Path(os.environ['MMO_PIE_DIR'])
endpoint = '127.0.0.1:' + os.environ['MMO_PIE_PORT']
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
rows = []
phase = 'start'
next_check = 0


def connections():
    # 별도 접속을 만들지 않고 같은 스냅샷에서 양쪽 소유 PID를 확인한다.
    output = subprocess.check_output(['netstat', '-ano', '-p', 'tcp'], creationflags=subprocess.CREATE_NO_WINDOW)
    return [line.split() for line in output.decode('ascii', errors='replace').splitlines() if len(line.split()) == 5]


def advance():
    global phase, next_check
    cycle = len(rows) + (phase != 'closed')
    if phase == 'start':
        assert not level.is_in_play_in_editor()
        unreal.log(f'PIE_BEGIN cycle={cycle}')
        level.editor_request_begin_play()
        phase = 'login'
        return
    server = (base / 'server/stdout.txt').read_bytes().decode('utf-8', errors='replace')
    if phase == 'login':
        log = (base / 'pie.log').read_bytes()
        sessions = re.findall(r'\[(\d+)\]\[bool [^\n]+\] event=login_accepted account_no=710114\b', server)
        if len(sessions) < cycle or log.count(b'event=ue_login_accepted account_no=710114 game_thread=1') < cycle:
            return
        assert level.is_in_play_in_editor() and len(sessions) == cycle
        world = editor.get_game_world().get_path_name()
        assert 'UEDPIE_' in world and 'Lvl_TopDown' in world, world
        sockets = connections()
        peers = [peer for _, local, peer, state, pid in sockets if local == endpoint and state == 'ESTABLISHED'
                 and pid == os.environ['MMO_PIE_SERVER']]
        assert len(peers) == 1 and ['TCP', peers[0], endpoint, 'ESTABLISHED', str(os.getpid())] in sockets, peers
        rows.append(dict(cycle=cycle, pid=os.getpid(), world=world, session=sessions[-1], peer=peers[0],
                         server_pid=int(os.environ['MMO_PIE_SERVER']), endpoint=endpoint))
        unreal.log(f'PIE_STOP cycle={cycle} world={world} session={sessions[-1]} peer={peers[0]}')
        level.editor_request_end_play()
        phase = 'closed'
        return
    row = rows[-1]
    if level.is_in_play_in_editor() or not re.search(rf"\[{row['session']}\]\[void [^\n]+\] event=session_leave\b", server):
        return
    owned = {(endpoint, row['peer'], os.environ['MMO_PIE_SERVER']), (row['peer'], endpoint, str(os.getpid()))}
    if any((local, peer, pid) in owned for _, local, peer, _, pid in connections()):
        return
    unreal.log(f"PIE_CLOSED cycle={cycle} session={row['session']}")
    if cycle == 1:
        phase = 'start'
        next_check = time.monotonic() + 1
    else:
        (base / 'cycles.json').write_text(json.dumps(rows, indent=2))
        finish()


def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)


def tick(delta_seconds):
    global next_check
    if time.monotonic() < next_check:
        return
    next_check = time.monotonic() + 0.25
    try:
        if time.monotonic() > deadline:
            raise TimeoutError(f'phase={phase}, rows={rows}')
        advance()
    except Exception:
        unreal.log_error(traceback.format_exc())
        finish()


deadline = time.monotonic() + 75
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle = unreal.register_slate_post_tick_callback(tick)
