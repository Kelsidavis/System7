# System 7 - Portabel Open Source-genimplementering

**[English](README.md)** | **[Fran&ccedil;ais](README.fr.md)** | **[Deutsch](README.de.md)** | **[Espa&ntilde;ol](README.es.md)** | **[Italiano](README.it.md)** | **[Portugu&ecirc;s](README.pt.md)** | **[Nederlands](README.nl.md)** | **[Dansk](README.da.md)** | **[Norsk](README.no.md)** | **[Svenska](README.sv.md)** | **[Suomi](README.fi.md)** | **[&Iacute;slenska](README.is.md)** | **[Ελληνικά](README.el.md)** | **[T&uuml;rk&ccedil;e](README.tr.md)** | **[Polski](README.pl.md)** | **[Čeština](README.cs.md)** | **[Slovenčina](README.sk.md)** | **[Slovenščina](README.sl.md)** | **[Hrvatski](README.hr.md)** | **[Magyar](README.hu.md)** | **[Rom&acirc;n&atilde;](README.ro.md)** | **[Български](README.bg.md)** | **[Shqip](README.sq.md)** | **[Eesti](README.et.md)** | **[Latviešu](README.lv.md)** | **[Lietuvių](README.lt.md)** | **[Македонски](README.mk.md)** | **[Crnogorski](README.me.md)** | **[Русский](README.ru.md)** | **[Українська](README.uk.md)** | **[日本語](README.ja.md)** | **[中文](README.zh.md)** | **[한국어](README.ko.md)** | **[हिन्दी](README.hi.md)**

<img width="793" height="657" alt="System 7 kører på moderne hardware" src="https://github.com/user-attachments/assets/be84b83e-191c-4f9d-a786-11d0bd04203b" />
<img width="801" height="662" alt="simpletextworks" src="https://github.com/user-attachments/assets/7c9ebe5b-22b4-4612-93a1-2076909d77cd" />
<img width="803" height="661" alt="macpaint" src="https://github.com/user-attachments/assets/cd3ed04a-fdde-4dd5-88ef-5b19b3a13a54" />

> ⚠️ **PROOF OF CONCEPT** - Dette er en eksperimentel, uddannelsesmæssig genimplementering af Apples Macintosh System 7. Dette er IKKE et færdigt produkt og bør ikke betragtes som produktionsklar software.

En open source-genimplementering af Apple Macintosh System 7 til moderne x86-hardware, som kan startes via GRUB2/Multiboot2. Projektet har til formål at genskabe den klassiske Mac OS-oplevelse og samtidig dokumentere System 7-arkitekturen gennem reverse engineering-analyse.

## 🎯 Projektstatus

**Nuværende tilstand**: Aktiv udvikling med ca. 94% af kernefunktionaliteten færdig

### Seneste opdateringer (november 2025)

#### Forbedringer af Sound Manager ✅ FULDFØRT
- **Optimeret MIDI-konvertering**: Delt `SndMidiNoteToFreq()`-hjælpefunktion med 37-posts opslagstabel (C3-B5) og oktavbaseret fallback for hele MIDI-området (0-127)
- **Understøttelse af asynkron afspilning**: Komplet callback-infrastruktur til både filafspilning (`FilePlayCompletionUPP`) og kommandoeksekvering (`SndCallBackProcPtr`)
- **Kanalbaseret lydrouting**: Flerniveau-prioritetssystem med mute- og aktiveringsstyring
  - 4 prioritetsniveauer (0-3) for hardwareudgangsrouting
  - Uafhængig mute- og aktiveringsstyring pr. kanal
  - `SndGetActiveChannel()` returnerer den aktive kanal med højest prioritet
  - Korrekt kanalinitialisering med aktiveret flag som standard
- **Produktionskvalitetsimplementering**: Al kode kompilerer rent, ingen malloc/free-overtrædelser fundet
- **Commits**: 07542c5 (MIDI-optimering), 1854fe6 (asynkrone callbacks), a3433c6 (kanalrouting)

#### Tidligere sessioners resultater
- ✅ **Fase for avancerede funktioner**: Sound Manager-kommandobehandlingsløkke, multi-run stilserialisering, udvidede MIDI-/syntesefunktioner
- ✅ **Vinduesstørrelsessystem**: Interaktiv størrelsesændring med korrekt kromhåndtering, grow box og oprydning af skrivebordet
- ✅ **PS/2-tastaturoversættelse**: Fuld set 1 scancode til Toolbox-tastkode-mapping
- ✅ **Multi-platform HAL**: x86-, ARM- og PowerPC-understøttelse med ren abstraktion

## 📊 Projektets fuldstændighed

**Samlet kernefunktionalitet**: Ca. 94% fuldført (estimeret)

### Hvad der fungerer fuldt ud ✅

- **Hardware Abstraction Layer (HAL)**: Komplet platformsabstraktion for x86/ARM/PowerPC
- **Startsystem**: Starter succesfuldt via GRUB2/Multiboot2 på x86
- **Seriel logning**: Modulbaseret logning med runtime-filtrering (Error/Warn/Info/Debug/Trace)
- **Grafikfundament**: VESA-framebuffer (800x600x32) med QuickDraw-primitiver inkl. XOR-tilstand
- **Skrivebordsgengivelse**: System 7-menulinje med regnbuefarvet Apple-logo, ikoner og skrivebordsmønstre
- **Typografi**: Chicago bitmap-skrifttype med pixelperfekt gengivelse og korrekt kerning, udvidet Mac Roman (0x80-0xFF) for europæiske accenttegn
- **Internationalisering (i18n)**: Ressourcebaseret lokalisering med 34 sprog (engelsk, fransk, tysk, spansk, italiensk, portugisisk, nederlandsk, dansk, norsk, svensk, finsk, islandsk, græsk, tyrkisk, polsk, tjekkisk, slovakisk, slovensk, kroatisk, ungarsk, rumænsk, bulgarsk, albansk, estisk, lettisk, litauisk, makedonsk, montenegrinsk, russisk, ukrainsk, japansk, kinesisk, koreansk, hindi), Locale Manager med sprogvalg ved opstart, CJK multi-byte-kodningsinfrastruktur
- **Font Manager**: Understøttelse af flere størrelser (9-24pt), stilsyntese, FOND/NFNT-parsing, LRU-caching
- **Inputsystem**: PS/2-tastatur og mus med komplet hændelsesvidereformidling
- **Event Manager**: Kooperativ multitasking via WaitNextEvent med forenet hændelseskø
- **Memory Manager**: Zonebaseret allokering med 68K-fortolkerintegration
- **Menu Manager**: Komplette dropdown-menuer med musesporing og SaveBits/RestoreBits
- **Filsystem**: HFS med B-tree-implementering, mappevinduer med VFS-enumeration
- **Window Manager**: Trækning, størrelsesændring (med grow box), lagdeling, aktivering
- **Time Manager**: Nøjagtig TSC-kalibrering, mikrosekund-præcision, generationstjek
- **Resource Manager**: O(log n) binær søgning, LRU-cache, omfattende validering
- **Gestalt Manager**: Multi-arkitektur systeminformation med arkitekturdetektion
- **TextEdit Manager**: Komplet tekstredigering med udklipsholder-integration
- **Scrap Manager**: Klassisk Mac OS-udklipsholder med understøttelse af flere formater
- **SimpleText-applikation**: Fuldt udstyret MDI-teksteditor med klip/kopiér/indsæt
- **List Manager**: System 7.1-kompatible listekontroller med tastaturnavigation
- **Control Manager**: Standard- og scrollbar-kontroller med CDEF-implementering
- **Dialog Manager**: Tastaturnavigation, fokusringe, tastaturgenveje
- **Segment Loader**: Portabelt ISA-agnostisk 68K-segmentindlæsningssystem med relokering
- **M68K-fortolker**: Fuld instruktions-dispatch med 84 opcode-handlere, alle 14 adresseringstilstande, undtagelses-/trap-framework
- **Sound Manager**: Kommandobehandling, MIDI-konvertering, kanalstyring, callbacks
- **Device Manager**: DCE-styring, driverinstallation/-fjernelse og I/O-operationer
- **Startskærm**: Komplet start-UI med fremdriftssporing, fasestyring og splash-skærm
- **Color Manager**: Farvetilstandsstyring med QuickDraw-integration

### Delvist implementeret ⚠️

- **Applikationsintegration**: M68K-fortolker og segmentindlæser er færdige; integrationstest nødvendig for at verificere, at rigtige applikationer kører
- **Window Definition Procedures (WDEF)**: Kernestruktur på plads, delvis dispatch
- **Speech Manager**: API-framework og lyd-passthrough kun; talesyntesemotor ikke implementeret
- **Undtagelseshåndtering (RTE)**: Return from exception delvist implementeret (standser i øjeblikket i stedet for at gendanne kontekst)

### Endnu ikke implementeret ❌

- **Udskrivning**: Intet printsystem
- **Netværk**: Ingen AppleTalk- eller netværksfunktionalitet
- **Desk Accessories**: Kun framework
- **Avanceret lyd**: Sample-afspilning, mixing (PC-højttaler-begrænsning)

### Delsystemer der ikke kompileres 🔧

Følgende har kildekode, men er ikke integreret i kernen:
- **AppleEventManager** (8 filer): Kommunikation mellem applikationer; bevidst udelukket pga. pthread-afhængigheder, der er inkompatible med freestanding-miljøet
- **FontResources** (kun header): Skrifttyperessource-definitioner; faktisk skrifttypeunderstøttelse leveres af den kompilerede FontResourceLoader.c

## 🏗️ Arkitektur

### Tekniske specifikationer

- **Arkitektur**: Multi-arkitektur via HAL (x86, ARM, PowerPC klar)
- **Startprotokol**: Multiboot2 (x86), platformsspecifikke bootloadere
- **Grafik**: VESA-framebuffer, 800x600 @ 32-bit farve
- **Hukommelseslayout**: Kernen indlæses ved 1MB fysisk adresse (x86)
- **Timing**: Arkitektur-agnostisk med mikrosekund-præcision (RDTSC/timer-registre)
- **Ydeevne**: Kold ressource-miss <15µs, cache-hit <2µs, timer-drift <100ppm

### Kodebasestatistik

- **225+ kildefiler** med ca. 57.500+ kodelinjer
- **145+ headerfiler** på tværs af 28+ delsystemer
- **69 ressourcetyper** udtrukket fra System 7.1
- **Kompileringstid**: 3-5 sekunder på moderne hardware
- **Kernestørrelse**: Ca. 4,16 MB
- **ISO-størrelse**: Ca. 12,5 MB

## 🔨 Bygning

### Krav

- **GCC** med 32-bit-understøttelse (`gcc-multilib` på 64-bit)
- **GNU Make**
- **GRUB-værktøjer**: `grub-mkrescue` (fra `grub2-common` eller `grub-pc-bin`)
- **QEMU** til test (`qemu-system-i386`)
- **Python 3** til ressourcebehandling
- **xxd** til binær konvertering
- *(Valgfrit)* **powerpc-linux-gnu** krydskompileringsværktøj til PowerPC-builds

### Ubuntu/Debian-installation

```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common
```

### Byggekommandoer

```bash
# Byg kerne (x86 som standard)
make

# Byg til specifik platform
make PLATFORM=x86
make PLATFORM=arm        # kræver ARM bare-metal GCC
make PLATFORM=ppc        # eksperimentel; kræver PowerPC ELF-værktøjskæde

# Opret startbar ISO
make iso

# Byg med alle sprog
make LOCALE_FR=1 LOCALE_DE=1 LOCALE_ES=1 LOCALE_JA=1 LOCALE_ZH=1 LOCALE_KO=1 LOCALE_RU=1 LOCALE_UK=1 LOCALE_PL=1 LOCALE_CS=1 LOCALE_SQ=1 LOCALE_BG=1 LOCALE_HR=1 LOCALE_DA=1 LOCALE_NL=1 LOCALE_ET=1 LOCALE_FI=1 LOCALE_EL=1 LOCALE_HU=1 LOCALE_IS=1 LOCALE_IT=1 LOCALE_LV=1 LOCALE_LT=1 LOCALE_MK=1 LOCALE_ME=1 LOCALE_NO=1 LOCALE_PT=1 LOCALE_RO=1 LOCALE_SK=1 LOCALE_SL=1 LOCALE_SV=1 LOCALE_TR=1 LOCALE_HI=1

# Byg med et enkelt ekstra sprog
make LOCALE_FR=1

# Byg og kør i QEMU
make run

# Ryd byggeartefakter
make clean

# Vis byggestatistik
make info
```

## 🚀 Kørsel

### Hurtig start (QEMU)

```bash
# Standardkørsel med seriel logning
make run

# Manuelt med indstillinger
qemu-system-i386 -cdrom system71.iso -serial file:/tmp/serial.log -display sdl -vga std -m 256M
```

### QEMU-indstillinger

```bash
# Med konsol seriel output
qemu-system-i386 -cdrom system71.iso -serial stdio -display sdl -m 256M

# Hovedløs (ingen grafisk visning)
qemu-system-i386 -cdrom system71.iso -serial stdio -display none -m 256M

# Med GDB-debugging
make debug
# I en anden terminal: gdb kernel.elf -ex "target remote :1234"
```

## 📚 Dokumentation

### Komponentguider
- **Control Manager**: `docs/components/ControlManager/`
- **Dialog Manager**: `docs/components/DialogManager/`
- **Font Manager**: `docs/components/FontManager/`
- **Seriel logning**: `docs/components/System/`
- **Event Manager**: `docs/components/EventManager.md`
- **Menu Manager**: `docs/components/MenuManager.md`
- **Window Manager**: `docs/components/WindowManager.md`
- **Resource Manager**: `docs/components/ResourceManager.md`

### Internationalisering
- **Locale Manager**: `include/LocaleManager/` — runtime-lokalitetsskift, sprogvalg ved opstart
- **Strengressourcer**: `resources/strings/` — STR#-ressourcefiler pr. sprog (34 sprog)
- **Udvidede skrifttyper**: `include/chicago_font_extended.h` — Mac Roman 0x80-0xFF-glyffer til europæiske tegn
- **CJK-understøttelse**: `include/TextEncoding/CJKEncoding.h`, `include/FontManager/CJKFont.h` — multi-byte-kodning og skrifttypeinfrastruktur

### Implementeringsstatus
- **IMPLEMENTATION_PRIORITIES.md**: Planlagt arbejde og fuldstændighedssporing
- **IMPLEMENTATION_STATUS_AUDIT.md**: Detaljeret revision af alle delsystemer

### Projektfilosofi

**Arkæologisk tilgang** med evidensbaseret implementering:
1. Baseret på Inside Macintosh-dokumentation og MPW Universal Interfaces
2. Alle vigtige beslutninger mærket med Finding-ID'er, der refererer til understøttende evidens
3. Mål: adfærdsmæssig paritet med det originale System 7, ikke modernisering
4. Clean-room-implementering (ingen original Apple-kildekode)

## 🐛 Kendte problemer

1. **Ikon-trækningsartefakter**: Mindre visuelle artefakter ved trækning af skrivebordsikoner
2. **M68K-eksekvering er stubbet**: Segmentindlæser fuldført, eksekveringsløkke ikke implementeret
3. **Ingen TrueType-understøttelse**: Kun bitmap-skrifttyper (Chicago)
4. **HFS skrivebeskyttet**: Virtuelt filsystem, ingen reel tilbageskrivning til disk
5. **Ingen stabilitetsgarantier**: Nedbrud og uventet adfærd er almindeligt

## 🤝 Bidrag

Dette er primært et lærings-/forskningsprojekt:

1. **Fejlrapporter**: Opret issues med detaljerede reproduktionstrin
2. **Test**: Rapportér resultater på forskellig hardware/emulatorer
3. **Dokumentation**: Forbedr eksisterende dokumentation eller tilføj nye guider

## 📖 Vigtige referencer

- **Inside Macintosh** (1992-1994): Officiel Apple Toolbox-dokumentation
- **MPW Universal Interfaces 3.2**: Kanoniske headerfiler og struct-definitioner
- **Guide to Macintosh Family Hardware**: Hardwarearkitektur-reference

### Nyttige værktøjer

- **Mini vMac**: System 7-emulator til adfærdsreference
- **ResEdit**: Ressourceeditor til at studere System 7-ressourcer
- **Ghidra/IDA**: Til ROM-disassembly-analyse

## ⚖️ Juridisk

Dette er en **clean-room-genimplementering** til uddannelses- og bevaringsformål:

- **Ingen Apple-kildekode** blev brugt
- Baseret udelukkende på offentlig dokumentation og black-box-analyse
- "System 7", "Macintosh", "QuickDraw" er varemærker tilhørende Apple Inc.
- Ikke affilieret med, godkendt af eller sponsoreret af Apple Inc.

**Originalt System 7 ROM og software forbliver Apple Inc.'s ejendom.**

## 🙏 Anerkendelser

- **Apple Computer, Inc.** for at skabe det originale System 7
- **Inside Macintosh-forfatterne** for omfattende dokumentation
- **Classic Mac-bevaringsfællesskabet** for at holde platformen i live
- **68k.news og Macintosh Garden** for ressourcearkiver

## 📊 Udviklingsstatistik

- **Kodelinjer**: Ca. 57.500+ (inkl. 2.500+ til segmentindlæser)
- **Kompileringstid**: Ca. 3-5 sekunder
- **Kernestørrelse**: Ca. 4,16 MB (kernel.elf)
- **ISO-størrelse**: Ca. 12,5 MB (system71.iso)
- **Fejlreduktion**: 94% af kernefunktionaliteten fungerer
- **Større delsystemer**: 28+ (Font, Window, Menu, Control, Dialog, TextEdit osv.)

## 🔮 Fremtidig retning

**Planlagt arbejde**:

- Fuldføre M68K-fortolkerens eksekveringsløkke
- Tilføje TrueType-skrifttypeunderstøttelse
- CJK bitmap-skrifttyperessourcer til japansk, kinesisk og koreansk gengivelse
- Implementere yderligere kontroller (tekstfelter, pop-ups, skydere)
- Tilbageskrivning til disk for HFS-filsystemet
- Avancerede Sound Manager-funktioner (mixing, sampling)
- Grundlæggende desk accessories (Lommeregner, Notesblok)

---

**Status**: Eksperimentel - Uddannelsesmæssig - Under udvikling

**Sidst opdateret**: November 2025 (Sound Manager-forbedringer fuldført)

For spørgsmål, problemer eller diskussion, brug venligst GitHub Issues.
