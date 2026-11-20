"""Run the native logger on Windows ConPTY (requires pywinpty)."""

import os
from pathlib import Path
from queue import Empty, Queue
import re
import sys
from threading import Thread
from time import monotonic, sleep

from winpty import Backend, PtyProcess


RUN_TIMEOUT_SECONDS = 10
REAP_TIMEOUT_SECONDS = 2


def read_output(child, records):
    try:
        while True:
            chunk = child.read(4096)
            if chunk:
                records.put(chunk)
    except EOFError:
        records.put(None)
    except Exception as error:
        records.put(error)


def wait_for_exit(child, deadline):
    while child.isalive():
        remaining = deadline - monotonic()
        if remaining <= 0:
            raise TimeoutError("ConPTY child did not exit before the deadline")
        sleep(min(0.05, remaining))
    return child.exitstatus


def main():
    exe = Path(sys.argv[1]).resolve(strict=True)
    evidence = Path(sys.argv[2]).resolve()
    evidence.mkdir(parents=True, exist_ok=False)
    deadline = monotonic() + RUN_TIMEOUT_SECONDS
    child = PtyProcess.spawn([str(exe)], cwd=str(evidence), dimensions=(24, 240),
                             env={**os.environ, "MMO_LOGGER_CONSOLE_TEST": "1"}, backend=Backend.ConPTY)
    records = Queue()
    reader = Thread(target=read_output, args=(child, records), daemon=True)
    chunks = []
    try:
        reader.start()
        while True:
            remaining = deadline - monotonic()
            if remaining <= 0:
                raise TimeoutError("ConPTY output did not reach EOF before the deadline")
            try:
                record = records.get(timeout=remaining)
            except Empty:
                raise TimeoutError("ConPTY output did not reach EOF before the deadline") from None
            if record is None:
                break
            if isinstance(record, Exception):
                raise record
            chunks.append(record)
        status = wait_for_exit(child, deadline)
    finally:
        try:
            if child.isalive():
                child.close(force=True)
                wait_for_exit(child, monotonic() + REAP_TIMEOUT_SECONDS)
        finally:
            # 타임아웃이 발생해도 그때까지 수집한 출력을 남긴다.
            (evidence / "conpty.txt").write_bytes("".join(chunks).encode("utf-8"))
    output = "".join(chunks)
    assert status == 0, status
    assert output.count("event=attached | 한글 😀 100% %ls\r\n") == 1, ascii(output)
    files = list(evidence.glob("AsyncLoggerNative/*/run-*/*.log"))
    assert len(files) == 1 and files[0].read_bytes() == b"", files
    assert re.fullmatch(rf"[0-9]{{6}}-[0-9]{{6}}-{re.escape(exe.stem)}\.log", files[0].name), files[0]
    print("PASS: Windows ConPTY, Unicode and CRLF, Info on console only")


if __name__ == "__main__":
    main()
