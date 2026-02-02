# System 7 - Portabel reimplementation med oppen kallkod

**[English](README.md)** | **[Fran&ccedil;ais](README.fr.md)** | **[Deutsch](README.de.md)** | **[Espa&ntilde;ol](README.es.md)** | **[Italiano](README.it.md)** | **[Portugu&ecirc;s](README.pt.md)** | **[Nederlands](README.nl.md)** | **[Dansk](README.da.md)** | **[Norsk](README.no.md)** | **[Svenska](README.sv.md)** | **[Suomi](README.fi.md)** | **[&Iacute;slenska](README.is.md)** | **[Ελληνικά](README.el.md)** | **[T&uuml;rk&ccedil;e](README.tr.md)** | **[Polski](README.pl.md)** | **[Čeština](README.cs.md)** | **[Slovenčina](README.sk.md)** | **[Slovenščina](README.sl.md)** | **[Hrvatski](README.hr.md)** | **[Magyar](README.hu.md)** | **[Rom&acirc;n&atilde;](README.ro.md)** | **[Български](README.bg.md)** | **[Shqip](README.sq.md)** | **[Eesti](README.et.md)** | **[Latviešu](README.lv.md)** | **[Lietuvių](README.lt.md)** | **[Македонски](README.mk.md)** | **[Crnogorski](README.me.md)** | **[Русский](README.ru.md)** | **[Українська](README.uk.md)** | **[العربية](README.ar.md)** | **[日本語](README.ja.md)** | **[简体中文](README.zh.md)** | **[繁體中文](README.tw.md)** | **[한국어](README.ko.md)** | **[हिन्दी](README.hi.md)** | **[বাংলা](README.bn.md)** | **[اردو](README.ur.md)**

<img width="793" height="657" alt="System 7 kors pa modern haardvara" src="https://github.com/user-attachments/assets/be84b83e-191c-4f9d-a786-11d0bd04203b" />
<img width="801" height="662" alt="simpletextworks" src="https://github.com/user-attachments/assets/7c9ebe5b-22b4-4612-93a1-2076909d77cd" />
<img width="803" height="661" alt="macpaint" src="https://github.com/user-attachments/assets/cd3ed04a-fdde-4dd5-88ef-5b19b3a13a54" />

> ⚠️ **KONCEPTBEVIS** - Detta ar en experimentell, pedagogisk reimplementation av Apples Macintosh System 7. Detta ar INTE en fardig produkt och bor inte betraktas som produktionsklar mjukvara.

En reimplementation med oppen kallkod av Apple Macintosh System 7 for modern x86-haardvara, startbar via GRUB2/Multiboot2. Projektet syftar till att aterskapa den klassiska Mac OS-upplevelsen samtidigt som System 7-arkitekturen dokumenteras genom omvand ingenjorsanalys.

## 🎯 Projektstatus

**Nuvarande tillstand**: Aktiv utveckling med cirka 94 % av karnfunktionaliteten fardig

### Senaste uppdateringar (november 2025)

#### Forbattringar av Sound Manager ✅ KLART
- **Optimerad MIDI-konvertering**: Delad `SndMidiNoteToFreq()`-hjalpreda med uppslagstabell pa 37 poster (C3-B5) och oktavbaserad reservmetod for hela MIDI-omfanget (0-127)
- **Stod for asynkron uppspelning**: Komplett callback-infrastruktur for bade filuppspelning (`FilePlayCompletionUPP`) och kommandoexekvering (`SndCallBackProcPtr`)
- **Kanalbaserad ljuddirigering**: Prioritetssystem pa flera nivaer med ljud av- och pa-kontroller
  - Prioritetskanaler pa 4 nivaer (0-3) for haardvaruutmatning
  - Oberoende ljud av- och pa-kontroller per kanal
  - `SndGetActiveChannel()` returnerar den aktiva kanalen med hogst prioritet
  - Korrekt kanalinitiering med aktiverad flagga som standard
- **Produktionskvalitet**: All kod kompilerar utan varningar, inga malloc/free-overtramp upptackta
- **Commits**: 07542c5 (MIDI-optimering), 1854fe6 (asynkrona callbacks), a3433c6 (kanaldirigering)

#### Tidigare sessioners framsteg
- ✅ **Avancerad funktionsfas**: Sound Manager-kommandobearbetningsslinga, serialisering av flerkorstilstandar, utokade MIDI-/syntesfunktioner
- ✅ **Fonsterandringshantering**: Interaktiv storleksandring med korrekt kromhantering, storleksandringshandtag och skrivbordsstadning
- ✅ **PS/2-tangentbordsoversattning**: Fullstandig uppsattning 1 scancode-till-Toolbox-tangentkodsmappning
- ✅ **Multiplattforms-HAL**: Stod for x86, ARM och PowerPC med ren abstraktion

## 📊 Projektets fullstandighet

**Overgripande karnfunktionalitet**: Cirka 94 % fardig (uppskattat)

### Fullt fungerande ✅

- **Hardware Abstraction Layer (HAL)**: Fullstandig plattformsabstraktion for x86/ARM/PowerPC
- **Startsystem**: Startar framgangsrikt via GRUB2/Multiboot2 pa x86
- **Seriell loggning**: Modulbaserad loggning med korstidsfiltrering (Error/Warn/Info/Debug/Trace)
- **Grafikgrund**: VESA-framebuffer (800x600x32) med QuickDraw-primitiver inklusive XOR-lage
- **Skrivbordsrendering**: System 7-menyrad med regnbagsfargat Apple-logotyp, ikoner och skrivbordsmonster
- **Typografi**: Chicago-bitmapptypsnitt med pixelperfekt rendering och korrekt kerning, utokad Mac Roman (0x80-0xFF) for europeiska accenttecken
- **Internationalisering (i18n)**: Resursbaserad lokalisering med 38 sprak (engelska, franska, tyska, spanska, italienska, portugisiska, nederlandska, danska, norska, svenska, finska, islandska, grekiska, turkiska, polska, tjeckiska, slovakiska, slovenska, kroatiska, ungerska, rumanska, bulgariska, albanska, estniska, lettiska, litauiska, makedonska, montenegrinska, ryska, ukrainska, arabiska, japanska, forenklad kinesiska, traditionell kinesiska, koreanska, hindi, bengali, urdu), Locale Manager med sprakval vid uppstart, CJK-infrastruktur for multibyte-kodning
- **Font Manager**: Stod for flera storlekar (9-24pt), stilsyntes, FOND/NFNT-parsning, LRU-cachelagring
- **Inmatningssystem**: PS/2-tangentbord och mus med fullstandig handelseviderbefordran
- **Event Manager**: Kooperativ multitasking via WaitNextEvent med enhetlig handelseko
- **Memory Manager**: Zonbaserad allokering med 68K-tolkintegration
- **Menu Manager**: Fullstandiga rullgardinsmenyer med musspaarning och SaveBits/RestoreBits
- **Filsystem**: HFS med B-trad-implementation, mappfonster med VFS-upprakning
- **Window Manager**: Dra, storleksandring (med storleksandringshandtag), lagerhantering, aktivering
- **Time Manager**: Exakt TSC-kalibrering, mikrosekunds precision, generationskontroll
- **Resource Manager**: O(log n) binar sokning, LRU-cache, omfattande validering
- **Gestalt Manager**: Systeminformation for flera arkitekturer med arkitekturdetektering
- **TextEdit Manager**: Fullstandig textredigering med urklippsintegration
- **Scrap Manager**: Klassiskt Mac OS-urklipp med stod for flera dataformat
- **SimpleText-applikation**: Fullfjadrad MDI-textredigerare med klipp ut/kopiera/klistra in
- **List Manager**: System 7.1-kompatibla listkontroller med tangentbordsnavigation
- **Control Manager**: Standard- och rullningslistkontroller med CDEF-implementation
- **Dialog Manager**: Tangentbordsnavigation, fokusringar, kortkommandon
- **Segment Loader**: Portabelt ISA-agnostiskt 68K-segmentladdningssystem med omplacering
- **M68K-tolk**: Fullstandig instruktionsformedling med 84 opcode-hanterare, alla 14 adresseringslagen, undantags-/trap-ramverk
- **Sound Manager**: Kommandobearbetning, MIDI-konvertering, kanalhantering, callbacks
- **Device Manager**: DCE-hantering, drivrutinsinstallation/-borttagning och I/O-operationer
- **Uppstartsskarm**: Komplett stargrenssnitt med forloppsspaarning, fashantering och startbild
- **Color Manager**: Fargstillstandshantering med QuickDraw-integration

### Delvis implementerat ⚠️

- **Applikationsintegration**: M68K-tolk och segmentladdare klara; integrationstestning behovs for att verifiera att riktiga applikationer kors
- **Window Definition Procedures (WDEF)**: Grundstruktur pa plats, delvis formedling
- **Speech Manager**: API-ramverk och ljudgenomkoppling; talsyntesmotor ej implementerad
- **Undantagshantering (RTE)**: Atergang fran undantag delvis implementerad (stannar for narvarande istallet for att aterstalla kontext)

### Annu ej implementerat ❌

- **Utskrift**: Inget utskriftssystem
- **Natverk**: Ingen AppleTalk- eller natverksfunktionalitet
- **Skrivbordstillbehor**: Enbart ramverk
- **Avancerat ljud**: Sampeluppspelning, mixning (begransning av PC-hogtalare)

### Delsystem som inte kompileras 🔧

Foljande har kallkod men ar inte integrerade i karnan:
- **AppleEventManager** (8 filer): Meddelanden mellan applikationer; medvetet exkluderad pa grund av pthread-beroenden som inte ar kompatibla med fristaende miljo
- **FontResources** (enbart header): Typsnittsresursdefinitioner; faktiskt typsnittsstod tillhandahalls av kompilerad FontResourceLoader.c

## 🏗️ Arkitektur

### Tekniska specifikationer

- **Arkitektur**: Multiarkitektur via HAL (x86, ARM, PowerPC redo)
- **Startprotokoll**: Multiboot2 (x86), plattformsspecifika startladdare
- **Grafik**: VESA-framebuffer, 800x600 @ 32-bitars farg
- **Minneslayout**: Karnan laddas vid 1 MB fysisk adress (x86)
- **Tidtagning**: Arkitekturagnostisk med mikrosekunds precision (RDTSC/timerregister)
- **Prestanda**: Kallstart vid resursmiss <15 us, cachetraff <2 us, tidsdrift <100 ppm

### Kodbasstatistik

- **225+ kallfiler** med ~57 500+ rader kod
- **145+ headerfiler** over 28+ delsystem
- **69 resurstyper** extraherade fran System 7.1
- **Kompileringstid**: 3-5 sekunder pa modern haardvara
- **Karnstorlek**: ~4,16 MB
- **ISO-storlek**: ~12,5 MB

## 🔨 Bygga

### Krav

- **GCC** med 32-bitarsstod (`gcc-multilib` pa 64-bitarssystem)
- **GNU Make**
- **GRUB-verktyg**: `grub-mkrescue` (fran `grub2-common` eller `grub-pc-bin`)
- **QEMU** for testning (`qemu-system-i386`)
- **Python 3** for resursbearbetning
- **xxd** for binarkonvertering
- *(Valfritt)* **powerpc-linux-gnu** korsverktygskedja for PowerPC-byggen

### Installation pa Ubuntu/Debian

```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common
```

### Byggkommandon

```bash
# Bygg karna (x86 som standard)
make

# Bygg for specifik plattform
make PLATFORM=x86
make PLATFORM=arm        # kraver ARM bare-metal GCC
make PLATFORM=ppc        # experimentellt; kraver PowerPC ELF-verktygskedja

# Skapa startbar ISO
make iso

# Bygg med alla sprak
make LOCALE_FR=1 LOCALE_DE=1 LOCALE_ES=1 LOCALE_JA=1 LOCALE_ZH=1 LOCALE_KO=1 LOCALE_RU=1 LOCALE_UK=1 LOCALE_PL=1 LOCALE_CS=1 LOCALE_SQ=1 LOCALE_BG=1 LOCALE_HR=1 LOCALE_DA=1 LOCALE_NL=1 LOCALE_ET=1 LOCALE_FI=1 LOCALE_EL=1 LOCALE_HU=1 LOCALE_IS=1 LOCALE_IT=1 LOCALE_LV=1 LOCALE_LT=1 LOCALE_MK=1 LOCALE_ME=1 LOCALE_NO=1 LOCALE_PT=1 LOCALE_RO=1 LOCALE_SK=1 LOCALE_SL=1 LOCALE_SV=1 LOCALE_TR=1 LOCALE_HI=1 LOCALE_TW=1 LOCALE_AR=1 LOCALE_BN=1 LOCALE_UR=1

# Bygg med ett enda ytterligare sprak
make LOCALE_FR=1

# Bygg och kor i QEMU
make run

# Rensa byggartefakter
make clean

# Visa byggstatistik
make info
```

## 🚀 Kora

### Snabbstart (QEMU)

```bash
# Standardkorning med seriell loggning
make run

# Manuellt med alternativ
qemu-system-i386 -cdrom system71.iso -serial file:/tmp/serial.log -display sdl -vga std -m 256M
```

### QEMU-alternativ

```bash
# Med konsol-seriell utmatning
qemu-system-i386 -cdrom system71.iso -serial stdio -display sdl -m 256M

# Huvudlos (ingen grafikvisning)
qemu-system-i386 -cdrom system71.iso -serial stdio -display none -m 256M

# Med GDB-felsokning
make debug
# I en annan terminal: gdb kernel.elf -ex "target remote :1234"
```

## 📚 Dokumentation

### Komponentguider
- **Control Manager**: `docs/components/ControlManager/`
- **Dialog Manager**: `docs/components/DialogManager/`
- **Font Manager**: `docs/components/FontManager/`
- **Seriell loggning**: `docs/components/System/`
- **Event Manager**: `docs/components/EventManager.md`
- **Menu Manager**: `docs/components/MenuManager.md`
- **Window Manager**: `docs/components/WindowManager.md`
- **Resource Manager**: `docs/components/ResourceManager.md`

### Internationalisering
- **Locale Manager**: `include/LocaleManager/` -- sprakbyte vid korning, sprakval vid uppstart
- **Strangresurser**: `resources/strings/` -- STR#-resursfiler per sprak (34 sprak)
- **Utokade typsnitt**: `include/chicago_font_extended.h` -- Mac Roman 0x80-0xFF-glyfer for europeiska tecken
- **CJK-stod**: `include/TextEncoding/CJKEncoding.h`, `include/FontManager/CJKFont.h` -- multibyte-kodning och typsnittsinfrastruktur

### Implementationsstatus
- **IMPLEMENTATION_PRIORITIES.md**: Planerat arbete och fullstandighetsuppfoljning
- **IMPLEMENTATION_STATUS_AUDIT.md**: Detaljerad granskning av alla delsystem

### Projektfilosofi

**Arkeologiskt tillvagagangssatt** med evidensbaserad implementation:
1. Bygger pa Inside Macintosh-dokumentation och MPW Universal Interfaces
2. Alla viktiga beslut markerade med Finding-ID:n som refererar till stodbevis
3. Mal: beteendeparitet med original System 7, inte modernisering
4. Implementation i renrum (ingen original Apple-kallkod)

## 🐛 Kanda problem

1. **Ikondragningsartefakter**: Mindre visuella artefakter vid dragning av skrivbordsikoner
2. **M68K-exekvering stubbad**: Segmentladdare klar, exekveringsslinga ej implementerad
3. **Inget TrueType-stod**: Enbart bitmapptypsnitt (Chicago)
4. **HFS skrivskyddat**: Virtuellt filsystem, ingen skrivning till disk
5. **Inga stabilitetsgarantier**: Krascher och ovantad beteende ar vanligt

## 🤝 Bidra

Detta ar framst ett larande-/forskningsprojekt:

1. **Felrapporter**: Skapa arenden med detaljerade reproduktionssteg
2. **Testning**: Rapportera resultat pa olika haardvara/emulatorer
3. **Dokumentation**: Forbattra befintlig dokumentation eller lagg till nya guider

## 📖 Viktiga referenser

- **Inside Macintosh** (1992-1994): Officiell Apple Toolbox-dokumentation
- **MPW Universal Interfaces 3.2**: Kanoniska headerfiler och strukturdefinitioner
- **Guide to Macintosh Family Hardware**: Referens for haardvaruarkitektur

### Anvandbar verktyg

- **Mini vMac**: System 7-emulator for beteendereferens
- **ResEdit**: Resursredigerare for att studera System 7-resurser
- **Ghidra/IDA**: For ROM-disassembleringsanalys

## ⚖️ Juridiskt

Detta ar en **reimplementation i renrum** i utbildnings- och bevarandesyfte:

- **Ingen Apple-kallkod** anvandes
- Baserad enbart pa offentlig dokumentation och svartladeanalys
- "System 7", "Macintosh", "QuickDraw" ar varumarken tillhorande Apple Inc.
- Inte anslutet till, godkant av eller sponsrat av Apple Inc.

**Originalversionen av System 7:s ROM och mjukvara forblir Apple Inc.:s egendom.**

## 🙏 Tackord

- **Apple Computer, Inc.** for att ha skapat det ursprungliga System 7
- **Forfattarna till Inside Macintosh** for omfattande dokumentation
- **Gemenskapen for bevarande av klassisk Mac** for att halla plattformen vid liv
- **68k.news och Macintosh Garden** for resursarkiv

## 📊 Utvecklingsstatistik

- **Kodrader**: ~57 500+ (inklusive 2 500+ for segmentladdare)
- **Kompileringstid**: ~3-5 sekunder
- **Karnstorlek**: ~4,16 MB (kernel.elf)
- **ISO-storlek**: ~12,5 MB (system71.iso)
- **Felminskning**: 94 % av karnfunktionaliteten fungerar
- **Stora delsystem**: 28+ (Font, Window, Menu, Control, Dialog, TextEdit m.fl.)

## 🔮 Framtida riktning

**Planerat arbete**:

- Farstallande av M68K-tolkens exekveringsslinga
- Lagga till stod for TrueType-typsnitt
- CJK-bitmapptypsnittsresurser for japansk, kinesisk och koreansk rendering
- Implementera ytterligare kontroller (textfalt, popup-menyer, reglage)
- Diskskrivning for HFS-filsystemet
- Avancerade Sound Manager-funktioner (mixning, sampling)
- Grundlaggande skrivbordstillbehor (Minirakare, Anteckningsblock)

---

**Status**: Experimentell - Pedagogisk - Under utveckling

**Senast uppdaterad**: November 2025 (Sound Manager-forbattringar klara)

For fragor, problem eller diskussion, anvand GitHub Issues.
