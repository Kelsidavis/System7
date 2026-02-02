# System 7 - Flytjanleg endursmidun med opnum hugbunadarkodum

**[English](README.md)** | **[Fran&ccedil;ais](README.fr.md)** | **[Deutsch](README.de.md)** | **[Espa&ntilde;ol](README.es.md)** | **[Italiano](README.it.md)** | **[Portugu&ecirc;s](README.pt.md)** | **[Nederlands](README.nl.md)** | **[Dansk](README.da.md)** | **[Norsk](README.no.md)** | **[Svenska](README.sv.md)** | **[Suomi](README.fi.md)** | **[&Iacute;slenska](README.is.md)** | **[Ελληνικά](README.el.md)** | **[T&uuml;rk&ccedil;e](README.tr.md)** | **[Polski](README.pl.md)** | **[Čeština](README.cs.md)** | **[Slovenčina](README.sk.md)** | **[Slovenščina](README.sl.md)** | **[Hrvatski](README.hr.md)** | **[Magyar](README.hu.md)** | **[Rom&acirc;n&atilde;](README.ro.md)** | **[Български](README.bg.md)** | **[Shqip](README.sq.md)** | **[Eesti](README.et.md)** | **[Latviešu](README.lv.md)** | **[Lietuvių](README.lt.md)** | **[Македонски](README.mk.md)** | **[Crnogorski](README.me.md)** | **[Русский](README.ru.md)** | **[Українська](README.uk.md)** | **[العربية](README.ar.md)** | **[日本語](README.ja.md)** | **[简体中文](README.zh.md)** | **[繁體中文](README.tw.md)** | **[한국어](README.ko.md)** | **[हिन्दी](README.hi.md)** | **[বাংলা](README.bn.md)** | **[اردو](README.ur.md)**

<img width="793" height="657" alt="System 7 keyrt a nuthima velbunadi" src="https://github.com/user-attachments/assets/be84b83e-191c-4f9d-a786-11d0bd04203b" />
<img width="801" height="662" alt="simpletextworks" src="https://github.com/user-attachments/assets/7c9ebe5b-22b4-4612-93a1-2076909d77cd" />
<img width="803" height="661" alt="macpaint" src="https://github.com/user-attachments/assets/cd3ed04a-fdde-4dd5-88ef-5b19b3a13a54" />

> ⚠️ **PRUFSTODUVERKEFNI** - Thetta er tilraunakennd og fraedsluleg endursmidun a Apple Macintosh System 7. Thetta er EKKI fullunnin afurdh og aetti ekki adh teljast framleidhslutilbuinn hugbunadhur.

Endursmidun med opnum hugbunadarkodum a Apple Macintosh System 7 fyrir nuthima x86-velbunah, raesanleg gegnum GRUB2/Multiboot2. Verkefnidh midar adh endurskapa klassiska Mac OS-upplifunina og skjalfestar System 7-arkitekturfraedhi medh oefugverkfraedhi.

## 🎯 Stada verkefnisins

**Nuverandi stada**: Virk throun medh ~94% kjarnavirknigetu lokidh

### Nyjastar uppfaerslur (november 2025)

#### Endurbaeturr a hljodstjora ✅ LOKIDH
- **Bestun a MIDI-umbreytingu**: Samnytttur `SndMidiNoteToFreq()`-hjalpari medh 37-faerslu uppflettitoflu (C3-B5) og attprefuvaravisun fyrir fullt MIDI-svid (0-127)
- **Osasamstillt afspilunarstudhningur**: Fullkominn innvidir tilbakakalla baedhi fyrir skraafspilun (`FilePlayCompletionUPP`) og skippanakeyrslur (`SndCallBackProcPtr`)
- **Rasabunadh hljodbeina**: Fjolthrepakerfidh forgangsstigs medh thogun og virkjunarstjornun
  - 4-stiga forgangsraesir (0-3) fyrir uttak a velbunadi
  - Sjalfstaedh thogun og virkjunarstjornun a hverja ras
  - `SndGetActiveChannel()` skilar haesta-forgangs virkri ras
  - Rett frumstilling rasa medh virkjunarfana adh sjelfgefnu
- **Framleidslugaedha utfaersla**: Allur kodi thydist hreinlega, engin malloc/free-brot fundin
- **Innlegg**: 07542c5 (MIDI-bestun), 1854fe6 (osamstilltir tilbakakallar), a3433c6 (rasabunadh hljodbeina)

#### Afrek fyrri lotu
- ✅ **Framhaldsthrepidh**: Skipanaurvinnslulykja hljodstjora, margkeyrslu-stigaradhgreining, uthvekkjudhr MIDI/tonsmindunareiginleikar
- ✅ **Gluggastaerdarbreytingakerfi**: Gagnvirk staerdarbreyting medh rettum gluggaumgjordhum, staerdharhandstaedhum og skjabordhshreingun
- ✅ **PS/2-lyklabordhsthydhingar**: Fullkominn sett 1 skannkodhi yfir i Toolbox-lyklakodha
- ✅ **Fjolvettvangur HAL**: x86, ARM og PowerPC-studningur medh hreinu utdraetti

## 📊 Naekni verkefnisins

**Heildarnaekni kjarnavirkni**: ~94% lokidh (mat)

### Thadh sem virkar adh fullu ✅

- **Velbunaadhsutdraettarlag (HAL)**: Fullkomidh vettvangsutallag fyrir x86/ARM/PowerPC
- **Raesikerfi**: Raesist med godhum arangri gegnum GRUB2/Multiboot2 a x86
- **Radhtengt skraaning**: Einindabunadhr skraningagrunnur medh keyrslutimasiu (Error/Warn/Info/Debug/Trace)
- **Myndgrunnur**: VESA-rammabidhminni (800x600x32) medh QuickDraw-frumgedum thadh medh talidh XOR-ham
- **Skjabordhsteikni**: System 7 valmyndastika medh regnboga Apple-merki, taknmyndum og skjabordhmynstrum
- **Leturgerdhafraedhi**: Chicago-bitamynda letur medh pixlanakvaemri teiknigerdhingu og rettri jofnun, uthvikkadhur Mac Roman (0x80-0xFF) fyrir evroepsk brodhstafi
- **Althjodavaedhing (i18n)**: Audlindabunadh stadhaerfaersla medh 38 tungumolum (enska, franska, thyska, spanska, italska, portogalska, hollensku, donsku, norsku, soensku, finsku, islensku, grisku, tyrknesku, polsku, tsjekknesku, slovakisku, slovensku, kroatisku, ungversku, romoensku, bulgarsku, alboensku, eistnnesku, lettnnesku, lithaensku, makedonisku, svartfjallsensku, russnnesku, ukrainsku, arabsku, japonsku, einfoldadhri kiversku, hefdhbundinni kiversku, koresku, hindi, bengalsku, urdu), Stadhsetningarstjori medh tungumalasvali vidh raesingu, CJK-fjolbitakothunnarinnvidhir
- **Leturstjori**: Margra staerdha studningur (9-24pt), stilsmidhi, FOND/NFNT-tholgun, LRU-skyndharminni
- **Inntakskerfi**: PS/2-lyklabordh og mus medh fullri atburdhafraesendingu
- **Atburdhastjori**: Samvinnufjolverkadh gegnum WaitNextEvent medh sameinadhri atburdharodh
- **Minnissjori**: Svaedhisbundna utthlutun medh samthattun 68K-tulks
- **Valmyndastjori**: Fullkomin fellivalyndir medh musarrakningu og SaveBits/RestoreBits
- **Skrakerfi**: HFS medh B-tre-utfaerslu, moeppugluggum medh VFS-talningu
- **Gluggastjori**: Drattir, staerdarbreytingar (medh staerdharhandstaedhum), lagskipting, virkjun
- **Timastjori**: Naekvaem TSC-kvardhi, orsekjundunakvaemni, kynslodharathugun
- **Audlindastjori**: O(log n) tviundaleit, LRU-skyndharminni, yfirgripsmikil sannprófun
- **Gestalt-stjori**: Fjolarkitekturs kerfisupplysingar medh arkitektursgreiningu
- **TextEdit-stjori**: Fullkomin textavinnsla medh klippispjaldsamthattun
- **Skraaspjaldsstjori**: Klassiskt Mac OS klippispjaldh medh studhningi vidh margar snidhaafgerdhir
- **SimpleText-forrit**: Fullbuinn MDI-textaritill medh klippa/afrita/lîma
- **Listastjori**: System 7.1-samhaefir listastjornhlutir medh lyklabordhsleidhsogu
- **Stjorentaerastjori**: Stadhaladhr stjorentaeki og skrunstikur medh CDEF-utfaerslu
- **Gluggastjori**: Lyklabordhsleidhsaga, thensluhringir, flyrvileidhir
- **Hlutahlethsli**: Flytjanlegt ISA-ohadh 68K-hlutahletslakerfis medh endurstadh-setningu
- **M68K-tulkur**: Full skipunasendingargerth medh 84 adgerdharkodhamedhlolendum, ollum 14 vistfangshamum, undanthekningar-/gildruramma
- **Hljodstjori**: Skipanaurvinnslulykja, MIDI-umbreyting, rasabunadh, tilbakakallar
- **Taekjastjori**: DCE-stjornun, rekilsuppsetning/-fjaerlaegingu og I/O-adhgerdhir
- **Raesiskjar**: Fullkomidh raesing-nothendavidhmoet medh framvindustoflum, fasastjornun og kynningarskja
- **Litastjori**: Litastadhusstjornun medh QuickDraw-samthattun

### Adh hluta utfaert ⚠️

- **Forritasamthattun**: M68K-tulkur og hlutahletslari tilbuinn; samthattunarprofanir naudhsynlegar til adh stadhfesta adh raunforrit keyrist
- **Gluggaskilgreiningarferlar (WDEF)**: Grunnbygging til stadhar, virknisendingin adh hluta
- **Talstjori**: API-rammi og hljodgegnum-leidhsla einungis; talsmidhunarvel ekki utfaerdhur
- **Undanthekningamedhlondun (RTE)**: Snuidh ur undanthekningum adh hluta utfaert (stodvar sem stendur i stadh thess adh endurheimta samhengi)

### Ekki enn utfaert ❌

- **Prentun**: Ekkert prentkerfi
- **Netkerfi**: Enginn AppleTalk- ne netverksvirknig
- **Skrifbordhsauthildartaeki**: Aeinungis rammi
- **Framhaldsthrepakvaedhi**: Synishornsafspilun, blondun (takmarkun PC-hatarans)

### Undirkerfum okompileradh 🔧

Eftirfarandi eru medh frumkodha en eru ekki samthett inn i kjarnann:
- **AppleEventManager** (8 skrar): Samskipti millum forrita; uthilokadhur viljandi vegna pthread-hagnidha sem samraemast ekki sjelfstaedhu umhverfi
- **FontResources** (haus eingoengu): Skilgreiningar a leturaudlindategundum; raunverulegur leturstudhningur veittir af FontResourceLoader.c

## 🏗️ Arkitektur

### Taeknilegar krofur

- **Arkitektur**: Fjolarkitektur gegnum HAL (x86, ARM, PowerPC tilbuid)
- **Raesibokun**: Multiboot2 (x86), vettvangs-haedh raesihlethslarar
- **Grafik**: VESA-rammaminnisdh, 800x600 vidh 32-bita liti
- **Minnisuppsetninga**: Kjarninn hlethst a 1MB edhlis vidhfangi (x86)
- **Timastilling**: Arkitektur-ohadh medh orsekjundunakvaemni (RDTSC/timatelja)
- **Afkostasemi**: Kalt audlindamiss <15µs, skyndharminnishitt <2µs, timavik <100ppm

### Tolkur frumkodhans

- **225+ frumkodaskrar** medh ~57.500+ koodalonum
- **145+ hausskrar** yfir 28+ undirkerfi
- **69 audlindategundir** unnar ur System 7.1
- **Thydhingatimi**: 3-5 sekundur a nuthima velbunidhi
- **Kjarnastaerdh**: ~4,16 MB
- **ISO-staerdh**: ~12,5 MB

## 🔨 Smidhi

### Krofur

- **GCC** medh 32-bita studningi (`gcc-multilib` a 64-bita kerfum)
- **GNU Make**
- **GRUB-verkfaeri**: `grub-mkrescue` (ur `grub2-common` edha `grub-pc-bin`)
- **QEMU** til pruefana (`qemu-system-i386`)
- **Python 3** fyrir audlindavinnslu
- **xxd** fyrir tviundarumbreytingu
- *(Valfrjalst)* **powerpc-linux-gnu** krosspakki fyrir PowerPC-smidhir

### Uppsetning a Ubuntu/Debian

```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common
```

### Smidhiskipanir

```bash
# Smidha kjarna (x86 adh sjelfgefnu)
make

# Smidha fyrir adkhilinn vettvang
make PLATFORM=x86
make PLATFORM=arm        # requires ARM bare-metal GCC
make PLATFORM=ppc        # experimental; requires PowerPC ELF toolchain

# Budha til raesanlega ISO-mynd
make iso

# Smidha medh ollum tungumolum
make LOCALE_FR=1 LOCALE_DE=1 LOCALE_ES=1 LOCALE_JA=1 LOCALE_ZH=1 LOCALE_KO=1 LOCALE_RU=1 LOCALE_UK=1 LOCALE_PL=1 LOCALE_CS=1 LOCALE_SQ=1 LOCALE_BG=1 LOCALE_HR=1 LOCALE_DA=1 LOCALE_NL=1 LOCALE_ET=1 LOCALE_FI=1 LOCALE_EL=1 LOCALE_HU=1 LOCALE_IS=1 LOCALE_IT=1 LOCALE_LV=1 LOCALE_LT=1 LOCALE_MK=1 LOCALE_ME=1 LOCALE_NO=1 LOCALE_PT=1 LOCALE_RO=1 LOCALE_SK=1 LOCALE_SL=1 LOCALE_SV=1 LOCALE_TR=1 LOCALE_HI=1 LOCALE_TW=1 LOCALE_AR=1 LOCALE_BN=1 LOCALE_UR=1

# Smidha medh einu vidheykandi tungumalinu
make LOCALE_FR=1

# Smidha og keyra i QEMU
make run

# Hreinsadh smidhaafurdhir
make clean

# Birta smidhatolfraedhi
make info
```

## 🚀 Keyrsla

### Fljotraesing (QEMU)

```bash
# Stadhlaedh keyrsla medh radhtenngdri skraningu
make run

# Handvirkt medh valkostum
qemu-system-i386 -cdrom system71.iso -serial file:/tmp/serial.log -display sdl -vga std -m 256M
```

### QEMU-valkostir

```bash
# Medh radhtengdu skilabordhautstaki
qemu-system-i386 -cdrom system71.iso -serial stdio -display sdl -m 256M

# Hauslaus (engin grafisk skjamynd)
qemu-system-i386 -cdrom system71.iso -serial stdio -display none -m 256M

# Medh GDB-villuleit
make debug
# I odhru skelinni: gdb kernel.elf -ex "target remote :1234"
```

## 📚 Skjolfestur

### Leidhbeiningar um hluta

- **Stjorentaerastjori**: `docs/components/ControlManager/`
- **Gluggastjori**: `docs/components/DialogManager/`
- **Leturstjori**: `docs/components/FontManager/`
- **Radhtenngd skraning**: `docs/components/System/`
- **Atburdhastjori**: `docs/components/EventManager.md`
- **Valmyndastjori**: `docs/components/MenuManager.md`
- **Gluggastjori**: `docs/components/WindowManager.md`
- **Audlindastjori**: `docs/components/ResourceManager.md`

### Althjodavaedhing

- **Stadhsetningarstjori**: `include/LocaleManager/` — keyrslutimastadhsetning, tungumalaval vidh raesingu
- **Strengjaaudlindir**: `resources/strings/` — STR#-audlindaskrar fyrir hvert tungumol (34 tungumol)
- **Uthvikkuth letur**: `include/chicago_font_extended.h` — Mac Roman 0x80-0xFF teiknmyndir fyrir evropskan brodhstafi
- **CJK-studningur**: `include/TextEncoding/CJKEncoding.h`, `include/FontManager/CJKFont.h` — fjolbitakothun og leturinnvithir

### Stada utfaerslu

- **IMPLEMENTATION_PRIORITIES.md**: Skipulagdhar verkaaetlanir og naeknimaeling
- **IMPLEMENTATION_STATUS_AUDIT.md**: Itharlegt endurskodhun allra undirkerfa

### Heimspeki verkefnisins

**Fornleifafraedhinaelgun** medh soennunarbundinni utfaerslu:
1. Stutt af Inside Macintosh-skjolfestun og MPW Universal Interfaces
2. Allar storar akvardharnir merktar medh Finningu-audkennum sem visa til gagnlegrar soknarheimilda
3. Markmidh: hegdhunarsamsvaering vidh upprunalega System 7, ekki nuthimavaeding
4. Hreinherbergis-utfaersla (enginn upprunalegur Apple-frumkodhi notadhur)

## 🐛 Thekkt vandamal

1. **Taknmynddragsgallar**: Minni hattar synilegar villur vidh dratt taeknmynda a skjabordhinu
2. **M68K-keyrsla stubbaedh**: Hlutahletslari fullnaegjanlega utfaerdhur, keyrslulykja ekki utfaerdh
3. **Enginn TrueType-studningur**: Bitamyndaletur einungis (Chicago)
4. **HFS lesadh-eingoengu**: Sjaelflegt skrakerfi, engin raunveruleg diskaskrifun
5. **Engar stadhugleikaafyrirheitur**: Hrun og ovaent hegdhun eru algengar

## 🤝 Framlag

Thetta er adallega nam-/rannsoknarverkefni:

1. **Villutilkynningar**: Sendu inn mohl medh itharlega afraeslunarskrefum
2. **Profanir**: Tilkinna nidurstodur a olikum velbunadi/hermum
3. **Skjolfesting**: Baettu nuverandi skjolfestur edha baettu vidh nyjum leidhbeiningum

## 📖 Naudsynleg tilvisan

- **Inside Macintosh** (1992-1994): Opinber Apple Toolbox-skjolfesting
- **MPW Universal Interfaces 3.2**: Visadhar hausskrar og struct-skilgreiningar
- **Guide to Macintosh Family Hardware**: Velbunaadhsarkitekturvisan

### Hjolp verkfaeri

- **Mini vMac**: System 7 hermir sem hegdhunarvisan
- **ResEdit**: Audlindabreytir til adh rannsoeka System 7-audlindir
- **Ghidra/IDA**: Til ROM-sundurhlutunargreninga

## ⚖️ Lagaleg atridhir

Thetta er **hreinherbergis endursmidun** i fraedslulegu skyni og til vardhveislu:

- **Enginn Apple-frumkodhi** var notadhur
- Byggt a opinberri skjolfestun og svorthymagreiningu einvordungu
- "System 7", "Macintosh", "QuickDraw" eru vorumerki Apple Inc.
- Ekki tengt, samthykkt af, edha styrkt af Apple Inc.

**Upprunalegt System 7 ROM og hugbunadhur er eign Apple Inc.**

## 🙏 Thakkir

- **Apple Computer, Inc.** fyrir adh skapa upprunalega System 7
- **Hofundar Inside Macintosh** fyrir itharlega skjolfestun
- **Klassiska Mac-vardhveislusamfelagidh** fyrir adh halda vettvanginum lifandi
- **68k.news og Macintosh Garden** fyrir audlindarsafnin

## 📊 Throunartoelfraedhi

- **Koodalonum**: ~57.500+ (thadh a medh talidh 2.500+ fyrir hlutahletslara)
- **Thydhingatimi**: ~3-5 sekundur
- **Kjarnastaerdh**: ~4,16 MB (kernel.elf)
- **ISO-staerdh**: ~12,5 MB (system71.iso)
- **Villuminnkun**: 94% kjarnavirknigetu virk
- **Megindundirkerfi**: 28+ (Font, Window, Menu, Control, Dialog, TextEdit, o.fl.)

## 🔮 Framtidharsyn

**Fyrirhugudh verk**:

- Ljuka M68K-tulkskeyrslulykju
- Baeta vidh TrueType-leturstudningi
- CJK-bitamyndaletursaudlindir fyrir japanskt, kverskt og koreskt leturgerdhateikni
- Utfaera vidhboetarstjornhluti (textasvaedhir, sprettivalmyndir, rennitaeki)
- Diskaskrifun-til-baka fyrir HFS-skrakerfi
- Framhaldsthrepakvaedhi hljodstjora (blondun, synishornstaka)
- Einfoldh skrifbordhsauthildartaeki (Reiknivel, Minnisbladh)

---

**Stadha**: Tilraunakennt - Fraedslulegt - I throun

**Sidhast uppfaert**: November 2025 (Endurbaeturr a hljodstjora lokidh)

Fyrir spurningar, vandamal edha umraedhu, vinsamlegast notidh GitHub Issues.
