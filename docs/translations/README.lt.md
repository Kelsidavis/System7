# System 7 - Portatyvus atvirojo kodo reimplementacija

**[English](../../README.md)** | **[Fran&ccedil;ais](README.fr.md)** | **[Deutsch](README.de.md)** | **[Espa&ntilde;ol](README.es.md)** | **[Italiano](README.it.md)** | **[Portugu&ecirc;s](README.pt.md)** | **[Nederlands](README.nl.md)** | **[Dansk](README.da.md)** | **[Norsk](README.no.md)** | **[Svenska](README.sv.md)** | **[Suomi](README.fi.md)** | **[&Iacute;slenska](README.is.md)** | **[Ελληνικά](README.el.md)** | **[T&uuml;rk&ccedil;e](README.tr.md)** | **[Polski](README.pl.md)** | **[Čeština](README.cs.md)** | **[Slovenčina](README.sk.md)** | **[Slovenščina](README.sl.md)** | **[Hrvatski](README.hr.md)** | **[Magyar](README.hu.md)** | **[Rom&acirc;n&atilde;](README.ro.md)** | **[Български](README.bg.md)** | **[Shqip](README.sq.md)** | **[Eesti](README.et.md)** | **[Latviešu](README.lv.md)** | **[Lietuvių](README.lt.md)** | **[Македонски](README.mk.md)** | **[Crnogorski](README.me.md)** | **[Русский](README.ru.md)** | **[Українська](README.uk.md)** | **[العربية](README.ar.md)** | **[日本語](README.ja.md)** | **[简体中文](README.zh.md)** | **[繁體中文](README.tw.md)** | **[한국어](README.ko.md)** | **[हिन्दी](README.hi.md)** | **[বাংলা](README.bn.md)** | **[اردو](README.ur.md)**

<img width="793" height="657" alt="System 7 veikia ant modernios aparatinės įrangos" src="https://github.com/user-attachments/assets/be84b83e-191c-4f9d-a786-11d0bd04203b" />
<img width="801" height="662" alt="simpletextworks" src="https://github.com/user-attachments/assets/7c9ebe5b-22b4-4612-93a1-2076909d77cd" />
<img width="803" height="661" alt="macpaint" src="https://github.com/user-attachments/assets/cd3ed04a-fdde-4dd5-88ef-5b19b3a13a54" />

> ⚠️ **KONCEPCIJOS PATVIRTINIMAS** - Tai eksperimentinė, edukacinė Apple Macintosh System 7 reimplementacija. Tai NERA baigtas produktas ir neturėtų būti laikoma gamybai tinkama programine įranga.

Atvirojo kodo Apple Macintosh System 7 reimplementacija šiuolaikinei x86 aparatinei įrangai, paleidžiama per GRUB2/Multiboot2. Šis projektas siekia atkurti klasikinę Mac OS patirtį, kartu dokumentuojant System 7 architektūrą per atvirkštinės inžinerijos analizę.

## 🎯 Projekto būsena

**Dabartinė būsena**: Aktyvus vystymas, ~94% pagrindinės funkcionalumo užbaigta

### Naujausi atnaujinimai (2025 m. lapkritis)

#### Garso tvarkyklės patobulinimai ✅ UŽBAIGTA
- **Optimizuota MIDI konversija**: Bendra `SndMidiNoteToFreq()` pagalbinė funkcija su 37 įrašų peržvalgos lentele (C3-B5) ir oktavomis pagrįstu atsarginiu variantu visam MIDI diapazonui (0-127)
- **Asinchroninio atkūrimo palaikymas**: Pilna atgalinio iškvietimo infrastruktūra tiek failų atkūrimui (`FilePlayCompletionUPP`), tiek komandų vykdymui (`SndCallBackProcPtr`)
- **Kanalais pagrįstas garso maršrutizavimas**: Daugiapakopė prioritetų sistema su nutildymo ir įjungimo valdikliais
  - 4 prioritetų lygių kanalai (0-3) aparatinės išvesties maršrutizavimui
  - Nepriklausomi nutildymo ir įjungimo valdikliai kiekvienam kanalui
  - `SndGetActiveChannel()` grąžina aukščiausio prioriteto aktyvų kanalą
  - Tinkamas kanalų inicializavimas su įjungtu žymekliu pagal numatytuosius nustatymus
- **Gamybinės kokybės implementacija**: Visas kodas kompiliuojamas be klaidų, nerasta malloc/free pažeidimų
- **Commitai**: 07542c5 (MIDI optimizavimas), 1854fe6 (asinchroniniai atgaliniai iškvietimai), a3433c6 (kanalų maršrutizavimas)

#### Ankstesnės sesijos pasiekimai
- ✅ **Išplėstinių funkcijų fazė**: Garso tvarkyklės komandų apdorojimo ciklas, daugiapakopė stilių serializacija, išplėstinės MIDI/sintezės funkcijos
- ✅ **Langų dydžio keitimo sistema**: Interaktyvus dydžio keitimas su tinkamu chromo apdorojimu, dydžio keitimo rankena ir darbalaukio valymu
- ✅ **PS/2 klaviatūros transliacija**: Pilnas 1 rinkinio skankodų vertimas į Toolbox klavišų kodus
- ✅ **Daugiaplatformis HAL**: x86, ARM ir PowerPC palaikymas su aiškia abstrakcija

## 📊 Projekto užbaigtumas

**Bendras pagrindinis funkcionalumas**: ~94% užbaigta (įvertinta)

### Kas veikia pilnai ✅

- **Aparatinės įrangos abstrakcijos sluoksnis (HAL)**: Pilna platformos abstrakcija x86/ARM/PowerPC
- **Paleidimo sistema**: Sėkmingai paleidžiama per GRUB2/Multiboot2 x86 platformoje
- **Nuoseklusis registravimas**: Moduliais pagrįstas registravimas su vykdymo metu filtravimu (Error/Warn/Info/Debug/Trace)
- **Grafikos pagrindas**: VESA kadro buferis (800x600x32) su QuickDraw primityvais, įskaitant XOR režimą
- **Darbalaukio atvaizdavimas**: System 7 meniu juosta su vaivorykštiniu Apple logotipu, piktogramomis ir darbalaukio šablonais
- **Tipografija**: Chicago bitmapinis šriftas su pikselių tikslumu ir tinkamu tarpų kėlimu, išplėstas Mac Roman (0x80-0xFF) europietiškiems kirčiuotiems simboliams
- **Internacionalizacija (i18n)**: Ištekliais pagrįsta lokalizacija su 38 kalbomis (anglų, prancūzų, vokiečių, ispanų, italų, portugalų, olandų, danų, norvegų, švedų, suomių, islandų, graikų, turkų, lenkų, čekų, slovakų, slovėnų, kroatų, vengrų, rumunų, bulgarų, albanų, estų, latvių, lietuvių, makedonų, juodkalniečių, rusų, ukrainiečių, arabų, japonų, supaprastintosios kinų, tradicinės kinų, korėjiečių, hindi, bengalų, urdu), Lokalės tvarkyklė su paleidimo metu pasirenkama kalba, CJK daugiabaitės koduotės infrastruktūra
- **Šriftų tvarkyklė**: Kelių dydžių palaikymas (9-24pt), stilių sintezė, FOND/NFNT analizė, LRU spartinimas
- **Įvesties sistema**: PS/2 klaviatūra ir pelė su pilnu įvykių persiųntimu
- **Įvykių tvarkyklė**: Kooperatyvusis daugiažadinis darbas per WaitNextEvent su vieninga įvykių eile
- **Atminties tvarkyklė**: Zonomis pagrįstas paskirstymas su 68K interpretatoriaus integracija
- **Meniu tvarkyklė**: Pilni išskleidžiamieji meniu su pelės sekimu ir SaveBits/RestoreBits
- **Failų sistema**: HFS su B-medžio implementacija, aplankų langai su VFS numeravimu
- **Langų tvarkyklė**: Vilkimas, dydžio keitimas (su dydžio keitimo rankena), sluoksniavimas, aktyvavimas
- **Laiko tvarkyklė**: Tikslus TSC kalibravimas, mikrosekundžių tikslumas, kartų tikrinimas
- **Išteklių tvarkyklė**: O(log n) dvejetainė paieška, LRU spartinimas, išsami validacija
- **Gestalt tvarkyklė**: Daugiaarchitektūrinė sistemos informacija su architektūros aptikimu
- **TextEdit tvarkyklė**: Pilnas teksto redagavimas su iškarpinės integracija
- **Scrap tvarkyklė**: Klasikinė Mac OS iškarpinė su kelių formatų palaikymu
- **SimpleText programa**: Pilnafunkcė MDI teksto rengyklė su iškirpimo/kopijavimo/įklijavimo funkcijomis
- **Sąrašo tvarkyklė**: Su System 7.1 suderinama sąrašo valdikliai su navigacija klaviatūra
- **Valdiklių tvarkyklė**: Standartiniai ir slinkties juostos valdikliai su CDEF implementacija
- **Dialogų tvarkyklė**: Navigacija klaviatūra, fokuso žiedai, sparčiosios klavišų kombinacijos
- **Segmentų įkroviklis**: Portatyvi nuo ISA nepriklausoma 68K segmentų įkėlimo sistema su perkėlimu
- **M68K interpretatorius**: Pilnas instrukcijų paskirstymas su 84 opkodo apdorotojais, visais 14 adresavimo režimų, išimčių/pertraukčių karkasas
- **Garso tvarkyklė**: Komandų apdorojimas, MIDI konversija, kanalų valdymas, atgaliniai iškvietimai
- **Įrenginių tvarkyklė**: DCE valdymas, tvarkyklių diegimas/šalinimas ir I/O operacijos
- **Paleidimo ekranas**: Pilna paleidimo vartotojo sąsaja su eigos sekimu, fazių valdymu ir pradinio ekrano rodymu
- **Spalvų tvarkyklė**: Spalvų būsenos valdymas su QuickDraw integracija

### Iš dalies implementuota ⚠️

- **Programų integracija**: M68K interpretatorius ir segmentų įkroviklis užbaigti; reikia integracinio testavimo, kad būtų patvirtinta, jog realios programos vykdomos
- **Langų apibrėžimo procedūros (WDEF)**: Pagrindinė struktūra sukurta, dalinis paskirstymas
- **Kalbos tvarkyklė**: API karkasas ir garso praleidimas; kalbos sintezės variklis neimplementuotas
- **Išimčių apdorojimas (RTE)**: Grįžimas iš išimties iš dalies implementuotas (šiuo metu sustoja, užuot atkūrus kontekstą)

### Dar neimplementuota ❌

- **Spausdinimas**: Nėra spausdinimo sistemos
- **Tinklas**: Nėra AppleTalk ar tinklo funkcionalumo
- **Darbalaukio priedai**: Tik karkasas
- **Išplėstinis garsas**: Pavyzdžių atkūrimas, maišymas (PC garsiakalbio apribojimai)

### Nekompiliuojami posistemiai 🔧

Šie posistemiai turi išeities kodą, bet nėra integruoti į branduolį:
- **AppleEventManager** (8 failai): Tarprograminiai pranešimai; sąmoningai neįtraukti dėl pthread priklausomybių, nesuderinamų su izoliuota aplinka
- **FontResources** (tik antraštė): Šriftų išteklių tipų apibrėžimai; faktinį šriftų palaikymą teikia kompiliuotas FontResourceLoader.c

## 🏗️ Architektūra

### Techninės specifikacijos

- **Architektūra**: Daugiaarchitektūrinė per HAL (x86, ARM, PowerPC paruošta)
- **Paleidimo protokolas**: Multiboot2 (x86), platformai specifiniai paleidimo įkrovikliai
- **Grafika**: VESA kadro buferis, 800x600 @ 32 bitų spalvos
- **Atminties išdėstymas**: Branduolys įkeliamas į 1MB fizinį adresą (x86)
- **Laikas**: Nuo architektūros nepriklausomas su mikrosekundžių tikslumu (RDTSC/laiko registrai)
- **Našumas**: Šaltas išteklių nepataikymas <15µs, spartinimo pataikymas <2µs, laiko nukrypimas <100ppm

### Kodų bazės statistika

- **225+ išeities failų** su ~57 500+ kodo eilučių
- **145+ antraštės failų** per 28+ posistemius
- **69 išteklių tipai** išgauti iš System 7.1
- **Kompiliavimo laikas**: 3-5 sekundės šiuolaikinėje aparatinėje įrangoje
- **Branduolio dydis**: ~4,16 MB
- **ISO dydis**: ~12,5 MB

## 🔨 Kompiliavimas

### Reikalavimai

- **GCC** su 32 bitų palaikymu (`gcc-multilib` 64 bitų sistemose)
- **GNU Make**
- **GRUB įrankiai**: `grub-mkrescue` (iš `grub2-common` arba `grub-pc-bin`)
- **QEMU** testavimui (`qemu-system-i386`)
- **Python 3** išteklių apdorojimui
- **xxd** dvejetainei konversijai
- *(Pasirinktinai)* **powerpc-linux-gnu** kryžminio kompiliavimo įrankių rinkinys PowerPC kūrimams

### Ubuntu/Debian diegimas

```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common
```

### Kompiliavimo komandos

```bash
# Kompiliuoti branduolį (x86 pagal nutylėjimą)
make

# Kompiliuoti konkrečiai platformai
make PLATFORM=x86
make PLATFORM=arm        # reikia ARM bare-metal GCC
make PLATFORM=ppc        # eksperimentinis; reikia PowerPC ELF įrankių rinkinio

# Sukurti paleidžiamą ISO
make iso

# Kompiliuoti su visomis kalbomis
make LOCALE_FR=1 LOCALE_DE=1 LOCALE_ES=1 LOCALE_JA=1 LOCALE_ZH=1 LOCALE_KO=1 LOCALE_RU=1 LOCALE_UK=1 LOCALE_PL=1 LOCALE_CS=1 LOCALE_SQ=1 LOCALE_BG=1 LOCALE_HR=1 LOCALE_DA=1 LOCALE_NL=1 LOCALE_ET=1 LOCALE_FI=1 LOCALE_EL=1 LOCALE_HU=1 LOCALE_IS=1 LOCALE_IT=1 LOCALE_LV=1 LOCALE_LT=1 LOCALE_MK=1 LOCALE_ME=1 LOCALE_NO=1 LOCALE_PT=1 LOCALE_RO=1 LOCALE_SK=1 LOCALE_SL=1 LOCALE_SV=1 LOCALE_TR=1 LOCALE_HI=1 LOCALE_TW=1 LOCALE_AR=1 LOCALE_BN=1 LOCALE_UR=1

# Kompiliuoti su viena papildoma kalba
make LOCALE_FR=1

# Kompiliuoti ir paleisti QEMU
make run

# Išvalyti artefaktus
make clean

# Rodyti kompiliavimo statistiką
make info
```

## 🚀 Paleidimas

### Greitas pradžia (QEMU)

```bash
# Standartinis paleidimas su nuosekliuoju registravimu
make run

# Rankiniu būdu su parinktimis
qemu-system-i386 -cdrom system71.iso -serial file:/tmp/serial.log -display sdl -vga std -m 256M
```

### QEMU parinktys

```bash
# Su konsolės nuosekliąja išvestimi
qemu-system-i386 -cdrom system71.iso -serial stdio -display sdl -m 256M

# Be grafikos (be grafinio ekrano)
qemu-system-i386 -cdrom system71.iso -serial stdio -display none -m 256M

# Su GDB derinimu
make debug
# Kitame terminale: gdb kernel.elf -ex "target remote :1234"
```

## 📚 Dokumentacija

### Komponentų vadovai
- **Valdiklių tvarkyklė**: `docs/components/ControlManager/`
- **Dialogų tvarkyklė**: `docs/components/DialogManager/`
- **Šriftų tvarkyklė**: `docs/components/FontManager/`
- **Nuoseklusis registravimas**: `docs/components/System/`
- **Įvykių tvarkyklė**: `docs/components/EventManager.md`
- **Meniu tvarkyklė**: `docs/components/MenuManager.md`
- **Langų tvarkyklė**: `docs/components/WindowManager.md`
- **Išteklių tvarkyklė**: `docs/components/ResourceManager.md`

### Internacionalizacija
- **Lokalės tvarkyklė**: `include/LocaleManager/` — vykdymo metu lokalės perjungimas, paleidimo metu kalbos pasirinkimas
- **Eilučių ištekliai**: `resources/strings/` — kiekvienai kalbai atskiri STR# išteklių failai (34 kalbos)
- **Išplėstiniai šriftai**: `include/chicago_font_extended.h` — Mac Roman 0x80-0xFF glifai europietiškiems simboliams
- **CJK palaikymas**: `include/TextEncoding/CJKEncoding.h`, `include/FontManager/CJKFont.h` — daugiabaitės koduotės ir šriftų infrastruktūra

### Implementacijos būsena
- **IMPLEMENTATION_PRIORITIES.md**: Planuojami darbai ir užbaigtumo sekimas
- **IMPLEMENTATION_STATUS_AUDIT.md**: Išsamus visų posistemių auditas

### Projekto filosofija

**Archeologinis požiūris** su įrodymais pagrįsta implementacija:
1. Paremta Inside Macintosh dokumentacija ir MPW Universal Interfaces
2. Visi svarbūs sprendimai pažymėti radinių identifikatoriais, nurodančiais pagrindžiančius įrodymus
3. Tikslas: elgesio paritetą su originaliu System 7, ne modernizacija
4. Švaraus kambario implementacija (nenaudojamas originalus Apple išeities kodas)

## 🐛 Žinomos problemos

1. **Piktogramų vilkimo artefaktai**: Nedideli vaizdiniai artefaktai vilkiant darbalaukio piktogramas
2. **M68K vykdymas stubais**: Segmentų įkroviklis užbaigtas, vykdymo ciklas neimplementuotas
3. **Nėra TrueType palaikymo**: Tik bitmapiniai šriftai (Chicago)
4. **HFS tik skaitymui**: Virtuali failų sistema, nėra tikro įrašymo į diską
5. **Nėra stabilumo garantijų**: Strigimai ir netikėtas elgesys yra dažni

## 🤝 Prisidėjimas

Tai pirmiausia mokymosi/tyrimo projektas:

1. **Klaidų pranešimai**: Pateikite problemas su išsamiais atkūrimo žingsniais
2. **Testavimas**: Praneškite rezultatus su skirtinga aparatine įranga/emuliatoriais
3. **Dokumentacija**: Tobulinkite esamą dokumentaciją arba pridėkite naujus vadovus

## 📖 Svarbios nuorodos

- **Inside Macintosh** (1992-1994): Oficiali Apple Toolbox dokumentacija
- **MPW Universal Interfaces 3.2**: Kanoniniai antraštės failai ir struktūrų apibrėžimai
- **Guide to Macintosh Family Hardware**: Aparatinės įrangos architektūros informacija

### Naudingi įrankiai

- **Mini vMac**: System 7 emuliatorius elgesio palyginimui
- **ResEdit**: Išteklių rengyklė System 7 išteklių tyrimui
- **Ghidra/IDA**: ROM disasembliavimo analizei

## ⚖️ Teisinė informacija

Tai **švaraus kambario reimplementacija** edukaciniais ir saugojimo tikslais:

- **Nenaudotas Apple išeities kodas**
- Paremta tik viešąja dokumentacija ir juodosios dėžės analize
- „System 7", „Macintosh", „QuickDraw" yra Apple Inc. prekių ženklai
- Nesusijęs su Apple Inc., nepatvirtintas ir neremiamas Apple Inc.

**Originalus System 7 ROM ir programinė įranga lieka Apple Inc. nuosavybe.**

## 🙏 Padėkos

- **Apple Computer, Inc.** už originalios System 7 sukūrimą
- **Inside Macintosh autoriams** už išsamią dokumentaciją
- **Klasikinio Mac išsaugojimo bendruomenei** už platformos palaikymą gyvą
- **68k.news ir Macintosh Garden** už išteklių archyvus

## 📊 Vystymo statistika

- **Kodo eilutės**: ~57 500+ (įskaitant 2 500+ segmentų įkrovikliui)
- **Kompiliavimo laikas**: ~3-5 sekundės
- **Branduolio dydis**: ~4,16 MB (kernel.elf)
- **ISO dydis**: ~12,5 MB (system71.iso)
- **Klaidų sumažinimas**: 94% pagrindinio funkcionalumo veikia
- **Pagrindiniai posistemiai**: 28+ (Font, Window, Menu, Control, Dialog, TextEdit ir kt.)

## 🔮 Ateities kryptys

**Planuojami darbai**:

- Užbaigti M68K interpretatoriaus vykdymo ciklą
- Pridėti TrueType šriftų palaikymą
- CJK bitmapinių šriftų ištekliai japonų, kinų ir korėjiečių atvaizdavimui
- Implementuoti papildomus valdiklius (teksto laukus, iššokančius meniu, slankiklius)
- Įrašymo į diską palaikymas HFS failų sistemai
- Išplėstinės garso tvarkyklės funkcijos (maišymas, diskretizavimas)
- Pagrindiniai darbalaukio priedai (skaičiuoklė, užrašų knygelė)

---

**Būsena**: Eksperimentinis - Edukacinis - Vystymo stadijoje

**Paskutinį kartą atnaujinta**: 2025 m. lapkritis (Garso tvarkyklės patobulinimai užbaigti)

Klausimams, problemoms ar diskusijoms naudokite GitHub Issues.
