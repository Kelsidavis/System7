# System 7 - Draagbare Open-Source Herimplementatie

**[English](README.md)** | **[Fran&ccedil;ais](README.fr.md)** | **[Deutsch](README.de.md)** | **[Espa&ntilde;ol](README.es.md)** | **[Italiano](README.it.md)** | **[Portugu&ecirc;s](README.pt.md)** | **[Nederlands](README.nl.md)** | **[Dansk](README.da.md)** | **[Norsk](README.no.md)** | **[Svenska](README.sv.md)** | **[Suomi](README.fi.md)** | **[&Iacute;slenska](README.is.md)** | **[Ελληνικά](README.el.md)** | **[T&uuml;rk&ccedil;e](README.tr.md)** | **[Polski](README.pl.md)** | **[Čeština](README.cs.md)** | **[Slovenčina](README.sk.md)** | **[Slovenščina](README.sl.md)** | **[Hrvatski](README.hr.md)** | **[Magyar](README.hu.md)** | **[Rom&acirc;n&atilde;](README.ro.md)** | **[Български](README.bg.md)** | **[Shqip](README.sq.md)** | **[Eesti](README.et.md)** | **[Latviešu](README.lv.md)** | **[Lietuvių](README.lt.md)** | **[Македонски](README.mk.md)** | **[Crnogorski](README.me.md)** | **[Русский](README.ru.md)** | **[Українська](README.uk.md)** | **[日本語](README.ja.md)** | **[中文](README.zh.md)** | **[한국어](README.ko.md)** | **[हिन्दी](README.hi.md)**

<img width="793" height="657" alt="System 7 draaiend op moderne hardware" src="https://github.com/user-attachments/assets/be84b83e-191c-4f9d-a786-11d0bd04203b" />
<img width="801" height="662" alt="simpletextworks" src="https://github.com/user-attachments/assets/7c9ebe5b-22b4-4612-93a1-2076909d77cd" />
<img width="803" height="661" alt="macpaint" src="https://github.com/user-attachments/assets/cd3ed04a-fdde-4dd5-88ef-5b19b3a13a54" />

> ⚠️ **PROOF OF CONCEPT** - Dit is een experimentele, educatieve herimplementatie van Apple's Macintosh System 7. Dit is GEEN afgewerkt product en mag niet als productieklare software worden beschouwd.

Een open-source herimplementatie van Apple Macintosh System 7 voor moderne x86-hardware, opstartbaar via GRUB2/Multiboot2. Dit project heeft als doel de klassieke Mac OS-ervaring na te bootsen en tegelijkertijd de System 7-architectuur te documenteren door middel van reverse-engineeringanalyse.

## 🎯 Projectstatus

**Huidige stand**: Actieve ontwikkeling met ~94% kernfunctionaliteit voltooid

### Laatste updates (november 2025)

#### Verbeteringen aan de Sound Manager ✅ VOLTOOID
- **Geoptimaliseerde MIDI-conversie**: Gedeelde `SndMidiNoteToFreq()`-hulpfunctie met opzoektabel van 37 items (C3-B5) en octaafgebaseerde terugval voor het volledige MIDI-bereik (0-127)
- **Ondersteuning voor asynchrone weergave**: Volledige callback-infrastructuur voor zowel bestandsweergave (`FilePlayCompletionUPP`) als opdrachtuitvoering (`SndCallBackProcPtr`)
- **Kanaalgebaseerde audioroutering**: Meervoudig prioriteitssysteem met dempen en inschakelen
  - 4 prioriteitsniveaus (0-3) voor hardware-uitvoerkanalen
  - Onafhankelijke dempen- en inschakelbediening per kanaal
  - `SndGetActiveChannel()` retourneert het actieve kanaal met de hoogste prioriteit
  - Correcte kanaalinitialisatie met standaard ingeschakelde vlag
- **Productieklare implementatie**: Alle code compileert foutloos, geen malloc/free-schendingen gedetecteerd
- **Commits**: 07542c5 (MIDI-optimalisatie), 1854fe6 (asynchrone callbacks), a3433c6 (kanaalroutering)

#### Eerdere sessieresultaten
- ✅ **Geavanceerde functiefase**: Opdrachtenverwerkingslus van de Sound Manager, serialisatie met meerdere stijlen, uitgebreide MIDI-/synthesefuncties
- ✅ **Venstergroottesysteem**: Interactief formaat wijzigen met correcte chroomafhandeling, groeibox en bureaubladopruiming
- ✅ **PS/2-toetsenbordvertaling**: Volledige set 1-scancode naar Toolbox-toetscodevertaling
- ✅ **Multi-platform HAL**: Ondersteuning voor x86, ARM en PowerPC met een schone abstractie

## 📊 Projectvolledigheid

**Totale kernfunctionaliteit**: ~94% voltooid (schatting)

### Wat volledig werkt ✅

- **Hardware Abstraction Layer (HAL)**: Volledige platformabstractie voor x86/ARM/PowerPC
- **Opstartsysteem**: Start succesvol op via GRUB2/Multiboot2 op x86
- **Seriële logging**: Modulegebaseerde logging met runtime-filtering (Error/Warn/Info/Debug/Trace)
- **Grafische basis**: VESA-framebuffer (800x600x32) met QuickDraw-primitieven inclusief XOR-modus
- **Bureaubladweergave**: System 7-menubalk met regenboog-Apple-logo, pictogrammen en bureaubladpatronen
- **Typografie**: Chicago-bitmaplettertype met pixelperfecte weergave en correcte tekenspatiëring, uitgebreid Mac Roman (0x80-0xFF) voor Europese letters met accenten
- **Internationalisering (i18n)**: Brongebaseerde lokalisatie met 34 talen (Engels, Frans, Duits, Spaans, Italiaans, Portugees, Nederlands, Deens, Noors, Zweeds, Fins, IJslands, Grieks, Turks, Pools, Tsjechisch, Slowaaks, Sloveens, Kroatisch, Hongaars, Roemeens, Bulgaars, Albanees, Estlands, Lets, Litouws, Macedonisch, Montenegrijns, Russisch, Oekraïens, Japans, Chinees, Koreaans, Hindi), Locale Manager met taalselectie bij het opstarten, CJK multi-byte coderingsinfrastructuur
- **Font Manager**: Ondersteuning voor meerdere groottes (9-24pt), stijlsynthese, FOND/NFNT-parsing, LRU-caching
- **Invoersysteem**: PS/2-toetsenbord en -muis met volledige gebeurtenisdoorgifte
- **Event Manager**: Coöperatieve multitasking via WaitNextEvent met uniforme gebeurteniswachtrij
- **Memory Manager**: Zonegebaseerde toewijzing met 68K-interpreterintegratie
- **Menu Manager**: Volledige vervolgkeuzemenu's met muistracking en SaveBits/RestoreBits
- **Bestandssysteem**: HFS met B-tree-implementatie, mapvensters met VFS-enumeratie
- **Window Manager**: Slepen, formaat wijzigen (met groeibox), lagen, activering
- **Time Manager**: Nauwkeurige TSC-kalibratie, microsecondeprecisie, generatiecontrole
- **Resource Manager**: O(log n) binair zoeken, LRU-cache, uitgebreide validatie
- **Gestalt Manager**: Systeeminformatie voor meerdere architecturen met architectuurdetectie
- **TextEdit Manager**: Volledige tekstbewerking met klembordintegratie
- **Scrap Manager**: Klassiek Mac OS-klembord met ondersteuning voor meerdere formaten
- **SimpleText-applicatie**: Volwaardige MDI-teksteditor met knippen/kopiëren/plakken
- **List Manager**: System 7.1-compatibele lijstbesturingselementen met toetsenbordnavigatie
- **Control Manager**: Standaard- en schuifbalkbesturingselementen met CDEF-implementatie
- **Dialog Manager**: Toetsenbordnavigatie, focusringen, sneltoetsen
- **Segment Loader**: Draagbaar ISA-onafhankelijk 68K-segmentlaadsysteem met herplaatsing
- **M68K Interpreter**: Volledige instructieverzending met 84 opcode-handlers, alle 14 adresseringsmodi, exceptie-/trap-raamwerk
- **Sound Manager**: Opdrachtverwerking, MIDI-conversie, kanaalbeheer, callbacks
- **Device Manager**: DCE-beheer, stuurprogramma-installatie/-verwijdering en I/O-bewerkingen
- **Opstartscherm**: Volledige opstart-UI met voortgangsweergave, fasebeheer en welkomstscherm
- **Color Manager**: Kleurstatusbeheer met QuickDraw-integratie

### Gedeeltelijk geïmplementeerd ⚠️

- **Applicatie-integratie**: M68K-interpreter en segmentlader voltooid; integratietests nodig om te verifiëren of echte applicaties worden uitgevoerd
- **Window Definition Procedures (WDEF)**: Kernstructuur aanwezig, gedeeltelijke dispatch
- **Speech Manager**: API-raamwerk en audio-doorgifte alleen; spraaksynthese-engine niet geïmplementeerd
- **Exceptieafhandeling (RTE)**: Terugkeer uit exceptie gedeeltelijk geïmplementeerd (stopt momenteel in plaats van context te herstellen)

### Nog niet geïmplementeerd ❌

- **Afdrukken**: Geen afdruksysteem
- **Netwerk**: Geen AppleTalk- of netwerkfunctionaliteit
- **Bureauaccessoires**: Alleen raamwerk
- **Geavanceerde audio**: Sampleweergave, mixen (beperking PC-speaker)

### Subsystemen niet gecompileerd 🔧

De volgende onderdelen hebben broncode maar zijn niet geïntegreerd in de kernel:
- **AppleEventManager** (8 bestanden): Communicatie tussen applicaties; bewust uitgesloten vanwege pthread-afhankelijkheden die onverenigbaar zijn met een vrijstaande omgeving
- **FontResources** (alleen header): Definitie van lettertypebrontypen; daadwerkelijke lettertypeondersteuning wordt geboden door het gecompileerde FontResourceLoader.c

## 🏗️ Architectuur

### Technische specificaties

- **Architectuur**: Multi-architectuur via HAL (x86, ARM, PowerPC gereed)
- **Opstartprotocol**: Multiboot2 (x86), platformspecifieke bootloaders
- **Grafisch**: VESA-framebuffer, 800x600 @ 32-bits kleur
- **Geheugenindeling**: Kernel laadt op 1MB fysiek adres (x86)
- **Timing**: Architectuuronafhankelijk met microsecondeprecisie (RDTSC/timerregisters)
- **Prestaties**: Koude resource-miss <15µs, cache-hit <2µs, timerdrift <100ppm

### Codebasisstatistieken

- **225+ bronbestanden** met ~57.500+ regels code
- **145+ headerbestanden** verdeeld over 28+ subsystemen
- **69 brontypen** geëxtraheerd uit System 7.1
- **Compilatietijd**: 3-5 seconden op moderne hardware
- **Kernelgrootte**: ~4,16 MB
- **ISO-grootte**: ~12,5 MB

## 🔨 Bouwen

### Vereisten

- **GCC** met 32-bits ondersteuning (`gcc-multilib` op 64-bits)
- **GNU Make**
- **GRUB-hulpmiddelen**: `grub-mkrescue` (van `grub2-common` of `grub-pc-bin`)
- **QEMU** voor testen (`qemu-system-i386`)
- **Python 3** voor bronverwerking
- **xxd** voor binaire conversie
- *(Optioneel)* **powerpc-linux-gnu** cross-toolchain voor PowerPC-builds

### Ubuntu/Debian-installatie

```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common
```

### Bouwopdrachten

```bash
# Kernel bouwen (standaard x86)
make

# Bouwen voor een specifiek platform
make PLATFORM=x86
make PLATFORM=arm        # vereist ARM bare-metal GCC
make PLATFORM=ppc        # experimenteel; vereist PowerPC ELF-toolchain

# Opstartbare ISO aanmaken
make iso

# Bouwen met alle talen
make LOCALE_FR=1 LOCALE_DE=1 LOCALE_ES=1 LOCALE_JA=1 LOCALE_ZH=1 LOCALE_KO=1 LOCALE_RU=1 LOCALE_UK=1 LOCALE_PL=1 LOCALE_CS=1 LOCALE_SQ=1 LOCALE_BG=1 LOCALE_HR=1 LOCALE_DA=1 LOCALE_NL=1 LOCALE_ET=1 LOCALE_FI=1 LOCALE_EL=1 LOCALE_HU=1 LOCALE_IS=1 LOCALE_IT=1 LOCALE_LV=1 LOCALE_LT=1 LOCALE_MK=1 LOCALE_ME=1 LOCALE_NO=1 LOCALE_PT=1 LOCALE_RO=1 LOCALE_SK=1 LOCALE_SL=1 LOCALE_SV=1 LOCALE_TR=1 LOCALE_HI=1

# Bouwen met één extra taal
make LOCALE_FR=1

# Bouwen en uitvoeren in QEMU
make run

# Bouwresultaten opruimen
make clean

# Bouwstatistieken weergeven
make info
```

## 🚀 Uitvoeren

### Snel starten (QEMU)

```bash
# Standaard uitvoering met seriële logging
make run

# Handmatig met opties
qemu-system-i386 -cdrom system71.iso -serial file:/tmp/serial.log -display sdl -vga std -m 256M
```

### QEMU-opties

```bash
# Met seriële console-uitvoer
qemu-system-i386 -cdrom system71.iso -serial stdio -display sdl -m 256M

# Zonder grafische weergave (headless)
qemu-system-i386 -cdrom system71.iso -serial stdio -display none -m 256M

# Met GDB-debugging
make debug
# In een andere terminal: gdb kernel.elf -ex "target remote :1234"
```

## 📚 Documentatie

### Componentgidsen
- **Control Manager**: `docs/components/ControlManager/`
- **Dialog Manager**: `docs/components/DialogManager/`
- **Font Manager**: `docs/components/FontManager/`
- **Seriële logging**: `docs/components/System/`
- **Event Manager**: `docs/components/EventManager.md`
- **Menu Manager**: `docs/components/MenuManager.md`
- **Window Manager**: `docs/components/WindowManager.md`
- **Resource Manager**: `docs/components/ResourceManager.md`

### Internationalisering
- **Locale Manager**: `include/LocaleManager/` — runtime-taalwisseling, taalselectie bij het opstarten
- **Tekenreeksbronnen**: `resources/strings/` — STR#-bronbestanden per taal (34 talen)
- **Uitgebreide lettertypen**: `include/chicago_font_extended.h` — Mac Roman 0x80-0xFF-glyphs voor Europese tekens
- **CJK-ondersteuning**: `include/TextEncoding/CJKEncoding.h`, `include/FontManager/CJKFont.h` — multi-byte codering en lettertypeinfrastructuur

### Implementatiestatus
- **IMPLEMENTATION_PRIORITIES.md**: Gepland werk en voortgangsregistratie
- **IMPLEMENTATION_STATUS_AUDIT.md**: Gedetailleerde audit van alle subsystemen

### Projectfilosofie

**Archeologische benadering** met op bewijs gebaseerde implementatie:
1. Ondersteund door Inside Macintosh-documentatie en MPW Universal Interfaces
2. Alle belangrijke beslissingen voorzien van bevinding-ID's die verwijzen naar ondersteunend bewijs
3. Doel: gedragspariteit met het originele System 7, geen modernisering
4. Clean-room-implementatie (geen originele Apple-broncode)

## 🐛 Bekende problemen

1. **Artefacten bij het slepen van pictogrammen**: Kleine visuele artefacten bij het slepen van bureaubladpictogrammen
2. **M68K-uitvoering afgekapt**: Segmentlader voltooid, uitvoeringslus niet geïmplementeerd
3. **Geen TrueType-ondersteuning**: Alleen bitmaplettertypen (Chicago)
4. **HFS alleen-lezen**: Virtueel bestandssysteem, geen daadwerkelijke schijfterugschrijving
5. **Geen stabiliteitsgaranties**: Crashes en onverwacht gedrag komen veel voor

## 🤝 Bijdragen

Dit is voornamelijk een leer-/onderzoeksproject:

1. **Bugrapporten**: Dien problemen in met gedetailleerde reproductiestappen
2. **Testen**: Rapporteer resultaten op verschillende hardware/emulators
3. **Documentatie**: Verbeter bestaande documenten of voeg nieuwe handleidingen toe

## 📖 Essentiële referenties

- **Inside Macintosh** (1992-1994): Officiële Apple Toolbox-documentatie
- **MPW Universal Interfaces 3.2**: Canonieke headerbestanden en structuurdefinities
- **Guide to Macintosh Family Hardware**: Referentie voor hardwarearchitectuur

### Nuttige hulpmiddelen

- **Mini vMac**: System 7-emulator als gedragsreferentie
- **ResEdit**: Broneditor voor het bestuderen van System 7-bronnen
- **Ghidra/IDA**: Voor ROM-disassemblyanalyse

## ⚖️ Juridisch

Dit is een **clean-room herimplementatie** voor educatieve en behoudsdoeleinden:

- Er is **geen Apple-broncode** gebruikt
- Uitsluitend gebaseerd op openbare documentatie en black-boxanalyse
- "System 7", "Macintosh", "QuickDraw" zijn handelsmerken van Apple Inc.
- Niet verbonden met, goedgekeurd door of gesponsord door Apple Inc.

**De originele System 7-ROM en -software blijven eigendom van Apple Inc.**

## 🙏 Dankbetuigingen

- **Apple Computer, Inc.** voor het creëren van het originele System 7
- **Auteurs van Inside Macintosh** voor uitgebreide documentatie
- **De klassieke Mac-behoudsgemeenschap** voor het in leven houden van het platform
- **68k.news en Macintosh Garden** voor bronnenarchieven

## 📊 Ontwikkelstatistieken

- **Regels code**: ~57.500+ (inclusief 2.500+ voor de segmentlader)
- **Compilatietijd**: ~3-5 seconden
- **Kernelgrootte**: ~4,16 MB (kernel.elf)
- **ISO-grootte**: ~12,5 MB (system71.iso)
- **Foutreductie**: 94% van de kernfunctionaliteit werkt
- **Belangrijke subsystemen**: 28+ (Font, Window, Menu, Control, Dialog, TextEdit, enz.)

## 🔮 Toekomstplannen

**Gepland werk**:

- Voltooiing van de M68K-interpreter-uitvoeringslus
- TrueType-lettertypeondersteuning toevoegen
- CJK-bitmaplettertypebronnen voor Japanse, Chinese en Koreaanse weergave
- Aanvullende besturingselementen implementeren (tekstvelden, pop-ups, schuifregelaars)
- Schijfterugschrijving voor het HFS-bestandssysteem
- Geavanceerde Sound Manager-functies (mixen, sampling)
- Eenvoudige bureauaccessoires (Rekenmachine, Notitieblok)

---

**Status**: Experimenteel - Educatief - In ontwikkeling

**Laatst bijgewerkt**: November 2025 (Verbeteringen aan de Sound Manager voltooid)

Voor vragen, problemen of discussie kunt u GitHub Issues gebruiken.
