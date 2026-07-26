# System 7 - Rizbatim Portativ me Burim të Hapur

**[English](../../README.md)** | **[Fran&ccedil;ais](README.fr.md)** | **[Deutsch](README.de.md)** | **[Espa&ntilde;ol](README.es.md)** | **[Italiano](README.it.md)** | **[Portugu&ecirc;s](README.pt.md)** | **[Nederlands](README.nl.md)** | **[Dansk](README.da.md)** | **[Norsk](README.no.md)** | **[Svenska](README.sv.md)** | **[Suomi](README.fi.md)** | **[&Iacute;slenska](README.is.md)** | **[Ελληνικά](README.el.md)** | **[T&uuml;rk&ccedil;e](README.tr.md)** | **[Polski](README.pl.md)** | **[Čeština](README.cs.md)** | **[Slovenčina](README.sk.md)** | **[Slovenščina](README.sl.md)** | **[Hrvatski](README.hr.md)** | **[Magyar](README.hu.md)** | **[Rom&acirc;n&atilde;](README.ro.md)** | **[Български](README.bg.md)** | **[Shqip](README.sq.md)** | **[Eesti](README.et.md)** | **[Latviešu](README.lv.md)** | **[Lietuvių](README.lt.md)** | **[Македонски](README.mk.md)** | **[Crnogorski](README.me.md)** | **[Русский](README.ru.md)** | **[Українська](README.uk.md)** | **[日本語](README.ja.md)** | **[中文](README.zh.md)** | **[한국어](README.ko.md)** | **[हिन्दी](README.hi.md)**

<img width="793" height="657" alt="System 7 duke funksionuar në pajisje moderne" src="https://github.com/user-attachments/assets/be84b83e-191c-4f9d-a786-11d0bd04203b" />
<img width="801" height="662" alt="simpletextworks" src="https://github.com/user-attachments/assets/7c9ebe5b-22b4-4612-93a1-2076909d77cd" />
<img width="803" height="661" alt="macpaint" src="https://github.com/user-attachments/assets/cd3ed04a-fdde-4dd5-88ef-5b19b3a13a54" />

> ⚠️ **PROVË KONCEPTI** - Ky është një rizbatim eksperimental dhe edukativ i Apple Macintosh System 7. Ky NUK është një produkt i përfunduar dhe nuk duhet të konsiderohet softuer i gatshëm për përdorim.

Një rizbatim me burim të hapur i Apple Macintosh System 7 për pajisje moderne x86, i nisshëm përmes GRUB2/Multiboot2. Ky projekt synon të rikrijojë përvojën klasike të Mac OS duke dokumentuar arkitekturën e System 7 përmes analizës së inxhinierisë së kundërt.

## 🎯 Gjendja e Projektit

**Gjendja Aktuale**: Zhvillim aktiv me ~94% të funksionalitetit bazë të përfunduar

### Përditësimet e Fundit (Nëntor 2025)

#### Përmirësimet e Menaxherit të Zërit ✅ TË PËRFUNDUARA
- **Konvertim i optimizuar MIDI**: Funksioni ndihmës i përbashkët `SndMidiNoteToFreq()` me tabelë kërkimi prej 37 hyrjesh (C3-B5) dhe rikthim bazuar në oktavë për gamën e plotë MIDI (0-127)
- **Mbështetje për luajtje asinkrone**: Infrastrukturë e plotë thirrjesh kthyese për luajtjen e skedarëve (`FilePlayCompletionUPP`) dhe ekzekutimin e komandave (`SndCallBackProcPtr`)
- **Drejtim audio me bazë kanalesh**: Sistem prioritetesh me shumë nivele me kontrolle heshtjeje dhe aktivizimi
  - Kanale prioriteti me 4 nivele (0-3) për drejtimin e daljes së pajisjeve
  - Kontrolle të pavarura heshtjeje dhe aktivizimi për çdo kanal
  - `SndGetActiveChannel()` kthen kanalin aktiv me prioritetin më të lartë
  - Inicializim i duhur i kanalit me flamurin e aktivizimit si parazgjedhje
- **Zbatim me cilësi produksioni**: I gjithë kodi kompilohet pastër, nuk janë zbuluar shkelje malloc/free
- **Commits**: 07542c5 (optimizim MIDI), 1854fe6 (thirrje kthyese asinkrone), a3433c6 (drejtim kanalesh)

#### Arritjet e Sesioneve të Mëparshme
- ✅ **Faza e Veçorive të Avancuara**: Laku i përpunimit të komandave të Menaxherit të Zërit, serializim me shumë stile ekzekutimi, veçori të zgjeruara MIDI/sinteze
- ✅ **Sistemi i Ripërmasimit të Dritareve**: Ripërmasim interaktiv me trajtim të duhur të kuadrit, kutia e rritjes, dhe pastrimi i desktopit
- ✅ **Përkthimi i Tastierës PS/2**: Hartëzim i plotë i kodeve të skanimit të setit 1 në kodet e tasteve Toolbox
- ✅ **HAL Shumë-platformësh**: Mbështetje për x86, ARM dhe PowerPC me abstraksion të pastër

## 📊 Përfundueshmëria e Projektit

**Funksionaliteti i Përgjithshëm Bazë**: ~94% i përfunduar (i vlerësuar)

### Çfarë Funksionon Plotësisht ✅

- **Shtresa e Abstraksionit të Pajisjeve (HAL)**: Abstraksion i plotë i platformës për x86/ARM/PowerPC
- **Sistemi i Nisjes**: Niset me sukses përmes GRUB2/Multiboot2 në x86
- **Regjistrimi Serial**: Regjistrim me bazë modulesh me filtrim në kohë ekzekutimi (Gabim/Paralajmërim/Info/Korrigjim/Gjurmim)
- **Baza Grafike**: Framebuffer VESA (800x600x32) me primitiva QuickDraw përfshirë modalitetin XOR
- **Renderimi i Desktopit**: Shiriti i menusë i System 7 me logon ylberi të Apple, ikona dhe modele desktopi
- **Tipografia**: Fonti bitmap Chicago me renderim piksel-perfekt dhe kerning i duhur, Mac Roman i zgjeruar (0x80-0xFF) për karaktere evropiane me theks
- **Ndërkombëtarizimi (i18n)**: Lokalizim me bazë burimesh me 34 gjuhë (Anglisht, Frëngjisht, Gjermanisht, Spanjisht, Italisht, Portugalisht, Holandisht, Danisht, Norvegjisht, Suedisht, Finlandisht, Islandisht, Greqisht, Turqisht, Polonisht, Çekisht, Sllovakisht, Sllovenisht, Kroatisht, Hungarisht, Rumanisht, Bullgarisht, Shqip, Estonisht, Letonisht, Lituanisht, Maqedonisht, Malazezisht, Rusisht, Ukrainisht, Japonisht, Kinezisht, Koreanisht, Hindi), Menaxheri i Lokaleve me zgjedhjen e gjuhës në nisje, infrastrukturë kodimi CJK me shumë bajte
- **Menaxheri i Fonteve**: Mbështetje për shumë madhësi (9-24pt), sintezë stilesh, analizim FOND/NFNT, ruajtje LRU në memorie
- **Sistemi i Hyrjeve**: Tastierë dhe mi PS/2 me përcjellje të plotë ngjarjesh
- **Menaxheri i Ngjarjeve**: Shumëdetyrim bashkëpunues përmes WaitNextEvent me radhë të unifikuar ngjarjesh
- **Menaxheri i Kujtesës**: Alokim me bazë zonash me integrim të interpretuesit 68K
- **Menaxheri i Menuve**: Menu zbritëse të plota me ndjekje miu dhe SaveBits/RestoreBits
- **Sistemi i Skedarëve**: HFS me zbatim B-tree, dritare dosjesh me numërim VFS
- **Menaxheri i Dritareve**: Tërheqje, ripërmasim (me kutinë e rritjes), shtresëzim, aktivizim
- **Menaxheri i Kohës**: Kalibrim i saktë TSC, saktësi mikrosekondash, kontroll gjeneratash
- **Menaxheri i Burimeve**: Kërkim binar O(log n), memorie LRU, validim gjithëpërfshirës
- **Menaxheri Gestalt**: Informacion sistemi shumë-arkitekturash me zbulim arkitekture
- **Menaxheri TextEdit**: Redaktim i plotë teksti me integrim clipboard-i
- **Menaxheri Scrap**: Clipboard klasik i Mac OS me mbështetje për shumë formate
- **Aplikacioni SimpleText**: Redaktues teksti MDI me veçori të plota me prerje/kopjim/ngjitje
- **Menaxheri i Listave**: Kontrolle listash të pajtueshme me System 7.1 me navigim nga tastiera
- **Menaxheri i Kontrolleve**: Kontrolle standarde dhe shiritat e lëvizjes me zbatim CDEF
- **Menaxheri i Dialogëve**: Navigim nga tastiera, unaza fokusi, shkurtore tastiere
- **Ngarkuesi i Segmenteve**: Sistem portativ i ngarkimit të segmenteve 68K i pavarur nga ISA me rivendosje
- **Interpretuesi M68K**: Dërgim i plotë instruksionesh me 84 trajtues opcode-sh, të 14 mënyrat e adresimit, kuadri i përjashtimeve/kurtheve
- **Menaxheri i Zërit**: Përpunim komandash, konvertim MIDI, menaxhim kanalesh, thirrje kthyese
- **Menaxheri i Pajisjeve**: Menaxhim DCE, instalim/heqje drejtuesish, dhe veprime I/O
- **Ekrani i Nisjes**: Ndërfaqe e plotë nisje me ndjekje progresi, menaxhim fazash, dhe ekran përshëndetës
- **Menaxheri i Ngjyrave**: Menaxhim i gjendjes së ngjyrave me integrim QuickDraw

### Të Zbatuara Pjesërisht ⚠️

- **Integrimi i Aplikacioneve**: Interpretuesi M68K dhe ngarkuesi i segmenteve të përfunduara; nevojitet testim integrimi për të verifikuar që aplikacionet reale ekzekutohen
- **Procedurat e Përcaktimit të Dritareve (WDEF)**: Struktura bazë e vendosur, dërgim i pjesshëm
- **Menaxheri i të Folurit**: Kuadri API dhe kalim audio vetëm; motori i sintezës së të folurit nuk është zbatuar
- **Trajtimi i Përjashtimeve (RTE)**: Kthimi nga përjashtimi i zbatuar pjesërisht (aktualisht ndalet në vend që të rikthejë kontekstin)

### Ende Pa u Zbatuar ❌

- **Printimi**: Asnjë sistem printimi
- **Rrjeti**: Asnjë funksionalitet AppleTalk ose rrjeti
- **Aksesorët e Desktopit**: Vetëm kuadri
- **Audio e Avancuar**: Luajtje kampionësh, përzierje (kufizim i altoparlantit të PC-së)

### Nënsisteme të Pakompiluara 🔧

Këto kanë kod burimor por nuk janë integruar në kernel:
- **AppleEventManager** (8 skedarë): Mesazhim ndërmjet aplikacioneve; i përjashtuar qëllimisht për shkak të varësive pthread të papajtueshme me mjedisin pa sistem operativ
- **FontResources** (vetëm header): Përcaktime të tipit të burimeve të fonteve; mbështetja aktuale e fonteve ofrohet nga FontResourceLoader.c i kompiluar

## 🏗️ Arkitektura

### Specifikimet Teknike

- **Arkitektura**: Shumë-arkitekturash përmes HAL (x86, ARM, PowerPC gati)
- **Protokolli i Nisjes**: Multiboot2 (x86), ngarkues platformësh specifike
- **Grafika**: Framebuffer VESA, 800x600 @ 32-bit ngjyrash
- **Paraqitja e Kujtesës**: Kerneli ngarkohet në adresën fizike 1MB (x86)
- **Koha**: E pavarur nga arkitektura me saktësi mikrosekondash (RDTSC/regjistra kohëmatësi)
- **Performanca**: Dështim i ftohtë i burimeve <15µs, goditje memorje <2µs, devijim kohëmatësi <100ppm

### Statistikat e Bazës së Kodit

- **225+ skedarë burimorë** me ~57,500+ rreshta kodi
- **145+ skedarë header** në 28+ nënsisteme
- **69 tipe burimesh** të nxjerra nga System 7.1
- **Koha e kompilimit**: 3-5 sekonda në pajisje moderne
- **Madhësia e kernelit**: ~4.16 MB
- **Madhësia e ISO**: ~12.5 MB

## 🔨 Ndërtimi

### Kërkesat

- **GCC** me mbështetje 32-bit (`gcc-multilib` në 64-bit)
- **GNU Make**
- **Mjetet GRUB**: `grub-mkrescue` (nga `grub2-common` ose `grub-pc-bin`)
- **QEMU** për testim (`qemu-system-i386`)
- **Python 3** për përpunimin e burimeve
- **xxd** për konvertimin binar
- *(Opsionale)* Toolchain ndërkryqëzuar **powerpc-linux-gnu** për ndërtime PowerPC

### Instalimi në Ubuntu/Debian

```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common
```

### Komandat e Ndërtimit

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
make LOCALE_FR=1 LOCALE_DE=1 LOCALE_ES=1 LOCALE_JA=1 LOCALE_ZH=1 LOCALE_KO=1 LOCALE_RU=1 LOCALE_UK=1 LOCALE_PL=1 LOCALE_CS=1 LOCALE_SQ=1 LOCALE_BG=1 LOCALE_HR=1 LOCALE_DA=1 LOCALE_NL=1 LOCALE_ET=1 LOCALE_FI=1 LOCALE_EL=1 LOCALE_HU=1 LOCALE_IS=1 LOCALE_IT=1 LOCALE_LV=1 LOCALE_LT=1 LOCALE_MK=1 LOCALE_ME=1 LOCALE_NO=1 LOCALE_PT=1 LOCALE_RO=1 LOCALE_SK=1 LOCALE_SL=1 LOCALE_SV=1 LOCALE_TR=1 LOCALE_HI=1

# Build with a single additional language
make LOCALE_FR=1

# Build and run in QEMU
make run

# Clean artifacts
make clean

# Display build statistics
make info
```

## 🚀 Ekzekutimi

### Fillimi i Shpejtë (QEMU)

```bash
# Standard run with serial logging
make run

# Manually with options
qemu-system-i386 -cdrom system71.iso -serial file:/tmp/serial.log -display sdl -vga std -m 256M
```

### Opsionet e QEMU

```bash
# With console serial output
qemu-system-i386 -cdrom system71.iso -serial stdio -display sdl -m 256M

# Headless (no graphics display)
qemu-system-i386 -cdrom system71.iso -serial stdio -display none -m 256M

# With GDB debugging
make debug
# In another terminal: gdb kernel.elf -ex "target remote :1234"
```

## 📚 Dokumentacioni

### Udhëzuesit e Komponentëve
- **Menaxheri i Kontrolleve**: `docs/components/ControlManager/`
- **Menaxheri i Dialogëve**: `docs/components/DialogManager/`
- **Menaxheri i Fonteve**: `docs/components/FontManager/`
- **Regjistrimi Serial**: `docs/components/System/`
- **Menaxheri i Ngjarjeve**: `docs/components/EventManager.md`
- **Menaxheri i Menuve**: `docs/components/MenuManager.md`
- **Menaxheri i Dritareve**: `docs/components/WindowManager.md`
- **Menaxheri i Burimeve**: `docs/components/ResourceManager.md`

### Ndërkombëtarizimi
- **Menaxheri i Lokaleve**: `include/LocaleManager/` — ndërrim i lokaleve në kohë ekzekutimi, zgjedhja e gjuhës në nisje
- **Burimet e Vargjeve**: `resources/strings/` — skedarë burimesh STR# për çdo gjuhë (34 gjuhë)
- **Fontet e Zgjeruara**: `include/chicago_font_extended.h` — glifet Mac Roman 0x80-0xFF për karaktere evropiane
- **Mbështetja CJK**: `include/TextEncoding/CJKEncoding.h`, `include/FontManager/CJKFont.h` — infrastrukturë kodimi dhe fontesh me shumë bajte

### Gjendja e Zbatimit
- **IMPLEMENTATION_PRIORITIES.md**: Puna e planifikuar dhe ndjekja e përfundueshmërisë
- **IMPLEMENTATION_STATUS_AUDIT.md**: Auditim i detajuar i të gjitha nënsistemeve

### Filozofia e Projektit

**Qasje Arkeologjike** me zbatim të bazuar në dëshmi:
1. E mbështetur nga dokumentacioni Inside Macintosh dhe MPW Universal Interfaces
2. Të gjitha vendimet kryesore janë etiketuar me ID Gjetjesh që referojnë dëshmitë mbështetëse
3. Qëllimi: barazi sjelljes me System 7 origjinal, jo modernizim
4. Zbatim i pastër (pa kodin burimor origjinal të Apple)

## 🐛 Probleme të Njohura

1. **Artifakte të Tërheqjes së Ikonave**: Artifakte të vogla vizuale gjatë tërheqjes së ikonave në desktop
2. **Ekzekutimi M68K i Stubar**: Ngarkuesi i segmenteve i përfunduar, laku i ekzekutimit nuk është zbatuar
3. **Pa Mbështetje TrueType**: Vetëm fonte bitmap (Chicago)
4. **HFS Vetëm-Lexim**: Sistem skedarësh virtual, pa rikthim shkrimi në disk
5. **Pa Garanci Stabiliteti**: Përplasjet dhe sjelljet e papritura janë të zakonshme

## 🤝 Kontribuimi

Ky është kryesisht një projekt mësimi/kërkimi:

1. **Raportime Gabimesh**: Hapni çështje me hapa të hollësishme riprodhimi
2. **Testimi**: Raportoni rezultatet në pajisje/emulatorë të ndryshëm
3. **Dokumentacioni**: Përmirësoni dokumentet ekzistuese ose shtoni udhëzues të rinj

## 📖 Referenca Thelbësore

- **Inside Macintosh** (1992-1994): Dokumentacioni zyrtar i Apple Toolbox
- **MPW Universal Interfaces 3.2**: Skedarë header kanonike dhe përcaktime strukturash
- **Guide to Macintosh Family Hardware**: Referencë e arkitekturës së pajisjeve

### Mjete të Dobishme

- **Mini vMac**: Emulator i System 7 për referencë sjelljeje
- **ResEdit**: Redaktues burimesh për studimin e burimeve të System 7
- **Ghidra/IDA**: Për analizën e çmontimit të ROM-it

## ⚖️ Aspekti Ligjor

Ky është një **rizbatim i pastër** për qëllime edukative dhe ruajtjeje:

- **Asnjë kod burimor i Apple** nuk u përdor
- I bazuar vetëm në dokumentacion publik dhe analizë me kuti të zezë
- "System 7", "Macintosh", "QuickDraw" janë marka tregtare të Apple Inc.
- Nuk është i lidhur me, i miratuar nga, ose i sponsorizuar nga Apple Inc.

**ROM-i origjinal i System 7 dhe softueri mbeten pronë e Apple Inc.**

## 🙏 Falënderime

- **Apple Computer, Inc.** për krijimin e System 7 origjinal
- **Autorët e Inside Macintosh** për dokumentacionin gjithëpërfshirës
- **Komuniteti i ruajtjes së Mac klasik** për mbajtjen gjallë të platformës
- **68k.news dhe Macintosh Garden** për arkivat e burimeve

## 📊 Statistikat e Zhvillimit

- **Rreshta Kodi**: ~57,500+ (përfshirë 2,500+ për ngarkuesin e segmenteve)
- **Koha e Kompilimit**: ~3-5 sekonda
- **Madhësia e Kernelit**: ~4.16 MB (kernel.elf)
- **Madhësia e ISO**: ~12.5 MB (system71.iso)
- **Reduktimi i Gabimeve**: 94% e funksionalitetit bazë funksionon
- **Nënsisteme Kryesore**: 28+ (Font, Dritare, Menu, Kontrolle, Dialog, TextEdit, etj.)

## 🔮 Drejtimi i Ardhshëm

**Puna e Planifikuar**:

- Përfundimi i lakut të ekzekutimit të interpretuesit M68K
- Shtimi i mbështetjes për fonte TrueType
- Burime fontesh bitmap CJK për renderimin Japonisht, Kinezisht dhe Koreanisht
- Zbatimi i kontrolleve shtesë (fusha teksti, pop-up, rrëshqitës)
- Rikthimi i shkrimit në disk për sistemin e skedarëve HFS
- Veçori të avancuara të Menaxherit të Zërit (përzierje, kampionim)
- Aksesorë bazë desktopi (Makina llogaritëse, Blloku i Shënimeve)

---

**Gjendja**: Eksperimentale - Edukative - Në Zhvillim

**Përditësimi i Fundit**: Nëntor 2025 (Përmirësimet e Menaxherit të Zërit të Përfunduara)

Për pyetje, çështje ose diskutime, ju lutem përdorni GitHub Issues.
