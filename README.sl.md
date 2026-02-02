# System 7 - Prenosna odprtokodna ponovna implementacija

**[English](README.md)** | **[Fran&ccedil;ais](README.fr.md)** | **[Deutsch](README.de.md)** | **[Espa&ntilde;ol](README.es.md)** | **[Italiano](README.it.md)** | **[Portugu&ecirc;s](README.pt.md)** | **[Nederlands](README.nl.md)** | **[Dansk](README.da.md)** | **[Norsk](README.no.md)** | **[Svenska](README.sv.md)** | **[Suomi](README.fi.md)** | **[&Iacute;slenska](README.is.md)** | **[Ελληνικά](README.el.md)** | **[T&uuml;rk&ccedil;e](README.tr.md)** | **[Polski](README.pl.md)** | **[Čeština](README.cs.md)** | **[Slovenčina](README.sk.md)** | **[Slovenščina](README.sl.md)** | **[Hrvatski](README.hr.md)** | **[Magyar](README.hu.md)** | **[Rom&acirc;n&atilde;](README.ro.md)** | **[Български](README.bg.md)** | **[Shqip](README.sq.md)** | **[Eesti](README.et.md)** | **[Latviešu](README.lv.md)** | **[Lietuvių](README.lt.md)** | **[Македонски](README.mk.md)** | **[Crnogorski](README.me.md)** | **[Русский](README.ru.md)** | **[Українська](README.uk.md)** | **[العربية](README.ar.md)** | **[日本語](README.ja.md)** | **[简体中文](README.zh.md)** | **[繁體中文](README.tw.md)** | **[한국어](README.ko.md)** | **[हिन्दी](README.hi.md)** | **[বাংলা](README.bn.md)** | **[اردو](README.ur.md)**

<img width="793" height="657" alt="System 7 deluje na sodobni strojni opremi" src="https://github.com/user-attachments/assets/be84b83e-191c-4f9d-a786-11d0bd04203b" />
<img width="801" height="662" alt="simpletextworks" src="https://github.com/user-attachments/assets/7c9ebe5b-22b4-4612-93a1-2076909d77cd" />
<img width="803" height="661" alt="macpaint" src="https://github.com/user-attachments/assets/cd3ed04a-fdde-4dd5-88ef-5b19b3a13a54" />

> ⚠️ **DOKAZ KONCEPTA** - To je eksperimentalna, izobraževalna ponovna implementacija Applovega Macintosh System 7. To NI končni izdelek in ga ne smete obravnavati kot produkcijsko programsko opremo.

Odprtokodna ponovna implementacija Apple Macintosh System 7 za sodobno strojno opremo x86, ki se zažene prek GRUB2/Multiboot2. Cilj tega projekta je poustvariti klasično izkušnjo Mac OS ter hkrati dokumentirati arhitekturo System 7 z analizo povratnega inženirstva.

## 🎯 Stanje projekta

**Trenutno stanje**: Aktiven razvoj s ~94% dokončane osnovne funkcionalnosti

### Zadnje posodobitve (november 2025)

#### Izboljšave upravljalnika zvoka ✅ DOKONČANO
- **Optimizirana pretvorba MIDI**: Skupna pomožna funkcija `SndMidiNoteToFreq()` s preglednico 37 vnosov (C3-B5) in nadomestnim izračunom na podlagi oktav za celoten obseg MIDI (0-127)
- **Podpora za asinhrono predvajanje**: Popolna infrastruktura povratnih klicev za predvajanje datotek (`FilePlayCompletionUPP`) in izvajanje ukazov (`SndCallBackProcPtr`)
- **Usmerjanje zvoka na podlagi kanalov**: Večnivojski sistem prioritet z nadzorom utišanja in omogočanja
  - 4-nivojski prioritetni kanali (0-3) za usmerjanje na strojno opremo
  - Neodvisen nadzor utišanja in omogočanja za vsak kanal
  - `SndGetActiveChannel()` vrne aktivni kanal z najvišjo prioriteto
  - Pravilna inicializacija kanalov s privzeto omogočeno zastavico
- **Implementacija produkcijske kakovosti**: Vsa koda se prevede brez opozoril, zaznanih ni kršitev malloc/free
- **Potrjevanja**: 07542c5 (optimizacija MIDI), 1854fe6 (asinhroni povratni klici), a3433c6 (usmerjanje kanalov)

#### Dosežki prejšnjih sej
- ✅ **Faza naprednih funkcij**: Zanka za obdelavo ukazov upravljalnika zvoka, serializacija slogov za večkratni zagon, razširjene funkcije MIDI/sinteze
- ✅ **Sistem za spreminjanje velikosti oken**: Interaktivno spreminjanje velikosti s pravilno obdelavo okraskov, rastezalnim poljem in čiščenjem namizja
- ✅ **Prevajanje tipkovnice PS/2**: Popolno preslikovanje razporedalnih kod nabora 1 v kode tipk orodjarne
- ✅ **Večplatformni HAL**: Podpora za x86, ARM in PowerPC s čisto abstrakcijo

## 📊 Dokončanost projekta

**Skupna osnovna funkcionalnost**: ~94% dokončano (ocena)

### Kar v celoti deluje ✅

- **Abstrakcijska plast strojne opreme (HAL)**: Popolna platformna abstrakcija za x86/ARM/PowerPC
- **Zagonski sistem**: Uspešen zagon prek GRUB2/Multiboot2 na x86
- **Serijsko beleženje**: Modularno beleženje s filtriranjem med izvajanjem (Error/Warn/Info/Debug/Trace)
- **Grafična osnova**: Medpomnilnik slike VESA (800x600x32) s primitivami QuickDraw, vključno z načinom XOR
- **Izris namizja**: Menijska vrstica System 7 z mavrično logotipom Apple, ikonami in vzorci namizja
- **Tipografija**: Bitna pisava Chicago s pikselsko natančnim izrisom in pravilnim razmikanjem, razširjen nabor Mac Roman (0x80-0xFF) za evropske naglašene znake
- **Internacionalizacija (i18n)**: Lokalizacija na podlagi virov s 38 jeziki (angleščina, francoščina, nemščina, španščina, italijanščina, portugalščina, nizozemščina, danščina, norveščina, švedščina, finščina, islandščina, grščina, turščina, poljščina, češčina, slovaščina, slovenščina, hrvaščina, madžarščina, romunščina, bolgarščina, albanščina, estonščina, latvijščina, litovščina, makedonščina, črnogorščina, ruščina, ukrajinščina, arabščina, japonščina, poenostavljena kitajščina, tradicionalna kitajščina, korejščina, hindijščina, bengalščina, urdujščina), upravljalnik lokalnih nastavitev z izbiro jezika ob zagonu, infrastruktura za večbajtno kodiranje CJK
- **Upravljalnik pisav**: Podpora za več velikosti (9-24pt), sinteza slogov, razčlenjevanje FOND/NFNT, predpomnilnik LRU
- **Vhodni sistem**: Tipkovnica in miška PS/2 s popolnim posredovanjem dogodkov
- **Upravljalnik dogodkov**: Sodelovalna večopravilnost prek WaitNextEvent z enotno čakalno vrsto dogodkov
- **Upravljalnik pomnilnika**: Dodeljevanje na podlagi con s povezavo z interpreterjem 68K
- **Upravljalnik menijev**: Popolni spustni meniji s sledenjem miške in SaveBits/RestoreBits
- **Datotečni sistem**: HFS z implementacijo B-drevesa, okna map z naštevanjem VFS
- **Upravljalnik oken**: Vlečenje, spreminjanje velikosti (z rastezalnim poljem), plastenje, aktivacija
- **Upravljalnik časa**: Natančna kalibracija TSC, mikrosekndna natančnost, preverjanje generacij
- **Upravljalnik virov**: Binarno iskanje O(log n), predpomnilnik LRU, celovita validacija
- **Upravljalnik Gestalt**: Večarhitekturne sistemske informacije z zaznavanjem arhitekture
- **Upravljalnik TextEdit**: Popolno urejanje besedila s povezavo z odložiščem
- **Upravljalnik odložišča (Scrap)**: Klasično odložišče Mac OS s podporo za več formatov
- **Aplikacija SimpleText**: Polno opremljen urejevalnik besedil MDI z izrezovanjem/kopiranjem/lepljenjem
- **Upravljalnik seznamov**: Nadzor seznamov, združljiv s System 7.1, s krmarjenjem po tipkovnici
- **Upravljalnik kontrol**: Standardne kontrole in drsniki z implementacijo CDEF
- **Upravljalnik pogovornih oken**: Krmarjenje po tipkovnici, obrobe fokusa, bližnjice na tipkovnici
- **Nalagalnik segmentov**: Prenosni ISA-neodvisen sistem za nalaganje segmentov 68K s premesti tvami
- **Interpreter M68K**: Popolno razpošiljanje ukazov z 84 obdelovalci ukazov, vseh 14 naslovalnih načinov, ogrodje za izjeme/pasti
- **Upravljalnik zvoka**: Obdelava ukazov, pretvorba MIDI, upravljanje kanalov, povratni klici
- **Upravljalnik naprav**: Upravljanje DCE, nameščanje/odstranjevanje gonilnikov in operacije V/I
- **Zagonski zaslon**: Popoln zagonski uporabniški vmesnik s sledenjem napredka, upravljanjem faz in pozdravnim zaslonom
- **Upravljalnik barv**: Upravljanje stanja barv s povezavo z QuickDraw

### Delno implementirano ⚠️

- **Integracija aplikacij**: Interpreter M68K in nalagalnik segmentov sta dokončana; potrebno je integacijsko testiranje za preverjanje izvajanja pravih aplikacij
- **Postopki za opredelitev oken (WDEF)**: Osnovna struktura vzpostavljena, delno razpošiljanje
- **Upravljalnik govora**: Samo ogrodje API in prehod zvoka; mehanizem za sintezo govora ni implementiran
- **Obdelava izjem (RTE)**: Vrnitev iz izjeme delno implementirana (trenutno se ustavi namesto obnovitve konteksta)

### Še ni implementirano ❌

- **Tiskanje**: Brez tiskalnega sistema
- **Omrežje**: Brez funkcionalnosti AppleTalk ali omrežja
- **Namizni pripomočki**: Samo ogrodje
- **Napredni zvok**: Predvajanje vzorcev, mešanje (omejitev zvočnika PC)

### Podsistemi, ki niso prevedeni 🔧

Naslednji imajo izvorno kodo, vendar niso vključeni v jedro:
- **AppleEventManager** (8 datotek): Medsporočanje med aplikacijami; namenoma izključen zaradi odvisnosti od pthread, ki niso združljive s samostojnim okoljem
- **FontResources** (samo glava): Definicije tipov virov pisav; dejansko podporo za pisave zagotavlja prevedena datoteka FontResourceLoader.c

## 🏗️ Arhitektura

### Tehnične specifikacije

- **Arhitektura**: Večarhitekturna prek HAL (x86, ARM, PowerPC pripravljeno)
- **Zagonski protokol**: Multiboot2 (x86), platformno specifični zagonski nalagalniki
- **Grafika**: Medpomnilnik slike VESA, 800x600 @ 32-bitna barva
- **Razporeditev pomnilnika**: Jedro se naloži na fizični naslov 1 MB (x86)
- **Merjenje časa**: Arhitekturno neodvisno z mikrosekundno natančnostjo (RDTSC/časovni registri)
- **Zmogljivost**: Zgrešitev hladnega vira <15 µs, zadetek predpomnilnika <2 µs, časovni zdrs <100 ppm

### Statistika kodne baze

- **225+ izvornih datotek** s ~57.500+ vrsticami kode
- **145+ glavnih datotek** v 28+ podsistemih
- **69 tipov virov**, pridobljenih iz System 7.1
- **Čas prevajanja**: 3-5 sekund na sodobni strojni opremi
- **Velikost jedra**: ~4,16 MB
- **Velikost ISO**: ~12,5 MB

## 🔨 Prevajanje

### Zahteve

- **GCC** s podporo za 32-bitno arhitekturo (`gcc-multilib` na 64-bitni)
- **GNU Make**
- **Orodja GRUB**: `grub-mkrescue` (iz `grub2-common` ali `grub-pc-bin`)
- **QEMU** za testiranje (`qemu-system-i386`)
- **Python 3** za obdelavo virov
- **xxd** za binarno pretvorbo
- *(Neobvezno)* **powerpc-linux-gnu** navzkrižna orodja za gradnje PowerPC

### Namestitev na Ubuntu/Debian

```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common
```

### Ukazi za prevajanje

```bash
# Prevedi jedro (privzeto x86)
make

# Prevedi za določeno platformo
make PLATFORM=x86
make PLATFORM=arm        # requires ARM bare-metal GCC
make PLATFORM=ppc        # experimental; requires PowerPC ELF toolchain

# Ustvari zagonsko sliko ISO
make iso

# Prevedi z vsemi jeziki
make LOCALE_FR=1 LOCALE_DE=1 LOCALE_ES=1 LOCALE_JA=1 LOCALE_ZH=1 LOCALE_KO=1 LOCALE_RU=1 LOCALE_UK=1 LOCALE_PL=1 LOCALE_CS=1 LOCALE_SQ=1 LOCALE_BG=1 LOCALE_HR=1 LOCALE_DA=1 LOCALE_NL=1 LOCALE_ET=1 LOCALE_FI=1 LOCALE_EL=1 LOCALE_HU=1 LOCALE_IS=1 LOCALE_IT=1 LOCALE_LV=1 LOCALE_LT=1 LOCALE_MK=1 LOCALE_ME=1 LOCALE_NO=1 LOCALE_PT=1 LOCALE_RO=1 LOCALE_SK=1 LOCALE_SL=1 LOCALE_SV=1 LOCALE_TR=1 LOCALE_HI=1 LOCALE_TW=1 LOCALE_AR=1 LOCALE_BN=1 LOCALE_UR=1

# Prevedi z enim dodatnim jezikom
make LOCALE_FR=1

# Prevedi in zaženi v QEMU
make run

# Počisti artefakte
make clean

# Prikaži statistiko prevajanja
make info
```

## 🚀 Zagon

### Hitri začetek (QEMU)

```bash
# Standardni zagon s serijskim beleženjem
make run

# Ročno z možnostmi
qemu-system-i386 -cdrom system71.iso -serial file:/tmp/serial.log -display sdl -vga std -m 256M
```

### Možnosti QEMU

```bash
# S serijskim izhodom na konzolo
qemu-system-i386 -cdrom system71.iso -serial stdio -display sdl -m 256M

# Brez grafičnega vmesnika
qemu-system-i386 -cdrom system71.iso -serial stdio -display none -m 256M

# Z razhroščevanjem GDB
make debug
# V drugem terminalu: gdb kernel.elf -ex "target remote :1234"
```

## 📚 Dokumentacija

### Vodniki po komponentah
- **Upravljalnik kontrol**: `docs/components/ControlManager/`
- **Upravljalnik pogovornih oken**: `docs/components/DialogManager/`
- **Upravljalnik pisav**: `docs/components/FontManager/`
- **Serijsko beleženje**: `docs/components/System/`
- **Upravljalnik dogodkov**: `docs/components/EventManager.md`
- **Upravljalnik menijev**: `docs/components/MenuManager.md`
- **Upravljalnik oken**: `docs/components/WindowManager.md`
- **Upravljalnik virov**: `docs/components/ResourceManager.md`

### Internacionalizacija
- **Upravljalnik lokalnih nastavitev**: `include/LocaleManager/` — preklapljanje lokalnih nastavitev med izvajanjem, izbira jezika ob zagonu
- **Nizovni viri**: `resources/strings/` — datoteke virov STR# za posamezne jezike (34 jezikov)
- **Razširjene pisave**: `include/chicago_font_extended.h` — glifi Mac Roman 0x80-0xFF za evropske znake
- **Podpora CJK**: `include/TextEncoding/CJKEncoding.h`, `include/FontManager/CJKFont.h` — infrastruktura za večbajtno kodiranje in pisave

### Stanje implementacije
- **IMPLEMENTATION_PRIORITIES.md**: Načrtovano delo in sledenje dokončanosti
- **IMPLEMENTATION_STATUS_AUDIT.md**: Podrobna revizija vseh podsistemov

### Filozofija projekta

**Arheološki pristop** z implementacijo na podlagi dokazov:
1. Podprto z dokumentacijo Inside Macintosh in univerzalnimi vmesniki MPW
2. Vse pomembne odločitve označene z identifikatorji ugotovitev, ki se sklicujejo na dokazno gradivo
3. Cilj: vedenjska enakovrednost z izvirnim System 7, ne posodobitev
4. Implementacija v čisti sobi (brez izvorne kode Apple)

## 🐛 Znane težave

1. **Artefakti pri vlečenju ikon**: Manjši vizualni artefakti med vlečenjem ikon po namizju
2. **Izvajanje M68K je ogrodje**: Nalagalnik segmentov je dokončan, izvajalna zanka ni implementirana
3. **Brez podpore za TrueType**: Samo bitne pisave (Chicago)
4. **HFS samo za branje**: Navidezni datotečni sistem, brez dejanskega zapisovanja na disk
5. **Brez jamstev za stabilnost**: Sesutja in nepričakovano vedenje so pogosti

## 🤝 Prispevanje

To je predvsem učni/raziskovalni projekt:

1. **Poročila o napakah**: Oddajte zahtevke s podrobnimi koraki za reprodukcijo
2. **Testiranje**: Poročajte o rezultatih na različni strojni opremi/emulatorjih
3. **Dokumentacija**: Izboljšajte obstoječe dokumente ali dodajte nove vodnike

## 📖 Bistveni viri

- **Inside Macintosh** (1992-1994): Uradna dokumentacija orodjarne Apple
- **MPW Universal Interfaces 3.2**: Kanonične glavne datoteke in definicije struktur
- **Guide to Macintosh Family Hardware**: Referenca za arhitekturo strojne opreme

### Uporabna orodja

- **Mini vMac**: Emulator System 7 za vedenjsko referenco
- **ResEdit**: Urejevalnik virov za preučevanje virov System 7
- **Ghidra/IDA**: Za analizo razstavljanja ROM-a

## ⚖️ Pravne informacije

To je **ponovna implementacija v čisti sobi** za izobraževalne namene in namene ohranjanja:

- **Nobena izvorna koda Apple** ni bila uporabljena
- Temelji izključno na javni dokumentaciji in analizi črne skrinjice
- "System 7", "Macintosh", "QuickDraw" so blagovne znamke Apple Inc.
- Ni povezan z Apple Inc., niti ga Apple Inc. ne podpira ali sponzorira

**Izvirni ROM in programska oprema System 7 ostajajo last Apple Inc.**

## 🙏 Zahvale

- **Apple Computer, Inc.** za ustvarjanje izvirnega System 7
- **Avtorji Inside Macintosh** za celovito dokumentacijo
- **Skupnost za ohranjanje klasičnega Maca** za ohranjanje platforme pri življenju
- **68k.news in Macintosh Garden** za arhive virov

## 📊 Statistika razvoja

- **Vrstice kode**: ~57.500+ (vključno z 2.500+ za nalagalnik segmentov)
- **Čas prevajanja**: ~3-5 sekund
- **Velikost jedra**: ~4,16 MB (kernel.elf)
- **Velikost ISO**: ~12,5 MB (system71.iso)
- **Zmanjšanje napak**: 94% osnovne funkcionalnosti deluje
- **Glavni podsistemi**: 28+ (Font, Window, Menu, Control, Dialog, TextEdit itd.)

## 🔮 Prihodnje usmeritve

**Načrtovano delo**:

- Dokončanje izvajalne zanke interpreterja M68K
- Dodajanje podpore za pisave TrueType
- Bitni viri pisav CJK za izris japonščine, kitajščine in korejščine
- Implementacija dodatnih kontrol (besedilna polja, pojavni meniji, drsniki)
- Zapisovanje na disk za datotečni sistem HFS
- Napredne funkcije upravljalnika zvoka (mešanje, vzorčenje)
- Osnovni namizni pripomočki (kalkulator, beležnica)

---

**Stanje**: Eksperimentalno - Izobraževalno - V razvoju

**Nazadnje posodobljeno**: November 2025 (Izboljšave upravljalnika zvoka dokončane)

Za vprašanja, težave ali razpravo prosimo uporabite GitHub Issues.
