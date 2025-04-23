# 📌MHLib

## 📦 라이브러리 설명
**MHLib**(My Header Library, MyungHyun Library)는 고성능 멀티스레드 환경을 위한 **C++ 헤더 온리 라이브러리**입니다.  
Lock-Free 자료구조, Memory Pool, 싱글톤, 스마트 포인터 등 다양한 유틸리티가 포함되어 있어,  
**경량성**과 **속도**, 그리고 **사용 편의성**을 동시에 제공합니다.

> ⚙️ C++17 이상 지원  
> 📁 Header-Only (빌드 없이 바로 사용 가능)  
> 🧵 Lock-Free 자료구조, 메모리 풀, 유틸리티, 암호화 등 포함

## ⚡️ 설치 방법
1. `include/` 디렉토리를 프로젝트에 추가하세요.
2. Visual Studio or CMake에서 **헤더 경로만 지정**하면 사용 준비 완료!

## ⚙️ 요구 사항
1. C++17 이상
2. Windows 전용

## 📂 폴더 구조
```
MHLib/
├── include/
│   └── MHLib/        			        # 모든 공개 헤더 파일이 여기에 위치
│       ├── containers/      		    # Lock-Free Queue, Stack, Deque 등
│       │   ├── CLFQueue.h
│       │   ├── CLFStack.h
│       │   └── CDeque.h
│       ├── debug/           		    # 크래시 덤프 등 디버깅 도구
│       │   └── CCrashDump.h
│       ├── memory/          		    # Memory Pool 관련 헤더들
│       │   ├── CLFMemoryPool.h
│       │   ├── CTLSMemoryPool.h
│       │   ├── CTLSPagePool.h
│       │   └── CSingleMemoryPool.h
│       └── security/        		    # 암호화 관련
│           └── CEncryption.h
│       ├── utils/           			# 유틸리티, 스마트 포인터 등
│       │   ├── CFileLoader.h
│       │   ├── CLockGuard.h
│       │   ├── CLogger.h
│       │   ├── CProfileManager.h
│       │   ├── CSmartPtr.h
│       │   ├── CDefineSingleton.h
│       │   └── LFDefine.h
│       └── MHLib.h
```

## 📚 주요 컴포넌트

| 폴더 경로            | 파일명                        | 설명 |
|----------------------|-------------------------------|------|
| `containers/`        | `CLFQueue.h`                  | Lock-Free 큐 |
|                      | `CLFStack.h`                  | Lock-Free 스택|
|                      | `CDeque.h`                    | 덱(양방향 리스트) |
| `memory/`            | `CLFMemoryPool.h`             | Lock-Free 메모리 풀 |
|                      | `CTLSMemoryPool.h`            | TLS 기반 메모리 풀 |
|                      | `CTLSPagePool.h`              | TLS 페이지 할당 풀 |
|                      | `CSingleMemoryPool.h`         | 싱글 스레드용 메모리 풀 |
| `utils/`             | `CLockGuard.h`                | SRW 기반 락 가드 |
|                      | `CSmartPtr.h`                 | 참조 기반 스마트 포인터 |
|                      | `CFileLoader.h`               | Config 파일 로더 |
|                      | `CLogger.h`                   | 로그 출력 유틸리티 |
|                      | `CProfileManager.h`           | 코드 실행 시간 측정용 프로파일러 |
|                      | `CDefineSingleton.h`          | 상속 기반 싱글톤 유틸 |
|                      | `LFDefine.h`                  | Lock-Free define |
| `debug/`             | `CCrashDump.h`                | 크래시 덤프 설정 유틸 |
| `security/`          | `CEncryption.h`               | 간단한 XOR 암호화/복호화 유틸 |