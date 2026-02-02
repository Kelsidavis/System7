# System 7 - Portat&#299;va atvērt&#257; koda reimplementācija

**[English](README.md)** | **[Fran&ccedil;ais](README.fr.md)** | **[Deutsch](README.de.md)** | **[Espa&ntilde;ol](README.es.md)** | **[Italiano](README.it.md)** | **[Portugu&ecirc;s](README.pt.md)** | **[Nederlands](README.nl.md)** | **[Dansk](README.da.md)** | **[Norsk](README.no.md)** | **[Svenska](README.sv.md)** | **[Suomi](README.fi.md)** | **[&Iacute;slenska](README.is.md)** | **[Ελληνικά](README.el.md)** | **[T&uuml;rk&ccedil;e](README.tr.md)** | **[Polski](README.pl.md)** | **[Čeština](README.cs.md)** | **[Slovenčina](README.sk.md)** | **[Slovenščina](README.sl.md)** | **[Hrvatski](README.hr.md)** | **[Magyar](README.hu.md)** | **[Rom&acirc;n&atilde;](README.ro.md)** | **[Български](README.bg.md)** | **[Shqip](README.sq.md)** | **[Eesti](README.et.md)** | **[Latviešu](README.lv.md)** | **[Lietuvių](README.lt.md)** | **[Македонски](README.mk.md)** | **[Crnogorski](README.me.md)** | **[Русский](README.ru.md)** | **[Українська](README.uk.md)** | **[العربية](README.ar.md)** | **[日本語](README.ja.md)** | **[简体中文](README.zh.md)** | **[繁體中文](README.tw.md)** | **[한국어](README.ko.md)** | **[हिन्दी](README.hi.md)** | **[বাংলা](README.bn.md)** | **[اردو](README.ur.md)**

<img width="793" height="657" alt="System 7 darbojas uz modernas aparatūras" src="https://github.com/user-attachments/assets/be84b83e-191c-4f9d-a786-11d0bd04203b" />
<img width="801" height="662" alt="simpletextworks" src="https://github.com/user-attachments/assets/7c9ebe5b-22b4-4612-93a1-2076909d77cd" />
<img width="803" height="661" alt="macpaint" src="https://github.com/user-attachments/assets/cd3ed04a-fdde-4dd5-88ef-5b19b3a13a54" />

> ⚠️ **KONCEPCIJAS PIERĀDĪJUMS** - Šī ir eksperimentāla, izglītojoša Apple Macintosh System 7 reimplementācija. Tas NAV pabeigts produkts, un to nevajadzētu uzskatīt par produkcijas kvalitātes programmatūru.

Atvērtā koda Apple Macintosh System 7 reimplementācija modernai x86 aparatūrai, kas sāknējama caur GRUB2/Multiboot2. Šī projekta mērķis ir atjaunot klasisko Mac OS pieredzi, vienlaikus dokumentējot System 7 arhitektūru, izmantojot reversās inženierijas analīzi.

## 🎯 Projekta statuss

**Pašreizējais stāvoklis**: Aktīva izstrāde ar ~94% pamatfunkcionalitātes pabeigšanu

### Jaunākie atjauninājumi (2025. gada novembris)

#### Sound Manager uzlabojumi ✅ PABEIGTS
- **Optimizēta MIDI konvertēšana**: Koplietots `SndMidiNoteToFreq()` palīgs ar 37 ierakstu uzmeklēšanas tabulu (C3-B5) un oktāvu balstītu rezerves mehānismu pilnam MIDI diapazonam (0-127)
- **Asinhronās atskaņošanas atbalsts**: Pilnīga atzvanu infrastruktūra gan failu atskaņošanai (`FilePlayCompletionUPP`), gan komandu izpildei (`SndCallBackProcPtr`)
- **Kanālu balstīta audio maršrutēšana**: Daudzlīmeņu prioritāšu sistēma ar izslēgšanas un iespējošanas vadīklām
  - 4 līmeņu prioritāšu kanāli (0-3) aparatūras izvades maršrutēšanai
  - Neatkarīgas izslēgšanas un iespējošanas vadīklas katram kanālam
  - `SndGetActiveChannel()` atgriež augstākās prioritātes aktīvo kanālu
  - Pareiza kanālu inicializācija ar iespējotu karogu pēc noklusējuma
- **Produkcijas kvalitātes implementācija**: Viss kods kompilējas tīri, nav konstatētu malloc/free pārkāpumu
- **Komiti**: 07542c5 (MIDI optimizācija), 1854fe6 (asinhronie atzvani), a3433c6 (kanālu maršrutēšana)

#### Iepriekšējās sesijas sasniegumi
- ✅ **Paplašināto funkciju fāze**: Sound Manager komandu apstrādes cilpa, daudzpiegājienu stila serializācija, paplašinātas MIDI/sintēzes funkcijas
- ✅ **Logu izmēru maiņas sistēma**: Interaktīva izmēru maiņa ar pareizu hroma apstrādi, palielināšanas lodziņu un darbvirsmas tīrīšanu
- ✅ **PS/2 tastatūras tulkošana**: Pilna 1. kopas skenēšanas kodu kartēšana uz Toolbox taustiņu kodiem
- ✅ **Daudzplatformu HAL**: x86, ARM un PowerPC atbalsts ar tīru abstrakciju

## 📊 Projekta pilnīgums

**Kopējā pamatfunkcionalitāte**: ~94% pabeigta (aptuveni)

### Pilnībā strādā ✅

- **Aparatūras abstrakcijas slānis (HAL)**: Pilnīga platformas abstrakcija x86/ARM/PowerPC
- **Sāknēšanas sistēma**: Veiksmīgi sāknējas caur GRUB2/Multiboot2 uz x86
- **Seriālā žurnalēšana**: Moduļu balstīta žurnalēšana ar izpildlaika filtrēšanu (Error/Warn/Info/Debug/Trace)
- **Grafikas pamats**: VESA kadru buferis (800x600x32) ar QuickDraw primitīviem, ieskaitot XOR režīmu
- **Darbvirsmas renderēšana**: System 7 izvēlņu josla ar varavīksnes Apple logotipu, ikonām un darbvirsmas rakstiem
- **Tipogrāfija**: Chicago bitmapes fonts ar pikseļu precīzu renderēšanu un pareizu kerningu, paplašināts Mac Roman (0x80-0xFF) Eiropas diakritiskajām rakstzīmēm
- **Internacionalizācija (i18n)**: Resursu balstīta lokalizācija ar 38 valodām (angļu, franču, vācu, spāņu, itāļu, portugāļu, holandiešu, dāņu, norvēģu, zviedru, somu, islandiešu, grieķu, turku, poļu, čehu, slovāku, slovēņu, horvātu, ungāru, rumāņu, bulgāru, albāņu, igauņu, latviešu, lietuviešu, maķedoniešu, melnkalniešu, krievu, ukraiņu, arābu, japāņu, vienkāršotā ķīniešu, tradicionālā ķīniešu, korejiešu, hindi, bengāļu, urdu), Locale Manager ar sāknēšanas laika valodas izvēli, CJK daudzbitu kodēšanas infrastruktūra
- **Font Manager**: Daudzizmēru atbalsts (9-24pt), stilu sintēze, FOND/NFNT parsēšana, LRU kešošana
- **Ievades sistēma**: PS/2 tastatūra un pele ar pilnīgu notikumu pārsūtīšanu
- **Event Manager**: Kooperatīvā daudzuzdevumu veikšana caur WaitNextEvent ar vienotu notikumu rindu
- **Memory Manager**: Zonu balstīta piešķiršana ar 68K interpretatora integrāciju
- **Menu Manager**: Pilnīgas nolaižamās izvēlnes ar peles izsekošanu un SaveBits/RestoreBits
- **Failu sistēma**: HFS ar B-koku implementāciju, mapju logi ar VFS uzskaitījumu
- **Window Manager**: Vilkšana, izmēru maiņa (ar palielināšanas lodziņu), slāņošana, aktivizēšana
- **Time Manager**: Precīza TSC kalibrēšana, mikrosekunžu precizitāte, paaudžu pārbaude
- **Resource Manager**: O(log n) binārā meklēšana, LRU kešatmiņa, visaptveroša validācija
- **Gestalt Manager**: Daudzarhitektūru sistēmas informācija ar arhitektūras noteikšanu
- **TextEdit Manager**: Pilnīga teksta rediģēšana ar starpliktuves integrāciju
- **Scrap Manager**: Klasiskā Mac OS starpliktuve ar daudzu formātu atbalstu
- **SimpleText lietojumprogramma**: Pilnvērtīgs MDI teksta redaktors ar izgriešanu/kopēšanu/ielīmēšanu
- **List Manager**: System 7.1 saderīgas sarakstu vadīklas ar tastatūras navigāciju
- **Control Manager**: Standarta un ritjoslu vadīklas ar CDEF implementāciju
- **Dialog Manager**: Tastatūras navigācija, fokusa gredzeni, tastatūras īsceļi
- **Segment Loader**: Portatīva ISA-neatkarīga 68K segmentu ielādes sistēma ar pārvietošanu
- **M68K interpretators**: Pilna instrukciju dispečēšana ar 84 operāciju kodu apstrādātājiem, visiem 14 adresēšanas režīmiem, izņēmumu/slazdu ietvaru
- **Sound Manager**: Komandu apstrāde, MIDI konvertēšana, kanālu pārvaldība, atzvani
- **Device Manager**: DCE pārvaldība, draiveru instalēšana/noņemšana un I/O operācijas
- **Startēšanas ekrāns**: Pilnīgs sāknēšanas UI ar progresa izsekošanu, fāžu pārvaldību un uzplaiksnījuma ekrānu
- **Color Manager**: Krāsu stāvokļa pārvaldība ar QuickDraw integrāciju

### Daļēji implementēts ⚠️

- **Lietojumprogrammu integrācija**: M68K interpretators un segmentu ielādētājs pabeigti; nepieciešama integrācijas testēšana, lai pārbaudītu reālu lietojumprogrammu izpildi
- **Logu definīciju procedūras (WDEF)**: Pamata struktūra izveidota, daļēja dispečēšana
- **Speech Manager**: Tikai API ietvars un audio caurlaišana; runas sintēzes dzinējs nav implementēts
- **Izņēmumu apstrāde (RTE)**: Atgriešanās no izņēmuma daļēji implementēta (pašlaik apstājas, nevis atjauno kontekstu)

### Vēl nav implementēts ❌

- **Drukāšana**: Nav drukas sistēmas
- **Tīklošana**: Nav AppleTalk vai tīkla funkcionalitātes
- **Darbvirsmas piederumi**: Tikai ietvars
- **Paplašinātais audio**: Paraugu atskaņošana, miksēšana (PC skaļruņa ierobežojums)

### Nekompilētās apakšsistēmas 🔧

Šīm apakšsistēmām ir pirmkods, bet tās nav integrētas kodolā:
- **AppleEventManager** (8 faili): Starplietojumprogrammu ziņapmaiņa; apzināti izslēgts pthread atkarību dēļ, kas nav saderīgas ar brīvstāvošu vidi
- **FontResources** (tikai galvene): Fontu resursu tipu definīcijas; faktisko fontu atbalstu nodrošina kompilētais FontResourceLoader.c

## 🏗️ Arhitektūra

### Tehniskās specifikācijas

- **Arhitektūra**: Daudzarhitektūru caur HAL (x86, ARM, PowerPC gatavs)
- **Sāknēšanas protokols**: Multiboot2 (x86), platformai specifiski sāknēšanas ielādētāji
- **Grafika**: VESA kadru buferis, 800x600 @ 32 bitu krāsa
- **Atmiņas izkārtojums**: Kodols ielādējas 1MB fiziskajā adresē (x86)
- **Laika noteikšana**: Arhitektūrai neatkarīga ar mikrosekunžu precizitāti (RDTSC/taimeru reģistri)
- **Veiktspēja**: Aukstais resursu promaha <15µs, kešatmiņas trāpījums <2µs, taimera novirze <100ppm

### Kodu bāzes statistika

- **225+ pirmkoda faili** ar ~57 500+ koda rindām
- **145+ galveņu faili** 28+ apakšsistēmās
- **69 resursu tipi**, kas iegūti no System 7.1
- **Kompilēšanas laiks**: 3-5 sekundes uz modernas aparatūras
- **Kodola izmērs**: ~4,16 MB
- **ISO izmērs**: ~12,5 MB

## 🔨 Kompilēšana

### Prasības

- **GCC** ar 32 bitu atbalstu (`gcc-multilib` uz 64 bitu sistēmām)
- **GNU Make**
- **GRUB rīki**: `grub-mkrescue` (no `grub2-common` vai `grub-pc-bin`)
- **QEMU** testēšanai (`qemu-system-i386`)
- **Python 3** resursu apstrādei
- **xxd** binārai konvertēšanai
- *(Neobligāti)* **powerpc-linux-gnu** krustkompilatora rīku ķēde PowerPC būvējumiem

### Ubuntu/Debian instalēšana

```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common
```

### Kompilēšanas komandas

```bash
# Kompilēt kodolu (x86 pēc noklusējuma)
make

# Kompilēt konkrētai platformai
make PLATFORM=x86
make PLATFORM=arm        # nepieciešams ARM bare-metal GCC
make PLATFORM=ppc        # eksperimentāls; nepieciešama PowerPC ELF rīku ķēde

# Izveidot sāknējamu ISO
make iso

# Kompilēt ar visām valodām
make LOCALE_FR=1 LOCALE_DE=1 LOCALE_ES=1 LOCALE_JA=1 LOCALE_ZH=1 LOCALE_KO=1 LOCALE_RU=1 LOCALE_UK=1 LOCALE_PL=1 LOCALE_CS=1 LOCALE_SQ=1 LOCALE_BG=1 LOCALE_HR=1 LOCALE_DA=1 LOCALE_NL=1 LOCALE_ET=1 LOCALE_FI=1 LOCALE_EL=1 LOCALE_HU=1 LOCALE_IS=1 LOCALE_IT=1 LOCALE_LV=1 LOCALE_LT=1 LOCALE_MK=1 LOCALE_ME=1 LOCALE_NO=1 LOCALE_PT=1 LOCALE_RO=1 LOCALE_SK=1 LOCALE_SL=1 LOCALE_SV=1 LOCALE_TR=1 LOCALE_HI=1 LOCALE_TW=1 LOCALE_AR=1 LOCALE_BN=1 LOCALE_UR=1

# Kompilēt ar vienu papildu valodu
make LOCALE_FR=1

# Kompilēt un palaist QEMU
make run

# Notīrīt artefaktus
make clean

# Parādīt kompilēšanas statistiku
make info
```

## 🚀 Palaišana

### Ātrais sākums (QEMU)

```bash
# Standarta palaišana ar seriālo žurnalēšanu
make run

# Manuāli ar opcijām
qemu-system-i386 -cdrom system71.iso -serial file:/tmp/serial.log -display sdl -vga std -m 256M
```

### QEMU opcijas

```bash
# Ar konsoles seriālo izvadi
qemu-system-i386 -cdrom system71.iso -serial stdio -display sdl -m 256M

# Bez grafiskā displeja
qemu-system-i386 -cdrom system71.iso -serial stdio -display none -m 256M

# Ar GDB atkļūdošanu
make debug
# Citā terminālī: gdb kernel.elf -ex "target remote :1234"
```

## 📚 Dokumentācija

### Komponentu rokasgrāmatas
- **Control Manager**: `docs/components/ControlManager/`
- **Dialog Manager**: `docs/components/DialogManager/`
- **Font Manager**: `docs/components/FontManager/`
- **Seriālā žurnalēšana**: `docs/components/System/`
- **Event Manager**: `docs/components/EventManager.md`
- **Menu Manager**: `docs/components/MenuManager.md`
- **Window Manager**: `docs/components/WindowManager.md`
- **Resource Manager**: `docs/components/ResourceManager.md`

### Internacionalizācija
- **Locale Manager**: `include/LocaleManager/` — izpildlaika lokāles pārslēgšana, sāknēšanas laika valodas izvēle
- **Virkņu resursi**: `resources/strings/` — valodai specifiski STR# resursu faili (34 valodas)
- **Paplašinātie fonti**: `include/chicago_font_extended.h` — Mac Roman 0x80-0xFF glifi Eiropas rakstzīmēm
- **CJK atbalsts**: `include/TextEncoding/CJKEncoding.h`, `include/FontManager/CJKFont.h` — daudzbitu kodēšanas un fontu infrastruktūra

### Implementācijas statuss
- **IMPLEMENTATION_PRIORITIES.md**: Plānotais darbs un pilnīguma izsekošana
- **IMPLEMENTATION_STATUS_AUDIT.md**: Detalizēts visu apakšsistēmu audits

### Projekta filozofija

**Arheoloģiskā pieeja** ar pierādījumos balstītu implementāciju:
1. Pamatota ar Inside Macintosh dokumentāciju un MPW Universal Interfaces
2. Visi galvenie lēmumi atzīmēti ar atrašanas ID, kas atsaucas uz pamatojošiem pierādījumiem
3. Mērķis: uzvedības paritāte ar oriģinālo System 7, nevis modernizācija
4. Tīras istabas implementācija (nav izmantots oriģinālais Apple pirmkods)

## 🐛 Zināmās problēmas

1. **Ikonu vilkšanas artefakti**: Nelieli vizuālie artefakti darbvirsmas ikonu vilkšanas laikā
2. **M68K izpilde aizstāta ar spraudni**: Segmentu ielādētājs pabeigts, izpildes cilpa nav implementēta
3. **Nav TrueType atbalsta**: Tikai bitmapes fonti (Chicago)
4. **HFS tikai lasāms**: Virtuālā failu sistēma, nav reālas diska rakstīšanas
5. **Nav stabilitātes garantiju**: Avārijas un neparedzēta uzvedība ir bieža

## 🤝 Ieguldījums

Šis galvenokārt ir mācību/pētniecības projekts:

1. **Kļūdu ziņojumi**: Iesniedziet problēmas ar detalizētiem reproducēšanas soļiem
2. **Testēšana**: Ziņojiet par rezultātiem uz dažādas aparatūras/emulatoriem
3. **Dokumentācija**: Uzlabojiet esošo dokumentāciju vai pievienojiet jaunas rokasgrāmatas

## 📖 Būtiskās atsauces

- **Inside Macintosh** (1992-1994): Oficiālā Apple Toolbox dokumentācija
- **MPW Universal Interfaces 3.2**: Kanoniskie galveņu faili un struktūru definīcijas
- **Guide to Macintosh Family Hardware**: Aparatūras arhitektūras atsauce

### Noderīgi rīki

- **Mini vMac**: System 7 emulators uzvedības atsaucei
- **ResEdit**: Resursu redaktors System 7 resursu izpētei
- **Ghidra/IDA**: ROM izjaukšanas analīzei

## ⚖️ Juridiskā informācija

Šī ir **tīras istabas reimplementācija** izglītības un saglabāšanas nolūkos:

- **Nav izmantots Apple pirmkods**
- Balstīta tikai uz publisku dokumentāciju un melnās kastes analīzi
- "System 7", "Macintosh", "QuickDraw" ir Apple Inc. preču zīmes
- Nav saistīts ar Apple Inc., nav tās apstiprināts vai sponsorēts

**Oriģinālā System 7 ROM un programmatūra paliek Apple Inc. īpašums.**

## 🙏 Pateicības

- **Apple Computer, Inc.** par oriģinālā System 7 izveidi
- **Inside Macintosh autoriem** par visaptverošu dokumentāciju
- **Klasiskā Mac saglabāšanas kopienai** par platformas uzturēšanu dzīvu
- **68k.news un Macintosh Garden** par resursu arhīviem

## 📊 Izstrādes statistika

- **Koda rindas**: ~57 500+ (ieskaitot 2 500+ segmentu ielādētājam)
- **Kompilēšanas laiks**: ~3-5 sekundes
- **Kodola izmērs**: ~4,16 MB (kernel.elf)
- **ISO izmērs**: ~12,5 MB (system71.iso)
- **Kļūdu samazinājums**: 94% pamatfunkcionalitātes darbojas
- **Galvenās apakšsistēmas**: 28+ (Font, Window, Menu, Control, Dialog, TextEdit u.c.)

## 🔮 Nākotnes virziens

**Plānotais darbs**:

- Pabeigt M68K interpretatora izpildes cilpu
- Pievienot TrueType fontu atbalstu
- CJK bitmapes fontu resursi japāņu, ķīniešu un korejiešu renderēšanai
- Implementēt papildu vadīklas (teksta lauki, uznirstošās izvēlnes, slīdņi)
- Diska rakstīšanas atbalsts HFS failu sistēmai
- Paplašinātas Sound Manager funkcijas (miksēšana, paraugu ņemšana)
- Pamata darbvirsmas piederumi (Calculator, Note Pad)

---

**Statuss**: Eksperimentāls - Izglītojošs - Izstrādē

**Pēdējo reizi atjaunināts**: 2025. gada novembris (Sound Manager uzlabojumi pabeigti)

Jautājumiem, problēmām vai diskusijām, lūdzu, izmantojiet GitHub Issues.
