# System 7 - Hordozható, nyílt forrású újraimplementáció

**[English](README.md)** | **[Fran&ccedil;ais](README.fr.md)** | **[Deutsch](README.de.md)** | **[Espa&ntilde;ol](README.es.md)** | **[Italiano](README.it.md)** | **[Portugu&ecirc;s](README.pt.md)** | **[Nederlands](README.nl.md)** | **[Dansk](README.da.md)** | **[Norsk](README.no.md)** | **[Svenska](README.sv.md)** | **[Suomi](README.fi.md)** | **[&Iacute;slenska](README.is.md)** | **[Ελληνικά](README.el.md)** | **[T&uuml;rk&ccedil;e](README.tr.md)** | **[Polski](README.pl.md)** | **[Čeština](README.cs.md)** | **[Slovenčina](README.sk.md)** | **[Slovenščina](README.sl.md)** | **[Hrvatski](README.hr.md)** | **[Magyar](README.hu.md)** | **[Rom&acirc;n&atilde;](README.ro.md)** | **[Български](README.bg.md)** | **[Shqip](README.sq.md)** | **[Eesti](README.et.md)** | **[Latviešu](README.lv.md)** | **[Lietuvių](README.lt.md)** | **[Македонски](README.mk.md)** | **[Crnogorski](README.me.md)** | **[Русский](README.ru.md)** | **[Українська](README.uk.md)** | **[العربية](README.ar.md)** | **[日本語](README.ja.md)** | **[简体中文](README.zh.md)** | **[繁體中文](README.tw.md)** | **[한국어](README.ko.md)** | **[हिन्दी](README.hi.md)** | **[বাংলা](README.bn.md)** | **[اردو](README.ur.md)**

<img width="793" height="657" alt="System 7 modern hardveren futtatva" src="https://github.com/user-attachments/assets/be84b83e-191c-4f9d-a786-11d0bd04203b" />
<img width="801" height="662" alt="simpletextworks" src="https://github.com/user-attachments/assets/7c9ebe5b-22b4-4612-93a1-2076909d77cd" />
<img width="803" height="661" alt="macpaint" src="https://github.com/user-attachments/assets/cd3ed04a-fdde-4dd5-88ef-5b19b3a13a54" />

> ⚠️ **KONCEPCIÓBIZONYÍTÉK** - Ez az Apple Macintosh System 7 kísérleti, oktatási célú újraimplementációja. Ez NEM egy kész termék, és nem tekinthető éles használatra kész szoftvernek.

Az Apple Macintosh System 7 nyílt forrású újraimplementációja modern x86 hardverre, amely GRUB2/Multiboot2 segítségével indítható. A projekt célja a klasszikus Mac OS élmény újrateremtése, miközben a System 7 architektúráját visszafejtési elemzéssel dokumentálja.

## 🎯 Projekt állapota

**Jelenlegi állapot**: Aktív fejlesztés, az alapfunkciók ~94%-a kész

### Legfrissebb változások (2025. november)

#### Sound Manager fejlesztések ✅ KÉSZ
- **Optimalizált MIDI konverzió**: Megosztott `SndMidiNoteToFreq()` segédfüggvény 37 bejegyzéses keresőtáblával (C3-B5) és oktáv alapú tartalék megoldással a teljes MIDI tartományra (0-127)
- **Aszinkron lejátszás támogatás**: Teljes callback infrastruktúra fájllejátszáshoz (`FilePlayCompletionUPP`) és parancsvégrehajtáshoz (`SndCallBackProcPtr`)
- **Csatornaalapú hangútvonalválasztás**: Többszintű prioritási rendszer némítási és engedélyezési vezérlőkkel
  - 4 szintű prioritásos csatornák (0-3) hardver kimeneti útvonalválasztáshoz
  - Független némítási és engedélyezési vezérlők csatornánként
  - A `SndGetActiveChannel()` a legmagasabb prioritású aktív csatornát adja vissza
  - Megfelelő csatornainicializálás alapértelmezetten engedélyezett jelzővel
- **Termelési minőségű implementáció**: Minden kód tisztán fordítható, nincs malloc/free szabálysértés
- **Commitok**: 07542c5 (MIDI optimalizálás), 1854fe6 (aszinkron callbackek), a3433c6 (csatornaútvonalválasztás)

#### Korábbi munkamenet eredményei
- ✅ **Haladó funkciók fázisa**: Sound Manager parancsfeldolgozó hurok, többszörös futtatási stílus szerializáció, kiterjesztett MIDI/szintézis funkciók
- ✅ **Ablakátméretezési rendszer**: Interaktív átméretezés megfelelő krómkezeléssel, növekedési mezővel és asztaltisztítással
- ✅ **PS/2 billentyűzetfordítás**: Teljes 1-es készlet szkenkód-leképezés Toolbox billentyűkódokra
- ✅ **Többplatformos HAL**: x86, ARM és PowerPC támogatás tiszta absztrakcióval

## 📊 Projekt teljesség

**Teljes alapfunkcionalitás**: ~94% kész (becsült)

### Teljesen működő ✅

- **Hardver absztrakciós réteg (HAL)**: Teljes platformabsztrakció x86/ARM/PowerPC rendszerekhez
- **Rendszerindító**: Sikeresen indul GRUB2/Multiboot2 segítségével x86-on
- **Soros naplózás**: Modulalapú naplózás futásidejű szűréssel (Error/Warn/Info/Debug/Trace)
- **Grafikai alapok**: VESA framebuffer (800x600x32) QuickDraw primitívekkel, beleértve az XOR módot
- **Asztali megjelenítés**: System 7 menüsor szivárványos Apple logóval, ikonokkal és asztali mintákkal
- **Tipográfia**: Chicago bittérképes betűtípus pixelpontos megjelenítéssel és megfelelő betűközzel, kiterjesztett Mac Roman (0x80-0xFF) európai ékezetes karakterekhez
- **Nemzetköziesítés (i18n)**: Erőforrás-alapú lokalizáció 38 nyelven (angol, francia, német, spanyol, olasz, portugál, holland, dán, norvég, svéd, finn, izlandi, görög, török, lengyel, cseh, szlovák, szlovén, horvát, magyar, román, bolgár, albán, észt, lett, litván, macedón, montenegrói, orosz, ukrán, arab, japán, egyszerűsített kínai, hagyományos kínai, koreai, hindi, bengáli, urdu), Locale Manager rendszerindításkori nyelvválasztással, CJK többbájtos kódolási infrastruktúra
- **Font Manager**: Többméretű támogatás (9-24pt), stílusszintézis, FOND/NFNT elemzés, LRU gyorsítótárazás
- **Beviteli rendszer**: PS/2 billentyűzet és egér teljes eseménytovábbítással
- **Event Manager**: Kooperatív többfeladatos működés WaitNextEvent segítségével egységes eseménysorral
- **Memory Manager**: Zónaalapú memóriafoglalás 68K értelmező integrációval
- **Menu Manager**: Teljes legördülő menük egérkövetéssel és SaveBits/RestoreBits funkciókkal
- **Fájlrendszer**: HFS B-fa implementációval, mappanézetek VFS felsorolással
- **Window Manager**: Húzás, átméretezés (növekedési mezővel), rétegzés, aktiválás
- **Time Manager**: Pontos TSC kalibrálás, mikroszekundumos pontosság, generációellenőrzés
- **Resource Manager**: O(log n) bináris keresés, LRU gyorsítótár, átfogó érvényesítés
- **Gestalt Manager**: Többarchitektúrás rendszerinformáció architektúraérzékeléssel
- **TextEdit Manager**: Teljes szövegszerkesztés vágólapintegrációval
- **Scrap Manager**: Klasszikus Mac OS vágólap többféle formátumtámogatással
- **SimpleText alkalmazás**: Teljes funkcionalitású MDI szövegszerkesztő kivágás/másolás/beillesztés funkciókkal
- **List Manager**: System 7.1-kompatibilis listakezelők billentyűzetnavigációval
- **Control Manager**: Szabványos és görgetősáv vezérlők CDEF implementációval
- **Dialog Manager**: Billentyűzetnavigáció, fókuszgyűrűk, billentyűparancsok
- **Segment Loader**: Hordozható, ISA-független 68K szegmensbetöltő rendszer relokációval
- **M68K értelmező**: Teljes utasításdiszpécser 84 opkódkezelővel, mind a 14 címzési móddal, kivétel/trap keretrendszerrel
- **Sound Manager**: Parancsfeldolgozás, MIDI konverzió, csatornakezelés, callbackek
- **Device Manager**: DCE kezelés, meghajtó telepítés/eltávolítás és I/O műveletek
- **Indítóképernyő**: Teljes rendszerindítási felhasználói felület haladáskövetéssel, fáziskezeléssel és üdvözlőképernyővel
- **Color Manager**: Színállapot-kezelés QuickDraw integrációval

### Részlegesen implementált ⚠️

- **Alkalmazásintegráció**: Az M68K értelmező és a szegmensbetöltő kész; integrációs tesztelés szükséges a valós alkalmazások futtatásának ellenőrzéséhez
- **Ablakdefiníciós eljárások (WDEF)**: Az alapstruktúra megvan, részleges diszpécser
- **Speech Manager**: Csak API keretrendszer és hangáteresztés; beszédszintézis motor nincs implementálva
- **Kivételkezelés (RTE)**: A kivételből való visszatérés részlegesen implementált (jelenleg megáll a kontextus visszaállítása helyett)

### Még nincs implementálva ❌

- **Nyomtatás**: Nincs nyomtatási rendszer
- **Hálózatkezelés**: Nincs AppleTalk vagy hálózati funkció
- **Asztali kiegészítők**: Csak keretrendszer
- **Haladó hangkezelés**: Mintavisszajátszás, keverés (PC hangszóró korlátok)

### Nem fordított alrendszerek 🔧

A következőknek van forráskódjuk, de nincsenek integrálva a kernelbe:
- **AppleEventManager** (8 fájl): Alkalmazások közötti üzenetkezelés; szándékosan kizárva a pthread függőségek miatt, amelyek nem kompatibilisek az önálló környezettel
- **FontResources** (csak fejlécfájl): Betűtípus-erőforrás típusdefiníciók; a tényleges betűtípus-támogatást a fordított FontResourceLoader.c biztosítja

## 🏗️ Architektúra

### Műszaki specifikációk

- **Architektúra**: Többarchitektúrás HAL-on keresztül (x86, ARM, PowerPC kész)
- **Rendszerindítási protokoll**: Multiboot2 (x86), platformspecifikus rendszertöltők
- **Grafika**: VESA framebuffer, 800x600 @ 32 bites szín
- **Memóriaelrendezés**: A kernel 1 MB fizikai címre töltődik (x86)
- **Időzítés**: Architektúrafüggetlen mikroszekundumos pontossággal (RDTSC/időzítő regiszterek)
- **Teljesítmény**: Hideg erőforrás-tévesztés <15µs, gyorsítótár-találat <2µs, időzítő eltérés <100ppm

### Kódbázis statisztikák

- **225+ forrásfájl** ~57 500+ kódsorral
- **145+ fejlécfájl** 28+ alrendszerben
- **69 erőforrástípus** a System 7.1-ből kinyerve
- **Fordítási idő**: 3-5 másodperc modern hardveren
- **Kernel mérete**: ~4,16 MB
- **ISO mérete**: ~12,5 MB

## 🔨 Fordítás

### Követelmények

- **GCC** 32 bites támogatással (`gcc-multilib` 64 bites rendszeren)
- **GNU Make**
- **GRUB eszközök**: `grub-mkrescue` (a `grub2-common` vagy `grub-pc-bin` csomagból)
- **QEMU** teszteléshez (`qemu-system-i386`)
- **Python 3** erőforrás-feldolgozáshoz
- **xxd** bináris konverzióhoz
- *(Opcionális)* **powerpc-linux-gnu** keresztfordító eszközkészlet PowerPC buildekhez

### Ubuntu/Debian telepítés

```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common
```

### Fordítási parancsok

```bash
# Kernel fordítása (alapértelmezetten x86)
make

# Fordítás adott platformra
make PLATFORM=x86
make PLATFORM=arm        # ARM bare-metal GCC szükséges
make PLATFORM=ppc        # kísérleti; PowerPC ELF eszközkészlet szükséges

# Indítható ISO létrehozása
make iso

# Fordítás az összes nyelvvel
make LOCALE_FR=1 LOCALE_DE=1 LOCALE_ES=1 LOCALE_JA=1 LOCALE_ZH=1 LOCALE_KO=1 LOCALE_RU=1 LOCALE_UK=1 LOCALE_PL=1 LOCALE_CS=1 LOCALE_SQ=1 LOCALE_BG=1 LOCALE_HR=1 LOCALE_DA=1 LOCALE_NL=1 LOCALE_ET=1 LOCALE_FI=1 LOCALE_EL=1 LOCALE_HU=1 LOCALE_IS=1 LOCALE_IT=1 LOCALE_LV=1 LOCALE_LT=1 LOCALE_MK=1 LOCALE_ME=1 LOCALE_NO=1 LOCALE_PT=1 LOCALE_RO=1 LOCALE_SK=1 LOCALE_SL=1 LOCALE_SV=1 LOCALE_TR=1 LOCALE_HI=1 LOCALE_TW=1 LOCALE_AR=1 LOCALE_BN=1 LOCALE_UR=1

# Fordítás egyetlen további nyelvvel
make LOCALE_FR=1

# Fordítás és futtatás QEMU-ban
make run

# Fordítási eredmények törlése
make clean

# Fordítási statisztikák megjelenítése
make info
```

## 🚀 Futtatás

### Gyorsindítás (QEMU)

```bash
# Szabványos futtatás soros naplózással
make run

# Kézi futtatás beállításokkal
qemu-system-i386 -cdrom system71.iso -serial file:/tmp/serial.log -display sdl -vga std -m 256M
```

### QEMU beállítások

```bash
# Konzolos soros kimenettel
qemu-system-i386 -cdrom system71.iso -serial stdio -display sdl -m 256M

# Fejnélküli mód (grafikus megjelenítés nélkül)
qemu-system-i386 -cdrom system71.iso -serial stdio -display none -m 256M

# GDB hibakereséssel
make debug
# Egy másik terminálban: gdb kernel.elf -ex "target remote :1234"
```

## 📚 Dokumentáció

### Komponens útmutatók
- **Control Manager**: `docs/components/ControlManager/`
- **Dialog Manager**: `docs/components/DialogManager/`
- **Font Manager**: `docs/components/FontManager/`
- **Soros naplózás**: `docs/components/System/`
- **Event Manager**: `docs/components/EventManager.md`
- **Menu Manager**: `docs/components/MenuManager.md`
- **Window Manager**: `docs/components/WindowManager.md`
- **Resource Manager**: `docs/components/ResourceManager.md`

### Nemzetköziesítés
- **Locale Manager**: `include/LocaleManager/` — futásidejű nyelvváltás, rendszerindításkori nyelvválasztás
- **Karakterlánc-erőforrások**: `resources/strings/` — nyelvenkénti STR# erőforrásfájlok (34 nyelv)
- **Kiterjesztett betűtípusok**: `include/chicago_font_extended.h` — Mac Roman 0x80-0xFF karakterjelek európai karakterekhez
- **CJK támogatás**: `include/TextEncoding/CJKEncoding.h`, `include/FontManager/CJKFont.h` — többbájtos kódolás és betűtípus-infrastruktúra

### Implementációs állapot
- **IMPLEMENTATION_PRIORITIES.md**: Tervezett munkák és készültségi nyomonkövetés
- **IMPLEMENTATION_STATUS_AUDIT.md**: Minden alrendszer részletes auditja

### Projekt filozófia

**Régészeti megközelítés** bizonyítékalapú implementációval:
1. Az Inside Macintosh dokumentáció és az MPW Universal Interfaces támasztja alá
2. Minden fontosabb döntés Finding ID-kkel van jelölve, amelyek alátámasztó bizonyítékokra hivatkoznak
3. Cél: viselkedési azonosság az eredeti System 7-tel, nem modernizáció
4. Tisztaszobás implementáció (eredeti Apple forráskód nélkül)

## 🐛 Ismert problémák

1. **Ikonhúzási műtermékek**: Kisebb vizuális hibák asztali ikonok húzásakor
2. **M68K végrehajtás csonkolt**: A szegmensbetöltő kész, a végrehajtási hurok nincs implementálva
3. **Nincs TrueType támogatás**: Csak bittérképes betűtípusok (Chicago)
4. **HFS csak olvasható**: Virtuális fájlrendszer, valós lemezvisszaírás nélkül
5. **Nincs stabilitási garancia**: Összeomlások és váratlan viselkedés gyakori

## 🤝 Közreműködés

Ez elsősorban egy tanulási/kutatási projekt:

1. **Hibajelentések**: Nyisson hibajegyet részletes reprodukciós lépésekkel
2. **Tesztelés**: Jelezze eredményeit különböző hardvereken/emulátorokon
3. **Dokumentáció**: Javítsa a meglévő dokumentációt vagy írjon új útmutatókat

## 📖 Alapvető hivatkozások

- **Inside Macintosh** (1992-1994): Az Apple hivatalos Toolbox dokumentációja
- **MPW Universal Interfaces 3.2**: Kanonikus fejlécfájlok és struktúradefiníciók
- **Guide to Macintosh Family Hardware**: Hardverarchitektúra-referencia

### Hasznos eszközök

- **Mini vMac**: System 7 emulátor viselkedési referenciaként
- **ResEdit**: Erőforrás-szerkesztő a System 7 erőforrások tanulmányozásához
- **Ghidra/IDA**: ROM visszafejtési elemzéshez

## ⚖️ Jogi nyilatkozat

Ez egy **tisztaszobás újraimplementáció** oktatási és megőrzési célokra:

- **Nem használtunk Apple forráskódot**
- Kizárólag nyilvános dokumentáción és feketedobozos elemzésen alapul
- A „System 7", „Macintosh", „QuickDraw" az Apple Inc. védjegyei
- Nem áll kapcsolatban az Apple Inc.-vel, nem az ő jóváhagyásával vagy támogatásával készült

**Az eredeti System 7 ROM és szoftver az Apple Inc. tulajdona marad.**

## 🙏 Köszönetnyilvánítás

- **Apple Computer, Inc.** az eredeti System 7 megalkotásáért
- **Az Inside Macintosh szerzői** az átfogó dokumentációért
- **A klasszikus Mac megőrzési közösség** a platform életben tartásáért
- **68k.news és Macintosh Garden** az erőforrás-archívumokért

## 📊 Fejlesztési statisztikák

- **Kódsorok száma**: ~57 500+ (beleértve 2 500+ sort a szegmensbetöltőhöz)
- **Fordítási idő**: ~3-5 másodperc
- **Kernel mérete**: ~4,16 MB (kernel.elf)
- **ISO mérete**: ~12,5 MB (system71.iso)
- **Hibacsökkentés**: Az alapfunkciók 94%-a működik
- **Főbb alrendszerek**: 28+ (Font, Window, Menu, Control, Dialog, TextEdit stb.)

## 🔮 Jövőbeli irányok

**Tervezett munkák**:

- Az M68K értelmező végrehajtási hurkának befejezése
- TrueType betűtípus-támogatás hozzáadása
- CJK bittérképes betűtípus-erőforrások japán, kínai és koreai megjelenítéshez
- További vezérlők implementálása (szövegmezők, felugró menük, csúszkák)
- Lemezvisszaírás a HFS fájlrendszerhez
- Haladó Sound Manager funkciók (keverés, mintavételezés)
- Alapvető asztali kiegészítők (Számológép, Jegyzettömb)

---

**Állapot**: Kísérleti - Oktatási - Fejlesztés alatt

**Utolsó frissítés**: 2025. november (Sound Manager fejlesztések befejezve)

Kérdésekkel, problémákkal vagy megbeszéléshez kérjük, használja a GitHub Issues felületet.
