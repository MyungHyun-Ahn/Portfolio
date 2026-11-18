# 로드맵
## 현재 목표
### 1차: 채팅 서버 복원
05번 프로젝트의 채팅 구현을 12_NetworkLib 기반으로 복원하고, CNetClient를 사용하는 콘솔 클라이언트 사이에서 실제 채팅을 확인한다.
- 상태: 1~5단계 완료. 6단계 DummyClient는 미착수이며, ChatServer·ChatClient의 PCH 정리는 보류한다.
- [작업 계획과 범위](Plan/001_1차_채팅서버_복원계획.md)
- [1단계 결과와 코드 확인 안내](Server/001_라이브러리_이전_정리.md)

## 큰 작업 내용의 로드맵
- [x] MHLib·NetworkLib 복사 및 Visual Studio 2026 / v145 빌드 연결
- [x] CNetServer 상속 기반 채팅 서버 복원
- [x] CNetClient 및 콘솔 채팅 클라이언트 구현
- [x] 여러 콘솔 사이의 실제 채팅 최종 확인 (사용자 확인)
- [x] Packet Generator 추가 — [사용법](../Tools/PacketGenerator/README.md) · [채팅 패킷 송수신 흐름](Server/002_채팅_패킷_송수신_흐름.md)
- [ ] DummyClient 추가 — `07_DummyClient`를 참고해 서버 부하 테스트와 동작 검증에 사용할 클라이언트 개발

## 완료된 작업

- 문서 링크