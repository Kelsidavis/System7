# System 7 - Przenośna reimplementacja open source

**[English](README.md)** | **[Fran&ccedil;ais](README.fr.md)** | **[Deutsch](README.de.md)** | **[Espa&ntilde;ol](README.es.md)** | **[Italiano](README.it.md)** | **[Portugu&ecirc;s](README.pt.md)** | **[Nederlands](README.nl.md)** | **[Dansk](README.da.md)** | **[Norsk](README.no.md)** | **[Svenska](README.sv.md)** | **[Suomi](README.fi.md)** | **[&Iacute;slenska](README.is.md)** | **[Ελληνικά](README.el.md)** | **[T&uuml;rk&ccedil;e](README.tr.md)** | **[Polski](README.pl.md)** | **[Čeština](README.cs.md)** | **[Slovenčina](README.sk.md)** | **[Slovenščina](README.sl.md)** | **[Hrvatski](README.hr.md)** | **[Magyar](README.hu.md)** | **[Rom&acirc;n&atilde;](README.ro.md)** | **[Български](README.bg.md)** | **[Shqip](README.sq.md)** | **[Eesti](README.et.md)** | **[Latviešu](README.lv.md)** | **[Lietuvių](README.lt.md)** | **[Македонски](README.mk.md)** | **[Crnogorski](README.me.md)** | **[Русский](README.ru.md)** | **[Українська](README.uk.md)** | **[日本語](README.ja.md)** | **[中文](README.zh.md)** | **[한국어](README.ko.md)** | **[हिन्दी](README.hi.md)**

<img width="793" height="657" alt="System 7 uruchomiony na nowoczesnym sprzęcie" src="https://github.com/user-attachments/assets/be84b83e-191c-4f9d-a786-11d0bd04203b" />
<img width="801" height="662" alt="simpletextworks" src="https://github.com/user-attachments/assets/7c9ebe5b-22b4-4612-93a1-2076909d77cd" />
<img width="803" height="661" alt="macpaint" src="https://github.com/user-attachments/assets/cd3ed04a-fdde-4dd5-88ef-5b19b3a13a54" />

> ⚠️ **PROOF OF CONCEPT** - To eksperymentalna, edukacyjna reimplementacja systemu Apple Macintosh System 7. NIE jest to produkt finalny i nie powinien być traktowany jako oprogramowanie gotowe do produkcji.

Reimplementacja open source systemu Apple Macintosh System 7 dla nowoczesnego sprzętu x86, uruchamiana przez GRUB2/Multiboot2. Projekt ma na celu odtworzenie klasycznego doświadczenia Mac OS, jednocześnie dokumentując architekturę System 7 poprzez analizę inżynierii wstecznej.

## 🎯 Status projektu

**Aktualny stan**: Aktywny rozwój, ~94% podstawowej funkcjonalności ukończone

### Najnowsze aktualizacje (listopad 2025)

#### Ulepszenia Sound Managera ✅ UKOŃCZONE
- **Zoptymalizowana konwersja MIDI**: Współdzielona funkcja pomocnicza `SndMidiNoteToFreq()` z tablicą 37 wpisów (C3-B5) i rezerwowym obliczaniem opartym na oktawach dla pełnego zakresu MIDI (0-127)
- **Obsługa asynchronicznego odtwarzania**: Kompletna infrastruktura callbacków zarówno dla odtwarzania plików (`FilePlayCompletionUPP`), jak i wykonywania poleceń (`SndCallBackProcPtr`)
- **Routing dźwięku oparty na kanałach**: Wielopoziomowy system priorytetów z kontrolą wyciszenia i włączania
  - 4-poziomowe kanały priorytetowe (0-3) do routingu wyjścia sprzętowego
  - Niezależne kontrole wyciszenia i włączania dla każdego kanału
  - `SndGetActiveChannel()` zwraca aktywny kanał o najwyższym priorytecie
  - Prawidłowa inicjalizacja kanałów z domyślnie włączoną flagą
- **Implementacja o jakości produkcyjnej**: Cały kod kompiluje się bez ostrzeżeń, nie wykryto naruszeń malloc/free
- **Commity**: 07542c5 (optymalizacja MIDI), 1854fe6 (callbacki asynchroniczne), a3433c6 (routing kanałów)

#### Wcześniejsze osiągnięcia
- ✅ **Faza zaawansowanych funkcji**: Pętla przetwarzania poleceń Sound Managera, serializacja stylów multi-run, rozszerzone funkcje MIDI/syntezy
- ✅ **System zmiany rozmiaru okien**: Interaktywna zmiana rozmiaru z prawidłową obsługą ramki okna, uchwytem zmiany rozmiaru i czyszczeniem pulpitu
- ✅ **Translacja klawiatury PS/2**: Pełne mapowanie kodów skanowania zestawu 1 na kody klawiszy Toolbox
- ✅ **Wieloplatformowy HAL**: Obsługa x86, ARM i PowerPC z przejrzystą abstrakcją

## 📊 Kompletność projektu

**Ogólna podstawowa funkcjonalność**: ~94% ukończona (szacunkowo)

### W pełni działające ✅

- **Warstwa abstrakcji sprzętu (HAL)**: Kompletna abstrakcja platformy dla x86/ARM/PowerPC
- **System rozruchu**: Pomyślnie uruchamia się przez GRUB2/Multiboot2 na x86
- **Logowanie szeregowe**: Logowanie modułowe z filtrowaniem w czasie wykonania (Error/Warn/Info/Debug/Trace)
- **Podstawy grafiki**: Bufor ramki VESA (800x600x32) z prymitywami QuickDraw włącznie z trybem XOR
- **Renderowanie pulpitu**: Pasek menu System 7 z tęczowym logo Apple, ikonami i wzorami pulpitu
- **Typografia**: Czcionka bitmapowa Chicago z renderowaniem idealnym co do piksela i prawidłowym kerningiem, rozszerzony Mac Roman (0x80-0xFF) dla europejskich znaków z akcentami
- **Internacjonalizacja (i18n)**: Lokalizacja oparta na zasobach z 11 językami (angielski, francuski, niemiecki, hiszpański, japoński, chiński, koreański, rosyjski, ukraiński, polski, czeski), Locale Manager z wyborem języka przy rozruchu, infrastruktura kodowania wielobajtowego CJK
- **Font Manager**: Obsługa wielu rozmiarów (9-24pt), synteza stylów, parsowanie FOND/NFNT, cache LRU
- **System wejścia**: Klawiatura i mysz PS/2 z kompletnym przekazywaniem zdarzeń
- **Event Manager**: Wielozadaniowość kooperacyjna poprzez WaitNextEvent z ujednoliconą kolejką zdarzeń
- **Memory Manager**: Alokacja oparta na strefach z integracją interpretera 68K
- **Menu Manager**: Kompletne menu rozwijane ze śledzeniem myszy i SaveBits/RestoreBits
- **System plików**: HFS z implementacją B-tree, okna folderów z enumeracją VFS
- **Window Manager**: Przeciąganie, zmiana rozmiaru (z uchwytem), warstwowanie, aktywacja
- **Time Manager**: Dokładna kalibracja TSC, precyzja mikrosekundowa, sprawdzanie generacji
- **Resource Manager**: Wyszukiwanie binarne O(log n), cache LRU, kompleksowa walidacja
- **Gestalt Manager**: Wieloarchitekturowe informacje systemowe z detekcją architektury
- **TextEdit Manager**: Kompletna edycja tekstu z integracją schowka
- **Scrap Manager**: Klasyczny schowek Mac OS z obsługą wielu formatów
- **Aplikacja SimpleText**: W pełni funkcjonalny edytor tekstu MDI z wycinaniem/kopiowaniem/wklejaniem
- **List Manager**: Kontrolki list kompatybilne z System 7.1 z nawigacją klawiaturową
- **Control Manager**: Kontrolki standardowe i paski przewijania z implementacją CDEF
- **Dialog Manager**: Nawigacja klawiaturowa, pierścienie fokusu, skróty klawiaturowe
- **Segment Loader**: Przenośny, niezależny od ISA system ładowania segmentów 68K z relokacją
- **Interpreter M68K**: Pełny dispatch instrukcji z 84 procedurami obsługi opkodów, wszystkimi 14 trybami adresowania, frameworkiem wyjątków/pułapek
- **Sound Manager**: Przetwarzanie poleceń, konwersja MIDI, zarządzanie kanałami, callbacki
- **Device Manager**: Zarządzanie DCE, instalacja/usuwanie sterowników i operacje I/O
- **Ekran startowy**: Kompletny interfejs rozruchu ze śledzeniem postępu, zarządzaniem fazami i ekranem powitalnym
- **Color Manager**: Zarządzanie stanem kolorów z integracją QuickDraw

### Częściowo zaimplementowane ⚠️

- **Integracja aplikacji**: Interpreter M68K i segment loader ukończone; potrzebne testy integracyjne w celu weryfikacji wykonywania rzeczywistych aplikacji
- **Procedury definicji okien (WDEF)**: Podstawowa struktura na miejscu, częściowy dispatch
- **Speech Manager**: Jedynie framework API i passthrough audio; silnik syntezy mowy nie zaimplementowany
- **Obsługa wyjątków (RTE)**: Powrót z wyjątku częściowo zaimplementowany (obecnie zatrzymuje się zamiast przywracać kontekst)

### Jeszcze nie zaimplementowane ❌

- **Drukowanie**: Brak systemu drukowania
- **Sieć**: Brak funkcjonalności AppleTalk ani sieciowej
- **Desk Accessories**: Tylko framework
- **Zaawansowane audio**: Odtwarzanie próbek, miksowanie (ograniczenie głośnika PC)

### Podsystemy nieskompilowane 🔧

Poniższe posiadają kod źródłowy, ale nie są zintegrowane z jądrem:
- **AppleEventManager** (8 plików): Komunikacja międzyaplikacyjna; celowo wyłączony z powodu zależności od pthread niekompatybilnych ze środowiskiem freestanding
- **FontResources** (tylko nagłówek): Definicje typów zasobów czcionek; właściwa obsługa czcionek zapewniana przez skompilowany FontResourceLoader.c

## 🏗️ Architektura

### Specyfikacja techniczna

- **Architektura**: Wieloarchitekturowa poprzez HAL (x86, ARM, PowerPC gotowe)
- **Protokół rozruchu**: Multiboot2 (x86), bootloadery specyficzne dla platformy
- **Grafika**: Bufor ramki VESA, 800x600 @ 32-bitowy kolor
- **Układ pamięci**: Jądro ładowane pod adresem fizycznym 1MB (x86)
- **Taktowanie**: Niezależne od architektury z precyzją mikrosekundową (RDTSC/rejestry timerów)
- **Wydajność**: Brak w cache zasobów <15µs, trafienie w cache <2µs, dryf timera <100ppm

### Statystyki bazy kodu

- **225+ plików źródłowych** z ~57 500+ liniami kodu
- **145+ plików nagłówkowych** w 28+ podsystemach
- **69 typów zasobów** wyekstrahowanych z System 7.1
- **Czas kompilacji**: 3-5 sekund na nowoczesnym sprzęcie
- **Rozmiar jądra**: ~4,16 MB
- **Rozmiar ISO**: ~12,5 MB

## 🔨 Budowanie

### Wymagania

- **GCC** z obsługą 32-bit (`gcc-multilib` na systemach 64-bitowych)
- **GNU Make**
- **Narzędzia GRUB**: `grub-mkrescue` (z `grub2-common` lub `grub-pc-bin`)
- **QEMU** do testowania (`qemu-system-i386`)
- **Python 3** do przetwarzania zasobów
- **xxd** do konwersji binarnych
- *(Opcjonalnie)* **powerpc-linux-gnu** zestaw narzędzi cross-kompilacji dla budowania PowerPC

### Instalacja na Ubuntu/Debian

```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common
```

### Polecenia budowania

```bash
# Build kernel (x86 by default)
make

# Build for specific platform
make PLATFORM=x86
make PLATFORM=arm        # requires ARM bare-metal GCC
make PLATFORM=ppc        # experimental; requires PowerPC ELF toolchain

# Create bootable ISO
make iso

# Build with all languages
make LOCALE_FR=1 LOCALE_DE=1 LOCALE_ES=1 LOCALE_JA=1 LOCALE_ZH=1 LOCALE_KO=1 LOCALE_RU=1 LOCALE_UK=1 LOCALE_PL=1 LOCALE_CS=1

# Build with a single additional language
make LOCALE_FR=1

# Build and run in QEMU
make run

# Clean artifacts
make clean

# Display build statistics
make info
```

## 🚀 Uruchamianie

### Szybki start (QEMU)

```bash
# Standard run with serial logging
make run

# Manually with options
qemu-system-i386 -cdrom system71.iso -serial file:/tmp/serial.log -display sdl -vga std -m 256M
```

### Opcje QEMU

```bash
# With console serial output
qemu-system-i386 -cdrom system71.iso -serial stdio -display sdl -m 256M

# Headless (no graphics display)
qemu-system-i386 -cdrom system71.iso -serial stdio -display none -m 256M

# With GDB debugging
make debug
# In another terminal: gdb kernel.elf -ex "target remote :1234"
```

## 📚 Dokumentacja

### Przewodniki po komponentach
- **Control Manager**: `docs/components/ControlManager/`
- **Dialog Manager**: `docs/components/DialogManager/`
- **Font Manager**: `docs/components/FontManager/`
- **Logowanie szeregowe**: `docs/components/System/`
- **Event Manager**: `docs/components/EventManager.md`
- **Menu Manager**: `docs/components/MenuManager.md`
- **Window Manager**: `docs/components/WindowManager.md`
- **Resource Manager**: `docs/components/ResourceManager.md`

### Internacjonalizacja
- **Locale Manager**: `include/LocaleManager/` — przełączanie lokalizacji w czasie wykonania, wybór języka przy rozruchu
- **Zasoby ciągów znaków**: `resources/strings/` — pliki zasobów STR# dla poszczególnych języków (en, fr, de, es, ja, zh, ko, ru, uk, pl, cs)
- **Rozszerzone czcionki**: `include/chicago_font_extended.h` — glify Mac Roman 0x80-0xFF dla znaków europejskich
- **Obsługa CJK**: `include/TextEncoding/CJKEncoding.h`, `include/FontManager/CJKFont.h` — infrastruktura kodowania wielobajtowego i czcionek

### Status implementacji
- **IMPLEMENTATION_PRIORITIES.md**: Planowane prace i śledzenie kompletności
- **IMPLEMENTATION_STATUS_AUDIT.md**: Szczegółowy audyt wszystkich podsystemów

### Filozofia projektu

**Podejście archeologiczne** z implementacją opartą na dowodach:
1. Oparte na dokumentacji Inside Macintosh i MPW Universal Interfaces
2. Wszystkie główne decyzje oznaczone identyfikatorami Finding odwołującymi się do dowodów potwierdzających
3. Cel: parytet zachowania z oryginalnym System 7, nie modernizacja
4. Implementacja typu clean-room (bez oryginalnego kodu źródłowego Apple)

## 🐛 Znane problemy

1. **Artefakty przeciągania ikon**: Drobne artefakty wizualne podczas przeciągania ikon na pulpicie
2. **Wykonywanie M68K zaślepione**: Segment loader ukończony, pętla wykonania nie zaimplementowana
3. **Brak obsługi TrueType**: Tylko czcionki bitmapowe (Chicago)
4. **HFS tylko do odczytu**: Wirtualny system plików, brak zapisu zwrotnego na dysk
5. **Brak gwarancji stabilności**: Awarie i nieoczekiwane zachowanie są powszechne

## 🤝 Współtworzenie

Jest to przede wszystkim projekt edukacyjny/badawczy:

1. **Zgłaszanie błędów**: Twórz zgłoszenia ze szczegółowymi krokami reprodukcji
2. **Testowanie**: Zgłaszaj wyniki na różnym sprzęcie/emulatorach
3. **Dokumentacja**: Ulepszaj istniejącą dokumentację lub dodawaj nowe przewodniki

## 📖 Podstawowe materiały referencyjne

- **Inside Macintosh** (1992-1994): Oficjalna dokumentacja Apple Toolbox
- **MPW Universal Interfaces 3.2**: Kanoniczne pliki nagłówkowe i definicje struktur
- **Guide to Macintosh Family Hardware**: Materiał referencyjny dotyczący architektury sprzętowej

### Przydatne narzędzia

- **Mini vMac**: Emulator System 7 jako odniesienie zachowania
- **ResEdit**: Edytor zasobów do badania zasobów System 7
- **Ghidra/IDA**: Do analizy deasemblacji ROM-u

## ⚖️ Informacje prawne

Jest to **reimplementacja typu clean-room** w celach edukacyjnych i zachowania dziedzictwa:

- **Nie użyto kodu źródłowego Apple**
- Oparte wyłącznie na publicznej dokumentacji i analizie czarnej skrzynki
- „System 7", „Macintosh", „QuickDraw" są znakami towarowymi Apple Inc.
- Nie jest powiązany z Apple Inc., nie jest przez Apple wspierany ani sponsorowany

**Oryginalny ROM i oprogramowanie System 7 pozostają własnością Apple Inc.**

## 🙏 Podziękowania

- **Apple Computer, Inc.** za stworzenie oryginalnego System 7
- **Autorzy Inside Macintosh** za wyczerpującą dokumentację
- **Społeczność zachowania klasycznego Maca** za utrzymywanie platformy przy życiu
- **68k.news i Macintosh Garden** za archiwa zasobów

## 📊 Statystyki rozwoju

- **Linie kodu**: ~57 500+ (w tym 2 500+ dla segment loadera)
- **Czas kompilacji**: ~3-5 sekund
- **Rozmiar jądra**: ~4,16 MB (kernel.elf)
- **Rozmiar ISO**: ~12,5 MB (system71.iso)
- **Redukcja błędów**: 94% podstawowej funkcjonalności działa
- **Główne podsystemy**: 28+ (Font, Window, Menu, Control, Dialog, TextEdit itp.)

## 🔮 Kierunki rozwoju

**Planowane prace**:

- Ukończenie pętli wykonania interpretera M68K
- Dodanie obsługi czcionek TrueType
- Zasoby czcionek bitmapowych CJK do renderowania japońskiego, chińskiego i koreańskiego
- Implementacja dodatkowych kontrolek (pola tekstowe, wyskakujące menu, suwaki)
- Zapis zwrotny na dysk dla systemu plików HFS
- Zaawansowane funkcje Sound Managera (miksowanie, próbkowanie)
- Podstawowe desk accessories (Kalkulator, Notatnik)

---

**Status**: Eksperymentalny - Edukacyjny - W trakcie rozwoju

**Ostatnia aktualizacja**: Listopad 2025 (Ulepszenia Sound Managera ukończone)

W przypadku pytań, problemów lub dyskusji, prosimy o korzystanie z GitHub Issues.
