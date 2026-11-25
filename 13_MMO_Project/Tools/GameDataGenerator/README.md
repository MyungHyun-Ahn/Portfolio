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

CSV는 UTF-8(BOM 없음), 레코드 구분은 CRLF다. 셀 안의 개행은 LF로 맞추고 쉼표·따옴표·개행이 있는 필드는 인용한다. 1~14행 메타데이터와 값 사이의 빈칸을 유지한다. `#Allowed`는 11행, `#Description`은 13행, `#Field`는 14행이며 데이터는 15행부터 시작한다. 끝의 서식뿐인 행은 내보내지 않는다. 수식·오류 셀이나 잘못된 선언을 발견하면 위치를 출력하고 기존 CSV를 바꾸지 않는다.

데이터 행의 `int32`·`uint32` 값은 Excel 숫자 셀의 정수여야 한다. `#Required=true`의 빈칸과 `#Min`, `#Max`, `#Allowed`를 위반한 값은 거절한다. PK는 테이블마다 `1..4294967295`의 고유한 값이어야 한다. `#Default`는 선택 컬럼에만 선언할 수 있고 같은 타입·범위·허용값 검사를 받는다. CSV에는 기본값 선언과 원본 빈칸을 그대로 남긴다. 완전히 빈 행은 데이터 레코드로 검사하지 않는다. 어느 입력에서든 검증에 실패하면 CSV 쓰기를 시작하지 않는다.

`#Reference`는 `Workbook.Sheet.Field`로 대상 PK를 지정한다. 예를 들어 `Player.PlayerMovement.PlayerMovementDataId`는 `Player.xlsx`의 `PlayerMovement` 시트에 있는 `PlayerMovementDataId`를 뜻한다. 확장자는 빼고 이름은 대소문자까지 맞춘다. 모든 테이블의 PK를 모은 뒤 대상 필드가 같은 타입의 PK인지 확인하고, 입력값과 선언된 기본값이 그 PK 집합에 있는지 검사한다. 선택 컬럼에 값도 기본값도 없으면 키 조회를 하지 않는다. 참조 검증이 실패하면 CSV를 하나도 쓰지 않는다.

바이너리·C++ 생성, 삭제된 시트의 CSV 정리, 여러 파일을 한 번에 교체하는 트랜잭션은 후속 작업이다.

## 검증

```text
dotnet build Tools/Tools.sln -c Release
python -B Tests/GameDataGenerator/test_excel_to_csv.py
```
