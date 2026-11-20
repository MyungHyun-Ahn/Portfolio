import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
BAT = ROOT / "Tools" / "LogSplitter" / "SplitLogs.bat"


def record(level, project, thread, session, message, ending="\n"):
    return (f"[2026-11-20T04:00:00.000Z][{level}][{project}][{thread}][{session}]"
            f"[void Test(unsigned char [4]) : 42] {message}{ending}").encode("utf-8")


class SplitLogsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="split logs ", dir=os.environ.get("TMPDIR"))
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def write(self, name, contents):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(contents)
        return path

    def run_tool(self, args, expected=0):
        env = os.environ.copy()
        env["LOG_SPLITTER_PYTHON"] = sys.executable
        command = subprocess.list2cmdline([str(BAT), *map(str, args)])
        result = subprocess.run(f'"{os.environ["COMSPEC"]}" /d /v:off /s /c "{command}"', cwd=self.root,
                                env=env, capture_output=True, encoding="utf-8", errors="replace", timeout=15)
        self.assertEqual(result.returncode, expected, (result.stdout, result.stderr))
        return result

    def test_explicit_file_combines_filters_and_preserves_bytes(self):
        one = record("Info", "ChatServer", 7, 42, "event=x | 한글 😀", "\r\n")
        two = record("Error", "ChatServer", 8, 84, "event=y | 100%", "")
        source = self.write("input file.log", one + record("Info", "Other", 7, 42, "excluded")
                            + record("Debug", "ChatServer", 7, 42, "excluded")
                            + record("Info", "ChatServer", 9, 42, "excluded")
                            + record("Info", "ChatServer", 7, 99, "excluded session_id=42") + two)
        original = source.read_bytes()
        self.run_tool([source, "--level", "info", "ERROR", "--thread-id", 7, 8,
                       "--session-id", 42, 84, "--project", "ChatServer"])
        self.assertEqual([p.read_bytes() for p in self.root.glob("Split/*/filtered.log")], [one + two])
        self.assertEqual(source.read_bytes(), original)

    def test_automatic_input_uses_start_time_then_numeric_run_and_ignores_output(self):
        old = record("Info", "ChatServer", 7, "-", "old")
        first = record("Info", "ChatServer", 7, "-", "first")
        last = record("Error", "NetworkLib", 8, 84, "last")
        source = self.write("LogFile/run-123-10/261120-040000-ChatServer.log", first + last + first)
        self.write("LogFile/run-123-9/261120-040000-ChatServer.log", old)
        self.write("LogFile/run-122-20/261120-040000-ChatServer.log", old)
        self.write("LogFile/run-999-0/261119-040000-ChatServer.log", old)
        self.write("LogFile/run-123-10/Split/split-old/991120-040000-fake.log", old)
        self.write("LogFile/run-999-1/991399-040000-fake.log", old)
        for _ in range(2):
            output = self.run_tool(["--split-by", "project", "level", "thread-id", "session-id"])
            self.assertIn(str(source), output.stdout)
        groups = list((source.parent / "Split").glob("split-*/project=*.log"))
        self.assertEqual(len(groups), 4)
        for directory in (source.parent / "Split").glob("split-*"):
            if directory.name == "split-old":
                continue
            self.assertEqual({p.name: p.read_bytes() for p in directory.glob("*.log")}, {
                "project=ChatServer__level=Info__thread-id=7__session-id=-.log": first + first,
                "project=NetworkLib__level=Error__thread-id=8__session-id=84.log": last,
            })
        self.assertEqual(source.read_bytes(), first + last + first)

    def test_missing_input_and_invalid_option_exit_nonzero(self):
        self.assertIn("LogFile", self.run_tool([], expected=1).stderr)
        self.run_tool(["--level", "verbose"], expected=2)
        self.run_tool(["--session-id", "0"], expected=2)
        self.assertEqual(list(self.root.iterdir()), [])

    def test_malformed_lines_and_no_match_are_reported(self):
        source = self.write("source.log", b"invalid line\n"
                            + record("Info", "ChatServer", 7, "-", "event=x | ok"))
        result = self.run_tool([source, "--level", "debug"])
        self.assertIn("matched=0", result.stdout)
        self.assertIn("malformed=1", result.stdout)
        self.assertFalse(list(self.root.glob("Split/*/*.log")))

    def test_project_names_cannot_escape_or_merge_output_files(self):
        lines = [record("Info", project, 7, "-", "event=x | ok") for project in ("../OUT", "chat", "Chat")]
        source = self.write("source.log", b"".join(lines))
        self.run_tool([source, "--split-by", "project"])
        groups = list(self.root.glob("Split/*/*.log"))
        self.assertCountEqual([p.read_bytes() for p in groups], lines)
        self.assertEqual(sorted(path.name for path in self.root.iterdir()), ["Split", "source.log"])

    def test_custom_log_dir_keeps_order_when_reopening_groups(self):
        first = record("Info", "ChatServer", 7, 1, "first")
        last = record("Info", "ChatServer", 7, 1, "last")
        other = [record("Info", "ChatServer", 7, session, "other") for session in range(2, 35)]
        log_dir = self.root / "Server Logs"
        source = self.write("Server Logs/run-123-0/261120-040000-ChatServer.log", first + b"".join(other) + last)
        self.run_tool(["--log-dir", log_dir, "--split-by", "session-id"])
        groups = list((source.parent / "Split").glob("split-*/*.log"))
        self.assertEqual(len(groups), 34)
        self.assertEqual(next(p for p in groups if p.name == "session-id=1.log").read_bytes(), first + last)


if __name__ == "__main__":
    unittest.main()
