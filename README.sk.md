# System 7 - Prenosna open-source reimplementacia

**[English](README.md)** | **[Fran&ccedil;ais](README.fr.md)** | **[Deutsch](README.de.md)** | **[Espa&ntilde;ol](README.es.md)** | **[Italiano](README.it.md)** | **[Portugu&ecirc;s](README.pt.md)** | **[Nederlands](README.nl.md)** | **[Dansk](README.da.md)** | **[Norsk](README.no.md)** | **[Svenska](README.sv.md)** | **[Suomi](README.fi.md)** | **[&Iacute;slenska](README.is.md)** | **[Ελληνικά](README.el.md)** | **[T&uuml;rk&ccedil;e](README.tr.md)** | **[Polski](README.pl.md)** | **[Čeština](README.cs.md)** | **[Slovenčina](README.sk.md)** | **[Slovenščina](README.sl.md)** | **[Hrvatski](README.hr.md)** | **[Magyar](README.hu.md)** | **[Rom&acirc;n&atilde;](README.ro.md)** | **[Български](README.bg.md)** | **[Shqip](README.sq.md)** | **[Eesti](README.et.md)** | **[Latviešu](README.lv.md)** | **[Lietuvių](README.lt.md)** | **[Македонски](README.mk.md)** | **[Crnogorski](README.me.md)** | **[Русский](README.ru.md)** | **[Українська](README.uk.md)** | **[العربية](README.ar.md)** | **[日本語](README.ja.md)** | **[简体中文](README.zh.md)** | **[繁體中文](README.tw.md)** | **[한국어](README.ko.md)** | **[हिन्दी](README.hi.md)** | **[বাংলা](README.bn.md)** | **[اردو](README.ur.md)**

<img width="793" height="657" alt="System 7 bezici na modernom hardveri" src="https://github.com/user-attachments/assets/be84b83e-191c-4f9d-a786-11d0bd04203b" />
<img width="801" height="662" alt="simpletextworks" src="https://github.com/user-attachments/assets/7c9ebe5b-22b4-4612-93a1-2076909d77cd" />
<img width="803" height="661" alt="macpaint" src="https://github.com/user-attachments/assets/cd3ed04a-fdde-4dd5-88ef-5b19b3a13a54" />

> ⚠️ **OVERENIE KONCEPTU** - Toto je experimentalna, vzdelavacia reimplementacia systemu Apple Macintosh System 7. NIE je to hotovy produkt a nemal by sa povazovat za produkcne pripraveny softver.

Open-source reimplementacia systemu Apple Macintosh System 7 pre moderny x86 hardver, bootovatelna cez GRUB2/Multiboot2. Cielom tohto projektu je znovu vytvorit klasicky zazitok z Mac OS a zaroven zdokumentovat architekturu System 7 prostrednictvom analyzy spatneho inzinierstva.

## 🎯 Stav projektu

**Aktualny stav**: Aktivny vyvoj s priblizne 94% dokoncenim zakladnej funkcionality

### Posledne aktualizacie (november 2025)

#### Vylepsenia Sound Managera ✅ DOKONCENE
- **Optimalizovana MIDI konverzia**: Zdielana pomocna funkcia `SndMidiNoteToFreq()` s 37-polozkovym vyhladavacim tabulkou (C3-B5) a oktavovym spatnym prechodom pre plny MIDI rozsah (0-127)
- **Podpora asynchronneho prehravania**: Kompletna infrastruktura spatnych volani pre prehravanje suborov (`FilePlayCompletionUPP`) aj vykonavanie prikazov (`SndCallBackProcPtr`)
- **Zvukove smerovanie na zaklade kanalov**: Viacurovnovy system priorit s ovladanim stlmenia a povolenia
  - 4-urovnove kanaly s prioritou (0-3) pre smerovanie hardveroveho vystupu
  - Nezavisle ovladanie stlmenia a povolenia pre kazdy kanal
  - `SndGetActiveChannel()` vracia kanal s najvyssou prioritou
  - Spravna inicializacia kanalov s predvolene povolenym priznakom
- **Implementacia produkcnej kvality**: Vsetok kod sa kompiluje cisto, neboli zistene ziadne porusenia malloc/free
- **Commity**: 07542c5 (MIDI optimalizacia), 1854fe6 (asynchronne spatne volania), a3433c6 (smerovanie kanalov)

#### Predchadzajuce dosiahnutia
- ✅ **Faza pokrocilych funkcii**: Spracovaci cyklus prikazov Sound Managera, serializacia stylom viacnasobneho behu, rozsirene MIDI/syntezne funkcie
- ✅ **System zmeny velkosti okien**: Interaktivna zmena velkosti so spravnym spracovanim dekoracii okna, grow box a cistenim pracovnej plochy
- ✅ **PS/2 preklad klavesnice**: Uplne mapovanie scankodov sady 1 na kody klavesov Toolboxu
- ✅ **Viacplatformovy HAL**: Podpora x86, ARM a PowerPC s cistou abstrakciou

## 📊 Uplnost projektu

**Celkova zakladna funkcionalita**: Priblizne 94% dokoncena (odhad)

### Co funguje uplne ✅

- **Vrstva hardverovej abstrakcie (HAL)**: Kompletna platformova abstrakcia pre x86/ARM/PowerPC
- **Bootovaci system**: Uspesne bootuje cez GRUB2/Multiboot2 na x86
- **Seriove logovanie**: Modulove logovanie s filtrovaniem za behu (Error/Warn/Info/Debug/Trace)
- **Zaklad grafiky**: VESA framebuffer (800x600x32) s primitivami QuickDraw vratane rezimu XOR
- **Vykreslovanie pracovnej plochy**: Panel s menu System 7 s duhovym logom Apple, ikonami a vzormi pracovnej plochy
- **Typografia**: Bitmapovy font Chicago s pixelovo dokonalym vykreslovaniem a spravnym kerningom, rozsireny Mac Roman (0x80-0xFF) pre europske znaky s diakritikou
- **Internacionalizacia (i18n)**: Lokalizacia zalozena na zdrojoch s 38 jazykmi (anglictina, francuzstina, nemcina, spanielcina, talianstina, portugalcina, holandcina, dancina, norvescina, svedcina, fincina, islandcina, grectina, turecina, polstina, cestina, slovencina, slovinscina, chorvatcina, madarcina, rumuncina, bulharcina, albancina, estoncina, lotyscina, litovscina, macedoncina, ciernohorcina, rustina, ukrajincina, arabcina, japoncina, zjednodusena cinstina, tradicionalna cinstina, korejcina, hindcina, bengalcina, urdcina), Locale Manager s vyberom jazyka pri spusteni, infrastruktura CJK viacbajtoveho kodovania
- **Font Manager**: Podpora viacerych velkosti (9-24pt), synteza stylov, parsovanie FOND/NFNT, LRU kache
- **Vstupny system**: PS/2 klavesnica a mys s kompletnym preposielanim udalosti
- **Event Manager**: Kooperativny multitasking cez WaitNextEvent s jednotnym frontom udalosti
- **Memory Manager**: Alokovanie zalozene na zonach s integraciou interpretera 68K
- **Menu Manager**: Kompletne rozbalovacie menu so sledovanim mysi a SaveBits/RestoreBits
- **Suborovy system**: HFS s implementaciou B-stromu, okna priecinkov s enumeraciou VFS
- **Window Manager**: Presuvanie, zmena velkosti (s grow box), vrstvenie, aktivacia
- **Time Manager**: Presna kalibracia TSC, mikrosekundova presnost, kontrola generacii
- **Resource Manager**: O(log n) binarne vyhladavanie, LRU kache, komplexna validacia
- **Gestalt Manager**: Viacarchitekturne systemove informacie s detekciou architektury
- **TextEdit Manager**: Kompletna uprava textu s integraciou schranky
- **Scrap Manager**: Klasicka Mac OS schranka s podporou viacerych formatov
- **Aplikacia SimpleText**: Plnohodnotny MDI textovy editor s vystrihnout/kopirovat/vlozit
- **List Manager**: Ovladacie prvky zoznamov kompatibilne so System 7.1 s navigaciou klavesnicou
- **Control Manager**: Standardne ovladacie prvky a posuvniky s implementaciou CDEF
- **Dialog Manager**: Navigacia klavesnicou, zvyraznovacie prstence fokusu, klavesove skratky
- **Segment Loader**: Prenosny ISA-agnosticky system nacitavania 68K segmentov s relokaciou
- **Interpreter M68K**: Uplne odoslanie instrukcii s 84 handlermi opkodov, vsetkych 14 adresnych rezimov, framework vynimiek/trapov
- **Sound Manager**: Spracovanie prikazov, MIDI konverzia, sprava kanalov, spatne volania
- **Device Manager**: Sprava DCE, instalacia/odstranovanie ovladacov a I/O operacie
- **Startovacia obrazovka**: Kompletne bootovace UI so sledovanim progresu, spravou faz a uvitacou obrazovkou
- **Color Manager**: Sprava stavu farieb s integraciou QuickDraw

### Ciastocne implementovane ⚠️

- **Integracia aplikacii**: Interpreter M68K a segment loader su kompletne; potrebne integracne testovanie na overenie, ci sa realne aplikacie vykonavaju
- **Procedury definiujuce okna (WDEF)**: Zakladna struktura je na mieste, ciastocne odosielanie
- **Speech Manager**: Iba framework API a prechod zvuku; engine syntezy reci nie je implementovany
- **Spracovanie vynimiek (RTE)**: Navrat z vynimky je ciastocne implementovany (v sucasnosti zastavi namiesto obnovenia kontextu)

### Este neimplementovane ❌

- **Tlac**: Ziadny tlacovy system
- **Siet**: Ziadna funkcionalita AppleTalk ani siete
- **Prislusenstvo pracovnej plochy (Desk Accessories)**: Iba framework
- **Pokrocily zvuk**: Prehravanje vzoriek, mixovanie (obmedzenie PC reproduktora)

### Nekomplilovane subsystemy 🔧

Nasledujuce maju zdrojovy kod, ale nie su integrovane do jadra:
- **AppleEventManager** (8 suborov): Medziaplikacne spravy; zamerne vylucene kvoli zavislostiam na pthread nekompatibilnym s freestanding prostredim
- **FontResources** (iba hlavickovy subor): Definiicie typov fontovych zdrojov; skutocnu podporu fontov poskytuje skompilovany FontResourceLoader.c

## 🏗️ Architektura

### Technicke specifikacie

- **Architektura**: Viacarchitekturna cez HAL (x86, ARM, PowerPC pripravene)
- **Bootovaci protokol**: Multiboot2 (x86), platformovo specificke bootloadery
- **Grafika**: VESA framebuffer, 800x600 @ 32-bitova farba
- **Rozlozenie pamati**: Jadro sa nacitava na fyzicku adresu 1MB (x86)
- **Casovanie**: Architekturne agnosticke s mikrosekundovou presnostou (RDTSC/casovacove registre)
- **Vykon**: Ztata zdrojov pri studenej kache <15us, zasah do kache <2us, drift casovaca <100ppm

### Statistiky kodovej zakladne

- **225+ zdrojovych suborov** s priblizne 57 500+ riadkami kodu
- **145+ hlavickovych suborov** v 28+ subsystemoch
- **69 typov zdrojov** extrahovanych zo System 7.1
- **Cas kompilacie**: 3-5 sekund na modernom hardveri
- **Velkost jadra**: priblizne 4,16 MB
- **Velkost ISO**: priblizne 12,5 MB

## 🔨 Kompilovanie

### Poziadavky

- **GCC** s podporou 32-bitov (`gcc-multilib` na 64-bitovych systemoch)
- **GNU Make**
- **Nastroje GRUB**: `grub-mkrescue` (z `grub2-common` alebo `grub-pc-bin`)
- **QEMU** na testovanie (`qemu-system-i386`)
- **Python 3** na spracovanie zdrojov
- **xxd** na binarnu konverziu
- *(Volitelne)* **powerpc-linux-gnu** krizovy nastroj pre PowerPC zostavenia

### Instalacia na Ubuntu/Debian

```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common
```

### Prikazy na zostavenie

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
make LOCALE_FR=1 LOCALE_DE=1 LOCALE_ES=1 LOCALE_JA=1 LOCALE_ZH=1 LOCALE_KO=1 LOCALE_RU=1 LOCALE_UK=1 LOCALE_PL=1 LOCALE_CS=1 LOCALE_SQ=1 LOCALE_BG=1 LOCALE_HR=1 LOCALE_DA=1 LOCALE_NL=1 LOCALE_ET=1 LOCALE_FI=1 LOCALE_EL=1 LOCALE_HU=1 LOCALE_IS=1 LOCALE_IT=1 LOCALE_LV=1 LOCALE_LT=1 LOCALE_MK=1 LOCALE_ME=1 LOCALE_NO=1 LOCALE_PT=1 LOCALE_RO=1 LOCALE_SK=1 LOCALE_SL=1 LOCALE_SV=1 LOCALE_TR=1 LOCALE_HI=1 LOCALE_TW=1 LOCALE_AR=1 LOCALE_BN=1 LOCALE_UR=1

# Build with a single additional language
make LOCALE_FR=1

# Build and run in QEMU
make run

# Clean artifacts
make clean

# Display build statistics
make info
```

## 🚀 Spustenie

### Rychly start (QEMU)

```bash
# Standard run with serial logging
make run

# Manually with options
qemu-system-i386 -cdrom system71.iso -serial file:/tmp/serial.log -display sdl -vga std -m 256M
```

### Moznosti QEMU

```bash
# With console serial output
qemu-system-i386 -cdrom system71.iso -serial stdio -display sdl -m 256M

# Headless (no graphics display)
qemu-system-i386 -cdrom system71.iso -serial stdio -display none -m 256M

# With GDB debugging
make debug
# In another terminal: gdb kernel.elf -ex "target remote :1234"
```

## 📚 Dokumentacia

### Sprievodcovia komponentmi
- **Control Manager**: `docs/components/ControlManager/`
- **Dialog Manager**: `docs/components/DialogManager/`
- **Font Manager**: `docs/components/FontManager/`
- **Seriove logovanie**: `docs/components/System/`
- **Event Manager**: `docs/components/EventManager.md`
- **Menu Manager**: `docs/components/MenuManager.md`
- **Window Manager**: `docs/components/WindowManager.md`
- **Resource Manager**: `docs/components/ResourceManager.md`

### Internacionalizacia
- **Locale Manager**: `include/LocaleManager/` — prepinanie lokalit za behu, vyber jazyka pri spusteni
- **Retazcove zdroje**: `resources/strings/` — subory zdrojov STR# pre kazdy jazyk (34 jazykov)
- **Rozsirene fonty**: `include/chicago_font_extended.h` — glyfy Mac Roman 0x80-0xFF pre europske znaky
- **Podpora CJK**: `include/TextEncoding/CJKEncoding.h`, `include/FontManager/CJKFont.h` — infrastruktura viacbajtoveho kodovania a fontov

### Stav implementacie
- **IMPLEMENTATION_PRIORITIES.md**: Planovana praca a sledovanie uplnosti
- **IMPLEMENTATION_STATUS_AUDIT.md**: Podrobny audit vsetkych subsystemov

### Filozofia projektu

**Archeologicky pristup** s implementaciou zalozenou na dokazoch:
1. Podlozene dokumentaciou Inside Macintosh a MPW Universal Interfaces
2. Vsetky hlavne rozhodnutia oznacene identifikatormi nalezov s odkazmi na podporne dokazy
3. Ciel: behavioralna parita s originalnym System 7, nie modernizacia
4. Implementacia v cistej miestnosti (ziadny originalny zdrojovy kod od Apple)

## 🐛 Zname problemy

1. **Artefakty pri presuvani ikon**: Drobne vizualne artefakty pocas presuvanych ikon na pracovnej ploche
2. **Vykonavanie M68K je stubovane**: Segment loader je kompletny, vykonavacie slucka nie je implementovana
3. **Ziadna podpora TrueType**: Iba bitmapove fonty (Chicago)
4. **HFS iba na citanie**: Virtualny suborovy system, ziadny spatny zapis na disk
5. **Ziadne zaruky stability**: Pady a neocakavane spravanie su bezne

## 🤝 Prispievanie

Toto je predovsetkym vzdelavaci/vyskumny projekt:

1. **Hlasenia chyb**: Zadavajte problemy s podrobnymi krokmi reprodukcie
2. **Testovanie**: Hlaste vysledky na roznom hardveri/emulatoroch
3. **Dokumentacia**: Vylepsujte existujucu dokumentaciu alebo pridavajte nove sprievodcovia

## 📖 Zakladne referencie

- **Inside Macintosh** (1992-1994): Oficialna dokumentacia Apple Toolbox
- **MPW Universal Interfaces 3.2**: Kanonicke hlavickove subory a definiicie struktur
- **Guide to Macintosh Family Hardware**: Referencia hardverovej architektury

### Uzitocne nastroje

- **Mini vMac**: Emulator System 7 na behavioralnu referenciu
- **ResEdit**: Editor zdrojov na studovanie zdrojov System 7
- **Ghidra/IDA**: Na analyzu disassembly ROM

## ⚖️ Pravne informacie

Toto je **reimplementacia v cistej miestnosti** na vzdelavacie ucely a ucely uchovanania:

- **Ziadny zdrojovy kod od Apple** nebol pouzity
- Zalozene iba na verejnej dokumentacii a analyze ciernej skrinky
- "System 7", "Macintosh", "QuickDraw" su ochranne znamky Apple Inc.
- Nie je pridrzane, schvalene ani sponzorovane spolocnostou Apple Inc.

**Originalne ROM a softver System 7 zostavaju majetkom Apple Inc.**

## 🙏 Podakovania

- **Apple Computer, Inc.** za vytvorenie originalneho System 7
- **Autori Inside Macintosh** za komplexnu dokumentaciu
- **Komunita pre zachovanie klasickeho Macu** za udrzanie platformy nazive
- **68k.news a Macintosh Garden** za archivy zdrojov

## 📊 Statistiky vyvoja

- **Riadky kodu**: priblizne 57 500+ (vratane 2 500+ pre segment loader)
- **Cas kompilacie**: priblizne 3-5 sekund
- **Velkost jadra**: priblizne 4,16 MB (kernel.elf)
- **Velkost ISO**: priblizne 12,5 MB (system71.iso)
- **Znizenie chybovosti**: 94% zakladnej funkcionality funguje
- **Hlavne subsystemy**: 28+ (Font, Window, Menu, Control, Dialog, TextEdit atd.)

## 🔮 Buduci smer

**Planovana praca**:

- Dokoncit vykonavaci cyklus interpretera M68K
- Pridat podporu fontov TrueType
- Bitmapove fontove zdroje CJK pre vykreslovanie japonciny, cinstiny a korejciny
- Implementovat dalsie ovladacie prvky (textove polia, vyskakovacie menu, posuvniky)
- Spatny zapis na disk pre suborovy system HFS
- Pokrocile funkcie Sound Managera (mixovanie, vzorkovanie)
- Zakladne prislusenstvo pracovnej plochy (Calculator, Note Pad)

---

**Stav**: Experimentalny - Vzdelavaci - Vo vyvoji

**Naposledy aktualizovane**: November 2025 (Vylepsenia Sound Managera dokoncene)

V pripade otazok, problemov alebo diskusie pouzite prosim GitHub Issues.
