# Packet Generator 사용법

콘텐츠별 YAML을 읽어 공용 C++ 패킷 구조체와 서버·클라이언트별 송수신 코드를 생성한다. Visual Studio의 `Generate` 프로젝트는 BAT를 찾기 위한 용도다. **솔루션 빌드나 `Generate` 단독 빌드에서는 생성기를 실행하지 않으며 `Generate`의 빌드 산출물도 만들지 않는다.** 평소에는 이미 생성된 C++ 파일을 빌드하고, 스키마나 템플릿을 바꿨을 때만 BAT를 직접 실행한다.

## 초기 설정과 실행

Windows 명령 프롬프트(`cmd.exe`)에서 `13_MMO_Project` 루트로 이동해 실행한다. 가상환경은 저장소 밖에 만든다.

```bat
python -m venv "%LOCALAPPDATA%\MMO_Project\PacketGenerator"
set "PACKET_GENERATOR_PYTHON=%LOCALAPPDATA%\MMO_Project\PacketGenerator\Scripts\python.exe"
"%PACKET_GENERATOR_PYTHON%" -m pip install -r Tools\PacketGenerator\requirements.txt
call Tools\PacketGenerator\GeneratePackets.bat
```

초기 설정 이후 같은 터미널에서는 BAT만 다시 실행하면 된다. 새 터미널에서는 `PACKET_GENERATOR_PYTHON`을 다시 설정한다. 변수를 지정하지 않으면 BAT는 `PATH`의 `python`을 사용한다. 실행 환경은 [requirements.txt](requirements.txt)의 PyYAML·Jinja2가 설치된 인터프리터여야 한다.

인수를 생략하면 `Chatting`을 생성한다. `World`는 고정 맵 패킷, `Game`은 현재 [패킷 ID 없는 공개 구조체](#패킷-id-없는-공개-구조체)를 생성한다.

## 스키마를 바꿀 때

1. `Common/Packet/<프로토콜>/Schema/`에 콘텐츠별로 기능 YAML을 둔다. [채팅 로그인 예시](../../Common/Packet/Chatting/Schema/Chatting/Login.yaml)처럼 `Req`와 `Res`는 쌍으로 정의한다. `Noti`는 [하트비트 예시](../../Common/Packet/Chatting/Schema/System/Heartbeat.yaml)처럼 단독으로도 정의할 수 있고, `direction`을 지정한다.
2. 실제 패킷의 `id`는 중복되지 않게 명시한다. `fields`의 순서가 전송 순서다. 기본 타입은 `int8`, `int16`, `int32`, `int64`, `uint8`, `uint16`, `uint32`, `uint64`이며, `bytes_fixed`는 `bytes`, `utf16_fixed`는 `code_units`로 고정 길이를 지정한다. 가변 `utf16`은 별도 길이 필드를 선언하지 않는다.
3. [GeneratePackets.bat](GeneratePackets.bat)에 해당 프로토콜을 지정해 실행한다. 새 수신 패킷의 `Handle*` 선언이 생기면 수동 클래스에 구현을 추가한 뒤 해당 프로젝트를 빌드한다. 생성기는 Handle 본문을 쓰지 않는다.

| 출력 | 내용 |
|---|---|
| `Generated/Common/` | 공용 구조체·직렬화·집계 헤더, 패킷이 있으면 ID 목록 |
| `Generated/Client/` | 클라이언트 송신 빌더, 수신 디스패처, Handle 선언 |
| `Generated/Server/` | 서버 송신 빌더, 수신 디스패처, Handle 선언 |

위 경로는 `Common/Packet/<프로토콜>/` 기준이다. 프로토콜에 패킷이 없으면 송수신 코드는 생성하지 않는다.

`.g.h`, `.g.cpp`, `.packet-generator.json`은 함께 관리하고 손으로 편집하지 않는다. 내용이 같으면 다시 쓰지 않는다. 스키마가 바뀌면 일반 빌드가 생성물을 자동으로 갱신하지 않으므로 BAT를 먼저 실행한다.

## 패킷 ID 없는 공개 구조체

```bat
call Tools\PacketGenerator\GeneratePackets.bat Game
```

[공개 캐릭터 YAML](../../Common/Packet/Game/Schema/Player/CharacterData.yaml)의 `struct`에서 [`Game::SCharacterData`](../../Common/Packet/Game/Generated/Common/Player/CharacterData.g.h)와 공용 집계 헤더를 생성한다. `struct`에는 `name`, `namespace`, 순서가 있는 정수 `fields`만 선언한다. 현재 `Game`에는 패킷 정의가 없어 ID 목록, 양쪽 송수신 코드, 클라이언트 `.inc` 뼈대가 나오지 않는다.

```mermaid
flowchart LR
    A[캐릭터 공개 YAML] --> B[GeneratePackets.bat Game]
    B --> C[공용 SCharacterData와 집계 헤더]
```

이 명령은 DB 헤더를 읽거나 생성하지 않는다. 서버에서 `SDbCharacter`의 값을 옮기는 함수는 [DBGenerator 사용법](../DBGenerator/README.md#공개-값-변환-생성)을 따른다.

### 패킷에 공용 구조체 넣기

패킷의 `fields`에서 같은 프로토콜의 `struct.name`을 `type`으로 참조한다. 예를 들어 `CharacterData`는 아래 선언에서 `SCharacterData` 값 멤버가 된다.

```yaml
fields:
  - {name: character_data, type: CharacterData}
```

집계 헤더가 구조체 정의를 패킷 정의보다 먼저 포함한다. 바이트에는 바깥 패킷 ID만 한 번 쓰고, 중첩 구조체는 선언 순서대로 필드만 쓴다. 수신 버퍼나 `View`에서 중첩 값 읽기에 실패하면 패킷도 실패한다. 패킷에 독립 구조체를 넣는 것만 지원한다. 구조체끼리의 중첩과 다른 프로토콜 타입 참조는 지원하지 않는다.

시험용 `ResLogin` 왕복은 x64 Native Tools 명령 프롬프트에서 x64 Release `NetworkCommonLib.lib`를 빌드한 뒤 실행한다. 시험 패킷은 임시 폴더에만 생성한다.

```bat
if not defined PACKET_GENERATOR_PYTHON set "PACKET_GENERATOR_PYTHON=python"
set "PACKET_GENERATOR_NATIVE_TESTS=1"
"%PACKET_GENERATOR_PYTHON%" -B -m unittest discover -s Tests\PacketGenerator -k nested_login_native_round_trip -v
set "PACKET_GENERATOR_NATIVE_TESTS="
```

이 시험은 제품 `ResLogin`의 확정이나 실제 로그인 서버·클라이언트의 연결을 의미하지 않는다.

## 생성된 송신 함수

- `CChatNetClient::SendReqMessage(accountNo, text)`처럼 패킷 필드만 받는다. 생성된 `CChatNetClient`는 `CNetClient`를 상속해 현재 연결을 사용하고, `IClientPacketHandler`는 비공개로 상속해 수신을 디스패치한다. 송신의 `bool`은 접수 결과이지 상대의 수신 확인이 아니다.
- 공통 규약: `GeneratePackets.bat <프로토콜>`은 생성 클라이언트 클래스 안에 `<클래스명>Members.inc`를 삽입한다. Chatting은 `Tests/Chatting/Client/CChatNetClientMembers.inc`, World는 `Tests/WorldServer/WorldClientMembers.inc`를 쓴다. 새 프로토콜도 클라이언트 소스 디렉터리를 지정해 같은 이름 규칙을 따른다. BAT는 파일이 없을 때만 뼈대를 만들고, 기존 파일은 비어 있어도 덮어쓰지 않는다. 상태와 앱 콜백 선언은 `.inc`에, 핸들러 본문은 수동 `.cpp`에 두며, 둘 다 생성 디렉터리와 manifest 밖에서 관리한다.
- 서버의 `Send*`는 첫 인자로 `sessionId`를 받고, 준비에 실패하면 `nullptr`을 반환한다. 성공적으로 준비한 버퍼는 첫 전송의 접수 여부와 무관하게 호출자 소유 참조 하나와 함께 반환한다. 호출자는 필요하면 같은 버퍼를 주변 세션에도 넘긴 뒤 `DecreaseRef()` 결과가 0이면 `Free()`를 호출한다.
- 응답 버퍼를 확보한 뒤 상태를 변경해야 하는 서버 처리에서는 `MakePacket*()`과 `submitPreparedPacket()`을 분리해 호출한다. 수신용 `Handle*` 인터페이스에는 송신 함수가 추가되지 않는다.

## 실패했을 때

- **종료 코드 `9009`:** BAT가 Python 명령을 실행하지 못한 경우다. `PACKET_GENERATOR_PYTHON`이 실제 `python.exe`를 가리키는지 확인한다.
- **`ModuleNotFoundError`:** BAT가 사용하는 동일한 Python에 `requirements.txt`를 설치한다.
- **스키마 오류 또는 생성물 충돌:** 출력된 파일과 원인을 먼저 확인한다. 미관리 파일이나 수동 변경된 생성 파일을 덮어쓰지 않는다. 필요하면 해당 생성물을 스키마와 일치하는 버전으로 복구한 뒤 다시 실행한다.

실제 패킷이 어떻게 전송되고 처리되는지는 [채팅 패킷 송수신 흐름](../../Docs/Server/002_채팅_패킷_송수신_흐름.md)을 참고한다.
