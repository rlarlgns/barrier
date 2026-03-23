# Barrier Serial Communication Modification Report

이 문서는 베리어(Barrier) 프로젝트를 기존의 TCP/IP 네트워크 방식에서 시리얼(RS232/UART) 통신 방식으로 개조하기 위해 수행된 모든 수정 사항을 기록합니다.

## 1. 아키텍처 및 하드웨어 추상화 (HAL)
시리얼 포트에 접근하기 위한 OS별 물리 계층을 구현했습니다.

*   **신규 파일**:
    *   `src/lib/arch/IArchSerial.h`: 공통 시리얼 포트 인터페이스 정의 (Open, Close, Read, Write, Poll).
    *   `src/lib/arch/unix/ArchSerialUnix.h/cpp`: macOS/Linux용 구현 (`termios` 사용).
    *   `src/lib/arch/win32/ArchSerialWin32.h/cpp`: Windows용 구현 (Win32 API 사용).
*   **수정 파일**:
    *   `src/lib/arch/Arch.h/cpp`: 전역 `ARCH` 객체가 시리얼 기능을 포함하도록 상속 구조 수정.

## 2. 네트워킹 모듈 개조
베리어의 네트워크 엔진을 시리얼 스트림으로 대체하기 위한 클래스들을 구현했습니다.

*   **신규 파일**:
    *   `src/lib/net/SerialSocket.h/cpp`: `IDataSocket`을 상속받아 시리얼 포트를 소켓처럼 동작하게 구현.
    *   `src/lib/net/SerialListenSocket.h/cpp`: 시리얼 포트를 "서버" 모드로 대기하게 하는 래퍼.
    *   `src/lib/net/SerialSocketFactory.h/cpp`: 시리얼 관련 소켓 객체를 생성하는 팩토리 클래스.
*   **수정 파일**:
    *   `src/lib/base/NonBlockingStream.cpp`: 시리얼 통신 시 발생할 수 있는 비정상 종료를 막기 위해 `EAGAIN` 에러에 대한 강제 Assertion 제거.

## 3. 백엔드 및 CLI 엔진 통합
명령줄 인자를 통해 시리얼 모드를 활성화하고 설정을 제어할 수 있도록 수정했습니다.

*   **수정 파일**:
    *   `src/lib/barrier/ArgsBase.h/cpp`: `--serial`, `--baud` 인자 저장을 위한 변수 추가.
    *   `src/lib/barrier/ArgParser.cpp`: CLI 인자 파싱 로직 추가 및 시리얼 모드 시 암호화(SSL) 자동 비활성화.
    *   `src/lib/barrier/ServerApp.cpp`, `src/lib/barrier/ClientApp.cpp`: `--serial` 인자가 있을 경우 `TCPSocketFactory` 대신 `SerialSocketFactory`를 사용하도록 주입 로직 수정.
    *   `src/lib/barrier/App.h`: 도움말(`--help`) 메세지에 시리얼 관련 설명 추가.

## 4. GUI (사용자 인터페이스) 최적화
사용자가 네트워크 설정 없이 시리얼 통신에만 집중할 수 있도록 UI를 개편했습니다.

*   **신규 파일**:
    *   `src/gui/src/SerialPortScanner.h/cpp`: `QtSerialPort`를 사용하여 시스템의 가용 시리얼 포트를 자동 감지하는 유틸리티.
*   **수정 파일**:
    *   `src/gui/CMakeLists.txt`: `Qt5SerialPort` 모듈 연동 및 신규 소스 파일 등록.
    *   `src/gui/src/AppConfig.h/cpp`: 시리얼 포트명과 속도를 설정 파일에 저장하고 불러오는 로직 추가.
    *   `src/gui/src/MainWindowBase.ui`: 메인 화면 상단에 시리얼 설정 그룹 추가.
    *   `src/gui/src/MainWindow.cpp`: 
        *   네트워크 관련 UI(IP 주소, SSL 지문, 서버 IP 입력칸) 숨김 처리.
        *   시리얼 포트 자동 스캔 및 드롭다운 목록 반영.
        *   시리얼 모드 시 호스트명이 비어있어도 시작 가능하도록 검증 로직 우회.
    *   `src/gui/src/SettingsDialog.h/cpp/ui`: 설정 창에 시리얼 포트 및 속도 설정 항목 추가.

## 5. macOS 보안 및 배포 강화
최신 macOS 정책에 대응하여 앱이 정상적으로 실행되도록 조치했습니다.

*   **신규 파일**:
    *   `Entitlements.plist`: 시리얼 포트 및 입력 제어 권한 정의.
*   **수정 파일**:
    *   `clean_build.sh`: ARM64 최적화 빌드 및 병렬 컴파일 설정.
    *   `dist/macos/bundle/build_dist.sh.in`: 
        *   **Aggressive Signing**: 앱 번들 내 모든 라이브러리를 개별적으로 서명.
        *   기존 서명 충돌 방지를 위한 `_CodeSignature` 강제 삭제 로직 추가.
    *   `src/gui/src/main.cpp`: 초기 설정 시 접근성(Accessibility) 체크를 일시적으로 우회하여 UI 진입 허용.

## 6. 리눅스 배포 환경 구축 (Docker)
맥에서 리눅스용 바이너리를 생성하기 위한 환경을 구축했습니다.

*   **생성된 스크립트**:
    *   `Dockerfile`: Ubuntu 기반의 크로스 빌드 환경 정의.
    *   `docker_build.sh`: x86_64 아키텍처용 바이너리 빌드 및 배포 패키지 생성 자동화.

---
**최종 상태**: 본 프로젝트는 이제 완전한 시리얼 전용 모드로 동작하며, 맥(ARM64)과 리눅스(x86_64) 환경 모두에서 시리얼 포트 선택만으로 키보드/마우스 공유가 가능합니다.
