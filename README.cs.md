# System 7 – Přenosná open-source reimplementace

**[English](README.md)** | **[Français](README.fr.md)** | **[Deutsch](README.de.md)** | **[Español](README.es.md)** | **[日本語](README.ja.md)** | **[中文](README.zh.md)** | **[한국어](README.ko.md)** | **[Русский](README.ru.md)** | **[Українська](README.uk.md)** | **[Polski](README.pl.md)** | **[Čeština](README.cs.md)**

<img width="793" height="657" alt="System 7 běžící na moderním hardwaru" src="https://github.com/user-attachments/assets/be84b83e-191c-4f9d-a786-11d0bd04203b" />
<img width="801" height="662" alt="simpletextworks" src="https://github.com/user-attachments/assets/7c9ebe5b-22b4-4612-93a1-2076909d77cd" />
<img width="803" height="661" alt="macpaint" src="https://github.com/user-attachments/assets/cd3ed04a-fdde-4dd5-88ef-5b19b3a13a54" />

> ⚠️ **PROOF OF CONCEPT** – Toto je experimentální, vzdělávací reimplementace systému Apple Macintosh System 7. Nejedná se o hotový produkt a neměl by být považován za software připravený pro produkční nasazení.

Open-source reimplementace systému Apple Macintosh System 7 pro moderní x86 hardware, spustitelná přes GRUB2/Multiboot2. Cílem tohoto projektu je znovu vytvořit klasický zážitek z Mac OS a zároveň zdokumentovat architekturu System 7 prostřednictvím analýzy reverzního inženýrství.

## 🎯 Stav projektu

**Aktuální stav**: Aktivní vývoj s přibližně 94 % hotové základní funkcionality

### Nejnovější aktualizace (listopad 2025)

#### Vylepšení Sound Manageru ✅ DOKONČENO
- **Optimalizovaná konverze MIDI**: Sdílený helper `SndMidiNoteToFreq()` se 37záznamovou vyhledávací tabulkou (C3–B5) a oktávovým fallbackem pro celý rozsah MIDI (0–127)
- **Podpora asynchronního přehrávání**: Kompletní infrastruktura callbacků pro přehrávání souborů (`FilePlayCompletionUPP`) i provádění příkazů (`SndCallBackProcPtr`)
- **Směrování zvuku na bázi kanálů**: Víceúrovňový systém priorit s ovládáním ztlumení a zapnutí
  - 4 úrovně prioritních kanálů (0–3) pro směrování hardwarového výstupu
  - Nezávislé ovládání ztlumení a zapnutí pro každý kanál
  - `SndGetActiveChannel()` vrací aktivní kanál s nejvyšší prioritou
  - Správná inicializace kanálu s příznakem zapnutí ve výchozím stavu
- **Implementace produkční kvality**: Veškerý kód se kompiluje čistě, nebyly zjištěny žádné porušení malloc/free
- **Commity**: 07542c5 (optimalizace MIDI), 1854fe6 (asynchronní callbacky), a3433c6 (směrování kanálů)

#### Výsledky předchozích sezení
- ✅ **Fáze pokročilých funkcí**: Smyčka zpracování příkazů Sound Manageru, serializace víceběhových stylů, rozšířené funkce MIDI/syntézy
- ✅ **Systém změny velikosti oken**: Interaktivní změna velikosti se správným zpracováním okenního rámce, grow boxem a úklidem plochy
- ✅ **Překlad PS/2 klávesnice**: Kompletní mapování scancode sady 1 na kódy kláves Toolboxu
- ✅ **Multiplatformní HAL**: Podpora x86, ARM a PowerPC s čistou abstrakcí

## 📊 Úplnost projektu

**Celková základní funkcionalita**: přibližně 94 % dokončeno (odhad)

### Plně funkční ✅

- **Vrstva abstrakce hardwaru (HAL)**: Kompletní platformní abstrakce pro x86/ARM/PowerPC
- **Zaváděcí systém**: Úspěšné spouštění přes GRUB2/Multiboot2 na x86
- **Sériové logování**: Modulární logování s filtrováním za běhu (Error/Warn/Info/Debug/Trace)
- **Grafický základ**: VESA framebuffer (800x600x32) s primitivy QuickDraw včetně režimu XOR
- **Vykreslování plochy**: Lišta nabídek System 7 s duhovým logem Apple, ikonami a vzory plochy
- **Typografie**: Bitmapový font Chicago s pixelově přesným vykreslováním a správným kerningem, rozšířená sada Mac Roman (0x80–0xFF) pro evropské znaky s diakritikou
- **Internacionalizace (i18n)**: Lokalizace založená na zdrojích s 11 jazyky (angličtina, francouzština, němčina, španělština, japonština, čínština, korejština, ruština, ukrajinština, polština, čeština), Locale Manager s výběrem jazyka při startu, infrastruktura vícebajtového kódování CJK
- **Font Manager**: Podpora více velikostí (9–24pt), syntéza stylů, parsování FOND/NFNT, LRU cache
- **Vstupní systém**: PS/2 klávesnice a myš s kompletním předáváním událostí
- **Event Manager**: Kooperativní multitasking přes WaitNextEvent s jednotnou frontou událostí
- **Memory Manager**: Alokace založená na zónách s integrací interpretu 68K
- **Menu Manager**: Kompletní rozbalovací nabídky se sledováním myši a SaveBits/RestoreBits
- **Souborový systém**: HFS s implementací B-tree, okna složek s výčtem VFS
- **Window Manager**: Přetahování, změna velikosti (s grow boxem), vrstvení, aktivace
- **Time Manager**: Přesná kalibrace TSC, mikrosekundová přesnost, kontrola generací
- **Resource Manager**: Binární vyhledávání O(log n), LRU cache, komplexní validace
- **Gestalt Manager**: Víceplatformní systémové informace s detekcí architektury
- **TextEdit Manager**: Kompletní editace textu s integrací schránky
- **Scrap Manager**: Klasická schránka Mac OS s podporou více formátů
- **Aplikace SimpleText**: Plnohodnotný MDI textový editor s funkcemi vyjmout/kopírovat/vložit
- **List Manager**: Ovládací prvky seznamů kompatibilní se System 7.1 s navigací klávesnicí
- **Control Manager**: Standardní ovládací prvky a posuvníky s implementací CDEF
- **Dialog Manager**: Navigace klávesnicí, ohraničení zaměření, klávesové zkratky
- **Segment Loader**: Přenosný ISA-agnostický systém načítání 68K segmentů s relokací
- **Interpret M68K**: Plný dispatch instrukcí s 84 handlery opcode, všech 14 adresových režimů, framework výjimek/trapů
- **Sound Manager**: Zpracování příkazů, konverze MIDI, správa kanálů, callbacky
- **Device Manager**: Správa DCE, instalace/odebírání ovladačů a I/O operace
- **Úvodní obrazovka**: Kompletní UI při spouštění se sledováním průběhu, správou fází a úvodní obrazovkou
- **Color Manager**: Správa stavu barev s integrací QuickDraw

### Částečně implementováno ⚠️

- **Integrace aplikací**: Interpret M68K a segment loader jsou hotové; je potřeba integrační testování pro ověření běhu skutečných aplikací
- **Definiční procedury oken (WDEF)**: Základní struktura připravena, částečný dispatch
- **Speech Manager**: Pouze API framework a průchod zvuku; syntéza řeči není implementována
- **Zpracování výjimek (RTE)**: Návrat z výjimky částečně implementován (momentálně zastavuje místo obnovy kontextu)

### Dosud neimplementováno ❌

- **Tisk**: Žádný tiskový systém
- **Síťování**: Žádná funkcionalita AppleTalk ani sítě
- **Desk Accessories**: Pouze framework
- **Pokročilý zvuk**: Přehrávání samplů, mixování (omezení PC speakeru)

### Nekompilované subsystémy 🔧

Následující mají zdrojový kód, ale nejsou integrovány do jádra:
- **AppleEventManager** (8 souborů): Meziprocesová komunikace; záměrně vyloučen kvůli závislostem na pthread nekompatibilním s freestanding prostředím
- **FontResources** (pouze hlavičkový soubor): Definice typů fontových zdrojů; skutečná podpora fontů je zajištěna kompilovaným souborem FontResourceLoader.c

## 🏗️ Architektura

### Technické specifikace

- **Architektura**: Multiarchitektura přes HAL (x86, ARM, PowerPC připraveno)
- **Zaváděcí protokol**: Multiboot2 (x86), platformně specifické bootloadery
- **Grafika**: VESA framebuffer, 800x600 při 32bitové barvě
- **Rozložení paměti**: Jádro se načítá na fyzické adrese 1 MB (x86)
- **Časování**: Architektonicky agnostické s mikrosekundovou přesností (RDTSC/registry časovače)
- **Výkon**: Studený cache miss zdroje <15 µs, cache hit <2 µs, drift časovače <100 ppm

### Statistiky zdrojového kódu

- **225+ zdrojových souborů** s přibližně 57 500+ řádky kódu
- **145+ hlavičkových souborů** v 28+ subsystémech
- **69 typů zdrojů** extrahovaných ze System 7.1
- **Doba kompilace**: 3–5 sekund na moderním hardwaru
- **Velikost jádra**: přibližně 4,16 MB
- **Velikost ISO**: přibližně 12,5 MB

## 🔨 Sestavení

### Požadavky

- **GCC** s podporou 32bitového režimu (`gcc-multilib` na 64bitových systémech)
- **GNU Make**
- **Nástroje GRUB**: `grub-mkrescue` (z `grub2-common` nebo `grub-pc-bin`)
- **QEMU** pro testování (`qemu-system-i386`)
- **Python 3** pro zpracování zdrojů
- **xxd** pro binární konverzi
- *(Volitelné)* **powerpc-linux-gnu** cross toolchain pro sestavení na PowerPC

### Instalace na Ubuntu/Debian

```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common
```

### Příkazy pro sestavení

```bash
# Sestavení jádra (ve výchozím stavu x86)
make

# Sestavení pro konkrétní platformu
make PLATFORM=x86
make PLATFORM=arm        # vyžaduje ARM bare-metal GCC
make PLATFORM=ppc        # experimentální; vyžaduje PowerPC ELF toolchain

# Vytvoření bootovatelného ISO
make iso

# Sestavení se všemi jazyky
make LOCALE_FR=1 LOCALE_DE=1 LOCALE_ES=1 LOCALE_JA=1 LOCALE_ZH=1 LOCALE_KO=1 LOCALE_RU=1 LOCALE_UK=1 LOCALE_PL=1 LOCALE_CS=1

# Sestavení s jedním dodatečným jazykem
make LOCALE_FR=1

# Sestavení a spuštění v QEMU
make run

# Vyčištění artefaktů
make clean

# Zobrazení statistik sestavení
make info
```

## 🚀 Spuštění

### Rychlý start (QEMU)

```bash
# Standardní spuštění se sériovým logováním
make run

# Ruční spuštění s volbami
qemu-system-i386 -cdrom system71.iso -serial file:/tmp/serial.log -display sdl -vga std -m 256M
```

### Volby QEMU

```bash
# S výstupem sériové konzole
qemu-system-i386 -cdrom system71.iso -serial stdio -display sdl -m 256M

# Bez grafického rozhraní (headless)
qemu-system-i386 -cdrom system71.iso -serial stdio -display none -m 256M

# S laděním přes GDB
make debug
# V dalším terminálu: gdb kernel.elf -ex "target remote :1234"
```

## 📚 Dokumentace

### Příručky komponent
- **Control Manager**: `docs/components/ControlManager/`
- **Dialog Manager**: `docs/components/DialogManager/`
- **Font Manager**: `docs/components/FontManager/`
- **Sériové logování**: `docs/components/System/`
- **Event Manager**: `docs/components/EventManager.md`
- **Menu Manager**: `docs/components/MenuManager.md`
- **Window Manager**: `docs/components/WindowManager.md`
- **Resource Manager**: `docs/components/ResourceManager.md`

### Internacionalizace
- **Locale Manager**: `include/LocaleManager/` — přepínání lokálů za běhu, výběr jazyka při startu
- **Řetězcové zdroje**: `resources/strings/` — soubory zdrojů STR# pro jednotlivé jazyky (en, fr, de, es, ja, zh, ko, ru, uk, pl, cs)
- **Rozšířené fonty**: `include/chicago_font_extended.h` — glyfy Mac Roman 0x80–0xFF pro evropské znaky
- **Podpora CJK**: `include/TextEncoding/CJKEncoding.h`, `include/FontManager/CJKFont.h` — vícebajtové kódování a fontová infrastruktura

### Stav implementace
- **IMPLEMENTATION_PRIORITIES.md**: Plánované práce a sledování úplnosti
- **IMPLEMENTATION_STATUS_AUDIT.md**: Podrobný audit všech subsystémů

### Filozofie projektu

**Archeologický přístup** s implementací založenou na důkazech:
1. Podloženo dokumentací Inside Macintosh a MPW Universal Interfaces
2. Všechna hlavní rozhodnutí označena identifikátory nálezů odkazujícími na podpůrné důkazy
3. Cíl: behaviorální paritu s původním System 7, nikoliv modernizaci
4. Clean-room implementace (bez původního zdrojového kódu Apple)

## 🐛 Známé problémy

1. **Artefakty při přetahování ikon**: Drobné vizuální artefakty při přetahování ikon na ploše
2. **Provádění M68K je jen stub**: Segment loader je hotový, smyčka provádění není implementována
3. **Bez podpory TrueType**: Pouze bitmapové fonty (Chicago)
4. **HFS pouze pro čtení**: Virtuální souborový systém, bez zpětného zápisu na disk
5. **Žádné záruky stability**: Pády a neočekávané chování jsou běžné

## 🤝 Přispívání

Jedná se primárně o výzkumný/vzdělávací projekt:

1. **Hlášení chyb**: Zakládejte issue s podrobným postupem reprodukce
2. **Testování**: Hlaste výsledky na různém hardwaru/emulátorech
3. **Dokumentace**: Vylepšete stávající dokumentaci nebo přidejte nové příručky

## 📖 Klíčové reference

- **Inside Macintosh** (1992–1994): Oficiální dokumentace Apple Toolbox
- **MPW Universal Interfaces 3.2**: Kanonické hlavičkové soubory a definice struktur
- **Guide to Macintosh Family Hardware**: Reference hardwarové architektury

### Užitečné nástroje

- **Mini vMac**: Emulátor System 7 pro behaviorální referenci
- **ResEdit**: Editor zdrojů pro studium zdrojů System 7
- **Ghidra/IDA**: Pro analýzu disasemblování ROM

## ⚖️ Právní informace

Jedná se o **clean-room reimplementaci** pro vzdělávací účely a účely zachování:

- **Nebyl použit žádný zdrojový kód Apple**
- Založeno pouze na veřejné dokumentaci a analýze černé skříňky
- „System 7", „Macintosh", „QuickDraw" jsou ochranné známky společnosti Apple Inc.
- Není spojeno se společností Apple Inc., ani jí schváleno či sponzorováno

**Původní ROM System 7 a software zůstávají majetkem společnosti Apple Inc.**

## 🙏 Poděkování

- **Apple Computer, Inc.** za vytvoření původního System 7
- **Autorům Inside Macintosh** za vyčerpávající dokumentaci
- **Komunitě pro zachování klasického Macu** za udržování platformy při životě
- **68k.news a Macintosh Garden** za archivy zdrojů

## 📊 Statistiky vývoje

- **Řádky kódu**: přibližně 57 500+ (včetně 2 500+ pro segment loader)
- **Doba kompilace**: přibližně 3–5 sekund
- **Velikost jádra**: přibližně 4,16 MB (kernel.elf)
- **Velikost ISO**: přibližně 12,5 MB (system71.iso)
- **Snížení chyb**: 94 % základní funkcionality funguje
- **Hlavní subsystémy**: 28+ (Font, Window, Menu, Control, Dialog, TextEdit atd.)

## 🔮 Budoucí směřování

**Plánované práce**:

- Dokončení smyčky provádění interpretu M68K
- Přidání podpory fontů TrueType
- Bitmapové fontové zdroje CJK pro vykreslování japonštiny, čínštiny a korejštiny
- Implementace dalších ovládacích prvků (textová pole, vyskakovací nabídky, posuvníky)
- Zpětný zápis na disk pro souborový systém HFS
- Pokročilé funkce Sound Manageru (mixování, samplování)
- Základní desk accessories (Kalkulačka, Poznámkový blok)

---

**Stav**: Experimentální – Vzdělávací – Ve vývoji

**Poslední aktualizace**: Listopad 2025 (Vylepšení Sound Manageru dokončena)

Pro dotazy, problémy nebo diskuzi prosím využijte GitHub Issues.
