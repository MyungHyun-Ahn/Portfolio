import argparse
from collections import OrderedDict
from contextlib import ExitStack
from datetime import datetime
from hashlib import sha256
from pathlib import Path
import re
import shutil
import sys
import tempfile
from urllib.parse import quote


FIELDS = ("level", "project", "thread-id", "session-id")
LEVELS = ("Debug", "Info", "Error")
RUN_NAME = re.compile(r"run-([0-9]+)-([0-9]+)")
SOURCE_NAME = re.compile(r"([0-9]{6}-[0-9]{6})-.+\.log")
LOG_HEADER = re.compile(
    r"\[[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}\.[0-9]{3}Z\]"
    r"\[(Debug|Info|Error)\]\[([^\]\r\n]+)\]\[([1-9][0-9]{0,9})\]\[(-|[1-9][0-9]{0,19})\]"
    r"\[[^\r\n]+? : [1-9][0-9]*\] "
)
MAX_OPEN_FILES = 32


def latest_source(log_dir):
    latest = None
    for run in log_dir.glob("run-*"):
        run_match = RUN_NAME.fullmatch(run.name)
        if not run_match or not run.is_dir():
            continue
        for path in run.glob("*.log"):
            name_match = SOURCE_NAME.fullmatch(path.name)
            if not name_match or not path.is_file():
                continue
            try:
                started_at = datetime.strptime("20" + name_match[1], "%Y%m%d-%H%M%S")
                key = (started_at, int(run_match[1]), int(run_match[2]), path.name)
            except ValueError:
                continue
            if latest is None or key > latest[0]:
                latest = (key, path)
    if latest is None:
        raise OSError(f"원본 로그를 찾지 못했습니다: {log_dir}")
    return latest[1]


def log_fields(line):
    try:
        match = LOG_HEADER.match(line.decode("utf-8"))
    except UnicodeDecodeError:
        return None
    return dict(zip(FIELDS, match.groups())) if match else None


def file_stem(split_by, group):
    if not split_by:
        return "filtered"

    parts = []
    for field, value in zip(split_by, group):
        safe_value = quote(value, safe="")
        if len(safe_value) > 96:
            safe_value = "sha256-" + sha256(value.encode("utf-8")).hexdigest()
        parts.append(f"{field}={safe_value}")
    return "__".join(parts)


def open_group(output_dir, split_by, group, paths):
    if group in paths:
        return paths[group].open("ab")

    stem = file_stem(split_by, group)
    suffix = 0
    # Windows에서 대소문자만 다른 그룹도 같은 파일에 섞지 않는다.
    while True:
        name = f"{stem}-{suffix}.log" if suffix else f"{stem}.log"
        path = output_dir / name
        try:
            stream = path.open("xb")
        except FileExistsError:
            suffix += 1
            continue
        paths[group] = path
        return stream


def split_logs(source, output_dir, filters, split_by):
    matched = malformed = 0
    paths = {}
    streams = OrderedDict()
    try:
        for line in source:
            fields = log_fields(line)
            if fields is None:
                malformed += 1
                continue
            if any(fields[field] not in values for field, values in filters.items()):
                continue

            group = tuple(fields[field] for field in split_by)
            if group in streams:
                streams.move_to_end(group)
            else:
                if len(streams) == MAX_OPEN_FILES:
                    streams.popitem(last=False)[1].close()
                streams[group] = open_group(output_dir, split_by, group, paths)
            streams[group].write(line)
            matched += 1
    finally:
        with ExitStack() as stack:
            for stream in streams.values():
                stack.callback(stream.close)
    return matched, malformed, len(paths)


def positive_id(value):
    if not re.fullmatch(r"[1-9][0-9]*", value):
        raise argparse.ArgumentTypeError("양의 정수 ID를 입력하세요.")
    return value


def session_id(value):
    return value if value == "-" else positive_id(value)


def main():
    parser = argparse.ArgumentParser(description="서버 원본 로그를 필터링하거나 그룹별로 분할합니다.", allow_abbrev=False)
    parser.add_argument("source", nargs="?", type=Path, help="원본 로그 (생략하면 최신 파일)")
    parser.add_argument("--log-dir", type=Path, default=Path("LogFile"), help="자동 탐색 폴더 (기본: ./LogFile)")
    parser.add_argument("--level", nargs="+", action="extend", type=str.title, choices=LEVELS)
    parser.add_argument("--project", nargs="+", action="extend")
    parser.add_argument("--thread-id", nargs="+", action="extend", type=positive_id)
    parser.add_argument("--session-id", nargs="+", action="extend", type=session_id)
    parser.add_argument("--split-by", nargs="+", action="extend", choices=FIELDS, default=[])
    args = parser.parse_args()
    filters = {field: set(values) for field in FIELDS if (values := getattr(args, field.replace("-", "_")))}
    split_by = tuple(dict.fromkeys(args.split_by))

    try:
        path = (args.source if args.source is not None else latest_source(args.log_dir)).absolute()
        with path.open("rb") as source:
            print(f"source: {path}", flush=True)
            output_root = path.parent / "Split"
            output_root.mkdir(exist_ok=True)
            output_dir = Path(tempfile.mkdtemp(dir=output_root, prefix="split-"))
            try:
                matched, malformed, files = split_logs(source, output_dir, filters, split_by)
            except (OSError, KeyboardInterrupt):
                shutil.rmtree(output_dir)
                raise
    except OSError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    print(f"output: {output_dir}")
    print(f"matched={matched} malformed={malformed} files={files}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
