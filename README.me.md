# System 7 - Portabilna reimplementacija otvorenog koda

**[English](README.md)** | **[Fran&ccedil;ais](README.fr.md)** | **[Deutsch](README.de.md)** | **[Espa&ntilde;ol](README.es.md)** | **[Italiano](README.it.md)** | **[Portugu&ecirc;s](README.pt.md)** | **[Nederlands](README.nl.md)** | **[Dansk](README.da.md)** | **[Norsk](README.no.md)** | **[Svenska](README.sv.md)** | **[Suomi](README.fi.md)** | **[&Iacute;slenska](README.is.md)** | **[Ελληνικά](README.el.md)** | **[T&uuml;rk&ccedil;e](README.tr.md)** | **[Polski](README.pl.md)** | **[Čeština](README.cs.md)** | **[Slovenčina](README.sk.md)** | **[Slovenščina](README.sl.md)** | **[Hrvatski](README.hr.md)** | **[Magyar](README.hu.md)** | **[Rom&acirc;n&atilde;](README.ro.md)** | **[Български](README.bg.md)** | **[Shqip](README.sq.md)** | **[Eesti](README.et.md)** | **[Latviešu](README.lv.md)** | **[Lietuvių](README.lt.md)** | **[Македонски](README.mk.md)** | **[Crnogorski](README.me.md)** | **[Русский](README.ru.md)** | **[Українська](README.uk.md)** | **[العربية](README.ar.md)** | **[日本語](README.ja.md)** | **[简体中文](README.zh.md)** | **[繁體中文](README.tw.md)** | **[한국어](README.ko.md)** | **[हिन्दी](README.hi.md)** | **[বাংলা](README.bn.md)** | **[اردو](README.ur.md)**

<img width="793" height="657" alt="System 7 pokrenut na modernom hardveru" src="https://github.com/user-attachments/assets/be84b83e-191c-4f9d-a786-11d0bd04203b" />
<img width="801" height="662" alt="simpletextworks" src="https://github.com/user-attachments/assets/7c9ebe5b-22b4-4612-93a1-2076909d77cd" />
<img width="803" height="661" alt="macpaint" src="https://github.com/user-attachments/assets/cd3ed04a-fdde-4dd5-88ef-5b19b3a13a54" />

> ⚠️ **DOKAZ KONCEPTA** - Ovo je eksperimentalna, edukativna reimplementacija Apple-ovog Macintosh System 7. Ovo NIJE gotov proizvod i ne treba ga smatrati softverom spremnim za produkciju.

Reimplementacija otvorenog koda Apple Macintosh System 7 za moderni x86 hardver, sa mogućnošću pokretanja preko GRUB2/Multiboot2. Ovaj projekat ima za cilj da ponovo stvori klasičan Mac OS doživljaj, dokumentujući arhitekturu System 7 kroz analizu obrnutog inženjeringa.

## 🎯 Status projekta

**Trenutno stanje**: Aktivni razvoj sa ~94% završene osnovne funkcionalnosti

### Poslednja ažuriranja (novembar 2025)

#### Unapređenja Sound Manager-a ✅ ZAVRŠENO
- **Optimizovana MIDI konverzija**: Zajednički `SndMidiNoteToFreq()` pomoćnik sa tabelom od 37 unosa (C3-B5) i rezervnim algoritmom zasnovanim na oktavama za puni MIDI opseg (0-127)
- **Podrška za asinhrono reprodukovanje**: Kompletna infrastruktura za povratne pozive kako za reprodukciju fajlova (`FilePlayCompletionUPP`) tako i za izvršavanje komandi (`SndCallBackProcPtr`)
- **Usmjeravanje zvuka bazirano na kanalima**: Višeslojni sistem prioriteta sa kontrolama za isključivanje i omogućavanje zvuka
  - Kanali sa 4 nivoa prioriteta (0-3) za usmjeravanje hardverskog izlaza
  - Nezavisne kontrole za isključivanje i omogućavanje po kanalu
  - `SndGetActiveChannel()` vraća aktivni kanal najvišeg prioriteta
  - Pravilna inicijalizacija kanala sa podrazumijevano uključenom zastavicom
- **Implementacija produkcijskog kvaliteta**: Sav kôd se kompajlira bez upozorenja, nijesu detektovana kršenja malloc/free
- **Komitovi**: 07542c5 (MIDI optimizacija), 1854fe6 (asinhroni povratni pozivi), a3433c6 (usmjeravanje kanala)

#### Prethodna postignuća
- ✅ **Faza naprednih funkcija**: Petlja za obradu komandi Sound Manager-a, serijalizacija stilova za višestruko pokretanje, proširene MIDI/sinteza funkcije
- ✅ **Sistem za promjenu veličine prozora**: Interaktivna promjena veličine sa pravilnim upravljanjem okvirom, kutijom za rastezanje i čišćenjem radne površine
- ✅ **PS/2 translacija tastature**: Potpuno mapiranje Set 1 skenkodova na Toolbox kodove tastera
- ✅ **Višeplatformski HAL**: Podrška za x86, ARM i PowerPC sa čistom apstrakcijom

## 📊 Kompletnost projekta

**Ukupna osnovna funkcionalnost**: ~94% završeno (procjena)

### Šta radi u potpunosti ✅

- **Sloj za apstrakciju hardvera (HAL)**: Kompletna platformska apstrakcija za x86/ARM/PowerPC
- **Sistem za pokretanje**: Uspješno se pokreće preko GRUB2/Multiboot2 na x86
- **Serijsko zapisivanje**: Zapisivanje zasnovano na modulima sa filtriranjem tokom izvršavanja (Error/Warn/Info/Debug/Trace)
- **Grafička osnova**: VESA framebuffer (800x600x32) sa QuickDraw primitivima uključujući XOR režim
- **Renderovanje radne površine**: System 7 traka menija sa duginim Apple logom, ikonama i obrascima radne površine
- **Tipografija**: Chicago bitmap font sa pikselski preciznim renderovanjem i pravilnim kerningom, prošireni Mac Roman (0x80-0xFF) za evropske znakove sa akcentima
- **Internacionalizacija (i18n)**: Lokalizacija zasnovana na resursima sa 38 jezika (engleski, francuski, njemački, španski, italijanski, portugalski, holandski, danski, norveški, švedski, finski, islandski, grčki, turski, poljski, češki, slovački, slovenački, hrvatski, mađarski, rumunski, bugarski, albanski, estonski, letonski, litvanski, makedonski, crnogorski, ruski, ukrajinski, arapski, japanski, pojednostavljeni kineski, tradicionalni kineski, korejski, hindi, bengalski, urdu), Locale Manager sa izborom jezika pri pokretanju, CJK infrastruktura za višebajtno enkodiranje
- **Font Manager**: Podrška za više veličina (9-24pt), sinteza stilova, FOND/NFNT parsiranje, LRU keširanje
- **Sistem unosa**: PS/2 tastatura i miš sa kompletnim prosljeđivanjem događaja
- **Event Manager**: Kooperativni multitasking putem WaitNextEvent sa objedinjenim redom događaja
- **Memory Manager**: Alokacija zasnovana na zonama sa integracijom 68K interpretera
- **Menu Manager**: Kompletni padajući meniji sa praćenjem miša i SaveBits/RestoreBits
- **Fajl sistem**: HFS sa implementacijom B-stabla, prozori fascikli sa VFS enumeracijom
- **Window Manager**: Prevlačenje, promjena veličine (sa kutijom za rastezanje), slojevitost, aktivacija
- **Time Manager**: Precizna TSC kalibracija, mikrosekunda preciznost, provjera generacije
- **Resource Manager**: O(log n) binarna pretraga, LRU keš, sveobuhvatna validacija
- **Gestalt Manager**: Sistemske informacije za više arhitektura sa detekcijom arhitekture
- **TextEdit Manager**: Kompletno uređivanje teksta sa integracijom klipborda
- **Scrap Manager**: Klasični Mac OS klipbord sa podrškom za više formata
- **Aplikacija SimpleText**: Potpuno funkcionalan MDI uređivač teksta sa funkcijama isjecanja/kopiranja/lijepljenja
- **List Manager**: Kontrole lista kompatibilne sa System 7.1 sa navigacijom tastaturom
- **Control Manager**: Standardne kontrole i klizači sa CDEF implementacijom
- **Dialog Manager**: Navigacija tastaturom, prstenovi fokusa, prečice na tastaturi
- **Segment Loader**: Portabilni sistem za učitavanje 68K segmenata nezavisan od ISA arhitekture sa relokacijom
- **M68K Interpreter**: Potpuno otpremanje instrukcija sa 84 hendlera za opkodove, svih 14 načina adresiranja, okvir za izuzetke/zamke
- **Sound Manager**: Obrada komandi, MIDI konverzija, upravljanje kanalima, povratni pozivi
- **Device Manager**: Upravljanje DCE-ovima, instalacija/uklanjanje drajvera i U/I operacije
- **Startni ekran**: Kompletno korisničko sučelje pri pokretanju sa praćenjem napretka, upravljanjem fazama i splješ ekranom
- **Color Manager**: Upravljanje stanjem boja sa integracijom QuickDraw-a

### Djelimično implementirano ⚠️

- **Integracija aplikacija**: M68K interpreter i segment loader završeni; potrebno je integracijsko testiranje da bi se potvrdilo da se prave aplikacije izvršavaju
- **Procedure za definiciju prozora (WDEF)**: Osnovna struktura postoji, djelimično otpremanje
- **Speech Manager**: Samo API okvir i audio propuštanje; mašina za sintezu govora nije implementirana
- **Upravljanje izuzecima (RTE)**: Povratak iz izuzetka djelimično implementiran (trenutno zaustavlja umjesto da vraća kontekst)

### Još nije implementirano ❌

- **Štampanje**: Nema sistema za štampanje
- **Umrežavanje**: Nema AppleTalk-a ili mrežne funkcionalnosti
- **Desk dodaci**: Samo okvir
- **Napredni zvuk**: Reprodukcija semplova, miksanje (ograničenje PC zvučnika)

### Podsistemi koji nijesu kompajlirani 🔧

Sljedeći imaju izvorni kôd ali nijesu integrisani u kernel:
- **AppleEventManager** (8 fajlova): Razmjena poruka između aplikacija; namjerno isključen zbog pthread zavisnosti nekompatibilnih sa samostalnim okruženjem
- **FontResources** (samo zaglavlje): Definicije tipova resurs fontova; stvarna podrška za fontove obezbijeđena je kompajliranim FontResourceLoader.c

## 🏗️ Arhitektura

### Tehničke specifikacije

- **Arhitektura**: Višearhitekturalna putem HAL-a (x86, ARM, PowerPC spremni)
- **Protokol pokretanja**: Multiboot2 (x86), pokretači specifični za platformu
- **Grafika**: VESA framebuffer, 800x600 @ 32-bitna boja
- **Raspored memorije**: Kernel se učitava na 1MB fizičke adrese (x86)
- **Tajming**: Nezavisan od arhitekture sa mikrosekunda preciznošću (RDTSC/registri tajmera)
- **Performanse**: Promašaj hladnog resursa <15µs, pogodak keša <2µs, odstupanje tajmera <100ppm

### Statistika baze koda

- **225+ izvornih fajlova** sa ~57.500+ linija koda
- **145+ zaglavlja** kroz 28+ podsistema
- **69 tipova resursa** izdvojenih iz System 7.1
- **Vrijeme kompilacije**: 3-5 sekundi na modernom hardveru
- **Veličina kernela**: ~4,16 MB
- **Veličina ISO-a**: ~12,5 MB

## 🔨 Kompajliranje

### Zahtjevi

- **GCC** sa podrškom za 32 bita (`gcc-multilib` na 64-bitnim sistemima)
- **GNU Make**
- **GRUB alati**: `grub-mkrescue` (iz `grub2-common` ili `grub-pc-bin`)
- **QEMU** za testiranje (`qemu-system-i386`)
- **Python 3** za obradu resursa
- **xxd** za binarnu konverziju
- *(Opcionalno)* **powerpc-linux-gnu** skup alata za unakrsno kompajliranje za PowerPC

### Ubuntu/Debian instalacija

```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common
```

### Komande za kompajliranje

```bash
# Kompajliranje kernela (x86 podrazumijevano)
make

# Kompajliranje za specifičnu platformu
make PLATFORM=x86
make PLATFORM=arm        # zahtijeva ARM bare-metal GCC
make PLATFORM=ppc        # eksperimentalno; zahtijeva PowerPC ELF alate

# Kreiranje ISO-a za pokretanje
make iso

# Kompajliranje sa svim jezicima
make LOCALE_FR=1 LOCALE_DE=1 LOCALE_ES=1 LOCALE_JA=1 LOCALE_ZH=1 LOCALE_KO=1 LOCALE_RU=1 LOCALE_UK=1 LOCALE_PL=1 LOCALE_CS=1 LOCALE_SQ=1 LOCALE_BG=1 LOCALE_HR=1 LOCALE_DA=1 LOCALE_NL=1 LOCALE_ET=1 LOCALE_FI=1 LOCALE_EL=1 LOCALE_HU=1 LOCALE_IS=1 LOCALE_IT=1 LOCALE_LV=1 LOCALE_LT=1 LOCALE_MK=1 LOCALE_ME=1 LOCALE_NO=1 LOCALE_PT=1 LOCALE_RO=1 LOCALE_SK=1 LOCALE_SL=1 LOCALE_SV=1 LOCALE_TR=1 LOCALE_HI=1 LOCALE_TW=1 LOCALE_AR=1 LOCALE_BN=1 LOCALE_UR=1

# Kompajliranje sa jednim dodatnim jezikom
make LOCALE_FR=1

# Kompajliranje i pokretanje u QEMU
make run

# Brisanje artefakata
make clean

# Prikaz statistike kompajliranja
make info
```

## 🚀 Pokretanje

### Brzi početak (QEMU)

```bash
# Standardno pokretanje sa serijskim zapisivanjem
make run

# Ručno sa opcijama
qemu-system-i386 -cdrom system71.iso -serial file:/tmp/serial.log -display sdl -vga std -m 256M
```

### QEMU opcije

```bash
# Sa serijskim izlazom na konzolu
qemu-system-i386 -cdrom system71.iso -serial stdio -display sdl -m 256M

# Bez grafičkog prikaza
qemu-system-i386 -cdrom system71.iso -serial stdio -display none -m 256M

# Sa GDB debugovanjem
make debug
# U drugom terminalu: gdb kernel.elf -ex "target remote :1234"
```

## 📚 Dokumentacija

### Vodiči za komponente
- **Control Manager**: `docs/components/ControlManager/`
- **Dialog Manager**: `docs/components/DialogManager/`
- **Font Manager**: `docs/components/FontManager/`
- **Serijsko zapisivanje**: `docs/components/System/`
- **Event Manager**: `docs/components/EventManager.md`
- **Menu Manager**: `docs/components/MenuManager.md`
- **Window Manager**: `docs/components/WindowManager.md`
- **Resource Manager**: `docs/components/ResourceManager.md`

### Internacionalizacija
- **Locale Manager**: `include/LocaleManager/` — prebacivanje lokalizacije tokom izvršavanja, izbor jezika pri pokretanju
- **Resurs stringova**: `resources/strings/` — STR# resurs fajlovi za svaki jezik (34 jezika)
- **Prošireni fontovi**: `include/chicago_font_extended.h` — Mac Roman 0x80-0xFF glifovi za evropske znakove
- **CJK podrška**: `include/TextEncoding/CJKEncoding.h`, `include/FontManager/CJKFont.h` — infrastruktura za višebajtno enkodiranje i fontove

### Status implementacije
- **IMPLEMENTATION_PRIORITIES.md**: Planirani rad i praćenje kompletnosti
- **IMPLEMENTATION_STATUS_AUDIT.md**: Detaljna revizija svih podsistema

### Filozofija projekta

**Arheološki pristup** sa implementacijom zasnovanom na dokazima:
1. Podržan dokumentacijom Inside Macintosh i MPW Universal Interfaces
2. Sve važne odluke označene sa Finding ID-jevima koji upućuju na prateće dokaze
3. Cilj: ponašanje identično originalnom System 7, ne modernizacija
4. Implementacija u čistoj sobi (bez originalnog Apple izvornog koda)

## 🐛 Poznati problemi

1. **Artefakti pri prevlačenju ikona**: Manji vizuelni artefakti tokom prevlačenja ikona na radnoj površini
2. **M68K izvršavanje je zamijenjeno stubovima**: Segment loader završen, petlja za izvršavanje nije implementirana
3. **Nema podrške za TrueType**: Samo bitmap fontovi (Chicago)
4. **HFS samo za čitanje**: Virtuelni fajl sistem, nema stvarnog zapisivanja na disk
5. **Nema garancija stabilnosti**: Padovi i neočekivano ponašanje su česti

## 🤝 Doprinos

Ovo je prvenstveno projekat za učenje/istraživanje:

1. **Prijave grešaka**: Podnesite prijave sa detaljnim koracima za reprodukciju
2. **Testiranje**: Prijavite rezultate na različitom hardveru/emulatorima
3. **Dokumentacija**: Poboljšajte postojeću dokumentaciju ili dodajte nove vodiče

## 📖 Osnovne reference

- **Inside Macintosh** (1992-1994): Zvanična Apple Toolbox dokumentacija
- **MPW Universal Interfaces 3.2**: Kanonski fajlovi zaglavlja i definicije struktura
- **Guide to Macintosh Family Hardware**: Referenca za hardversku arhitekturu

### Korisni alati

- **Mini vMac**: Emulator System 7 za provjeru ponašanja
- **ResEdit**: Uređivač resursa za proučavanje System 7 resursa
- **Ghidra/IDA**: Za analizu disasembliranja ROM-a

## ⚖️ Pravne napomene

Ovo je **reimplementacija u čistoj sobi** u edukativne svrhe i svrhe očuvanja:

- **Nije korišćen Apple izvorni kôd**
- Zasnovano isključivo na javnoj dokumentaciji i analizi crne kutije
- "System 7", "Macintosh", "QuickDraw" su zaštitni znakovi Apple Inc.
- Nije povezano sa, podržano od strane ili sponzorisano od strane Apple Inc.

**Originalni System 7 ROM i softver ostaju vlasništvo Apple Inc.**

## 🙏 Zahvalnice

- **Apple Computer, Inc.** za kreiranje originalnog System 7
- **Autori Inside Macintosh** za sveobuhvatnu dokumentaciju
- **Zajednica za očuvanje klasičnog Mac-a** za održavanje platforme živom
- **68k.news i Macintosh Garden** za arhive resursa

## 📊 Statistika razvoja

- **Linije koda**: ~57.500+ (uključujući 2.500+ za segment loader)
- **Vrijeme kompilacije**: ~3-5 sekundi
- **Veličina kernela**: ~4,16 MB (kernel.elf)
- **Veličina ISO-a**: ~12,5 MB (system71.iso)
- **Smanjenje grešaka**: 94% osnovne funkcionalnosti radi
- **Glavni podsistemi**: 28+ (Font, Window, Menu, Control, Dialog, TextEdit, itd.)

## 🔮 Budući pravac

**Planirani rad**:

- Završetak petlje za izvršavanje M68K interpretera
- Dodavanje podrške za TrueType fontove
- CJK bitmap resurs fontova za japansko, kinesko i korejsko renderovanje
- Implementacija dodatnih kontrola (tekstualna polja, iskačući meniji, klizači)
- Zapisivanje na disk za HFS fajl sistem
- Napredne funkcije Sound Manager-a (miksanje, semplovanje)
- Osnovni desk dodaci (Kalkulator, Blok za bilješke)

---

**Status**: Eksperimentalno - Edukativno - U razvoju

**Poslednje ažuriranje**: Novembar 2025 (Unapređenja Sound Manager-a završena)

Za pitanja, probleme ili diskusiju, koristite GitHub Issues.
