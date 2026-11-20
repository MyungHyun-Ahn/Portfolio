# 원본 로그 오프라인 분할

`SplitLogs.bat`는 Python 3.8 이상과 표준 라이브러리만 사용한다. CMD에서 **서버를 실행한 작업 디렉터리**로 이동해 실행한다. 다른 디렉터리에서 실행한다면 BAT의 절대 경로와 `--log-dir`을 사용한다. 기본 Python은 `PATH`의 `python`이며, 필요하면 `LOG_SPLITTER_PYTHON`에 `python.exe` 경로를 지정한다.

```cmd
Tools\LogSplitter\SplitLogs.bat --level info error --split-by level
Tools\LogSplitter\SplitLogs.bat --project ChatServer --session-id 42 84 --split-by session-id
Tools\LogSplitter\SplitLogs.bat --log-dir "C:\Chat Server\LogFile" --split-by project level thread-id
Tools\LogSplitter\SplitLogs.bat "C:\Chat Server\LogFile\run-123-0\261120-014453-ChatServer.log" --thread-id 1234
Tools\LogSplitter\SplitLogs.bat --help
```

- 파일 경로를 생략하면 `./LogFile/run-*` 바로 아래의 `YYMMDD-HHMMSS-실행파일명.log` 원본 중 파일명에 기록된 UTC 시작 시각이 가장 최근인 파일 하나를 선택한다. 같은 초에 시작했다면 실행 폴더의 시각 카운트와 번호를 숫자로 비교한다. 직접 경로를 지정할 때는 필터 옵션보다 앞에 둔다.
- 필터는 `--level`, `--project`, `--thread-id`, `--session-id`다. 같은 옵션의 값은 OR, 서로 다른 옵션은 AND로 처리한다. 레벨은 대소문자를 구분하지 않고, 프로젝트 이름은 구분한다. 세션이 없는 줄은 `--session-id -`로 선택한다. 본문의 `session_id=`가 아니라 헤더를 사용한다.
- `--split-by`를 생략하면 일치한 줄을 `filtered.log` 하나에 저장한다. 지정하면 `level=Error__session-id=42.log`처럼 헤더 값 조합마다 파일을 만든다.
- 출력은 원본 옆의 `Split\split-*` 아래에 매번 새로 생성한다. 원본과 이전 결과를 덮어쓰지 않으며, 출력 파일 안에서 줄의 순서와 원래 바이트를 유지한다.
- 선택한 원본과 출력 경로, 일치 줄·형식 오류 줄·출력 파일 수를 표시한다. 형식이 맞지 않거나 UTF-8로 읽을 수 없는 줄은 건너뛴다. 일치하는 줄이 없으면 파일은 만들지 않는다. 원본을 찾지 못하거나 파일 작업에 실패하면 `1`, CLI 인자가 잘못되면 `2`를 반환한다.

검증: `python -B Tests\LogSplitter\test_split_logs.py -v` (프로젝트 루트에서 실행).
