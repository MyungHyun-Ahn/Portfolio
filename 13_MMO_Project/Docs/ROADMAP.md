# 로드맵
## 현재 서버 목표
### 2차: 서버 로그 개선 및 DummyClient 검증

상태: 서버 로그 개선, 공통 클라이언트 응답 핸들러, 다중 앱 워커 `CDummyRunner` 작업을 완료했다. DummyClient는 x64 Debug·Release 루프백 검증과 사용자 검수를 마쳤다.

- [ ] 추가 전달 검증과 대규모 부하 — 고정 배치 수신 오라클과 무heartbeat 대조군은 별도로 검증한다. 포트 범위 규모의 접속 시험 전에는 [서버 설정](Plan/Server/004_DummyClient_검증.md#서버-설정-외부화)과 실제 사용 가능한 TCP 포트를 확인한다.
- [ ] Prometheus 모니터링 후속 — [모니터 계측 코드 개선](Plan/Server/006_프로메테우스_메트릭_설계.md#모니터-계측-코드-개선)으로 채팅 카운터의 소유권을 분리한다. 그 뒤에 프레임·섹터 전파 계측과 Grafana 패널을 추가해 750명·875명 실행을 비교한다.
- [ ] [원본 로그 수집](Plan/Server/005_서버_비동기_로그_설계.md#외부-로그-수집)은 후속 작업이다. 관측 연동은 로컬 DummyClient 실행의 선행 조건이 아니다.

## 완료된 작업
### 2차 완료 항목
- [x] Prometheus 모니터링 #1 — `MetricsLib`와 공통 서버 지표 매핑을 추가했다. ChatServer `/metrics`, 동일 호스트 Prometheus 수집, 사용자 코드 검토를 마쳤다. [설계 및 검증 결과](Plan/Server/006_프로메테우스_메트릭_설계.md#구현-순서와-확인-기준)
- [x] DummyClient `CDummyRunner` — 여러 앱 워커에서 반복 채팅·섹터 이동·heartbeat·재접속과 종료를 검증하고 사용자 검수를 마쳤다. [실행 및 검증 결과](Plan/Server/004_DummyClient_검증.md#단일-dummyrunner-실행과-검증)
- [x] 공통 클라이언트 응답 핸들러 — `CChatNetClient`에 공통 `Handle*()`와 `Req*()`를 두고, 콘솔 양방향 왕복 검증과 사용자 코드 검토를 마쳤다. [구현 및 검증 범위](Plan/Server/004_DummyClient_검증.md#공통-클라이언트-응답-핸들러)
- [x] 서버 이벤트의 `WriteLog()` 경로 통합과 로그 레벨 정리. [확인 결과](Plan/Server/005_서버_비동기_로그_설계.md#서버-이벤트-입력-통합-확인-결과)
- [x] MHLib SPSC 큐 구현·단독 검증 및 캐시라인 분리. [확인 결과](Plan/Server/005_서버_비동기_로그_설계.md#mhlib-spsc-큐-확인-결과)
- [x] 서버 비동기 로거 — 생산자별 큐, 단일 출력 스레드, 콘솔·파일 독립 필터, 실행별 원본 파일을 구현하고 검증했다. [확인 결과](Plan/Server/005_서버_비동기_로그_설계.md#서버-비동기-로거-확인-결과)
- [x] 원본 로그 오프라인 분할 도구 — 최신 원본 자동 선택과 필터·분할을 구현하고 원본 보존을 검증했다. [사용법](../Tools/LogSplitter/README.md)

### 1차: 채팅 서버 복원
05번 프로젝트의 채팅 구현을 12_NetworkLib 기반으로 복원하고, `CNetClientManager`를 사용하는 콘솔 클라이언트 사이에서 실제 채팅을 확인한다.
- 상태: **1차 완료.** 여러 콘솔의 실제 양방향 채팅을 확인했고, 전송 분리·공용 IOCP·세션 수명과 콘솔 송수신 연결의 구현·검증 및 클라이언트 코드 검토를 마쳤다. 기존 PCH 정리는 보류한다.
- [작업 계획과 범위](Plan/Server/001_1차_채팅서버_복원계획.md)
- [1단계 결과와 코드 확인 안내](Server/001_라이브러리_이전_정리.md)

#### 1차 완료 항목
- [x] MHLib·NetworkLib 복사 및 Visual Studio 2026 / v145 빌드 연결
- [x] CNetServer 상속 기반 채팅 서버 복원
- [x] CNetClient 및 콘솔 채팅 클라이언트 구현
- [x] 여러 콘솔 사이의 실제 채팅 최종 확인 (사용자 확인)
- [x] Packet Generator 추가 — [사용법](../Tools/PacketGenerator/README.md) · [채팅 패킷 송수신 흐름](Server/002_채팅_패킷_송수신_흐름.md)
- [x] 클라이언트·서버 `Send*` 함수 자동 생성 — [Packet Generator 후속 작업](Plan/Server/002_패킷_생성기_추가계획.md#후속-계획-패킷-송신-함수-자동-생성)
- [x] 클라이언트 네트워크 분리 — 서버·클라이언트 라이브러리 분리, 공용 IOCP·세션 수명, 콘솔 송수신 연결. [상세 계획과 확인 결과](Plan/Server/003_클라이언트_네트워크_분리_계획.md)

## 현재 클라 목표
### Win32 + Direct2D 2D 게임 개발 라이브러리 준비

상태: Phase 2.4까지 완료. 다음은 [Phase 3.1 — 플레이어 상태 분리](Plan/Client/001_클라이언트_기반_구축.md#phase-31--플레이어-상태-분리)다. 서버 연동 없이 독립 실행형 `Sandbox`에서 단계별로 확인한다. [전체 개발 계획](Plan/Client/001_클라이언트_기반_구축.md)

- [x] [Phase 1.1 — 빈 Win32 창 열고 닫기](Plan/Client/001_클라이언트_기반_구축.md#phase-11--빈-win32-창-열고-닫기) — 창 표시·종료 자동 검증 및 사용자 동작·코드 확인 완료.
- [x] [Phase 1.2 — 프레임 루프](Plan/Client/001_클라이언트_기반_구축.md#phase-12--프레임-루프) — `PeekMessageW` 기반 루프, 입력 없는 상태의 FPS 표시 및 정상 종료 검증과 사용자 확인 완료.
- [x] [Phase 1.3 — 고정 업데이트](Plan/Client/001_클라이언트_기반_구축.md#phase-13--고정-업데이트) — 밀린 틱을 버리지 않는 고정 업데이트 구현, x64 Debug·Release 자동 검증 및 사용자 코드 검토 완료.
- [x] [Phase 1.4 — Direct2D 초기화](Plan/Client/001_클라이언트_기반_구축.md#phase-14--direct2d-초기화) — `ClientRenderLib` 연결, x64 Debug·Release 단색 배경 표시·종료 자동 검증 및 사용자 코드 검토 완료.
- [x] [Phase 1.5 — 사각형 그리기](Plan/Client/001_클라이언트_기반_구축.md#phase-15--사각형-그리기) — x64 Debug·Release 사각형·배경 표시 자동 검증 및 사용자 완료 확인.
- [x] [Phase 1.6 — 리사이즈·최소화](Plan/Client/001_클라이언트_기반_구축.md#phase-16--리사이즈최소화) — 창 크기 변경, 최소화 중 고정 업데이트 유지, 복원 후 표시를 x64 Debug·Release에서 자동 검증하고 사용자 코드 검토를 마쳤다.
- [x] [Phase 1.7 — 클릭 위치 확인](Plan/Client/001_클라이언트_기반_구축.md#phase-17--클릭-위치-확인) — Win32 입력 어댑터와 `CInputManager`의 버튼 매핑·틱별 입력 상태 구현. x64 Debug·Release 자동 검증 및 사용자 완료 확인.
- [x] **Phase 1.8 — 클릭으로 사각형 이동** — 이동 중 재클릭과 새 목적지에서 정지를 x64 Debug·Release에서 자동 검증하고 사용자가 완료를 확인했다.
- [x] [Win32 기반과 Direct2D 첫 화면](Plan/Client/001_클라이언트_기반_구축.md#win32-기반과-direct2d-첫-화면) — 창과 고정 업데이트 루프, 클릭 위치로 이동하는 사각형 표시까지 완료했다.
- [x] [Phase 2.1 — 카메라 이동](Plan/Client/001_클라이언트_기반_구축.md#phase-21--카메라-이동) — 사각형 추적과 월드↔창 내부 좌표 변환 구현, 최종 구조에서 x64 Debug·Release 프레임워크 테스트와 Release Sandbox 검증, 사용자 완료 확인.
- [x] [Phase 2.2 — 카메라 줌](Plan/Client/001_클라이언트_기반_구축.md#phase-22--카메라-줌) — 마우스 휠 줌과 클릭 좌표 변환을 x64 Debug·Release에서 자동 검증했고 사용자 검토를 마쳤다.
- [x] [Phase 2.3 — PNG 한 장](Plan/Client/001_클라이언트_기반_구축.md#phase-23--png-한-장) — 투명 PNG의 첫 프레임을 기존 사각형 위에 표시한다. x64 Debug·Release 자동 검증 및 사용자 완료 확인.
- [x] [Phase 2.4 — 스프라이트 시트](Plan/Client/001_클라이언트_기반_구축.md#phase-24--스프라이트-시트) — 정지·이동 애니메이션과 전환을 x64 Debug·Release에서 자동 검증했으며, 사용자 확인으로 완료 처리했다. 구현 커밋 `ac70abe`의 `origin/DevClient` 반영을 확인했다.

**[구조 개편 순서와 완료 기준](Plan/Client/001_클라이언트_기반_구축.md#새-구현-순서와-확인-기준)** — Win32 실행 구조와 `Game → Scene → Actor → Component`를 기준으로 삼고, 그리기는 기존 Direct2D 백엔드를 쓴다.

- [ ] [Phase 3.1 — 플레이어 상태 분리](Plan/Client/001_클라이언트_기반_구축.md#phase-31--플레이어-상태-분리) — 다음 구현. 이동·방향·재생을 `CSandboxPlayer`로 옮긴다.
- [ ] [Phase 3.2 — 게임 루프 경계 분리](Plan/Client/001_클라이언트_기반_구축.md#phase-32--게임-루프-경계-분리) — 게임 호출과 창·고정 틱·프레임 제출의 책임을 분리한다.
- [ ] [Phase 3.3 — 비트맵 슬롯을 이미지 ID로 교체](Plan/Client/001_클라이언트_기반_구축.md#phase-33--비트맵-슬롯을-이미지-id로-교체) — 렌더러의 고정 두 장 제한을 없앤다.
- [ ] [Phase 3.4 — 두 번째 캐릭터로 공유와 독립 재생 검증](Plan/Client/001_클라이언트_기반_구축.md#phase-34--두-번째-캐릭터로-공유와-독립-재생-검증) — 프레임 루프를 고치지 않고 캐릭터를 추가한다.
- [ ] [Phase 3.5 — 단일 Scene으로 객체 소유권 정리](Plan/Client/001_클라이언트_기반_구축.md#phase-35--단일-scene으로-객체-소유권-정리) — 장면에서 객체의 수명과 순회를 관리한다.
- [ ] [Phase 4.1 — 타일맵 표시](Plan/Client/001_클라이언트_기반_구축.md#phase-41--타일맵-표시) — 카메라를 적용해 작은 맵을 그린다.
- [ ] [Phase 4.2 — 정적 벽 충돌](Plan/Client/001_클라이언트_기반_구축.md#phase-42--정적-벽-충돌) — 벽 뒤 클릭으로 벽을 통과하지 않게 한다.
- [ ] [Phase 4.3 — UI 입력 분리](Plan/Client/001_클라이언트_기반_구축.md#phase-43--ui-입력-분리) — 버튼 클릭을 월드 이동과 분리한다.
- [ ] [Phase 4.4 — 디버그 표시와 안정화](Plan/Client/001_클라이언트_기반_구축.md#phase-44--디버그-표시와-안정화) — 충돌 표시, 독립 실행·종료와 회귀 검증을 마친다.
