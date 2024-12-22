# LogClass 소개
## 로그 레벨
세 단계로 구분됨
* DEBUG(0)
* SYSTEM(1)
* ERR(2)

목적
* 로그 레벨을 통해서 런타임 중 남길 로그를 구분
  * 너무 많은 로그는 부담임
* 런타임 중 로그 레벨을 변경할 수 있음
  * SYSTEM 레벨로 수행 중 갑자기 DEBUG 레벨의 로그를 보고 싶을 때 유용!!

## 로그 클래스 사용법
싱글톤 클래스로 구현됨
* 전역 객체로 Main 함수 시작 시 초기화해 주어야 함
* 다른 스레드가 수행되기 전에 무조건 초기화 되어야 함
  * 초기화 과정에서의 동기화 처리는 딱히 구현하지 않음

전역 포인터 변수가 CLogger.h에 선언되어 있음
* Main 시작 부분에서 g_Logger = CLogger::GetInstance() 호출이 필요

이후 해줘야 할 것
* SetMainDirectory(const WCHAR *);
  * 로그 파일의 가장 상위 디렉터리 설정
  * 로그 레벨 설정(선택)
    * 설정하지 않으면 LOG_LEVEL::DEBUG로 설정됨

## 제공되는 인터페이스
### WriteLog
~~~Cpp
void WriteLog(const WCHAR *directory, const WCHAR *type, LOG_LEVEL logLevel, const WCHAR *fmt, ...);
~~~
* directory : mainLog 디렉터리에서 하위 디렉터리 지정
  * 만약 nullptr을 전달하는 경우 mainLog 디렉터리에 로그 파일 생성
* type : 파일 이름 지정
* logLevel : 로그 레벨 설정
* fmt ... : printf 와 같은 방법으로 사용 가능

설정한 파일에 로그를 남김

### WriteLogHex
~~~Cpp
void WriteLogHex(const WCHAR *directory, const WCHAR *type, LOG_LEVEL logLevel, const WCHAR *logName, BYTE *pBytes, int byteLen);
~~~
* logName : 로그의 첫번째 줄에 남을 메시지
* pBytes : 바이트 배열
* byteLen : 바이트 배열 길이

설정한 파일에 바이너리 로그를 남김

### WriteLogConsole
~~~Cpp
void WriteLogConsole(LOG_LEVEL logLevel, const WCHAR *fmt, ...);
~~~

콘솔에 로그를 남김

## 핵심 구현 내용
### strsafe.h의 안전 문자열 함수를 사용
C 런타임 라이브러리의 _s 함수와의 차이점
* _s 함수는 버퍼 오버런 시 그냥 짤라서 안전한 부분까지 써버리고 성공 처리
* strsafe.h의 StringCch_ 계열의 함수는 안전한 부분까지 쓰고 실패 처리함
  * 에러 코드로 확인 가능함

반환값이 S_OK가 아닐 시
* STRSAFE_E_INSUFFICIENT_BUFFER가 반환되면 문자열을 모두 쓰지 못했음을 의미
* 이때 나의 LogClass는 로그 버퍼 크기가 부족했다는 로그를 남김

### 파일 쓰기 동기화 처리
파일 별 SRWLock을 통해 동기화를 처리함
* unordered_map을 사용
  * FileName을 Key로 SRWLOCK을 Value로
  * WCHAR *인 fileName을 std::wstring으로 변환하여 key로 사용됨

처음 FileName을 만드는 작업을 할 때 디렉토리의 생성은
* 위 unordered_map을 조회하여 최초 1번만 수행됨