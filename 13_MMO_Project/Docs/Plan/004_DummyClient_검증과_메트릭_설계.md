# DummyClient 검증과 메트릭 설계

상태: [공통 클라이언트 응답 핸들러](#공통-클라이언트-응답-핸들러)와 이를 사용하는 다중 앱 워커 [`CDummyRunner`](#단일-dummyrunner-실행과-검증)는 구현·루프백 검증·사용자 검수를 마쳤다. 추가 전달 검증·대규모 부하·외부 메트릭 연동은 별도 작업이다. 전송 계층은 [클라이언트 네트워크 분리 계획](003_클라이언트_네트워크_분리_계획.md), 패킷 흐름은 [채팅 패킷 송수신 흐름](../Server/002_채팅_패킷_송수신_흐름.md)을 기준으로 한다.

## 1. 더미 동작과 책임

- 원본 `07_DummyClient`는 읽기 전용 참고용이며, 타이머/APC·전역 매니저·옛 로그인 코드를 통째로 이식하지 않는다. 로그는 기존 `CLogger`를 우선 사용한다.
- 구현 위치: `Tests/Chatting/DummyClient`. 프로세스 하나에 `CNetClientManager`와 `CDummyRunner`를 각각 하나씩 둔다. Runner는 전체 실행·종료를 조정하고, IOCP 워커는 전송과 Mailbox 발행만 담당한다. 첫 구현부터 더미를 앱 워커에 묶음으로 배정할 수 있도록 실행 소유권을 나눈다. 클라이언트마다 스레드를 만들지 않는다.
- 실행 중 각 더미의 `Tick()`·요청·타이머·콜백·재접속은 담당 앱 워커 한 곳에서만 처리한다. 같은 객체를 여러 스레드에서 동시에 돌리지 않고, 공통 핸들러에 락을 덧씌우거나 클라이언트 경로에 TLS 풀·락프리 컨테이너를 추가하지 않는다. `WORKER_COUNT`는 기존 IOCP 워커 설정이며 앱 워커 수와 구분한다.
- 진행 중인 요청과 결과 카운터는 워커별로 소유한다. Runner는 동기화된 진행 통지로 최초 Ready 및 종료를 조정하고, 다른 워커가 갱신 중인 결과를 직접 읽지 않는다. 종료 시 각 워커가 새 요청을 멈추고 담당 더미의 terminal 이벤트를 소비한 뒤 합류한다. Runner는 합류 후 최종 결과를 합산하고 공유 매니저를 `Stop()`한다.
- `CConsoleChatClient`와 `CChatDummyClient`는 `CChatNetClient`를 직접 상속하며, 생성 송수신뿐 아니라 같은 수동 `HandleRes*()` 구현도 사용한다. 콘솔 출력과 더미의 실행·검증 정책은 별도로 둔다. 생성물 복사본이나 별도 채팅 `.lib`는 만들지 않는다. 기존 패킷 규격과 서버 코드는 바꾸지 않는다.
- 상태: `ConnectRequested → Connected → LoginPending → SectorPending → Ready → Closing → Terminal`. `Connect()`의 반환값은 접수 여부다. 로그인 응답의 상태·계정을 확인하고, 요청한 좌표와 일치하는 섹터 응답을 받아야 Ready다. 각 단계에 별도 기한을 둔다. 로그인/섹터 시간은 생성 Send의 큐 접수 시점부터 앱이 해당 응답을 처리할 때까지다.
- 처음 `CLIENT_COUNT`개 계정이 모두 Ready가 되면 `RUN_DURATION_MS`를 한 번만 시작한다. 재접속으로 실행 시간을 다시 시작하지 않는다. 제한 시간이 끝나면 새 동작·heartbeat·재접속을 접수하지 않는다. 제한 시간 전에 접수된 제출 호출이 모두 반환한 뒤 `event=dummy_draining`을 남긴다. 이미 제출한 동작과 이미 시작한 재접속의 응답은 계속 처리한다. 이어서 연결을 닫고 terminal 이벤트를 소비한 뒤 매니저를 `Stop()`한다. 작업 정리와 terminal 대기에 `DRAIN_TIMEOUT_MS`를 각각 적용한다. 외부에서 시작한 서버는 종료하지 않는다.
- 후속 `sector`: 같은 섹터·인접 섹터·범위 밖을 각각 검증한다. `SendSector()`는 양축 ±1의 3×3 범위에서 송신자를 제외하지만, 송신자는 별도 응답을 받는다. 대상 집합 계산은 시나리오의 명시된 좌표를 기준으로 하고, 서버의 `SendSector()` 구현을 테스트 오라클로 복제하지 않는다. 좌표 전환은 해당 응답 후 다음 메시지를 보낸다.
- 후속 `lifecycle`: heartbeat만 보내는 연결이 서버의 수신 제한(현재 40초)을 지나도 유지되는지 검증한다. 정상 비교군과 달리 heartbeat·채팅을 모두 끈 연결은 서버의 1초 검사 주기와 스케줄링 여유를 반영한 기한 내 종료되어야 한다. `HeartbeatNoti`에는 응답이 없으므로 타임아웃 전후의 연결 상태와 채팅 왕복으로 확인한다. 재접속은 이전 종료 이벤트 소비 후 새 ID·다시 Ready·채팅 재개를 확인한다. 의도적 중단과 예상 밖 단절을 구분한다.
- 실행 중: 각 Ready 클라이언트는 `ACTION_INTERVAL_MS` 간격으로 `SectorMove` 또는 `Message`를 가중치에 따라 선택한다. 한 클라이언트에서 이전 동작이 진행 중이면 새 동작을 겹쳐 보내거나 밀린 요청을 몰아서 보내지 않는다. `SectorMove`는 유효한 다른 좌표를 요청하고 일치하는 응답을 받은 뒤 현재 좌표를 갱신한다. `Message`는 계정과 전체 본문이 일치하는 자기 응답만 완료로 센다. 다른 더미의 같은 실행 메시지는 관측 건수로만 센다. 동작이 정상 완료될 때마다 재접속 확률을 한 번 적용한다. 재접속에 당첨되면 이전 연결의 종료 이벤트를 소비한 뒤 같은 더미가 접속·로그인·섹터 진입을 다시 거쳐 Ready가 된다. 실패나 타임아웃은 의도적 재접속과 별도 집계한다.
- 로컬 결과에는 설정 수, 단계별 누적 진입 수, 동작별 제출·완료, heartbeat 제출, 재접속 요청·접수·Ready, 실패 수와 최대 16개 샘플을 남긴다. Ready 횟수는 재접속을 포함한 누적값이다. 피어 수신 수는 관측값이며 전달 완전성 판정이 아니다.

## 2. 검증과 오동작 탐지

아래 전달 오라클(전체 수신자 집합·누락·중복·오전달·지연 히스토그램)은 별도의 고정 섹터 검증 계획이다. 현재 장기 실행의 성공 기준은 상태/응답, 자기 echo 및 종료의 검증 실패가 없는 것이다.

| 검증 항목 | 탐지 방법 |
|---|---|
| 접속·로그인·섹터 | 접수 거부, 비동기 접속 실패, 접속 만료, 로그인/섹터 응답 만료·거부·필드 불일치를 단계별로 집계한다. Ready 이전에는 시나리오 채팅을 금지한다. |
| 정확한 메시지 전달 | 기존 `ReqMessage` 본문 안에 `run_id:sender:sequence`와 테스트 문자열을 넣는다. 송신 전에 기대 수신자 집합을 등록하고, 송신자의 `ResMessage` 및 피어의 `MessageNoti`에서 토큰·수신 객체·보낸 계정·본문을 대조한다. 메타데이터는 wire 필드를 추가하지 않고 본문에만 둔다. 테스트 문자열을 포함해 256 UTF-16 코드 단위 이하다. |
| 누락·중복·오전달 | 기대 수신자별 첫 정상 수신만 성공으로 센다. 미수신은 기한 만료, 두 번째 수신은 중복, 집합 밖은 오전달, 만료 뒤 첫 수신은 지연 도착으로 따로 센다. 종료 전 제한된 drain 구간에 늦은 결과를 모으되 실패를 성공으로 바꾸지 않는다. 이전 실행 토큰이나 외부 콘솔의 일반 채팅은 테스트 성공으로 세지 않는다. |
| 순서·데이터 무결성 | 같은 발신자→수신자 쌍에서만 sequence를 비교한다. 전체 발신자에 걸친 전역 순서는 요구하지 않는다. 작은 검증 시나리오에서 한글·이모지·최대 길이 본문과 서버 응답의 필드를 비교한다. 정상 클라이언트 시나리오에 패킷 변조·퍼징을 섞지 않는다. |
| 종료·재접속 | 정상 종료를 요청한 연결의 terminal 이벤트가 한 번만 처리되고, 다음 연결을 시작할 때 ID가 달라야 한다. 예상 밖 단절은 단계·로컬 오류 코드와 함께 실패로 기록한다. 피어가 닫았다는 사실만으로 서버의 종료 이유까지 알았다고 판정하지 않는다. |
| 부하 결과의 유효성 | 동작 예약, 큐 접수, 응답·전달 완료를 따로 센다. 앱 스케줄러 지연이나 더미 내부의 수신 결과 추적 한계로 요청을 못 냈다면 서버 실패와 구분한다. 시작 기준선과 drain 후 세션·풀 사용을 비교할 수 있지만, 프로세스 RSS나 풀 capacity가 즉시 원복되는 것을 누수 없음의 조건으로 삼지 않는다. |

이전 연결의 로컬 terminal 소비는 서버의 계정 정리 완료를 보장하지 않는다. 새 연결에서 로그인이 거부되면 중복 계정인지 다른 거부 사유인지 현재 응답으로 구분할 수 없으므로, 로그인 실패로 기록하고 자동 재시도하지 않는다.

계산은 같은 더미 프로세스의 단조 시계를 사용한다. 메시지 지연은 **송신 시도 직전 → 기대 수신자의 `Tick()` 콜백**이며, 네트워크 단독 시간이나 서버 처리 시간은 아니다. 실패·타임아웃은 지연 히스토그램에 가짜 값으로 넣지 않는다. 정상 결과는 검증 실패 0건, 필수 전달 누락 0건, 예상 밖 단절 0건이 기준이다. 부하 한계나 p95 합격 기준은 별도 기준선을 확보한 뒤 정한다. 현재 `RunSummary.json`에는 종료 코드(`0` 정상 종료, `1` 검증/실행 실패, `2` 설정 오류), 설정 요약·시작/종료 시각·단계/동작/재접속 수 및 제한된 실패 샘플을 남긴다. Grafana가 없어도 로컬 결과가 남아야 한다.

## 3. DummyClient Config

EXE 옆 `DummyClientConfig.conf`를 `CFileLoader`로 읽는다(UTF-16LE BOM). `IP`, `PORT`, `ACCOUNT_BASE`는 필수이고 다른 설정은 생략 시 표의 기본값을 쓴다. 일회성 smoke 모드와 `SCENARIO` 키는 두지 않는다. 결과는 EXE 옆 `DummyReports/<run_id>/RunSummary.json`에 저장한다.

| 설정 | 기본값 또는 계약 |
|---|---|
| `IP`, `PORT` | 배포 파일은 `127.0.0.1`, `6000`. 실제 접속 대상이며 IPv4 루프백만 허용한다. |
| `CLIENT_COUNT`, `WORKER_COUNT` | `2`, `1`. 대상 수는 양의 정수이며 별도 256 한도를 두지 않는다. 워커는 `1..16`. |
| `APP_WORKER_COUNT` | `1`. 설정 범위는 `1..16`; 실제 앱 워커 수는 `CLIENT_COUNT`와 이 값 중 작은 쪽이다. `WORKER_COUNT`로 정하는 IOCP 워커와 별개다. |
| `ACCOUNT_BASE`, `SECTOR_X`, `SECTOR_Y` | 양의 연속 계정 범위를 사용한다. 마지막 계정 오버플로를 사전 검사한다. 닉네임은 `dummy` + 번호(고정 폭을 넘으면 계정 번호). 초기 좌표는 `(10,10)`, 각 축 `0..49`. |
| `CONNECT_TIMEOUT_MS`, `LOGIN_TIMEOUT_MS`, `SECTOR_TIMEOUT_MS` | 각각 `5000`. 섹터 기한은 초기 진입과 이동 응답에 적용한다. |
| `RECV_BUFFER_BYTES`, `TICK_INTERVAL_MS` | `4096`, `1`. 수신 용량은 기존 전송 계층 허용 범위, Tick 주기는 `1..100` ms. |
| `RUN_DURATION_MS`, `ACTION_INTERVAL_MS` | `60000`, `1000`. 양의 정수. 동시에 진행하는 일반 동작은 더미당 하나로 제한하고, 밀린 동작은 몰아서 보내지 않는다. |
| `SECTOR_MOVE_WEIGHT`, `MESSAGE_WEIGHT` | 각각 `1`. 0은 해당 동작을 끄며, 둘 다 0일 수 없다. |
| `RECONNECT_PROBABILITY_BPS` | `0`. 동작 완료마다 `0..10000` 만분율 적용. 초기 진입·heartbeat에는 적용하지 않는다. |
| `HEARTBEAT_INTERVAL_MS`, `DELIVERY_TIMEOUT_MS`, `DRAIN_TIMEOUT_MS` | `30000`, `3000`, `5000`. Heartbeat 주기는 서버 수신 제한인 40초 미만의 양수. 전달 기한은 자기 echo에 적용한다. |

- 현재 **256은 테스트용 `ChatServer`의 `Main.cpp`가 설정한 서버 세션 슬롯 수**다. 클라이언트 PC의 포트 수나 더미의 입력 한도가 아니다. 용량 시험은 더미에 256 한도를 두지 않고, 해당 PC에서 테스트에 실제로 사용할 수 있는 클라이언트 TCP 포트 수까지 동시 접속하는 것을 목표로 한다. 서버 슬롯 설정과 16비트 슬롯 인덱스·개수 표현 범위를 먼저 확인하고, 포트 고갈·재접속 중 잔류 연결·서버 슬롯 부족을 구분해 목표 수와 실제 TCP 연결 수·Ready 수를 기록한다. 포트 범위 크기만으로 같은 수의 동시 연결을 보장하지 않는다. Windows에서는 `netsh int ipv4 show dynamicport tcp`로 설정 범위를 확인한다.

### 서버 설정 외부화

대규모 동시 접속 시험 전에 `serverSetting`에서 실행 시 조정 가능한 값을 EXE 옆 `ServerConfig.conf`로 옮긴다. `ChatServer`가 기존 `CFileLoader`로 읽고 검증한 뒤 서버를 시작한다. 네트워크 라이브러리가 채팅 앱의 설정 파일을 직접 읽지는 않는다. 설정 누락 시 현재 채팅 서버의 유효 기본값을 유지하고, 명시한 값이 잘못됐으면 서버를 시작하지 않는다.

| 대상 | 설정화 범위 |
|---|---|
| 리슨 주소 | `openIp`, `openPort`는 기존 `IP`, `PORT` 설정을 유지한다. 이번 테스트 범위는 루프백이다. |
| 접속·IOCP | `maxSessionCount`, `acceptexCount`, `iocpWorkerThread`, `iocpActiveThread`, `useZeroCopy`. `maxSessionCount`를 `MAX_SESSION_COUNT`로 노출하고 `Main.cpp`의 256 고정 대입을 없앤다. 슬롯 개수와 인덱스 타입이 표현할 수 없는 값은 거부하고, AcceptEx 수·워커 수·동시성 값의 제약을 시작 전에 확인한다. |
| 콘텐츠 스레드 | `contentsThreadCount`, `maxContentsFps`, `delayFrame`. 콘텐츠 스레드 수와 FPS는 0을 거부하고, 프레임 지연 계산이 오버플로하지 않는 범위를 확인한다. |
| Net 패킷 호환 | `packetCode`, `packetKey`는 서버 단독 튜닝 값이 아니다. 설정으로 노출하려면 콘솔·더미도 동일한 값을 사용하게 하고, 서버의 프레이밍·수신 검사·인코딩이 모두 검증된 같은 설정에서 읽도록 연결한다. 그 전에는 기존 공용 값을 유지한다. 키 값은 로그나 실행 요약에 남기지 않는다. |

기본 설정의 기존 채팅, 별도 설정으로 서버 슬롯·워커 수가 실제로 적용되는 경우, 범위를 벗어난 설정의 시작 거부를 검증한다. 현재 슬롯 수 이내의 로컬 실행에는 선행 조건이 아니며, 포트 범위 규모의 부하 시험 전에 완료한다.

## 4. 서버 모니터링: 지금 되는 것과 안 되는 것

현재 `Server/NetworkLib/SystemEvent.cpp`의 `SMonitorTimerEvent`가 약 1초마다 `CMonitor::Update()`를 호출한다. 현재 구간 카운터는 출력 뒤 리셋된다. 타이머 지연 가능성이 있으므로 한 번 읽은 값을 정확한 초당 값이나 Prometheus 누적 카운터로 쓰지 않는다. 특히 `Accept TPS`는 `AcceptExCompleted()` 진입, `acceptTotal`은 세션 초기화·등록 경로의 집계, `m_chatMsgRes`는 송신 경로 호출 건수다. 어떤 항목도 상대 앱 수신 건수를 뜻하지 않는다.

| 분류 | 항목·근거 | 계획 |
|---|---|---|
| PDH 비의존 | `GetSystemTimes`·`GetProcessTimes` CPU, `GetProcessMemoryInfo` private bytes. 앱의 세션 수·버퍼/뷰/수신/세션 풀 사용·용량, Accept/Recv/Send, 로그인/섹터/채팅, 작업 이전/훔치기도 별도 계측 경로다. | 이관 중에는 기존 콘솔을 유지하고, Grafana 패널에서 대체 지표를 확인한 뒤 주기 출력을 끈다. OS API 실패, 첫 CPU 샘플, 구간 분모가 0인 경우는 유효 수치로 내보내지 않는다. 풀은 사용량과 capacity를 구분한다. |
| PDH TODO이면서 이 호스트에서 미동작 확인 | `Process(IOCP_CHAT_SERVER)`의 nonpaged pool, 하드코딩된 Realtek NIC 2개의 경로. 로컬 PDH 프로브에서 옛 프로세스와 NIC 1 송신·NIC 2 수신 대표 경로는 추가에 성공했지만 두 샘플 후 `PDH_INVALID_DATA / PDH_CSTATUS_NO_INSTANCE`. 기존 API와 영어 경로 API 모두 동일했다. 나머지 방향의 개별 호출은 미검증. | 로컬 콘솔에서 꼭 필요하면 프로세스 PID로 인스턴스를 확인하거나 OS API로 직접 읽고, NIC는 실제 인터페이스 목록/설정으로 선택한다. 다른 호스트 정상 여부는 별도 확인한다. |
| PDH에서 유효 샘플 확인 | 시스템 nonpaged pool, Available MBytes, 호스트 TCPv4 retransmit/sec는 이 호스트의 두 샘플 프로브에서 유효했다. retransmit `0`도 유효 상태였으며, 서버별 재전송이나 루프백 성능을 입증하지 않는다. | 호스트 지표는 외부 exporter가 기본 소유자다. 물리 NIC·호스트 TCP 값의 범위를 대시보드에 명시한다. |
| 구현상 신뢰 불가 | `CMonitor.cpp`는 query open/add/collect/format 반환값과 `CStatus`를 해석하지 않아 없는 인스턴스·워밍업·정상 0을 구분하지 않는다. | 영어 이름은 `PdhAddEnglishCounterW`, 초기 2샘플 필요 항목 워밍업, 반환값·`CStatus`별 `available/warming_up/unavailable` 표시, 실패 전환만 로그. 유효하지 않은 샘플은 누락 또는 `N/A`; 0으로 채우거나 오래된 값을 최신으로 내보내지 않는다. |
| 현재 계측하지 않거나 다른 계층이 필요 | 서버별 wire 바이트, 큐 깊이·in-flight, 원인별 종료, 애플리케이션 수신 지연·오전달. | 앱 카운터/스냅샷 또는 더미 오라클에서 신설한다. NIC·TCP 호스트 카운터로 대신하지 않는다. 콘솔 `pending` 송신 집계는 실제 현재 pending 개수와 구분한다. |

## 5. 메트릭 수집과 재사용

```text
Windows host → windows_exporter ────────────┐
Server/Dummy counters → snapshot → /metrics ┼→ Prometheus → Grafana
Dummy final summary → RunSummary.json (관측 스택과 독립)
```

- `windows_exporter`는 호스트 CPU/메모리/NIC/TCP 및 필요한 경우 대상 프로세스 CPU·메모리·스레드·핸들을 수집한다. 프로세스 collector는 대상 필터와 실제 호스트 지원을 확인한다. 앱은 Windows OS 지표를 이중으로 내보내지 않는다. PDH 콘솔 개선과 외부 exporter 연동은 별도 작업으로 진행할 수 있다.
- 앱 exporter는 서버와 더미에서 같은 HTTP 노출/샘플 검증 코드를 재사용하되, `NetworkLib`·채팅 구조·Loki SDK를 의존하지 않는다. 각 앱의 어댑터만 현재 상태를 읽어 **불변 스냅샷**을 공개한다. HTTP 핸들러는 이미 공개된 스냅샷만 읽고 세션/콘텐츠 락을 잡거나 PDH를 조회하지 않는다. SDK/HTTP 라이브러리 선택은 구현 시 검토하며, 기본 비활성·로컬 바인딩·게임 포트와 분리한다. 관측 서비스가 중단돼도 채팅과 로컬 검증은 계속한다. 원격 수집이 필요할 때만 별도 주소·권한·방화벽을 정한다.
- Counter는 프로세스 시작 이후 누적값이다. `CMonitor`가 매 주기 리셋하는 기존 TPS 필드 자체를 내보내지 않는다. 동일 이벤트의 누적 계측을 추가하거나, 공용 집계기가 단일 소유자가 되도록 원자적 교환의 값을 누적하는 경로를 먼저 확정한다. Gauge는 현재 상태, Histogram은 성공 응답 지연과 부하 발생 지연에 공통 버킷을 사용한다. Prometheus는 `rate`/`increase`로 처리량을 계산한다. 짧은 로컬 실행은 scrape 없이 끝날 수 있으므로 pass/fail은 로컬 요약이 기준이다.

| 초기 메트릭 후보 | 의미 |
|---|---|
| `game_net_sessions`, `game_net_session_limit`, `game_net_pool_in_use`, `game_net_pool_capacity` | 활성 세션 및 슬롯 한도, 종류별 풀 사용량/용량. 풀 종류는 소수 고정값. |
| `game_net_accept_outcomes_total{result}`, `game_net_packets_total{direction}`, `game_net_wire_bytes_total{direction}` | 서버 수락 결과, 유효 수신 및 성공 송신 완료 패킷 수, IOCP 완료 기준 TCP 페이로드 바이트(패킷 헤더 포함, IP/TCP 헤더 제외). 바이트와 패킷 집계 위치를 명시한다. |
| `game_net_disconnects_total{reason}`, `game_net_send_batches_total{result}` | 종료 원인과 배치 결과. `reason`은 제한된 enum, 원본 Winsock 코드는 로그. 큐 깊이·in-flight 계측은 오버헤드를 확인한 뒤 추가. |
| `game_chat_requests_total{operation,result}`, `game_chat_delivery_attempts_total` | 로그인·섹터·메시지·heartbeat 처리 결과 및 채팅 대상 송신 시도 수. 누가 실제 받았는지는 더미가 측정. 채팅 도메인 전용. |
| `dummy_clients{phase}`, `dummy_phase_outcomes_total{phase,result}`, `dummy_deliveries_total{result}` | 단계별 현재 클라 수, 단계 결과, 기대 수신자별 검증 결과. 송신 제출 수와 별도. |
| `dummy_chat_delivery_duration_seconds`, `dummy_phase_duration_seconds{phase}`, `dummy_tick_interval_seconds` | 최초 정상 수신과 단계 지연, 발생기 루프 지연의 histogram. |
| `game_observability_snapshot_timestamp_seconds` | 최근 게시 시각. Prometheus의 `up`은 scrape 연결 상태이며, 오래된 스냅샷·Ready 수와는 독립이다. |

- 공통 라벨은 `service`, `environment`, `instance` 중심으로 제한하고, 메트릭별 `phase`, `direction`, `result`는 고정 집합만 쓴다. `run_id`, 두 종류의 세션 ID, 계정·메시지 ID는 라벨이 아니라 로그 필드로 둔다. 환경별 대상을 설정으로 바꾸고 공통 대시보드 JSON·알림은 재사용한다.
- Grafana는 공통 Windows/전송 대시보드, 채팅 도메인 대시보드, 더미 검증 대시보드로 나눈다. 첫 화면에서 목표 대비 Ready, 실제 제출률/전달률, 누락·중복·오전달, 지연, 세션/풀·호스트 자원을 같은 시간축에 배치한다. 로그는 `service`와 시간 범위로 이동해 `run_id`를 쿼리한다. 초기 알림은 `up` 실패·스냅샷 정체·검증 오류 증가를 구분하고, 선택 지표 미수집을 정상 0이나 회귀로 해석하지 않는다. 재사용 완료 기준은 채팅 코드 없이 두 번째 서버의 스냅샷 어댑터와 설정만으로 공통 전송 패널이 뜨는 것이다.

## 6. 구현 순서와 확인 기준

각 작업의 결과를 사용자 검토한 뒤 다음으로 진행한다. 구현 상태는 [로드맵](../ROADMAP.md)을 따른다.

### 공통 클라이언트 응답 핸들러

**완료: 공통 코드 구현 및 사용자 코드 검토.** x64 Debug·Release 콘솔 루프백에서 로그인 → 섹터 진입 → 양방향 채팅 → 정상 종료를 확인했다. 송신자는 자기 `ResMessage`, 상대는 `MessageNoti`를 각각 한 건 받았다. 이후 `IsValidString()` 변경은 네이티브 경계 테스트와 x64 Release 콘솔 루프백으로 확인했다. 더미의 공통 핸들러 재사용은 아래 실행 결과에 포함한다. 언리얼 연동은 미검증이다.

| 파일 | 책임 |
|---|---|
| `CChatNetClient.g.h` | `CNetClient`를 공개 상속하고 `IClientPacketHandler`를 비공개 상속하는 클래스와 `Send*()`·`Handle*()` 선언을 생성한다. 콘솔과 더미가 이 클래스를 직접 상속하며, 중간 `CChatClient` 클래스는 두지 않는다. |
| `CChatNetClient.g.cpp` | `Send*()`·`MakePacket*()`·`onRecv()`를 생성한다. 클라이언트 `Send*()`는 패킷 생성과 `sendNetworkPacket()` 호출만 하고 채팅 상태는 바꾸지 않는다. `onRecv()`는 기존 `DispatchClientPacket()`을 호출한다. |
| `CChatNetClientHandlers.cpp` | 생성 디렉터리 밖에서 공통 `ReqLogin()`·`ReqSectorMove()`와 `Handle*()` 본체를 구현한다. 더미는 이 구현을 재사용하고 요청 시점과 결과 판정만 별도로 둔다. |
| `CChatNetClientMembers.inc` | 공통 상태와 앱별 콜백을 수동 선언한다. BAT는 파일이 없을 때만 기본 템플릿으로 생성하며, 기존 파일은 덮어쓰지 않는다. |

1. 클라이언트 생성 파일을 `ClientPacketSend.g.h/.cpp`에서 `CChatNetClient.g.h/.cpp`로 바꾸고 생성기, include, 소유권 manifest, 프로젝트와 필터를 맞췄다. `IClientPacketHandler`와 디스패치 구조는 유지했다. 서버에는 별도 채팅 패킷 변경으로 상대방 전용 `MessageNoti`를 추가했다.
2. 생성 송신 템플릿에서 `submitChatRequest()` 경유를 제거하고 수동 구현과 `.inc` 선언·기본 템플릿도 정리한다. 응답 추적이 필요한 공통 요청은 `CChatNetClient`의 `protected`에 `Req<기능>()`으로 둔다. 최초 로그인은 `ReqLogin()`이 생성 `SendReqLogin()`의 접수 성공 후 기대 계정을 저장한다. 최초 진입과 이후 이동은 `ReqSectorMove()`가 생성 `SendReqSectorMove()`의 접수 성공 후 기대 좌표를 저장한다. 채팅·heartbeat는 생성 `Send*()`를 바로 호출한다.
3. 공통 `HandleResLogin()`은 로그인 응답을 검증하고 승인 상태를 확정한다. 최초 진입 시점과 좌표는 앱이 정한다. 콘솔은 `onLoginAccepted()`에서 설정 좌표로 `ReqSectorMove()`를 호출한다. `HandleResSectorMove()`는 기대 좌표와 응답을 대조한 뒤 현재 섹터를 확정한다. `on~()`은 공통 처리 뒤 다음 요청 결정, 콘솔 출력이나 더미의 echo 대조·통계처럼 앱마다 다른 후처리에 사용한다. 앱별 테스트 판정 실패와 연결을 닫는 프로토콜 실패는 구분한다.
4. 스텁이 필요하면 별도 명령으로 새 핸들러의 초안을 별도 파일에 출력한다. 기존 수동 `.cpp`를 파싱하거나 덮어쓰지 않는다. 호출되는 핸들러는 실행 파일을 링크하기 전에 정의한다. 임시 스텁을 포함할 경우 미구현을 표시하고 `false`를 반환한다.
5. 재생성에서 기존 수동 파일과 서버 산출물 보존을 확인했다. x64 Debug·Release의 ChatClient–ChatServer 루프백에서 로그인·섹터 진입·양방향 채팅과 정상 종료를 확인했다. 더미에서 같은 핸들러를 실행한 결과는 아래에 기록한다.

### 단일 DummyRunner 실행과 검증

**완료: 구현·루프백 검증·사용자 검수.**

x64 Debug·Release에서 구현을 검증했다. `Tests/Chatting/Chatting.sln`을 빌드한 뒤 `python -B Tests/Chatting/test_dummy_runner.py Debug --long-heartbeat`를 실행한다(Release는 `Debug` 대신 `Release`). 두 앱 워커에서 재접속 로그인 요청을 잡아 두었다가 cutoff 후 전달해 다시 Ready 및 terminal 4건을 확인했다. 두 구성에서 42초 heartbeat 실행, 41초 이후 자기 echo, 접속 거부·로그인 만료·설정 오류, 기존 콘솔 양방향 채팅도 확인했다.

- 1·2·4개 계정, 메시지/이동 단독 가중치와 재접속 확률 0%·100%를 확인했다. 앱 워커 2개와 IOCP 워커 최대 3개로 실행해 진행 중인 재접속을 정리한 뒤 종료 이벤트와 최종 집계를 대조했다.
- `RunSummary.json`과 종료 코드로 판정한다. 서버의 `/exit`는 fixture의 프로세스 종료용이며 서버 `Stop()`의 검증은 아니다.

### 추가 전달 검증과 대규모 부하

- 범위 밖 수신 금지, heartbeat 없는 대조군, 섹터 위치를 고정한 배치의 누락·중복·오전달은 별도로 검증한다. 랜덤 부하에서는 자기 `ResMessage`만 대조하고 상대 `MessageNoti`는 관측 건수로 집계한다. 이동과 채팅이 겹칠 때의 전체 수신자 판정은 범위에서 제외한다.
- 포트 범위 규모의 후속 접속 시험 전에 [서버 설정 외부화](#서버-설정-외부화)를 완료한다. 본격적인 서버 부하 한계 시험, 언리얼, 운영 배포, 기존 서버 동시성 개편은 여기의 완료 조건이 아니다.

### 메트릭과 검증 결과 연동

- 서버 지표의 의미·단위와 PDH 유효성을 대조하고, 서버·더미 스냅샷과 `/metrics`를 연결한다. Prometheus 수집과 Grafana 대체 패널을 확인한 뒤 기존 서버의 `MonitoringConsole()` 주기 출력과 콘솔 크기 강제 변경을 끈다. 이벤트 로그는 유지한다.
- Jenkins 연동 시 프로세스 종료 코드로 판정하고 `RunSummary.json`과 테스트에 해당하는 원본 로그를 아티팩트로 보관한다. Jenkins 자체 지표는 C++ 계측과 별개다.
- 외부 관측 중단 중에도 채팅과 로컬 판정이 동작하는지, 채팅 코드 없이 두 번째 서버에서 공통 전송 패널을 재사용할 수 있는지 확인한다. 로그 수집 연결은 서버 로그 설계의 [외부 로그 수집](005_서버_비동기_로그_설계.md#외부-로그-수집)을 따른다.

### 외부 근거

- Microsoft Learn: [PDH 수집](https://learn.microsoft.com/en-us/windows/win32/perfctrs/collecting-performance-data), [반환값과 CStatus](https://learn.microsoft.com/en-us/windows/win32/perfctrs/checking-pdh-interface-return-values).
- Prometheus: [계측 원칙](https://prometheus.io/docs/practices/instrumentation/), [이름과 라벨](https://prometheus.io/docs/practices/naming/), [windows_exporter collector 목록](https://github.com/prometheus-community/windows_exporter/blob/main/docs/README.md).
