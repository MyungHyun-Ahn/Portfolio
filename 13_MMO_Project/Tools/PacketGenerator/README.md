# Packet Generator 사용법

콘텐츠별 YAML을 읽어 공용 C++ 패킷 구조체와 서버·클라이언트별 송수신 코드를 생성한다. Visual Studio의 `Generate` 프로젝트는 BAT를 찾기 위한 용도다. **솔루션 빌드나 `Generate` 단독 빌드에서는 생성기를 실행하지 않는다.** 평소에는 이미 생성된 C++ 파일을 빌드하고, 스키마나 템플릿을 바꿨을 때만 BAT를 직접 실행한다.

## 초기 설정과 실행

Windows 명령 프롬프트(`cmd.exe`)에서 `13_MMO_Project` 루트로 이동해 실행한다. 가상환경은 저장소 밖에 만든다.

```bat
cd /d C:\MyServer\Portfolio\13_MMO_Project
python -m venv "%LOCALAPPDATA%\MMO_Project\PacketGenerator"
set "PACKET_GENERATOR_PYTHON=%LOCALAPPDATA%\MMO_Project\PacketGenerator\Scripts\python.exe"
"%PACKET_GENERATOR_PYTHON%" -m pip install -r Tools\PacketGenerator\requirements.txt
call Tools\PacketGenerator\GeneratePackets.bat
```

초기 설정 이후 같은 터미널에서는 BAT만 다시 실행하면 된다. 새 터미널에서는 `PACKET_GENERATOR_PYTHON`을 다시 설정한다. 변수를 지정하지 않으면 BAT는 `PATH`의 `python`을 사용한다. 실행 환경은 [requirements.txt](requirements.txt)의 PyYAML·Jinja2가 설치된 인터프리터여야 한다.

## 스키마를 바꿀 때

1. [`Common/Packet/Schema`](../../Common/Packet/Schema)에 콘텐츠별로 기능 YAML을 둔다. [로그인 예시](../../Common/Packet/Schema/Chatting/Login.yaml)처럼 `Req`와 `Res`는 쌍으로 정의한다. `Noti`는 [하트비트 예시](../../Common/Packet/Schema/System/Heartbeat.yaml)처럼 단독으로도 정의할 수 있고, `direction`을 지정한다.
2. 실제 패킷의 `id`는 중복되지 않게 명시한다. `fields`의 순서가 전송 순서다. 기본 타입은 `int8`, `int16`, `int32`, `int64`, `uint8`, `uint16`, `uint32`, `uint64`이며, `bytes_fixed`는 `bytes`, `utf16_fixed`는 `code_units`로 고정 길이를 지정한다. 가변 `utf16`은 별도 길이 필드를 선언하지 않는다.
3. [GeneratePackets.bat](GeneratePackets.bat)를 실행한다. 새 수신 패킷의 `Handle*` 선언이 생기면 수동 클래스에 구현을 추가하고, `Chatting.sln`에서 서버와 클라이언트를 함께 빌드한다. 생성기는 Handle 본문을 쓰지 않는다.

| 출력 | 내용 |
|---|---|
| [`Common/Packet/Struct`](../../Common/Packet/Struct) | 공용 패킷 ID, 구조체와 직렬화, include 집계 헤더 |
| [`Client/Generated/PacketHandler`](../../Tests/Chatting/Client/Generated/PacketHandler) | 클라이언트 송신 빌더, 수신 디스패처, Handle 선언 |
| [`Server/Generated/PacketHandler`](../../Tests/Chatting/Server/Generated/PacketHandler) | 서버 송신 빌더, 수신 디스패처, Handle 선언 |

`.g.h`, `.g.cpp`, `.packet-generator.json`은 함께 관리하고 손으로 편집하지 않는다. 내용이 같으면 다시 쓰지 않는다. 스키마가 바뀌면 일반 빌드가 생성물을 자동으로 갱신하지 않으므로 BAT를 먼저 실행한다.

## 실패했을 때

- **종료 코드 `9009`:** BAT가 Python 명령을 실행하지 못한 경우다. `PACKET_GENERATOR_PYTHON`이 실제 `python.exe`를 가리키는지 확인한다.
- **`ModuleNotFoundError`:** BAT가 사용하는 동일한 Python에 `requirements.txt`를 설치한다.
- **스키마 오류 또는 생성물 충돌:** 출력된 파일과 원인을 먼저 확인한다. 미관리 파일이나 수동 변경된 생성 파일을 덮어쓰지 않는다. 필요하면 해당 생성물을 스키마와 일치하는 버전으로 복구한 뒤 다시 실행한다.

실제 패킷이 어떻게 전송되고 처리되는지는 [채팅 패킷 송수신 흐름](../../Docs/Server/002_채팅_패킷_송수신_흐름.md)을 참고한다.
