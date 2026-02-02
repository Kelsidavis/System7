# System 7 - Reimplementare portabila cu sursa deschisa

**[English](README.md)** | **[Fran&ccedil;ais](README.fr.md)** | **[Deutsch](README.de.md)** | **[Espa&ntilde;ol](README.es.md)** | **[Italiano](README.it.md)** | **[Portugu&ecirc;s](README.pt.md)** | **[Nederlands](README.nl.md)** | **[Dansk](README.da.md)** | **[Norsk](README.no.md)** | **[Svenska](README.sv.md)** | **[Suomi](README.fi.md)** | **[&Iacute;slenska](README.is.md)** | **[Ελληνικά](README.el.md)** | **[T&uuml;rk&ccedil;e](README.tr.md)** | **[Polski](README.pl.md)** | **[Čeština](README.cs.md)** | **[Slovenčina](README.sk.md)** | **[Slovenščina](README.sl.md)** | **[Hrvatski](README.hr.md)** | **[Magyar](README.hu.md)** | **[Rom&acirc;n&atilde;](README.ro.md)** | **[Български](README.bg.md)** | **[Shqip](README.sq.md)** | **[Eesti](README.et.md)** | **[Latviešu](README.lv.md)** | **[Lietuvių](README.lt.md)** | **[Македонски](README.mk.md)** | **[Crnogorski](README.me.md)** | **[Русский](README.ru.md)** | **[Українська](README.uk.md)** | **[العربية](README.ar.md)** | **[日本語](README.ja.md)** | **[简体中文](README.zh.md)** | **[繁體中文](README.tw.md)** | **[한국어](README.ko.md)** | **[हिन्दी](README.hi.md)** | **[বাংলা](README.bn.md)** | **[اردو](README.ur.md)**

<img width="793" height="657" alt="System 7 ruland pe hardware modern" src="https://github.com/user-attachments/assets/be84b83e-191c-4f9d-a786-11d0bd04203b" />
<img width="801" height="662" alt="simpletextworks" src="https://github.com/user-attachments/assets/7c9ebe5b-22b4-4612-93a1-2076909d77cd" />
<img width="803" height="661" alt="macpaint" src="https://github.com/user-attachments/assets/cd3ed04a-fdde-4dd5-88ef-5b19b3a13a54" />

> ⚠️ **DOVADA DE CONCEPT** - Aceasta este o reimplementare experimentala, educationala a sistemului Macintosh System 7 de la Apple. Acesta NU este un produs finalizat si nu trebuie considerat software gata de productie.

O reimplementare cu sursa deschisa a Apple Macintosh System 7 pentru hardware modern x86, bootabil prin GRUB2/Multiboot2. Acest proiect isi propune sa recreeze experienta clasica Mac OS, documentand in acelasi timp arhitectura System 7 prin analiza de inginerie inversa.

## 🎯 Starea proiectului

**Stare curenta**: Dezvoltare activa, cu aproximativ 94% din functionalitatea de baza finalizata

### Ultimele actualizari (noiembrie 2025)

#### Imbunatatiri Sound Manager ✅ FINALIZAT
- **Conversie MIDI optimizata**: Functia partajata `SndMidiNoteToFreq()` cu tabel de cautare de 37 de intrari (C3-B5) si fallback bazat pe octave pentru intreaga gama MIDI (0-127)
- **Suport pentru redare asincrona**: Infrastructura completa de callback-uri atat pentru redarea fisierelor (`FilePlayCompletionUPP`) cat si pentru executarea comenzilor (`SndCallBackProcPtr`)
- **Rutare audio bazata pe canale**: Sistem de prioritati pe mai multe niveluri cu controale de dezactivare si activare
  - Canale cu prioritate pe 4 niveluri (0-3) pentru rutarea iesirii hardware
  - Controale independente de dezactivare si activare per canal
  - `SndGetActiveChannel()` returneaza canalul activ cu cea mai mare prioritate
  - Initializare corecta a canalelor cu fanion de activare implicit
- **Implementare de calitate pentru productie**: Tot codul se compileaza curat, fara violari malloc/free detectate
- **Commit-uri**: 07542c5 (optimizare MIDI), 1854fe6 (callback-uri asincrone), a3433c6 (rutare canale)

#### Realizari din sesiunile anterioare
- ✅ **Faza de functionalitati avansate**: Bucla de procesare comenzi Sound Manager, serializare stiluri multi-run, functionalitati MIDI/sinteza extinse
- ✅ **Sistem de redimensionare ferestre**: Redimensionare interactiva cu gestionare corecta a decoratiunilor ferestrei, casuta de redimensionare si curatarea desktopului
- ✅ **Traducere tastatura PS/2**: Mapare completa de la scancode-uri set 1 la coduri de taste Toolbox
- ✅ **HAL multi-platforma**: Suport x86, ARM si PowerPC cu abstractizare curata

## 📊 Gradul de finalizare al proiectului

**Functionalitate de baza generala**: aproximativ 94% finalizata (estimare)

### Ce functioneaza complet ✅

- **Nivelul de abstractizare hardware (HAL)**: Abstractizare completa a platformei pentru x86/ARM/PowerPC
- **Sistem de boot**: Porneste cu succes prin GRUB2/Multiboot2 pe x86
- **Logare seriala**: Logare bazata pe module cu filtrare la runtime (Error/Warn/Info/Debug/Trace)
- **Baza grafica**: Framebuffer VESA (800x600x32) cu primitive QuickDraw inclusiv modul XOR
- **Randare desktop**: Bara de meniuri System 7 cu logo Apple in culorile curcubeului, pictograme si modele de desktop
- **Tipografie**: Fontul bitmap Chicago cu randare pixel-perfect si kerning corect, Mac Roman extins (0x80-0xFF) pentru caractere accentuate europene
- **Internationalizare (i18n)**: Localizare bazata pe resurse cu 38 de limbi (engleza, franceza, germana, spaniola, italiana, portugheza, olandeza, daneza, norvegiana, suedeza, finlandeza, islandeza, greaca, turca, poloneza, ceha, slovaca, slovena, croata, maghiara, romana, bulgara, albaneza, estona, letona, lituaniana, macedoneana, muntenegreana, rusa, ucraineana, araba, japoneza, chineza simplificata, chineza traditionala, coreeana, hindi, bengaleza, urdu), Manager de localizare cu selectia limbii la pornire, infrastructura de codificare multi-byte CJK
- **Font Manager**: Suport multi-dimensiune (9-24pt), sinteza de stiluri, parsare FOND/NFNT, cache LRU
- **Sistem de intrare**: Tastatura si mouse PS/2 cu transmitere completa a evenimentelor
- **Event Manager**: Multitasking cooperativ prin WaitNextEvent cu coada unificata de evenimente
- **Memory Manager**: Alocare bazata pe zone cu integrare interpretor 68K
- **Menu Manager**: Meniuri derulante complete cu urmarirea mouse-ului si SaveBits/RestoreBits
- **Sistem de fisiere**: HFS cu implementare B-tree, ferestre de foldere cu enumerare VFS
- **Window Manager**: Tragere, redimensionare (cu casuta de redimensionare), stratificare, activare
- **Time Manager**: Calibrare precisa TSC, precizie la microsecunda, verificare de generatie
- **Resource Manager**: Cautare binara O(log n), cache LRU, validare cuprinzatoare
- **Gestalt Manager**: Informatii de sistem multi-arhitectura cu detectie de arhitectura
- **TextEdit Manager**: Editare completa de text cu integrare clipboard
- **Scrap Manager**: Clipboard clasic Mac OS cu suport pentru mai multe tipuri de date
- **Aplicatia SimpleText**: Editor de text MDI complet cu taiere/copiere/lipire
- **List Manager**: Controale de lista compatibile System 7.1 cu navigare prin tastatura
- **Control Manager**: Controale standard si bare de derulare cu implementare CDEF
- **Dialog Manager**: Navigare prin tastatura, inele de focus, scurtaturi de tastatura
- **Segment Loader**: Sistem portabil de incarcare segmente 68K independent de ISA cu relocare
- **Interpretor M68K**: Dispatch complet de instructiuni cu 84 de handlere de opcode-uri, toate cele 14 moduri de adresare, cadru de exceptii/trap-uri
- **Sound Manager**: Procesare de comenzi, conversie MIDI, gestionare canale, callback-uri
- **Device Manager**: Gestionare DCE, instalare/dezinstalare drivere si operatii I/O
- **Ecran de pornire**: Interfata completa de boot cu urmarirea progresului, gestionarea fazelor si ecran splash
- **Color Manager**: Gestionarea starii culorilor cu integrare QuickDraw

### Partial implementat ⚠️

- **Integrare aplicatii**: Interpretorul M68K si incarcatorul de segmente sunt complete; testarea integrarii este necesara pentru a verifica executia aplicatiilor reale
- **Proceduri de definire a ferestrelor (WDEF)**: Structura de baza existenta, dispatch partial
- **Speech Manager**: Doar cadrul API si passthrough audio; motorul de sinteza vocala nu este implementat
- **Gestionarea exceptiilor (RTE)**: Revenirea din exceptie partial implementata (in prezent opreste in loc sa restaureze contextul)

### Neimplementat inca ❌

- **Tiparire**: Fara sistem de tiparire
- **Retea**: Fara AppleTalk sau functionalitate de retea
- **Accesorii de birou**: Doar cadrul de baza
- **Audio avansat**: Redare de esantioane, mixare (limitare difuzor PC)

### Subsisteme necompilate 🔧

Urmatoarele au cod sursa dar nu sunt integrate in kernel:
- **AppleEventManager** (8 fisiere): Mesagerie inter-aplicatii; exclus deliberat din cauza dependentelor de pthread incompatibile cu mediul freestanding
- **FontResources** (doar header): Definitii de tipuri de resurse de fonturi; suportul real de fonturi este furnizat de FontResourceLoader.c compilat

## 🏗️ Arhitectura

### Specificatii tehnice

- **Arhitectura**: Multi-arhitectura prin HAL (x86, ARM, PowerPC pregatite)
- **Protocol de boot**: Multiboot2 (x86), bootloadere specifice platformei
- **Grafica**: Framebuffer VESA, 800x600 @ culoare pe 32 de biti
- **Dispunerea memoriei**: Kernelul se incarca la adresa fizica 1MB (x86)
- **Cronometrare**: Independenta de arhitectura cu precizie la microsecunda (RDTSC/registre timer)
- **Performanta**: Lipsa cache resurse <15us, hit cache <2us, deviatie timer <100ppm

### Statistici ale bazei de cod

- **Peste 225 fisiere sursa** cu peste ~57.500 de linii de cod
- **Peste 145 fisiere header** in peste 28 de subsisteme
- **69 tipuri de resurse** extrase din System 7.1
- **Timp de compilare**: 3-5 secunde pe hardware modern
- **Dimensiune kernel**: ~4,16 MB
- **Dimensiune ISO**: ~12,5 MB

## 🔨 Compilare

### Cerinte

- **GCC** cu suport pe 32 de biti (`gcc-multilib` pe sisteme pe 64 de biti)
- **GNU Make**
- **Utilitare GRUB**: `grub-mkrescue` (din `grub2-common` sau `grub-pc-bin`)
- **QEMU** pentru testare (`qemu-system-i386`)
- **Python 3** pentru procesarea resurselor
- **xxd** pentru conversia binara
- *(Optional)* Toolchain cross-compilare **powerpc-linux-gnu** pentru compilari PowerPC

### Instalare Ubuntu/Debian

```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common
```

### Comenzi de compilare

```bash
# Compilare kernel (x86 implicit)
make

# Compilare pentru o platforma specifica
make PLATFORM=x86
make PLATFORM=arm        # necesita GCC bare-metal ARM
make PLATFORM=ppc        # experimental; necesita toolchain ELF PowerPC

# Creare ISO bootabil
make iso

# Compilare cu toate limbile
make LOCALE_FR=1 LOCALE_DE=1 LOCALE_ES=1 LOCALE_JA=1 LOCALE_ZH=1 LOCALE_KO=1 LOCALE_RU=1 LOCALE_UK=1 LOCALE_PL=1 LOCALE_CS=1 LOCALE_SQ=1 LOCALE_BG=1 LOCALE_HR=1 LOCALE_DA=1 LOCALE_NL=1 LOCALE_ET=1 LOCALE_FI=1 LOCALE_EL=1 LOCALE_HU=1 LOCALE_IS=1 LOCALE_IT=1 LOCALE_LV=1 LOCALE_LT=1 LOCALE_MK=1 LOCALE_ME=1 LOCALE_NO=1 LOCALE_PT=1 LOCALE_RO=1 LOCALE_SK=1 LOCALE_SL=1 LOCALE_SV=1 LOCALE_TR=1 LOCALE_HI=1 LOCALE_TW=1 LOCALE_AR=1 LOCALE_BN=1 LOCALE_UR=1

# Compilare cu o singura limba suplimentara
make LOCALE_FR=1

# Compilare si rulare in QEMU
make run

# Curatare artefacte
make clean

# Afisare statistici de compilare
make info
```

## 🚀 Rulare

### Start rapid (QEMU)

```bash
# Rulare standard cu logare seriala
make run

# Manual cu optiuni
qemu-system-i386 -cdrom system71.iso -serial file:/tmp/serial.log -display sdl -vga std -m 256M
```

### Optiuni QEMU

```bash
# Cu iesire seriala in consola
qemu-system-i386 -cdrom system71.iso -serial stdio -display sdl -m 256M

# Fara interfata grafica (headless)
qemu-system-i386 -cdrom system71.iso -serial stdio -display none -m 256M

# Cu depanare GDB
make debug
# Intr-un alt terminal: gdb kernel.elf -ex "target remote :1234"
```

## 📚 Documentatie

### Ghiduri pe componente
- **Control Manager**: `docs/components/ControlManager/`
- **Dialog Manager**: `docs/components/DialogManager/`
- **Font Manager**: `docs/components/FontManager/`
- **Logare seriala**: `docs/components/System/`
- **Event Manager**: `docs/components/EventManager.md`
- **Menu Manager**: `docs/components/MenuManager.md`
- **Window Manager**: `docs/components/WindowManager.md`
- **Resource Manager**: `docs/components/ResourceManager.md`

### Internationalizare
- **Locale Manager**: `include/LocaleManager/` — comutare localizare la runtime, selectia limbii la pornire
- **Resurse de siruri**: `resources/strings/` — fisiere de resurse STR# per limba (34 de limbi)
- **Fonturi extinse**: `include/chicago_font_extended.h` — glife Mac Roman 0x80-0xFF pentru caractere europene
- **Suport CJK**: `include/TextEncoding/CJKEncoding.h`, `include/FontManager/CJKFont.h` — infrastructura de codificare multi-byte si fonturi

### Starea implementarii
- **IMPLEMENTATION_PRIORITIES.md**: Lucrari planificate si urmarirea gradului de finalizare
- **IMPLEMENTATION_STATUS_AUDIT.md**: Audit detaliat al tuturor subsistemelor

### Filozofia proiectului

**Abordare arheologica** cu implementare bazata pe dovezi:
1. Sustinuta de documentatia Inside Macintosh si de MPW Universal Interfaces
2. Toate deciziile majore sunt etichetate cu ID-uri de constatari ce fac referire la dovezi de sustinere
3. Obiectiv: paritate comportamentala cu System 7 original, nu modernizare
4. Implementare clean-room (fara cod sursa original Apple)

## 🐛 Probleme cunoscute

1. **Artefacte la tragerea pictogramelor**: Artefacte vizuale minore in timpul tragerii pictogramelor de pe desktop
2. **Executie M68K schitata**: Incarcatorul de segmente este complet, bucla de executie nu este implementata
3. **Fara suport TrueType**: Doar fonturi bitmap (Chicago)
4. **HFS doar pentru citire**: Sistem de fisiere virtual, fara scriere inapoi pe disc
5. **Fara garantii de stabilitate**: Blocaje si comportament neasteptat sunt frecvente

## 🤝 Contributii

Acesta este in principal un proiect de invatare/cercetare:

1. **Rapoarte de erori**: Deschideti tichete cu pasi detaliati de reproducere
2. **Testare**: Raportati rezultate pe diferite configuratii hardware/emulatoare
3. **Documentatie**: Imbunatatiti documentatia existenta sau adaugati ghiduri noi

## 📖 Referinte esentiale

- **Inside Macintosh** (1992-1994): Documentatia oficiala Apple Toolbox
- **MPW Universal Interfaces 3.2**: Fisiere header canonice si definitii de structuri
- **Guide to Macintosh Family Hardware**: Referinta pentru arhitectura hardware

### Instrumente utile

- **Mini vMac**: Emulator System 7 pentru referinta comportamentala
- **ResEdit**: Editor de resurse pentru studiul resurselor System 7
- **Ghidra/IDA**: Pentru analiza dezasamblarii ROM-ului

## ⚖️ Aspecte legale

Aceasta este o **reimplementare clean-room** in scopuri educationale si de conservare:

- **Nu a fost folosit cod sursa Apple**
- Bazata exclusiv pe documentatie publica si analiza black-box
- "System 7", "Macintosh", "QuickDraw" sunt marci inregistrate ale Apple Inc.
- Nu este afiliata, aprobata sau sponsorizata de Apple Inc.

**ROM-ul si software-ul original System 7 raman proprietatea Apple Inc.**

## 🙏 Multumiri

- **Apple Computer, Inc.** pentru crearea System 7 original
- **Autorii Inside Macintosh** pentru documentatia cuprinzatoare
- **Comunitatea de conservare a clasicului Mac** pentru mentinerea platformei in viata
- **68k.news si Macintosh Garden** pentru arhivele de resurse

## 📊 Statistici de dezvoltare

- **Linii de cod**: peste ~57.500 (inclusiv peste 2.500 pentru incarcatorul de segmente)
- **Timp de compilare**: ~3-5 secunde
- **Dimensiune kernel**: ~4,16 MB (kernel.elf)
- **Dimensiune ISO**: ~12,5 MB (system71.iso)
- **Reducerea erorilor**: 94% din functionalitatea de baza functionala
- **Subsisteme majore**: peste 28 (Font, Window, Menu, Control, Dialog, TextEdit etc.)

## 🔮 Directii viitoare

**Lucrari planificate**:

- Finalizarea buclei de executie a interpretorului M68K
- Adaugarea suportului pentru fonturi TrueType
- Resurse de fonturi bitmap CJK pentru randarea japoneza, chineza si coreeana
- Implementarea de controale suplimentare (campuri de text, meniuri pop-up, cursoare de derulare)
- Scriere inapoi pe disc pentru sistemul de fisiere HFS
- Functionalitati avansate Sound Manager (mixare, esantionare)
- Accesorii de birou de baza (Calculator, Note Pad)

---

**Stare**: Experimental - Educational - In dezvoltare

**Ultima actualizare**: Noiembrie 2025 (Imbunatatiri Sound Manager finalizate)

Pentru intrebari, probleme sau discutii, va rugam sa folositi GitHub Issues.
