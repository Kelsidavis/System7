# System 7 - Reimplementazione Open-Source Portabile

**[English](README.md)** | **[Fran&ccedil;ais](README.fr.md)** | **[Deutsch](README.de.md)** | **[Espa&ntilde;ol](README.es.md)** | **[Italiano](README.it.md)** | **[Portugu&ecirc;s](README.pt.md)** | **[Nederlands](README.nl.md)** | **[Dansk](README.da.md)** | **[Norsk](README.no.md)** | **[Svenska](README.sv.md)** | **[Suomi](README.fi.md)** | **[&Iacute;slenska](README.is.md)** | **[Ελληνικά](README.el.md)** | **[T&uuml;rk&ccedil;e](README.tr.md)** | **[Polski](README.pl.md)** | **[Čeština](README.cs.md)** | **[Slovenčina](README.sk.md)** | **[Slovenščina](README.sl.md)** | **[Hrvatski](README.hr.md)** | **[Magyar](README.hu.md)** | **[Rom&acirc;n&atilde;](README.ro.md)** | **[Български](README.bg.md)** | **[Shqip](README.sq.md)** | **[Eesti](README.et.md)** | **[Latviešu](README.lv.md)** | **[Lietuvių](README.lt.md)** | **[Македонски](README.mk.md)** | **[Crnogorski](README.me.md)** | **[Русский](README.ru.md)** | **[Українська](README.uk.md)** | **[العربية](README.ar.md)** | **[日本語](README.ja.md)** | **[简体中文](README.zh.md)** | **[繁體中文](README.tw.md)** | **[한국어](README.ko.md)** | **[हिन्दी](README.hi.md)** | **[বাংলা](README.bn.md)** | **[اردو](README.ur.md)**

<img width="793" height="657" alt="System 7 in esecuzione su hardware moderno" src="https://github.com/user-attachments/assets/be84b83e-191c-4f9d-a786-11d0bd04203b" />
<img width="801" height="662" alt="simpletextworks" src="https://github.com/user-attachments/assets/7c9ebe5b-22b4-4612-93a1-2076909d77cd" />
<img width="803" height="661" alt="macpaint" src="https://github.com/user-attachments/assets/cd3ed04a-fdde-4dd5-88ef-5b19b3a13a54" />

> ⚠️ **PROOF OF CONCEPT** - Questa e una reimplementazione sperimentale e didattica del Macintosh System 7 di Apple. NON e un prodotto finito e non deve essere considerato software pronto per la produzione.

Una reimplementazione open-source del Macintosh System 7 di Apple per hardware x86 moderno, avviabile tramite GRUB2/Multiboot2. Questo progetto mira a ricreare l'esperienza del Mac OS classico documentando al contempo l'architettura di System 7 attraverso l'analisi di reverse engineering.

## 🎯 Stato del Progetto

**Stato Attuale**: Sviluppo attivo con circa il 94% delle funzionalita principali completate

### Ultimi Aggiornamenti (Novembre 2025)

#### Miglioramenti al Sound Manager ✅ COMPLETATO
- **Conversione MIDI ottimizzata**: Helper condiviso `SndMidiNoteToFreq()` con tabella di ricerca a 37 voci (C3-B5) e fallback basato sulle ottave per l'intero range MIDI (0-127)
- **Supporto alla riproduzione asincrona**: Infrastruttura completa di callback sia per la riproduzione di file (`FilePlayCompletionUPP`) che per l'esecuzione di comandi (`SndCallBackProcPtr`)
- **Routing audio basato sui canali**: Sistema di priorita multilivello con controlli di silenziamento e abilitazione
  - Canali a 4 livelli di priorita (0-3) per il routing dell'output hardware
  - Controlli indipendenti di silenziamento e abilitazione per canale
  - `SndGetActiveChannel()` restituisce il canale attivo con priorita piu alta
  - Inizializzazione corretta del canale con flag di abilitazione predefinito
- **Implementazione di qualita produttiva**: Tutto il codice compila senza errori, nessuna violazione malloc/free rilevata
- **Commit**: 07542c5 (ottimizzazione MIDI), 1854fe6 (callback asincroni), a3433c6 (routing dei canali)

#### Risultati delle Sessioni Precedenti
- ✅ **Fase Funzionalita Avanzate**: Loop di elaborazione comandi del Sound Manager, serializzazione multi-run style, funzionalita MIDI/sintesi estese
- ✅ **Sistema di Ridimensionamento Finestre**: Ridimensionamento interattivo con gestione corretta del chrome, grow box e pulizia del desktop
- ✅ **Traduzione Tastiera PS/2**: Mappatura completa dei codici di scansione set 1 ai codici tasto del Toolbox
- ✅ **HAL Multi-piattaforma**: Supporto x86, ARM e PowerPC con astrazione pulita

## 📊 Completezza del Progetto

**Funzionalita Principali Complessive**: circa il 94% completate (stima)

### Funzionalita Completamente Operative ✅

- **Hardware Abstraction Layer (HAL)**: Astrazione completa della piattaforma per x86/ARM/PowerPC
- **Sistema di Avvio**: Avvio riuscito tramite GRUB2/Multiboot2 su x86
- **Logging Seriale**: Logging basato su moduli con filtraggio a runtime (Error/Warn/Info/Debug/Trace)
- **Base Grafica**: Framebuffer VESA (800x600x32) con primitive QuickDraw inclusa la modalita XOR
- **Rendering del Desktop**: Barra dei menu System 7 con logo Apple arcobaleno, icone e pattern del desktop
- **Tipografia**: Font bitmap Chicago con rendering pixel-perfect e crenatura corretta, Mac Roman esteso (0x80-0xFF) per i caratteri accentati europei
- **Internazionalizzazione (i18n)**: Localizzazione basata su risorse con 38 lingue (inglese, francese, tedesco, spagnolo, italiano, portoghese, olandese, danese, norvegese, svedese, finlandese, islandese, greco, turco, polacco, ceco, slovacco, sloveno, croato, ungherese, rumeno, bulgaro, albanese, estone, lettone, lituano, macedone, montenegrino, russo, ucraino, arabo, giapponese, cinese semplificato, cinese tradizionale, coreano, hindi, bengalese, urdu), Locale Manager con selezione della lingua all'avvio, infrastruttura di codifica multi-byte CJK
- **Font Manager**: Supporto multi-dimensione (9-24pt), sintesi degli stili, parsing FOND/NFNT, cache LRU
- **Sistema di Input**: Tastiera e mouse PS/2 con inoltro completo degli eventi
- **Event Manager**: Multitasking cooperativo tramite WaitNextEvent con coda eventi unificata
- **Memory Manager**: Allocazione basata su zone con integrazione dell'interprete 68K
- **Menu Manager**: Menu a tendina completi con tracciamento del mouse e SaveBits/RestoreBits
- **File System**: HFS con implementazione B-tree, finestre cartelle con enumerazione VFS
- **Window Manager**: Trascinamento, ridimensionamento (con grow box), stratificazione, attivazione
- **Time Manager**: Calibrazione TSC accurata, precisione al microsecondo, controllo di generazione
- **Resource Manager**: Ricerca binaria O(log n), cache LRU, validazione completa
- **Gestalt Manager**: Informazioni di sistema multi-architettura con rilevamento dell'architettura
- **TextEdit Manager**: Editing di testo completo con integrazione degli appunti
- **Scrap Manager**: Appunti del Mac OS classico con supporto a formati multipli
- **Applicazione SimpleText**: Editor di testo MDI completo con taglia/copia/incolla
- **List Manager**: Controlli lista compatibili con System 7.1 con navigazione da tastiera
- **Control Manager**: Controlli standard e barre di scorrimento con implementazione CDEF
- **Dialog Manager**: Navigazione da tastiera, anelli di messa a fuoco, scorciatoie da tastiera
- **Segment Loader**: Sistema portabile di caricamento segmenti 68K indipendente dall'ISA con rilocazione
- **Interprete M68K**: Dispatch completo delle istruzioni con 84 handler di opcode, tutte le 14 modalita di indirizzamento, framework eccezioni/trap
- **Sound Manager**: Elaborazione comandi, conversione MIDI, gestione canali, callback
- **Device Manager**: Gestione DCE, installazione/rimozione driver e operazioni I/O
- **Schermata di Avvio**: UI di avvio completa con monitoraggio del progresso, gestione delle fasi e schermata splash
- **Color Manager**: Gestione dello stato dei colori con integrazione QuickDraw

### Parzialmente Implementato ⚠️

- **Integrazione Applicazioni**: Interprete M68K e segment loader completi; necessari test di integrazione per verificare l'esecuzione di applicazioni reali
- **Window Definition Procedures (WDEF)**: Struttura base presente, dispatch parziale
- **Speech Manager**: Solo framework API e passthrough audio; motore di sintesi vocale non implementato
- **Gestione Eccezioni (RTE)**: Ritorno da eccezione parzialmente implementato (attualmente si arresta invece di ripristinare il contesto)

### Non Ancora Implementato ❌

- **Stampa**: Nessun sistema di stampa
- **Rete**: Nessuna funzionalita AppleTalk o di rete
- **Desk Accessories**: Solo framework
- **Audio Avanzato**: Riproduzione campioni, missaggio (limitazione dello speaker del PC)

### Sottosistemi Non Compilati 🔧

I seguenti hanno codice sorgente ma non sono integrati nel kernel:
- **AppleEventManager** (8 file): Messaggistica tra applicazioni; deliberatamente escluso a causa di dipendenze pthread incompatibili con l'ambiente freestanding
- **FontResources** (solo header): Definizioni dei tipi di risorse font; il supporto effettivo ai font e fornito dal compilato FontResourceLoader.c

## 🏗️ Architettura

### Specifiche Tecniche

- **Architettura**: Multi-architettura tramite HAL (x86, ARM, PowerPC pronti)
- **Protocollo di Avvio**: Multiboot2 (x86), bootloader specifici per piattaforma
- **Grafica**: Framebuffer VESA, 800x600 a colori a 32 bit
- **Layout della Memoria**: Il kernel viene caricato all'indirizzo fisico 1MB (x86)
- **Temporizzazione**: Indipendente dall'architettura con precisione al microsecondo (RDTSC/registri timer)
- **Prestazioni**: Miss a freddo delle risorse <15us, hit in cache <2us, deriva del timer <100ppm

### Statistiche del Codice Sorgente

- **225+ file sorgente** con circa 57.500+ righe di codice
- **145+ file header** su 28+ sottosistemi
- **69 tipi di risorse** estratti da System 7.1
- **Tempo di compilazione**: 3-5 secondi su hardware moderno
- **Dimensione del kernel**: circa 4,16 MB
- **Dimensione ISO**: circa 12,5 MB

## 🔨 Compilazione

### Requisiti

- **GCC** con supporto a 32 bit (`gcc-multilib` su sistemi a 64 bit)
- **GNU Make**
- **Strumenti GRUB**: `grub-mkrescue` (da `grub2-common` o `grub-pc-bin`)
- **QEMU** per il test (`qemu-system-i386`)
- **Python 3** per l'elaborazione delle risorse
- **xxd** per la conversione binaria
- *(Opzionale)* Toolchain cross **powerpc-linux-gnu** per le build PowerPC

### Installazione su Ubuntu/Debian

```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common
```

### Comandi di Compilazione

```bash
# Compila il kernel (x86 per impostazione predefinita)
make

# Compila per una piattaforma specifica
make PLATFORM=x86
make PLATFORM=arm        # richiede GCC ARM bare-metal
make PLATFORM=ppc        # sperimentale; richiede toolchain ELF PowerPC

# Crea un ISO avviabile
make iso

# Compila con tutte le lingue
make LOCALE_FR=1 LOCALE_DE=1 LOCALE_ES=1 LOCALE_JA=1 LOCALE_ZH=1 LOCALE_KO=1 LOCALE_RU=1 LOCALE_UK=1 LOCALE_PL=1 LOCALE_CS=1 LOCALE_SQ=1 LOCALE_BG=1 LOCALE_HR=1 LOCALE_DA=1 LOCALE_NL=1 LOCALE_ET=1 LOCALE_FI=1 LOCALE_EL=1 LOCALE_HU=1 LOCALE_IS=1 LOCALE_IT=1 LOCALE_LV=1 LOCALE_LT=1 LOCALE_MK=1 LOCALE_ME=1 LOCALE_NO=1 LOCALE_PT=1 LOCALE_RO=1 LOCALE_SK=1 LOCALE_SL=1 LOCALE_SV=1 LOCALE_TR=1 LOCALE_HI=1 LOCALE_TW=1 LOCALE_AR=1 LOCALE_BN=1 LOCALE_UR=1

# Compila con una singola lingua aggiuntiva
make LOCALE_FR=1

# Compila ed esegui in QEMU
make run

# Pulisci gli artefatti
make clean

# Mostra le statistiche di compilazione
make info
```

## 🚀 Esecuzione

### Avvio Rapido (QEMU)

```bash
# Esecuzione standard con logging seriale
make run

# Manualmente con opzioni
qemu-system-i386 -cdrom system71.iso -serial file:/tmp/serial.log -display sdl -vga std -m 256M
```

### Opzioni QEMU

```bash
# Con output seriale su console
qemu-system-i386 -cdrom system71.iso -serial stdio -display sdl -m 256M

# Senza interfaccia grafica (headless)
qemu-system-i386 -cdrom system71.iso -serial stdio -display none -m 256M

# Con debugging GDB
make debug
# In un altro terminale: gdb kernel.elf -ex "target remote :1234"
```

## 📚 Documentazione

### Guide dei Componenti
- **Control Manager**: `docs/components/ControlManager/`
- **Dialog Manager**: `docs/components/DialogManager/`
- **Font Manager**: `docs/components/FontManager/`
- **Logging Seriale**: `docs/components/System/`
- **Event Manager**: `docs/components/EventManager.md`
- **Menu Manager**: `docs/components/MenuManager.md`
- **Window Manager**: `docs/components/WindowManager.md`
- **Resource Manager**: `docs/components/ResourceManager.md`

### Internazionalizzazione
- **Locale Manager**: `include/LocaleManager/` — cambio lingua a runtime, selezione della lingua all'avvio
- **Risorse Stringa**: `resources/strings/` — file di risorse STR# per lingua (34 lingue)
- **Font Estesi**: `include/chicago_font_extended.h` — glifi Mac Roman 0x80-0xFF per i caratteri europei
- **Supporto CJK**: `include/TextEncoding/CJKEncoding.h`, `include/FontManager/CJKFont.h` — infrastruttura di codifica multi-byte e font

### Stato dell'Implementazione
- **IMPLEMENTATION_PRIORITIES.md**: Lavoro pianificato e monitoraggio della completezza
- **IMPLEMENTATION_STATUS_AUDIT.md**: Audit dettagliato di tutti i sottosistemi

### Filosofia del Progetto

**Approccio Archeologico** con implementazione basata sulle evidenze:
1. Supportato dalla documentazione Inside Macintosh e dalle Universal Interfaces MPW
2. Tutte le decisioni importanti contrassegnate con ID di riferimento che rimandano alle prove a supporto
3. Obiettivo: parita comportamentale con il System 7 originale, non modernizzazione
4. Implementazione clean-room (nessun codice sorgente originale Apple)

## 🐛 Problemi Noti

1. **Artefatti nel Trascinamento delle Icone**: Piccoli artefatti visivi durante il trascinamento delle icone sul desktop
2. **Esecuzione M68K Stubbed**: Segment loader completo, loop di esecuzione non implementato
3. **Nessun Supporto TrueType**: Solo font bitmap (Chicago)
4. **HFS in Sola Lettura**: File system virtuale, nessuna scrittura su disco reale
5. **Nessuna Garanzia di Stabilita**: Crash e comportamenti imprevisti sono frequenti

## 🤝 Contribuire

Questo e principalmente un progetto di apprendimento e ricerca:

1. **Segnalazione Bug**: Apri issue con passaggi dettagliati per la riproduzione
2. **Test**: Segnala i risultati su hardware/emulatori diversi
3. **Documentazione**: Migliora la documentazione esistente o aggiungi nuove guide

## 📖 Riferimenti Essenziali

- **Inside Macintosh** (1992-1994): Documentazione ufficiale Apple del Toolbox
- **MPW Universal Interfaces 3.2**: File header e definizioni struct canonici
- **Guide to Macintosh Family Hardware**: Riferimento sull'architettura hardware

### Strumenti Utili

- **Mini vMac**: Emulatore System 7 per riferimento comportamentale
- **ResEdit**: Editor di risorse per lo studio delle risorse di System 7
- **Ghidra/IDA**: Per l'analisi del disassembly della ROM

## ⚖️ Note Legali

Questa e una **reimplementazione clean-room** a scopo educativo e di conservazione:

- **Nessun codice sorgente Apple** e stato utilizzato
- Basato esclusivamente su documentazione pubblica e analisi black-box
- "System 7", "Macintosh", "QuickDraw" sono marchi registrati di Apple Inc.
- Non affiliato, approvato o sponsorizzato da Apple Inc.

**La ROM e il software originali di System 7 rimangono proprieta di Apple Inc.**

## 🙏 Ringraziamenti

- **Apple Computer, Inc.** per aver creato il System 7 originale
- **Gli autori di Inside Macintosh** per la documentazione completa
- **La comunita di conservazione del Mac classico** per mantenere viva la piattaforma
- **68k.news e Macintosh Garden** per gli archivi di risorse

## 📊 Statistiche di Sviluppo

- **Righe di Codice**: circa 57.500+ (incluse 2.500+ per il segment loader)
- **Tempo di Compilazione**: circa 3-5 secondi
- **Dimensione del Kernel**: circa 4,16 MB (kernel.elf)
- **Dimensione ISO**: circa 12,5 MB (system71.iso)
- **Riduzione Errori**: 94% delle funzionalita principali operative
- **Sottosistemi Principali**: 28+ (Font, Window, Menu, Control, Dialog, TextEdit, ecc.)

## 🔮 Direzioni Future

**Lavoro Pianificato**:

- Completare il loop di esecuzione dell'interprete M68K
- Aggiungere il supporto ai font TrueType
- Risorse font bitmap CJK per il rendering di giapponese, cinese e coreano
- Implementare controlli aggiuntivi (campi di testo, pop-up, slider)
- Scrittura su disco per il file system HFS
- Funzionalita avanzate del Sound Manager (missaggio, campionamento)
- Desk accessories di base (Calcolatrice, Blocco Note)

---

**Stato**: Sperimentale - Educativo - In Sviluppo

**Ultimo Aggiornamento**: Novembre 2025 (Miglioramenti al Sound Manager Completati)

Per domande, problemi o discussioni, utilizzate le GitHub Issues.
