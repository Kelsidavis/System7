# System 7 - Porditav avatud lahtekoodiga taasloomine

**[English](README.md)** | **[Fran&ccedil;ais](README.fr.md)** | **[Deutsch](README.de.md)** | **[Espa&ntilde;ol](README.es.md)** | **[Italiano](README.it.md)** | **[Portugu&ecirc;s](README.pt.md)** | **[Nederlands](README.nl.md)** | **[Dansk](README.da.md)** | **[Norsk](README.no.md)** | **[Svenska](README.sv.md)** | **[Suomi](README.fi.md)** | **[&Iacute;slenska](README.is.md)** | **[Ελληνικά](README.el.md)** | **[T&uuml;rk&ccedil;e](README.tr.md)** | **[Polski](README.pl.md)** | **[Čeština](README.cs.md)** | **[Slovenčina](README.sk.md)** | **[Slovenščina](README.sl.md)** | **[Hrvatski](README.hr.md)** | **[Magyar](README.hu.md)** | **[Rom&acirc;n&atilde;](README.ro.md)** | **[Български](README.bg.md)** | **[Shqip](README.sq.md)** | **[Eesti](README.et.md)** | **[Latviešu](README.lv.md)** | **[Lietuvių](README.lt.md)** | **[Македонски](README.mk.md)** | **[Crnogorski](README.me.md)** | **[Русский](README.ru.md)** | **[Українська](README.uk.md)** | **[日本語](README.ja.md)** | **[中文](README.zh.md)** | **[한국어](README.ko.md)** | **[हिन्दी](README.hi.md)**

<img width="793" height="657" alt="System 7 tootamas kaasaegsel riistvaral" src="https://github.com/user-attachments/assets/be84b83e-191c-4f9d-a786-11d0bd04203b" />
<img width="801" height="662" alt="simpletextworks" src="https://github.com/user-attachments/assets/7c9ebe5b-22b4-4612-93a1-2076909d77cd" />
<img width="803" height="661" alt="macpaint" src="https://github.com/user-attachments/assets/cd3ed04a-fdde-4dd5-88ef-5b19b3a13a54" />

> ⚠️ **KONTSEPTSIOONI TOESTUS** - See on eksperimentaalne, hariduslik taasloomine Apple'i Macintoshi System 7-st. See EI OLE valmis toode ja seda ei tohiks pidada tootmiskolglikuks tarkvaraks.

Avatud lahtekoodiga taasloomine Apple Macintosh System 7-st kaasaegsele x86 riistvarale, mis on kaeivitatav GRUB2/Multiboot2 kaudu. Selle projekti eesmaerk on taasluua klassikaline Mac OS-i kogemus, dokumenteerides samal ajal System 7 arhitektuuri poeoerdprojekteerimise analuusi kaudu.

## 🎯 Projekti olek

**Praegune seisund**: Aktiivne arendus, ~94% poehifunktsionaalsusest valmis

### Viimased uuendused (november 2025)

#### Sound Manager'i taeiendused ✅ VALMIS
- **Optimeeritud MIDI teisendus**: Jagatud `SndMidiNoteToFreq()` abifunktsioon 37-kirjelise otsingutabeliga (C3-B5) ja oktaavipohine varuvariant kogu MIDI vahemiku jaoks (0-127)
- **Asunkroonne taasesitus**: Taielik tagasikutsete infrastruktuur nii failide taasesituseks (`FilePlayCompletionUPP`) kui ka kaeskude taitmiseks (`SndCallBackProcPtr`)
- **Kanalipohine helimarsruutimine**: Mitmetasandiline prioriteedisusteem vaigistus- ja lubamisjuhtimisega
  - 4-tasandiline prioriteediga kanalid (0-3) riistvara valjundmarsruutimiseks
  - Solltumatu vaigistus- ja lubamisjuhtimine kanali kohta
  - `SndGetActiveChannel()` tagastab koergeima prioriteediga aktiivse kanali
  - Noetekohane kanali initsialiseerimine vaikimisi lubatud lipuga
- **Tootmiskvaliteediga juurutus**: Kogu kood kompileerub puhtalt, malloc/free rikkumisi ei tuvastatud
- **Commitid**: 07542c5 (MIDI optimeerimine), 1854fe6 (asunkroonsed tagasikutsed), a3433c6 (kanalimarsruutimine)

#### Eelmiste sessioonide saavutused
- ✅ **Taiustatud funktsioonide faas**: Sound Manager'i kaeskude tootlustsukkel, mitmekordse kasutamise stiili serialiseerimine, laiendatud MIDI/sunteesi funktsioonid
- ✅ **Akna suuruse muutmise susteem**: Interaktiivne suuruse muutmine noetekohase kroomi kasitlusega, suurenduskast ja toolaua puhastamine
- ✅ **PS/2 klaviatuuri translatatsioon**: Taielik set 1 skannkoodi teisendamine Toolbox'i klahvikoodideks
- ✅ **Mitmeplatvormne HAL**: x86, ARM ja PowerPC tugi puhta abstraktsiooniga

## 📊 Projekti valmidus

**Uldine poehifunktsionaalsus**: ~94% valmis (hinnanguline)

### Mis toeoetab taielikult ✅

- **Riistvara abstraktsioonikiht (HAL)**: Taielik platvormi abstraktsioon x86/ARM/PowerPC jaoks
- **Alglaadimissusteem**: Edukalt kaeivitatav GRUB2/Multiboot2 kaudu x86-l
- **Jadalogimine**: Moodulipohine logimine kaeitusaegse filtreerimisega (Error/Warn/Info/Debug/Trace)
- **Graafikabaas**: VESA kaadripuhver (800x600x32) koos QuickDraw primitiividega, sealhulgas XOR-reziim
- **Toolaua renderdamine**: System 7 menuueriba vikerkaarevaerilise Apple'i logoga, ikoonid ja toolaua mustrid
- **Tuepograafia**: Chicago bitikaardifont pikslitaeuslike renderdusel ja noetekohase kerninguga, laiendatud Mac Roman (0x80-0xFF) Euroopa taehemarkidega taehemaerkide jaoks
- **Rahvusvahelistamine (i18n)**: Ressursipohine lokaliseerimine 34 keelega (inglise, prantsuse, saksa, hispaania, itaalia, portugali, hollandi, taani, norra, rootsi, soome, islandi, kreeka, turgi, poola, tsehhi, slovaki, sloveeni, horvaadi, ungari, rumeenia, bulgaaria, albaania, eesti, laeti, leedu, makedoonia, montenegro, vene, ukraina, jaapani, hiina, korea, hindi), Locale Manager alglaadimisaegse keelevalikuga, CJK mitmebaidise kodeerimise infrastruktuur
- **Font Manager**: Mitme suuruse tugi (9-24pt), stiilisuntees, FOND/NFNT parsimine, LRU vahemalu
- **Sisendsusteem**: PS/2 klaviatuur ja hiir koos taieliku sundmuste edastamisega
- **Event Manager**: Kooperatiivne multitegumtoeo WaitNextEvent kaudu uhendatud sundmustejarjekorraga
- **Memory Manager**: Tsoonipohine maelujaotus 68K interpreteri integratsiooniga
- **Menu Manager**: Taielikud rippmenueud hiire jaelgimise ning SaveBits/RestoreBits-iga
- **Failisusteem**: HFS B-puu juurutusega, kaustade aknad VFS-i loetlemisega
- **Window Manager**: Lohistamine, suuruse muutmine (suurenduskastiga), kihistamine, aktiveerimine
- **Time Manager**: Taeuspaerane TSC kalibreerimine, mikrosekundiline taeupsis, pohkonna kontroll
- **Resource Manager**: O(log n) binaarne otsing, LRU vahemalu, pohjalik valideerimine
- **Gestalt Manager**: Mitmearitektuurne susteemiteave arhitektuuri tuvastamisega
- **TextEdit Manager**: Taielik tekstiredigeerimine loikelaua integratsiooniga
- **Scrap Manager**: Klassikaline Mac OS-i loikelaud mitme vormingu toega
- **SimpleText rakendus**: Taiefunktsionaalne MDI tekstiredaktor loikamise/kopeerimise/kleepimisega
- **List Manager**: System 7.1-ga uhilduv loendijuhtimine klaviatuurinavigatsiooniga
- **Control Manager**: Tavalised ja kerimisriba juhtelemendid CDEF-i juurutusega
- **Dialog Manager**: Klaviatuurinavigatsioon, fookusringid, kiirklahvid
- **Segment Loader**: Porditav ISA-agnostiline 68K segmendi laadimissusteem umber paigutamisega
- **M68K Interpreter**: Taielik kaeskude edastus 84 opkoodi kasitlejaga, koik 14 adresseerimisreziimi, erandi/loeksu raamistik
- **Sound Manager**: Kaeskude toeoeotlemine, MIDI teisendus, kanalihaldus, tagasikutsed
- **Device Manager**: DCE haldamine, draiverite installimine/eemaldamine ja I/O operatsioonid
- **Kaeiviisekraan**: Taielik alglaadimise kasutajaliides edenemise jaelgimise, faaside haldamise ja sissejuhatava ekraaniga
- **Color Manager**: Vaerviseisundi haldamine QuickDraw integratsiooniga

### Osaliselt juurutatud ⚠️

- **Rakenduste integratsioon**: M68K interpret ja segmendilaadija valmis; integratsiooni testimine vajalik parisrakenduste kaivitamise kontrollimiseks
- **Akna definitsiooniprotseduurid (WDEF)**: Pohistruktuur paigas, osaline edastus
- **Speech Manager**: API raamistik ja heli laebiviik ainult; koenesunteesi mootor pole juurutatud
- **Erandite kasitlemine (RTE)**: Erandist naasmine osaliselt juurutatud (praegu peatub konteksti taastamise asemel)

### Veel juurutamata ❌

- **Printimine**: Printimissusteem puudub
- **Voergustik**: AppleTalk ega voergufunktsionaalsus puudub
- **Toolaua tarvikud**: Ainult raamistik
- **Taiustatud heli**: Naeeidise taasesitus, miksimine (PC koelari piirang)

### Kompileerimata alamsuesteemid 🔧

Jaergnevatel on lahtekood olemas, kuid need pole kernelisse integreeritud:
- **AppleEventManager** (8 faili): Rakenduste vaheline soenumivahetamine; teadlikult vaelja jaeetud pthread soeltuvuste tottu, mis ei uehi iseseisvalt toeoetava keskkonnaga
- **FontResources** (ainult paeis): Fondiressursi tuuebi definitsioonid; tegelik fonditugi pakutakse kompileeritud FontResourceLoader.c kaudu

## 🏗️ Arhitektuur

### Tehnilised spetsifikatsioonid

- **Arhitektuur**: Mitmearitektuurne HAL-i kaudu (x86, ARM, PowerPC valmis)
- **Alglaadimisprotokoll**: Multiboot2 (x86), platvormispetsiifilised alglaadurid
- **Graafika**: VESA kaadripuhver, 800x600 @ 32-bitine vaervus
- **Maelukorraldus**: Kernel laetakse 1MB fuuesilisele aadressile (x86)
- **Ajastus**: Arhitektuurist soeltumatu mikrosekundilise taeususega (RDTSC/taimeri registrid)
- **Joedlus**: Kulma ressursi moedamoek <15us, vahemalu tabamus <2us, taimeri triiv <100ppm

### Koodibaasi statistika

- **225+ lahtekoodifaili** ~57 500+ koodireaga
- **145+ paesefaili** 28+ alamsuesteemis
- **69 ressursi tuuepi** System 7.1-st ekstraheeritud
- **Kompileerimisaeg**: 3-5 sekundit kaasaegsel riistvaral
- **Kerneli suurus**: ~4,16 MB
- **ISO suurus**: ~12,5 MB

## 🔨 Ehitamine

### Noeuded

- **GCC** 32-bitise toega (`gcc-multilib` 64-bitisel)
- **GNU Make**
- **GRUB toeoriistad**: `grub-mkrescue` (paketist `grub2-common` voi `grub-pc-bin`)
- **QEMU** testimiseks (`qemu-system-i386`)
- **Python 3** ressursside toeoeotlemiseks
- **xxd** binaarseks teisendamiseks
- *(Valikuline)* **powerpc-linux-gnu** riisttoeoeriist PowerPC ehituste jaoks

### Ubuntu/Debiani installimine

```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common
```

### Ehitamiskaesud

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

## 🚀 Kaivitamine

### Kiirstart (QEMU)

```bash
# Standard run with serial logging
make run

# Manually with options
qemu-system-i386 -cdrom system71.iso -serial file:/tmp/serial.log -display sdl -vga std -m 256M
```

### QEMU valikud

```bash
# With console serial output
qemu-system-i386 -cdrom system71.iso -serial stdio -display sdl -m 256M

# Headless (no graphics display)
qemu-system-i386 -cdrom system71.iso -serial stdio -display none -m 256M

# With GDB debugging
make debug
# In another terminal: gdb kernel.elf -ex "target remote :1234"
```

## 📚 Dokumentatsioon

### Komponendijuhendid
- **Control Manager**: `docs/components/ControlManager/`
- **Dialog Manager**: `docs/components/DialogManager/`
- **Font Manager**: `docs/components/FontManager/`
- **Jadalogimine**: `docs/components/System/`
- **Event Manager**: `docs/components/EventManager.md`
- **Menu Manager**: `docs/components/MenuManager.md`
- **Window Manager**: `docs/components/WindowManager.md`
- **Resource Manager**: `docs/components/ResourceManager.md`

### Rahvusvahelistamine
- **Locale Manager**: `include/LocaleManager/` -- kaeitusaegne lokaadivahetamine, alglaadimisaegne keelevalik
- **Soeneressursid**: `resources/strings/` -- keelepohised STR# ressursifailid (34 keelt)
- **Laiendatud fondid**: `include/chicago_font_extended.h` -- Mac Roman 0x80-0xFF gluefid Euroopa taehemarkide jaoks
- **CJK tugi**: `include/TextEncoding/CJKEncoding.h`, `include/FontManager/CJKFont.h` -- mitmebaidine kodeerimine ja fondi infrastruktuur

### Juurutuse olek
- **IMPLEMENTATION_PRIORITIES.md**: Planeeritud tooed ja valmiduse jaelgimine
- **IMPLEMENTATION_STATUS_AUDIT.md**: Koigi alamsuesteemide detailne audit

### Projekti filosoofia

**Arheoloogiline laehenemine** toendusmaterjalipohise juurutusega:
1. Toetatud Inside Macintosh'i dokumentatsiooni ja MPW Universal Interfaces'i poolt
2. Koik olulised otsused markitud leiuidentifikaatoritega, mis viitavad toendavatele materjalidele
3. Eesmaerk: kaeitumuslik samavaersus originaalse System 7-ga, mitte moderniseerimine
4. Puhta ruumi juurutus (Apple'i originaalset laehtekoodi ei kasutatud)

## 🐛 Teadaolevad probleemid

1. **Ikoonide lohistamise artefaktid**: Vaeikesed visuaalsed artefaktid toolaua ikoonide lohistamisel
2. **M68K taeitmine aseaine**: Segmendilaadija valmis, taitmistsukkel pole juurutatud
3. **TrueType tugi puudub**: Ainult bitikaardifondid (Chicago)
4. **HFS ainult lugemiseks**: Virtuaalne failisusteem, tegelikku kettakirjutust pole
5. **Stabiilsusgarantiid puuduvad**: Kokkujooksmised ja ootamatu kaeitumine on tavalised

## 🤝 Kaasaaitamine

See on peamiselt oppimis- ja uurimisprojekt:

1. **Veateated**: Esitage probleemid koos ueksikasjalike taasesitamisjuhistega
2. **Testimine**: Teatage tulemustest erinevatel riistvara/emulaatoritel
3. **Dokumentatsioon**: Taeustage olemasolevaid dokumente voi lisage uusi juhendeid

## 📖 Olulised viited

- **Inside Macintosh** (1992-1994): Apple'i ametlik Toolbox'i dokumentatsioon
- **MPW Universal Interfaces 3.2**: Kanoonilised paesefailid ja struktuuridefinitsioonid
- **Guide to Macintosh Family Hardware**: Riistvara arhitektuuri viide

### Kasulikud toeoriistad

- **Mini vMac**: System 7 emulaator kaeitumusliku viite jaoks
- **ResEdit**: Ressursiredaktor System 7 ressursside uurimiseks
- **Ghidra/IDA**: ROM-i disassemblemise analuusiks

## ⚖️ Oiguslik teave

See on **puhta ruumi taasloomine** hariduslikel ja saeilituseesmaerkidel:

- **Apple'i laehtekoodi** ei kasutatud
- Pohineb ainult avalikel dokumentidel ja musta kasti analuuesil
- "System 7", "Macintosh", "QuickDraw" on Apple Inc. kaubamargid
- Ei ole seotud, heakskiidetud ega toetatud Apple Inc. poolt

**Originaalne System 7 ROM ja tarkvara jaeavad Apple Inc. omandiks.**

## 🙏 Taenuavaldused

- **Apple Computer, Inc.** originaalse System 7 loomise eest
- **Inside Macintosh'i autorid** pohjaliku dokumentatsiooni eest
- **Klassikalise Maci saeilitmiskogukond** platvormi elushoeidmise eest
- **68k.news ja Macintosh Garden** ressursiarhiivide eest

## 📊 Arendusstatistika

- **Koodiridu**: ~57 500+ (sealhulgas 2 500+ segmendilaadija jaoks)
- **Kompileerimisaeg**: ~3-5 sekundit
- **Kerneli suurus**: ~4,16 MB (kernel.elf)
- **ISO suurus**: ~12,5 MB (system71.iso)
- **Vigade vaehendamine**: 94% poehifunktsionaalsusest toeoetab
- **Suuremad alamsuesteemid**: 28+ (Font, Window, Menu, Control, Dialog, TextEdit jne)

## 🔮 Tulevikusuund

**Planeeritud tooed**:

- M68K interpreteri taitmistsukli loeepuleviimine
- TrueType fontide toe lisamine
- CJK bitikaardifondressursid jaapani, hiina ja korea renderdamiseks
- Taiendavate juhtelementide juurutamine (tekstivaelaad, huepikmenueud, liugurid)
- Kettakirjutus HFS failisuesteemi jaoks
- Taiustatud Sound Manager'i funktsioonid (miksimine, diskreetimus)
- Poehilised toolaua tarvikud (Calculator, Note Pad)

---

**Olek**: Eksperimentaalne - Hariduslik - Arenduses

**Viimati uuendatud**: November 2025 (Sound Manager'i taeiendused valmis)

Kuesimuste, probleemide voi arutelu jaoks kasutage palun GitHub Issues'it.
