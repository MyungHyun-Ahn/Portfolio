# GameDataGenerator

`Tools/Tools.sln`의 .NET 10 콘솔 도구다. Excel을 CSV로 변환하고, CSV만 읽어 대상별 바이너리와 C++ 행 구조체를 생성한다. 실행에 Excel 설치는 필요하지 않다.

## 실행

프로젝트 루트에서 실행한다.

```text
dotnet run --project Tools/GameDataGenerator -c Release
```

기본 입력은 `GameData/Excel/`의 `.xlsx`, 출력은 `GameData/Csv/`다. 예를 들어 `Player.xlsx`의 `PlayerMovement` 시트는 `PlayerMovement.csv`가 된다. 시트가 여러 개면 각각 출력한다. 시트 이름과 `#Table` 값은 같아야 하고, 통합 문서 간 같은 이름은 대소문자를 무시하고 거절한다. 값이 없는 시트와 Excel 잠금 파일(`~$*.xlsx`)은 건너뛴다.

별도 입력·출력 경로에 실행하려면 두 경로를 함께 넘긴다.

```text
dotnet run --project Tools/GameDataGenerator -c Release -- sample/Excel sample/Csv
```

CSV는 UTF-8(BOM 없음), 레코드 구분은 CRLF다. 셀 안의 CRLF·CR은 LF로 맞추고 그 밖의 문자는 보존한다. 쉼표·따옴표·개행이 있는 필드는 인용한다. Excel의 3행은 빈 구분 행으로 유지하고 CSV에서만 `#Workbook,Player,...`처럼 확장자 없는 원본 파일명을 기록한다. 파일명은 C++ 네임스페이스와 Windows 폴더에 그대로 쓸 수 있는 식별자여야 한다. `#Allowed`는 11행, `#Description`은 13행, `#Field`는 14행이며 데이터는 15행부터 시작한다. 끝의 서식뿐인 행은 내보내지 않는다. 수식·오류 셀이나 잘못된 선언을 발견하면 위치를 출력하고 기존 CSV를 바꾸지 않는다.

데이터 행의 `int32`·`uint32` 값은 Excel 숫자 셀의 정수여야 한다. `#Required=true`의 빈칸과 `#Min`, `#Max`, `#Allowed`를 위반한 값은 거절한다. PK는 테이블마다 `1..4294967295`의 고유한 값이어야 한다. `#Default`는 선택 컬럼에만 선언할 수 있고 같은 타입·범위·허용값 검사를 받는다. CSV에는 기본값 선언과 원본 빈칸을 그대로 남긴다. 완전히 빈 행은 데이터 레코드로 검사하지 않는다. 어느 입력에서든 검증에 실패하면 CSV 쓰기를 시작하지 않는다.

- `float32`는 Excel 숫자 셀을 받아 단정밀도로 반올림한 값에 `#Min`·`#Max`·`#Allowed`를 적용한다. 메타데이터의 숫자 셀은 같은 방식으로 변환하고, 십진 텍스트는 `float32`로 직접 읽는다. 무한대·NaN 또는 0이 아닌 값이 변환 중 0이 되는 경우는 거절한다. 숫자 셀에서 나온 데이터와 메타데이터는 CSV에서 같은 `float32`로 다시 읽을 수 있게 기록한다.
- `string`의 데이터·`#Default`·`#Allowed`는 Excel 텍스트 셀이어야 한다. 공백과 대소문자를 그대로 비교하며, 문자열에는 `#Min`·`#Max`를 선언하지 않는다. `#Allowed`의 `|`는 항목 구분자다. 허용 목록을 쓰지 않는 데이터 셀이나 기본값에는 `|`를 그대로 쓸 수 있다.

`#Reference`는 `Workbook.Sheet.Field`로 대상 PK를 지정한다. 예를 들어 `Player.PlayerMovement.PlayerMovementDataId`는 `Player.xlsx`의 `PlayerMovement` 시트에 있는 `PlayerMovementDataId`를 뜻한다. 확장자는 빼고 이름은 대소문자까지 맞춘다. 모든 테이블의 PK를 모은 뒤 대상 필드가 같은 타입의 PK인지 확인하고, 입력값과 선언된 기본값이 그 PK 집합에 있는지 검사한다. 선택 컬럼에 값도 기본값도 없으면 키 조회를 하지 않는다. 참조 검증이 실패하면 CSV를 하나도 쓰지 않는다.

## Enum

`GameData/Excel/Enum.xlsx`는 Enum을 쓰는 테이블이 있을 때만 필요하다. 시트명은 타입명이고, 각 시트의 A1은 `Name`, B1은 `Value`다. 2행부터 A열에는 멤버명(텍스트), B열에는 `0..4294967295`의 명시 번호(숫자 셀)를 입력한다. 예를 들어 `DamageType` 시트에 `Fire`와 `1`, `Ice`와 `2`를 각 행에 넣는다. 타입과 멤버 이름은 영문자로 시작하고 영문자·숫자·밑줄만 쓴다. 연속 밑줄과 예약어, 타입과 같은 멤버명, 같은 타입 안의 중복 이름·번호는 거절한다.

일반 테이블에는 `#Type=enum:DamageType`을 선언하고 데이터·`#Default`·`#Allowed`에 `Fire`처럼 대소문자를 맞춰 멤버명을 쓴다. `#Allowed` 예는 `Fire | Ice`다. Enum은 `#Min`·`#Max`나 숫자 PK·PK 참조에 사용할 수 없다. 일반 CSV에는 멤버명을 그대로 남기고, 정의는 `Type,Name,Value` 형식의 `Enum.csv` 한 파일로 모은다. 일반 테이블에는 `Enum`이라는 이름을 쓸 수 없다. 시트와 행을 재배치해도 `Enum.csv`의 내용은 바뀌지 않는다.

## 배열

`#Type`에 `int32[]`, `uint32[]`, `float32[]`, `string[]`, `enum:DamageType[]`를 선언하고, 데이터는 Excel 텍스트 셀에 `[1 | 2 | 3]`, `[Fire | Ice]`처럼 쓴다. `string[]`의 원소는 `["검 | 방패" | "활"]`처럼 큰따옴표로 감싼다. 문자열 내부 큰따옴표는 `""`로 적는다. 원소 순서와 중복을 유지하고, 중첩 배열과 빈 원소(`""` 포함)는 거절한다.

- `[]`는 값이 입력된 빈 배열이다. 빈 셀은 미입력이라 `#Required=true`면 거절한다. 선택 컬럼의 `#Default`는 `[1 | 2]` 또는 `[]`처럼 배열 전체로 선언하고, CSV에서 미입력 데이터 셀을 채우지 않는다.
- `#Min`·`#Max`는 숫자 배열에서 각 원소에 적용한다. `#Allowed`는 괄호 없는 원소 목록(예: `1 | 2`, `"검 | 방패" | "활"`)이며, 데이터와 기본값의 원소 하나하나가 이 목록에 있어야 한다. 배열은 PK와 `#Reference`에 쓸 수 없다.
- CSV는 배열 데이터·기본값의 구분자를 ` | `로 맞추고, 숫자는 같은 값으로 다시 읽을 수 있게 기록한다. 인용된 문자열 내부 공백·파이프는 그대로 보존한다.

## CSV → 구조체와 바이너리

```text
dotnet run --project Tools/GameDataGenerator -c Release -- --emit GameData/Csv GameData/Generated PlayerMovement
```

이 모드에서는 Excel을 열지 않는다. 입력 목록은 확장자를 뺀 CSV 이름으로 명시한다. 여러 테이블과 참조 대상을 함께 선택하며, Enum을 사용하면 목록에 `Enum`도 넣는다. 선택하지 않은 CSV는 읽지 않는다. 검증에 실패하면 쓰기를 시작하지 않는다. 생성 전용 출력 디렉터리를 넘긴다. 현재 출력 목록에 없는 `.h`·`.sdat`·`.cdat` 또는 이전 형식의 `.dat`가 생성 디렉터리에 남아 있으면 새 출력 디렉터리를 요구하며 기존 파일을 자동 삭제하지 않는다.

- `Server/<Workbook>/`와 `Client/<Workbook>/`에는 `<Sheet>.h`와 `<Sheet>.TableBase.g.h`를 내보낸다. 바이너리는 `Data/Server/<Workbook>/<Sheet>.sdat`와 `Data/Client/<Workbook>/<Sheet>.cdat`에 둔다. `#Target`에 해당하는 테이블의 `Shared`와 그 대상의 `#Scope` 필드만 행 헤더·생성 `Load()`·바이너리에 같은 순서로 반영한다.
- `GameDataAll.g.h`와 `CGameDataTables.g.h`는 대상 루트에 생성한다. `CGameDataTables::Load(dir)`에 대상 디렉터리(`Data/Server` 또는 `Data/Client`)만 넘기면 생성 목록에서 상대 경로를 붙이고 `IGameDataTable` 배열을 **전체 `Load()` → 전체 `Init()` → 전체 `Validate()`** 순으로 순회한다. `CLogger`로 테이블별 완료는 `Debug`, 모든 테이블이 통과한 단계는 `Info`, 실패는 대상·단계·상대 경로와 함께 `Error`로 기록한다. 실패하면 `nullptr`을 반환한다.
- 생성 루트의 형제 디렉터리 `Tables/{Server,Client}/C<Table>Table.h`는 파일이 없을 때만 만든다. 기존 수기 파일은 읽거나 덮어쓰거나 삭제하지 않으며 생성물 목록에도 넣지 않는다. 최초 골격의 `Validate()`는 실패하도록 두고, 테이블 규칙은 파일 소유자가 구현한다. `Init()`도 수기 함수이며 할 일이 없으면 성공만 반환한다.
- `Common/GameDataEnums.h`는 공용이다. 선택된 `Enum.csv`에서만 선언을 만들며, 선택하지 않으면 Enum 선언이 없다. 행, 테이블 베이스, 수기 테이블은 `GameData::<Target>::<Workbook>`에 속한다. 바이너리 검증에 쓰는 포맷 상수는 그 안의 `<Sheet>Format`에 둔다. 기존 시트를 다른 워크북으로 옮기면 수기 헤더의 네임스페이스도 직접 맞춘다. `--emit`은 그 파일을 고치지 않는다.
- 행은 대상별 필드마다 읽기 전용 `Get<Field>() const noexcept`를 제공한다. 필수 숫자와 Enum은 값으로, 문자열·배열·선택값은 `const` 참조로 반환한다. 선택값의 미입력 상태를 그대로 확인할 수 있고, 반환된 참조는 원본 행의 수명을 넘겨 보관하지 않는다.
- `GetMessage`, `GetObject`, `GetRValue`, `GetGValue`, `GetBValue`를 만드는 `#Field` 이름은 Windows 매크로와 충돌하므로 산출물 생성 전에 거절한다.
- C++ 사용부는 PCH에서 `MHLib`의 `CLogger`와 `<array>`, `<memory>` 등 필요한 표준 헤더를 공급한 뒤 대상의 `GameDataAll.g.h`를 포함하고 `CGameDataReader.cpp`를 링크한다. `Common`, `GameData/Generated`, `GameData`를 include 경로에 둔다. 비동기 로거는 `Load(dir)` 전에 시작하고 현재 스레드를 로거에 등록한다. 워커를 시작하기 전에 성공한 묶음을 확보하고, `GameData::Server::Player::CPlayerMovementTableBase::Find(id)`처럼 정적으로 조회한다. `Find()` 본문은 대상의 묶음 헤더에 생성되므로 조회하는 소스는 집계 헤더를 사용한다. PK가 없거나 묶음이 없으면 `nullptr`이다. 대상별로 동시에 하나의 묶음만 소유하며, 마지막 조회 워커가 끝난 뒤 해제한다.

바이너리의 바이트 계약은 [게임 데이터 생성기 계획](../../Docs/Plan/Common/001_게임데이터_생성기_계획.md#바이너리와-배포-경계)을 따른다. 생성 `Load()`는 [`CGameDataReader`](../../Common/GameData/CGameDataReader.h)에 필드 판독을 연결하며 파일이 끝까지 정상일 때만 행·PK 조회를 바꾼다. MSVC x64 환경에서 `GAME_DATA_TEST_MSVC=1`을 설정하고 `python -B Tests/GameDataGenerator/test_csv_to_artifacts.py -v`를 실행하면 두 대상의 C++ 판독 시험을 포함한다. 앱 등록과 전체 묶음의 I/O 실패 복구는 후속 작업이다.

## C++ 데이터 조회 예제

```cpp
#include "Server/GameDataAll.g.h" // 프로젝트 PCH 다음에 포함
```

서버 시작 시 대상 디렉터리를 한 번 로드하고, 반환된 묶음은 워커가 종료할 때까지 보관한다.

```cpp
auto pGameDataTables = GameData::Server::CGameDataTables::Load(serverDataDirectory);
if (pGameDataTables == nullptr)
{
	return false; // 서버 시작 실패
}
```

게임 로직에서는 필요한 PK로 조회한다. PK가 없으면 `Find()`는 `nullptr`을 반환한다.

```cpp
const auto* pRow = GameData::Server::Player::CPlayerMovementTableBase::Find(playerMovementDataId);
if (pRow != nullptr)
{
	const auto moveSpeedCmPerSecond = pRow->GetMoveSpeedCmPerSecond();
	// 이동 처리에 사용한다.
}
```

## 검증

```text
dotnet build Tools/Tools.sln -c Release
python -B Tests/GameDataGenerator/test_excel_to_csv.py
python -B Tests/GameDataGenerator/test_csv_to_artifacts.py
```

x64 MSVC 개발자 프롬프트에서 `GAME_DATA_TEST_MSVC=1`을 설정하면 두 번째 시험에서 양쪽 생성 헤더와 `IGameDataTable`을 C++20으로 컴파일한다.
