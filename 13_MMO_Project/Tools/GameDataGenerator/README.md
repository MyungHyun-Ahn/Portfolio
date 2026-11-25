# GameDataGenerator

`Tools/Tools.sln`의 .NET 10 콘솔 도구다. 현재는 Excel에서 CSV까지만 변환한다. 실행에 Excel 설치는 필요하지 않다.

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

CSV는 UTF-8(BOM 없음), 레코드 구분은 CRLF다. 셀 안의 CRLF·CR은 LF로 맞추고 그 밖의 문자는 보존한다. 쉼표·따옴표·개행이 있는 필드는 인용한다. 1~14행 메타데이터와 값 사이의 빈칸을 유지한다. `#Allowed`는 11행, `#Description`은 13행, `#Field`는 14행이며 데이터는 15행부터 시작한다. 끝의 서식뿐인 행은 내보내지 않는다. 수식·오류 셀이나 잘못된 선언을 발견하면 위치를 출력하고 기존 CSV를 바꾸지 않는다.

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

공용 `GameDataEnums.h`와 일반 테이블의 바이너리·C++ 행 구조체는 0.4부터 CSV만 읽어 생성한다. 삭제된 입력의 CSV 정리와 여러 파일을 한 번에 교체하는 트랜잭션도 후속 작업이다.

## 검증

```text
dotnet build Tools/Tools.sln -c Release
python -B Tests/GameDataGenerator/test_excel_to_csv.py
```
