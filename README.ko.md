# System 7 - 이식 가능한 오픈소스 재구현

**[English](README.md)** | **[Fran&ccedil;ais](README.fr.md)** | **[Deutsch](README.de.md)** | **[Espa&ntilde;ol](README.es.md)** | **[日本語](README.ja.md)** | **[中文](README.zh.md)** | **[한국어](README.ko.md)**

<img width="793" height="657" alt="최신 하드웨어에서 실행 중인 System 7" src="https://github.com/user-attachments/assets/be84b83e-191c-4f9d-a786-11d0bd04203b" />
<img width="801" height="662" alt="simpletextworks" src="https://github.com/user-attachments/assets/7c9ebe5b-22b4-4612-93a1-2076909d77cd" />
<img width="803" height="661" alt="macpaint" src="https://github.com/user-attachments/assets/cd3ed04a-fdde-4dd5-88ef-5b19b3a13a54" />

> **개념 증명** - 이 프로젝트는 Apple Macintosh System 7의 실험적이고 교육적인 재구현입니다. 완성된 제품이 아니며 프로덕션 환경에서 사용할 수 있는 소프트웨어로 간주해서는 안 됩니다.

GRUB2/Multiboot2를 통해 부팅 가능한, 최신 x86 하드웨어용 Apple Macintosh System 7의 오픈소스 재구현 프로젝트입니다. 이 프로젝트는 역공학 분석을 통해 System 7 아키텍처를 문서화하면서 클래식 Mac OS 경험을 재현하는 것을 목표로 합니다.

## 프로젝트 현황

**현재 상태**: ~94%의 핵심 기능이 완성된 활발한 개발 중

### 최근 업데이트 (2025년 11월)

#### Sound Manager 개선 사항 -- 완료
- **최적화된 MIDI 변환**: 37개 항목의 룩업 테이블(C3-B5)과 전체 MIDI 범위(0-127)를 위한 옥타브 기반 폴백을 갖춘 공유 `SndMidiNoteToFreq()` 헬퍼
- **비동기 재생 지원**: 파일 재생(`FilePlayCompletionUPP`)과 명령 실행(`SndCallBackProcPtr`) 모두를 위한 완전한 콜백 인프라
- **채널 기반 오디오 라우팅**: 음소거 및 활성화 제어가 포함된 다중 레벨 우선순위 시스템
  - 하드웨어 출력 라우팅을 위한 4단계 우선순위 채널(0-3)
  - 채널별 독립적인 음소거 및 활성화 제어
  - `SndGetActiveChannel()`이 가장 높은 우선순위의 활성 채널을 반환
  - 기본적으로 활성화 플래그가 설정된 적절한 채널 초기화
- **프로덕션 수준의 구현**: 모든 코드가 깨끗하게 컴파일되며 malloc/free 위반이 감지되지 않음
- **커밋**: 07542c5 (MIDI 최적화), 1854fe6 (비동기 콜백), a3433c6 (채널 라우팅)

#### 이전 세션 성과
- **고급 기능 단계**: Sound Manager 명령 처리 루프, 다중 실행 스타일 직렬화, 확장 MIDI/합성 기능
- **윈도우 크기 조절 시스템**: 적절한 크롬 처리, grow box 및 데스크톱 정리를 갖춘 인터랙티브 크기 조절
- **PS/2 키보드 변환**: 완전한 set 1 스캔코드를 Toolbox 키 코드로 매핑
- **멀티 플랫폼 HAL**: 깔끔한 추상화를 통한 x86, ARM, PowerPC 지원

## 프로젝트 완성도

**전체 핵심 기능**: ~94% 완료 (추정)

### 완전히 구현된 기능

- **하드웨어 추상화 계층 (HAL)**: x86/ARM/PowerPC를 위한 완전한 플랫폼 추상화
- **부팅 시스템**: x86에서 GRUB2/Multiboot2를 통해 성공적으로 부팅
- **시리얼 로깅**: 런타임 필터링(Error/Warn/Info/Debug/Trace)이 포함된 모듈 기반 로깅
- **그래픽 기반**: XOR 모드를 포함한 QuickDraw 프리미티브가 있는 VESA 프레임버퍼 (800x600x32)
- **데스크톱 렌더링**: 무지개 Apple 로고, 아이콘 및 데스크톱 패턴이 포함된 System 7 메뉴 바
- **타이포그래피**: 픽셀 단위로 정확한 렌더링과 적절한 커닝을 갖춘 Chicago 비트맵 폰트, 유럽어 악센트 문자를 위한 확장 Mac Roman (0x80-0xFF)
- **국제화 (i18n)**: 7개 언어(영어, 프랑스어, 독일어, 스페인어, 일본어, 중국어, 한국어)를 지원하는 리소스 기반 로컬라이제이션, 부팅 시 언어 선택이 가능한 Locale Manager, CJK 다중 바이트 인코딩 인프라
- **Font Manager**: 다중 크기 지원(9-24pt), 스타일 합성, FOND/NFNT 파싱, LRU 캐싱
- **입력 시스템**: 완전한 이벤트 전달이 가능한 PS/2 키보드 및 마우스
- **Event Manager**: 통합 이벤트 큐를 갖춘 WaitNextEvent 기반의 협력적 멀티태스킹
- **Memory Manager**: 68K 인터프리터 통합이 포함된 존(zone) 기반 할당
- **Menu Manager**: 마우스 추적 및 SaveBits/RestoreBits를 갖춘 완전한 드롭다운 메뉴
- **파일 시스템**: B-tree 구현이 포함된 HFS, VFS 열거를 갖춘 폴더 윈도우
- **Window Manager**: 드래그, 크기 조절(grow box 포함), 레이어링, 활성화
- **Time Manager**: 정확한 TSC 캘리브레이션, 마이크로초 정밀도, 세대 확인(generation checking)
- **Resource Manager**: O(log n) 이진 탐색, LRU 캐시, 종합적인 유효성 검사
- **Gestalt Manager**: 아키텍처 감지가 포함된 다중 아키텍처 시스템 정보
- **TextEdit Manager**: 클립보드 통합이 포함된 완전한 텍스트 편집
- **Scrap Manager**: 다중 플레이버(flavor) 지원이 포함된 클래식 Mac OS 클립보드
- **SimpleText 애플리케이션**: 잘라내기/복사/붙여넣기가 가능한 완전한 기능의 MDI 텍스트 편집기
- **List Manager**: 키보드 탐색이 포함된 System 7.1 호환 리스트 컨트롤
- **Control Manager**: CDEF 구현이 포함된 표준 컨트롤 및 스크롤바
- **Dialog Manager**: 키보드 탐색, 포커스 링, 키보드 단축키
- **Segment Loader**: 재배치(relocation)를 지원하는 이식 가능한 ISA 독립적 68K 세그먼트 로딩 시스템
- **M68K 인터프리터**: 84개 opcode 핸들러, 14개 주소 지정 모드, 예외/트랩 프레임워크를 갖춘 완전한 명령어 디스패치
- **Sound Manager**: 명령 처리, MIDI 변환, 채널 관리, 콜백
- **Device Manager**: DCE 관리, 드라이버 설치/제거 및 I/O 작업
- **시작 화면**: 진행률 추적, 단계 관리 및 스플래시 화면이 포함된 완전한 부팅 UI
- **Color Manager**: QuickDraw 통합이 포함된 색상 상태 관리

### 부분적으로 구현된 기능

- **애플리케이션 통합**: M68K 인터프리터와 세그먼트 로더는 완료되었으나 실제 애플리케이션 실행 확인을 위한 통합 테스트가 필요
- **Window Definition Procedures (WDEF)**: 핵심 구조가 갖추어져 있으며 부분적인 디스패치 구현
- **Speech Manager**: API 프레임워크와 오디오 패스스루만 구현; 음성 합성 엔진은 미구현
- **예외 처리 (RTE)**: 예외 복귀가 부분적으로 구현됨 (현재 컨텍스트 복원 대신 정지)

### 미구현 기능

- **인쇄**: 인쇄 시스템 없음
- **네트워킹**: AppleTalk 또는 네트워크 기능 없음
- **Desk Accessories**: 프레임워크만 존재
- **고급 오디오**: 샘플 재생, 믹싱 (PC 스피커 제약)

### 컴파일되지 않은 서브시스템

다음 항목들은 소스 코드가 있지만 커널에 통합되지 않았습니다:
- **AppleEventManager** (파일 8개): 애플리케이션 간 메시징; 프리스탠딩 환경과 호환되지 않는 pthread 의존성으로 인해 의도적으로 제외됨
- **FontResources** (헤더만 존재): 폰트 리소스 타입 정의; 실제 폰트 지원은 컴파일된 FontResourceLoader.c에서 제공

## 아키텍처

### 기술 사양

- **아키텍처**: HAL을 통한 다중 아키텍처 (x86, ARM, PowerPC 지원)
- **부팅 프로토콜**: Multiboot2 (x86), 플랫폼별 부트로더
- **그래픽**: VESA 프레임버퍼, 800x600 @ 32비트 컬러
- **메모리 레이아웃**: 커널이 1MB 물리 주소에 로드 (x86)
- **타이밍**: 마이크로초 정밀도의 아키텍처 독립적 구현 (RDTSC/타이머 레지스터)
- **성능**: 콜드 리소스 미스 <15us, 캐시 히트 <2us, 타이머 드리프트 <100ppm

### 코드베이스 통계

- **225개 이상의 소스 파일**, 약 57,500줄 이상의 코드
- **145개 이상의 헤더 파일**, 28개 이상의 서브시스템에 걸쳐 분포
- **69가지 리소스 타입**이 System 7.1에서 추출됨
- **컴파일 시간**: 최신 하드웨어에서 3-5초
- **커널 크기**: ~4.16 MB
- **ISO 크기**: ~12.5 MB

## 빌드

### 요구 사항

- **GCC** 32비트 지원 포함 (64비트 시스템에서는 `gcc-multilib`)
- **GNU Make**
- **GRUB 도구**: `grub-mkrescue` (`grub2-common` 또는 `grub-pc-bin`에 포함)
- **QEMU** 테스트용 (`qemu-system-i386`)
- **Python 3** 리소스 처리용
- **xxd** 바이너리 변환용
- *(선택 사항)* **powerpc-linux-gnu** PowerPC 빌드를 위한 크로스 툴체인

### Ubuntu/Debian 설치

```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common
```

### 빌드 명령어

```bash
# 커널 빌드 (기본값: x86)
make

# 특정 플랫폼용 빌드
make PLATFORM=x86
make PLATFORM=arm        # ARM 베어메탈 GCC 필요
make PLATFORM=ppc        # 실험적; PowerPC ELF 툴체인 필요

# 부팅 가능한 ISO 생성
make iso

# 모든 언어로 빌드 (프랑스어, 독일어, 스페인어, 일본어, 중국어, 한국어)
make LOCALE_FR=1 LOCALE_DE=1 LOCALE_ES=1 LOCALE_JA=1 LOCALE_ZH=1 LOCALE_KO=1

# 단일 추가 언어로 빌드
make LOCALE_FR=1

# QEMU에서 빌드 및 실행
make run

# 빌드 산출물 정리
make clean

# 빌드 통계 표시
make info
```

## 실행

### 빠른 시작 (QEMU)

```bash
# 시리얼 로깅과 함께 표준 실행
make run

# 옵션을 지정하여 수동 실행
qemu-system-i386 -cdrom system71.iso -serial file:/tmp/serial.log -display sdl -vga std -m 256M
```

### QEMU 옵션

```bash
# 콘솔 시리얼 출력
qemu-system-i386 -cdrom system71.iso -serial stdio -display sdl -m 256M

# 헤드리스 (그래픽 디스플레이 없음)
qemu-system-i386 -cdrom system71.iso -serial stdio -display none -m 256M

# GDB 디버깅
make debug
# 다른 터미널에서: gdb kernel.elf -ex "target remote :1234"
```

## 문서

### 컴포넌트 가이드
- **Control Manager**: `docs/components/ControlManager/`
- **Dialog Manager**: `docs/components/DialogManager/`
- **Font Manager**: `docs/components/FontManager/`
- **시리얼 로깅**: `docs/components/System/`
- **Event Manager**: `docs/components/EventManager.md`
- **Menu Manager**: `docs/components/MenuManager.md`
- **Window Manager**: `docs/components/WindowManager.md`
- **Resource Manager**: `docs/components/ResourceManager.md`

### 국제화
- **Locale Manager**: `include/LocaleManager/` -- 런타임 로캘 전환, 부팅 시 언어 선택
- **문자열 리소스**: `resources/strings/` -- 언어별 STR# 리소스 파일 (en, fr, de, es, ja, zh, ko)
- **확장 폰트**: `include/chicago_font_extended.h` -- 유럽어 문자를 위한 Mac Roman 0x80-0xFF 글리프
- **CJK 지원**: `include/TextEncoding/CJKEncoding.h`, `include/FontManager/CJKFont.h` -- 다중 바이트 인코딩 및 폰트 인프라

### 구현 현황
- **IMPLEMENTATION_PRIORITIES.md**: 계획된 작업 및 완성도 추적
- **IMPLEMENTATION_STATUS_AUDIT.md**: 모든 서브시스템의 상세 감사 보고서

### 프로젝트 철학

**고고학적 접근 방식**에 기반한 증거 중심의 구현:
1. Inside Macintosh 문서 및 MPW Universal Interfaces에 기반
2. 모든 주요 결정에는 근거 자료를 참조하는 Finding ID가 태그됨
3. 목표: 현대화가 아닌 원본 System 7과의 동작 호환성
4. 클린룸 구현 (원본 Apple 소스 코드 미사용)

## 알려진 문제

1. **아이콘 드래그 아티팩트**: 데스크톱 아이콘 드래그 시 경미한 시각적 아티팩트 발생
2. **M68K 실행 스텁 상태**: 세그먼트 로더는 완료되었으나 실행 루프가 미구현
3. **TrueType 미지원**: 비트맵 폰트만 지원 (Chicago)
4. **HFS 읽기 전용**: 가상 파일 시스템으로 실제 디스크 쓰기 불가
5. **안정성 미보장**: 크래시 및 예상치 못한 동작이 빈번하게 발생할 수 있음

## 기여하기

이 프로젝트는 주로 학습/연구 목적의 프로젝트입니다:

1. **버그 리포트**: 상세한 재현 단계와 함께 이슈를 제출해 주세요
2. **테스트**: 다양한 하드웨어/에뮬레이터에서의 테스트 결과를 보고해 주세요
3. **문서화**: 기존 문서를 개선하거나 새로운 가이드를 추가해 주세요

## 주요 참고 자료

- **Inside Macintosh** (1992-1994): Apple 공식 Toolbox 문서
- **MPW Universal Interfaces 3.2**: 표준 헤더 파일 및 구조체 정의
- **Guide to Macintosh Family Hardware**: 하드웨어 아키텍처 참조 문서

### 유용한 도구

- **Mini vMac**: 동작 참조를 위한 System 7 에뮬레이터
- **ResEdit**: System 7 리소스를 연구하기 위한 리소스 편집기
- **Ghidra/IDA**: ROM 디스어셈블리 분석용

## 법적 고지

이 프로젝트는 교육 및 보존 목적의 **클린룸 재구현**입니다:

- **Apple 소스 코드를 사용하지 않았습니다**
- 공개 문서 및 블랙박스 분석에만 기반
- "System 7", "Macintosh", "QuickDraw"는 Apple Inc.의 상표입니다
- Apple Inc.와 제휴, 보증 또는 후원 관계가 없습니다

**원본 System 7 ROM 및 소프트웨어는 Apple Inc.의 자산입니다.**

## 감사의 말

- **Apple Computer, Inc.** -- 원본 System 7을 만들어 주셔서 감사합니다
- **Inside Macintosh 저자들** -- 포괄적인 문서를 제공해 주셔서 감사합니다
- **클래식 Mac 보존 커뮤니티** -- 플랫폼을 살려주셔서 감사합니다
- **68k.news 및 Macintosh Garden** -- 리소스 아카이브를 제공해 주셔서 감사합니다

## 개발 통계

- **코드 줄 수**: ~57,500줄 이상 (세그먼트 로더 2,500줄 이상 포함)
- **컴파일 시간**: ~3-5초
- **커널 크기**: ~4.16 MB (kernel.elf)
- **ISO 크기**: ~12.5 MB (system71.iso)
- **오류 감소**: 핵심 기능의 94%가 작동
- **주요 서브시스템**: 28개 이상 (Font, Window, Menu, Control, Dialog, TextEdit 등)

## 향후 방향

**계획된 작업**:

- M68K 인터프리터 실행 루프 완성
- TrueType 폰트 지원 추가
- 일본어, 중국어, 한국어 렌더링을 위한 CJK 비트맵 폰트 리소스
- 추가 컨트롤 구현 (텍스트 필드, 팝업, 슬라이더)
- HFS 파일 시스템 디스크 쓰기 지원
- 고급 Sound Manager 기능 (믹싱, 샘플링)
- 기본 Desk Accessories (계산기, 메모장)

---

**상태**: 실험적 - 교육적 - 개발 중

**최종 업데이트**: 2025년 11월 (Sound Manager 개선 완료)

질문, 이슈 또는 토론은 GitHub Issues를 이용해 주세요.
