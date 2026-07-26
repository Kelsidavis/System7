# System 7 - Prijenosna reimplementacija otvorenog koda

**[English](../../README.md)** | **[Fran&ccedil;ais](README.fr.md)** | **[Deutsch](README.de.md)** | **[Espa&ntilde;ol](README.es.md)** | **[Italiano](README.it.md)** | **[Portugu&ecirc;s](README.pt.md)** | **[Nederlands](README.nl.md)** | **[Dansk](README.da.md)** | **[Norsk](README.no.md)** | **[Svenska](README.sv.md)** | **[Suomi](README.fi.md)** | **[&Iacute;slenska](README.is.md)** | **[Ελληνικά](README.el.md)** | **[T&uuml;rk&ccedil;e](README.tr.md)** | **[Polski](README.pl.md)** | **[Čeština](README.cs.md)** | **[Slovenčina](README.sk.md)** | **[Slovenščina](README.sl.md)** | **[Hrvatski](README.hr.md)** | **[Magyar](README.hu.md)** | **[Rom&acirc;n&atilde;](README.ro.md)** | **[Български](README.bg.md)** | **[Shqip](README.sq.md)** | **[Eesti](README.et.md)** | **[Latviešu](README.lv.md)** | **[Lietuvių](README.lt.md)** | **[Македонски](README.mk.md)** | **[Crnogorski](README.me.md)** | **[Русский](README.ru.md)** | **[Українська](README.uk.md)** | **[日本語](README.ja.md)** | **[中文](README.zh.md)** | **[한국어](README.ko.md)** | **[हिन्दी](README.hi.md)**

<img width="793" height="657" alt="System 7 pokrenut na modernom hardveru" src="https://github.com/user-attachments/assets/be84b83e-191c-4f9d-a786-11d0bd04203b" />
<img width="801" height="662" alt="simpletextworks" src="https://github.com/user-attachments/assets/7c9ebe5b-22b4-4612-93a1-2076909d77cd" />
<img width="803" height="661" alt="macpaint" src="https://github.com/user-attachments/assets/cd3ed04a-fdde-4dd5-88ef-5b19b3a13a54" />

> ⚠️ **DOKAZ KONCEPTA** - Ovo je eksperimentalna, edukativna reimplementacija Appleovog Macintosh System 7. Ovo NIJE gotov proizvod i ne bi se trebalo smatrati softverom spremnim za produkciju.

Reimplementacija otvorenog koda Appleovog Macintosh System 7 za moderan x86 hardver, s mogucnoscu pokretanja putem GRUB2/Multiboot2. Ovaj projekt ima za cilj ponovno stvoriti klasicni Mac OS doživljaj uz dokumentiranje arhitekture System 7 kroz analizu obrnutog inženjeringa.

## 🎯 Status projekta

**Trenutno stanje**: Aktivni razvoj s ~94% dovršene osnovne funkcionalnosti

### Najnovija ažuriranja (studeni 2025.)

#### Poboljšanja Sound Managera ✅ DOVRŠENO
- **Optimizirana MIDI konverzija**: Dijeljeni `SndMidiNoteToFreq()` pomocnik s tablicom od 37 unosa (C3-B5) i zamjenskim izracunom temeljenim na oktavama za puni MIDI raspon (0-127)
- **Podrška za asinkrono reproduciranje**: Potpuna infrastruktura povratnih poziva za reprodukciju datoteka (`FilePlayCompletionUPP`) i izvršavanje naredbi (`SndCallBackProcPtr`)
- **Usmjeravanje zvuka temeljeno na kanalima**: Sustav prioriteta s više razina s kontrolama za utišavanje i omogucavanje
  - Kanali s 4 razine prioriteta (0-3) za usmjeravanje hardverskog izlaza
  - Neovisne kontrole za utišavanje i omogucavanje po kanalu
  - `SndGetActiveChannel()` vraca kanal s najvišim prioritetom
  - Ispravna inicijalizacija kanala sa zastavicom za omogucavanje prema zadanim postavkama
- **Implementacija proizvodne kvalitete**: Sav kod se kompajlira cisto, bez otkrivenih povreda malloc/free
- **Commitovi**: 07542c5 (MIDI optimizacija), 1854fe6 (asinkroni povratni pozivi), a3433c6 (usmjeravanje kanala)

#### Prethodni postignuti rezultati
- ✅ **Faza naprednih znacajki**: Petlja obrade naredbi Sound Managera, serijalizacija stila s višestrukim pokretanjem, proširene MIDI/sinteza znacajke
- ✅ **Sustav promjene velicine prozora**: Interaktivna promjena velicine s ispravnim rukovanjem okvirom, kutijom za povecanje i cišcenjem radne površine
- ✅ **PS/2 prijevod tipkovnice**: Potpuno mapiranje skeniranih kodova skupa 1 na kodove tipki Toolboxa
- ✅ **Višeplatformski HAL**: Podrška za x86, ARM i PowerPC s cistom apstrakcijom

## 📊 Dovršenost projekta

**Ukupna osnovna funkcionalnost**: ~94% dovršeno (procjena)

### Što u potpunosti radi ✅

- **Sloj apstrakcije hardvera (HAL)**: Potpuna apstrakcija platforme za x86/ARM/PowerPC
- **Sustav pokretanja**: Uspješno se pokrece putem GRUB2/Multiboot2 na x86
- **Serijsko zapisivanje**: Zapisivanje temeljeno na modulima s filtriranjem za vrijeme izvršavanja (Error/Warn/Info/Debug/Trace)
- **Graficka osnova**: VESA medumemorija okvira (800x600x32) s QuickDraw primitivima ukljucujuci XOR nacin rada
- **Renderiranje radne površine**: Traka izbornika System 7 s duginim Apple logotipom, ikonama i uzorcima radne površine
- **Tipografija**: Chicago bitmap font s piksel-savršenim renderiranjem i ispravnim razmicanjem znakova, prošireni Mac Roman (0x80-0xFF) za europske znakove s naglascima
- **Internacionalizacija (i18n)**: Lokalizacija temeljena na resursima s 34 jezika (engleski, francuski, njemacki, španjolski, talijanski, portugalski, nizozemski, danski, norveški, švedski, finski, islandski, grcki, turski, poljski, ceški, slovacki, slovenski, hrvatski, madarski, rumunjski, bugarski, albanski, estonski, latvijski, litavski, makedonski, crnogorski, ruski, ukrajinski, japanski, kineski, korejski, hindi), Locale Manager s odabirom jezika pri pokretanju, CJK infrastruktura za višebajtno kodiranje
- **Font Manager**: Podrška za više velicina (9-24pt), sinteza stilova, FOND/NFNT parsiranje, LRU predmemorija
- **Sustav unosa**: PS/2 tipkovnica i miš s potpunim prosljeðivanjem dogaðaja
- **Event Manager**: Kooperativni multitasking putem WaitNextEvent s objedinjenim redom dogaðaja
- **Memory Manager**: Alokacija temeljena na zonama s integracijom 68K interpretera
- **Menu Manager**: Potpuni padajuci izbornici s pracenjem miša i SaveBits/RestoreBits
- **Datotecni sustav**: HFS s implementacijom B-stabla, prozori mapa s VFS enumeracijom
- **Window Manager**: Povlacenje, promjena velicine (s kutijom za povecanje), slojevitost, aktivacija
- **Time Manager**: Tocna TSC kalibracija, preciznost u mikrosekundama, provjera generacije
- **Resource Manager**: O(log n) binarno pretraživanje, LRU predmemorija, sveobuhvatna validacija
- **Gestalt Manager**: Informacije o sustavu s više arhitektura s detekcijom arhitekture
- **TextEdit Manager**: Potpuno ureðivanje teksta s integracijom meðuspremnika
- **Scrap Manager**: Klasicni Mac OS meðuspremnik s podrškom za više formata
- **Aplikacija SimpleText**: Potpuno opremljen MDI ureðivac teksta s izreži/kopiraj/zalijepi
- **List Manager**: Kontrole popisa kompatibilne sa System 7.1 s navigacijom putem tipkovnice
- **Control Manager**: Standardne kontrole i klizaci s CDEF implementacijom
- **Dialog Manager**: Navigacija tipkovnicom, prsteni fokusa, tipkovnicki precaci
- **Segment Loader**: Prijenosni ISA-agnosticni sustav ucitavanja 68K segmenata s relokacijom
- **M68K Interpreter**: Potpuna otprema instrukcija s 84 rukovatelja opkodovima, svih 14 nacinâ adresiranja, okvir za iznimke/zamke
- **Sound Manager**: Obrada naredbi, MIDI konverzija, upravljanje kanalima, povratni pozivi
- **Device Manager**: Upravljanje DCE-ovima, instalacija/uklanjanje upravljackih programa i I/O operacije
- **Zaslon pokretanja**: Potpuno korisnicko sucelje pokretanja s pracenjem napretka, upravljanjem fazama i uvodnim zaslonom
- **Color Manager**: Upravljanje stanjem boja s integracijom QuickDrawa

### Djelomicno implementirano ⚠️

- **Integracija aplikacija**: M68K interpreter i ucitavac segmenata dovršeni; potrebno integracijsko testiranje za provjeru izvršavanja stvarnih aplikacija
- **Procedure definicije prozora (WDEF)**: Osnovna struktura na mjestu, djelomicna otprema
- **Speech Manager**: Samo okvir API-ja i propuštanje zvuka; mehanizam sinteze govora nije implementiran
- **Obrada iznimki (RTE)**: Povratak iz iznimke djelomicno implementiran (trenutno zaustavlja umjesto obnavljanja konteksta)

### Još nije implementirano ❌

- **Ispis**: Nema sustava ispisa
- **Umrežavanje**: Nema AppleTalk ili mrežne funkcionalnosti
- **Pribor za radnu površinu**: Samo okvir
- **Napredni zvuk**: Reprodukcija uzoraka, miksanje (ogranicenje PC zvucnika)

### Podsustavi koji se ne kompajliraju 🔧

Sljedeci imaju izvorni kod, ali nisu integrirani u jezgru:
- **AppleEventManager** (8 datoteka): Meðuaplikacijsko porucivanje; namjerno iskljuceno zbog ovisnosti o pthread-u nekompatibilnih sa samostojucem okruženjem
- **FontResources** (samo zaglavlje): Definicije tipova resursa fontova; stvarna podrška za fontove pruža kompajlirani FontResourceLoader.c

## 🏗️ Arhitektura

### Tehnicke specifikacije

- **Arhitektura**: Višearhitekturna putem HAL-a (x86, ARM, PowerPC spremni)
- **Protokol pokretanja**: Multiboot2 (x86), bootloaderi specificni za platformu
- **Grafika**: VESA medumemorija okvira, 800x600 @ 32-bitna boja
- **Raspored memorije**: Jezgra se ucitava na fizicku adresu od 1 MB (x86)
- **Mjerenje vremena**: Agnosticno prema arhitekturi s preciznoscu u mikrosekundama (RDTSC/registri timera)
- **Performanse**: Promašaj hladnog resursa <15µs, pogodak predmemorije <2µs, odstupanje timera <100ppm

### Statistika baze koda

- **225+ izvornih datoteka** s ~57.500+ redaka koda
- **145+ datoteka zaglavlja** u 28+ podsustava
- **69 tipova resursa** izvucenih iz System 7.1
- **Vrijeme kompajliranja**: 3-5 sekundi na modernom hardveru
- **Velicina jezgre**: ~4,16 MB
- **Velicina ISO-a**: ~12,5 MB

## 🔨 Izgradnja

### Zahtjevi

- **GCC** s podrškom za 32 bita (`gcc-multilib` na 64-bitnim sustavima)
- **GNU Make**
- **GRUB alati**: `grub-mkrescue` (iz `grub2-common` ili `grub-pc-bin`)
- **QEMU** za testiranje (`qemu-system-i386`)
- **Python 3** za obradu resursa
- **xxd** za binarnu konverziju
- *(Opcionalno)* **powerpc-linux-gnu** unakrsni skup alata za PowerPC izgradnje

### Instalacija na Ubuntu/Debian

```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common
```

### Naredbe za izgradnju

```bash
# Izgradnja jezgre (x86 prema zadanim postavkama)
make

# Izgradnja za odreðenu platformu
make PLATFORM=x86
make PLATFORM=arm        # zahtijeva ARM bare-metal GCC
make PLATFORM=ppc        # eksperimentalno; zahtijeva PowerPC ELF skup alata

# Stvaranje ISO-a za pokretanje
make iso

# Izgradnja sa svim jezicima
make LOCALE_FR=1 LOCALE_DE=1 LOCALE_ES=1 LOCALE_JA=1 LOCALE_ZH=1 LOCALE_KO=1 LOCALE_RU=1 LOCALE_UK=1 LOCALE_PL=1 LOCALE_CS=1 LOCALE_SQ=1 LOCALE_BG=1 LOCALE_HR=1 LOCALE_DA=1 LOCALE_NL=1 LOCALE_ET=1 LOCALE_FI=1 LOCALE_EL=1 LOCALE_HU=1 LOCALE_IS=1 LOCALE_IT=1 LOCALE_LV=1 LOCALE_LT=1 LOCALE_MK=1 LOCALE_ME=1 LOCALE_NO=1 LOCALE_PT=1 LOCALE_RO=1 LOCALE_SK=1 LOCALE_SL=1 LOCALE_SV=1 LOCALE_TR=1 LOCALE_HI=1

# Izgradnja s jednim dodatnim jezikom
make LOCALE_FR=1

# Izgradnja i pokretanje u QEMU-u
make run

# Cišcenje artefakata
make clean

# Prikaz statistike izgradnje
make info
```

## 🚀 Pokretanje

### Brzi pocetak (QEMU)

```bash
# Standardno pokretanje sa serijskim zapisivanjem
make run

# Rucno s opcijama
qemu-system-i386 -cdrom system71.iso -serial file:/tmp/serial.log -display sdl -vga std -m 256M
```

### QEMU opcije

```bash
# Sa serijskim izlazom na konzolu
qemu-system-i386 -cdrom system71.iso -serial stdio -display sdl -m 256M

# Bez grafickog prikaza
qemu-system-i386 -cdrom system71.iso -serial stdio -display none -m 256M

# S GDB otklanjanjem pogrešaka
make debug
# U drugom terminalu: gdb kernel.elf -ex "target remote :1234"
```

## 📚 Dokumentacija

### Vodici za komponente
- **Control Manager**: `docs/components/ControlManager/`
- **Dialog Manager**: `docs/components/DialogManager/`
- **Font Manager**: `docs/components/FontManager/`
- **Serijsko zapisivanje**: `docs/components/System/`
- **Event Manager**: `docs/components/EventManager.md`
- **Menu Manager**: `docs/components/MenuManager.md`
- **Window Manager**: `docs/components/WindowManager.md`
- **Resource Manager**: `docs/components/ResourceManager.md`

### Internacionalizacija
- **Locale Manager**: `include/LocaleManager/` — prebacivanje jezika za vrijeme izvršavanja, odabir jezika pri pokretanju
- **Resursi nizova**: `resources/strings/` — STR# datoteke resursa po jeziku (34 jezika)
- **Prošireni fontovi**: `include/chicago_font_extended.h` — Mac Roman 0x80-0xFF glifovi za europske znakove
- **CJK podrška**: `include/TextEncoding/CJKEncoding.h`, `include/FontManager/CJKFont.h` — višebajtno kodiranje i infrastruktura fontova

### Status implementacije
- **IMPLEMENTATION_PRIORITIES.md**: Planirani rad i pracenje dovršenosti
- **IMPLEMENTATION_STATUS_AUDIT.md**: Detaljni pregled svih podsustava

### Filozofija projekta

**Arheološki pristup** s implementacijom temeljenom na dokazima:
1. Podržano dokumentacijom Inside Macintosh i MPW Universal Interfaces
2. Sve važne odluke oznacene Finding ID-ovima koji upucuju na popratne dokaze
3. Cilj: podudaranje ponašanja s izvornim System 7, a ne modernizacija
4. Implementacija iz ciste sobe (bez izvornog Appleovog koda)

## 🐛 Poznati problemi

1. **Artefakti pri povlacenju ikona**: Manji vizualni artefakti tijekom povlacenja ikona na radnoj površini
2. **M68K izvršavanje zamijenjeno stubovima**: Ucitavac segmenata dovršen, petlja izvršavanja nije implementirana
3. **Nema podrške za TrueType**: Samo bitmap fontovi (Chicago)
4. **HFS samo za citanje**: Virtualni datotecni sustav, bez stvarnog zapisivanja na disk
5. **Nema jamstava stabilnosti**: Padovi i neocekivano ponašanje su ucestali

## 🤝 Doprinos

Ovo je primarno projekt za ucenje/istraživanje:

1. **Prijave pogrešaka**: Podnesite probleme s detaljnim koracima za reprodukciju
2. **Testiranje**: Prijavite rezultate na razlicitom hardveru/emulatorima
3. **Dokumentacija**: Poboljšajte postojecu dokumentaciju ili dodajte nove vodice

## 📖 Bitne reference

- **Inside Macintosh** (1992.-1994.): Službena Appleova dokumentacija za Toolbox
- **MPW Universal Interfaces 3.2**: Kanonicke datoteke zaglavlja i definicije struktura
- **Guide to Macintosh Family Hardware**: Referenca za hardversku arhitekturu

### Korisni alati

- **Mini vMac**: Emulator System 7 za referencu ponašanja
- **ResEdit**: Ureðivac resursa za proucavanje resursa System 7
- **Ghidra/IDA**: Za analizu rastavljanja ROM-a

## ⚖️ Pravno

Ovo je **reimplementacija iz ciste sobe** u edukativne svrhe i svrhe ocuvanja:

- **Nije korišten Appleov izvorni kod**
- Temeljeno iskljucivo na javnoj dokumentaciji i analizi crne kutije
- "System 7", "Macintosh", "QuickDraw" su zaštitni znakovi tvrtke Apple Inc.
- Nije povezano s, podržano od niti sponzorirano od strane tvrtke Apple Inc.

**Izvorni System 7 ROM i softver ostaju vlasništvo tvrtke Apple Inc.**

## 🙏 Zahvale

- **Apple Computer, Inc.** za stvaranje izvornog System 7
- **Autori Inside Macintosha** za sveobuhvatnu dokumentaciju
- **Zajednica za ocuvanje klasicnog Maca** za održavanje platforme na životu
- **68k.news i Macintosh Garden** za arhive resursa

## 📊 Statistika razvoja

- **Redaka koda**: ~57.500+ (ukljucujuci 2.500+ za ucitavac segmenata)
- **Vrijeme kompajliranja**: ~3-5 sekundi
- **Velicina jezgre**: ~4,16 MB (kernel.elf)
- **Velicina ISO-a**: ~12,5 MB (system71.iso)
- **Smanjenje pogrešaka**: 94% osnovne funkcionalnosti radi
- **Glavni podsustavi**: 28+ (Font, Window, Menu, Control, Dialog, TextEdit itd.)

## 🔮 Buduci smjer

**Planirani rad**:

- Dovršiti petlju izvršavanja M68K interpretera
- Dodati podršku za TrueType fontove
- CJK bitmap resursi fontova za renderiranje japanskoga, kineskoga i korejskoga
- Implementirati dodatne kontrole (tekstualna polja, skocni izbornici, klizaci)
- Zapisivanje na disk za HFS datotecni sustav
- Napredne znacajke Sound Managera (miksanje, uzorkovanje)
- Osnovni pribor za radnu površinu (Kalkulator, Notes)

---

**Status**: Eksperimentalno - Edukativno - U razvoju

**Zadnje ažuriranje**: Studeni 2025. (Poboljšanja Sound Managera dovršena)

Za pitanja, probleme ili raspravu, molimo koristite GitHub Issues.
