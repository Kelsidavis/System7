# System 7 - Portabel reimplementering med apen kildekode

**[English](README.md)** | **[Fran&ccedil;ais](README.fr.md)** | **[Deutsch](README.de.md)** | **[Espa&ntilde;ol](README.es.md)** | **[Italiano](README.it.md)** | **[Portugu&ecirc;s](README.pt.md)** | **[Nederlands](README.nl.md)** | **[Dansk](README.da.md)** | **[Norsk](README.no.md)** | **[Svenska](README.sv.md)** | **[Suomi](README.fi.md)** | **[&Iacute;slenska](README.is.md)** | **[Ελληνικά](README.el.md)** | **[T&uuml;rk&ccedil;e](README.tr.md)** | **[Polski](README.pl.md)** | **[Čeština](README.cs.md)** | **[Slovenčina](README.sk.md)** | **[Slovenščina](README.sl.md)** | **[Hrvatski](README.hr.md)** | **[Magyar](README.hu.md)** | **[Rom&acirc;n&atilde;](README.ro.md)** | **[Български](README.bg.md)** | **[Shqip](README.sq.md)** | **[Eesti](README.et.md)** | **[Latviešu](README.lv.md)** | **[Lietuvių](README.lt.md)** | **[Македонски](README.mk.md)** | **[Crnogorski](README.me.md)** | **[Русский](README.ru.md)** | **[Українська](README.uk.md)** | **[العربية](README.ar.md)** | **[日本語](README.ja.md)** | **[简体中文](README.zh.md)** | **[繁體中文](README.tw.md)** | **[한국어](README.ko.md)** | **[हिन्दी](README.hi.md)** | **[বাংলা](README.bn.md)** | **[اردو](README.ur.md)**

<img width="793" height="657" alt="System 7 kjoerer paa moderne maskinvare" src="https://github.com/user-attachments/assets/be84b83e-191c-4f9d-a786-11d0bd04203b" />
<img width="801" height="662" alt="simpletextworks" src="https://github.com/user-attachments/assets/7c9ebe5b-22b4-4612-93a1-2076909d77cd" />
<img width="803" height="661" alt="macpaint" src="https://github.com/user-attachments/assets/cd3ed04a-fdde-4dd5-88ef-5b19b3a13a54" />

> ⚠️ **KONSEPTBEVIS** - Dette er en eksperimentell, pedagogisk reimplementering av Apples Macintosh System 7. Dette er IKKE et ferdig produkt og bor ikke betraktes som produksjonsklar programvare.

En reimplementering med apen kildekode av Apple Macintosh System 7 for moderne x86-maskinvare, oppstartbar via GRUB2/Multiboot2. Prosjektet har som mal a gjenskape den klassiske Mac OS-opplevelsen, samtidig som System 7-arkitekturen dokumenteres gjennom omvendt utvikling (reverse engineering).

## 🎯 Prosjektstatus

**Naavaerende tilstand**: Aktiv utvikling med ca. 94 % av kjernefunksjonaliteten ferdigstilt

### Siste oppdateringer (november 2025)

#### Forbedringer i Sound Manager ✅ FULLFORT
- **Optimalisert MIDI-konvertering**: Delt `SndMidiNoteToFreq()`-hjelpefunksjon med 37-oppslags oppslagstabell (C3-B5) og oktavbasert reservelosning for fullt MIDI-omfang (0-127)
- **Stotte for asynkron avspilling**: Komplett infrastruktur for tilbakeringing (callbacks) bade for filavspilling (`FilePlayCompletionUPP`) og kommandokjoring (`SndCallBackProcPtr`)
- **Kanalbasert lydruting**: Flernivaas prioritetssystem med demp- og aktiveringskontroller
  - 4-nivaers prioritetskanaler (0-3) for maskinvareutgangsruting
  - Uavhengige demp- og aktiveringskontroller per kanal
  - `SndGetActiveChannel()` returnerer kanalen med hoyest prioritet som er aktiv
  - Korrekt kanalinitialisering med aktivert flagg som standard
- **Produksjonskvalitet**: All kode kompilerer uten advarsler, ingen malloc/free-brudd oppdaget
- **Commits**: 07542c5 (MIDI-optimalisering), 1854fe6 (asynkrone tilbakeringing), a3433c6 (kanalruting)

#### Tidligere oppnadde resultater
- ✅ **Avanserte funksjoner-fasen**: Sound Manager-kommandoprosesseringsloope, serialisering av flerkjoringsstiler, utvidede MIDI-/syntesefunksjoner
- ✅ **Vindusendringssystem**: Interaktiv storrelsesendring med korrekt vindusrammehandtering, vekstboks og skrivebordsopprydding
- ✅ **PS/2-tastaturoversettelse**: Komplett sett 1 scancode til Toolbox-tastkode-kartlegging
- ✅ **Flerplattforms HAL**: Stotte for x86, ARM og PowerPC med ren abstraksjon

## 📊 Prosjektets fullforingsgrad

**Samlet kjernefunksjonalitet**: Ca. 94 % ferdigstilt (estimert)

### Fullt fungerende ✅

- **Maskinvareabstraksjonslag (HAL)**: Komplett plattformabstraksjon for x86/ARM/PowerPC
- **Oppstartssystem**: Starter vellykket via GRUB2/Multiboot2 pa x86
- **Seriell logging**: Modulbasert logging med kjoretidens filtrering (Error/Warn/Info/Debug/Trace)
- **Grafikkgrunnlag**: VESA-rammebuffer (800x600x32) med QuickDraw-primitiver inkludert XOR-modus
- **Skrivebordsrendering**: System 7-menylinje med regnbuefarget Apple-logo, ikoner og skrivebordsmonstre
- **Typografi**: Chicago bitmap-skrifttype med pikselperfekt gjengivelse og korrekt kerning, utvidet Mac Roman (0x80-0xFF) for europeiske aksenttegn
- **Internasjonalisering (i18n)**: Ressursbasert lokalisering med 38 sprak (engelsk, fransk, tysk, spansk, italiensk, portugisisk, nederlandsk, dansk, norsk, svensk, finsk, islandsk, gresk, tyrkisk, polsk, tsjekkisk, slovakisk, slovensk, kroatisk, ungarsk, rumensk, bulgarsk, albansk, estisk, latvisk, litauisk, makedonsk, montenegrinsk, russisk, ukrainsk, arabisk, japansk, forenklet kinesisk, tradisjonelt kinesisk, koreansk, hindi, bengali, urdu), Locale Manager med sprakvalg ved oppstart, CJK flerbytekodings-infrastruktur
- **Font Manager**: Stotte for flere storrelser (9-24pt), stilsyntese, FOND/NFNT-parsing, LRU-hurtigbuffer
- **Inndatasystem**: PS/2-tastatur og -mus med komplett hendelsesvidereformidling
- **Event Manager**: Kooperativ fleroppgavekjoring via WaitNextEvent med samlet hendelseskø
- **Memory Manager**: Sonebasert tildeling med 68K-tolkintegrasjon
- **Menu Manager**: Komplette nedtrekksmenyer med musesporing og SaveBits/RestoreBits
- **Filsystem**: HFS med B-tre-implementering, mappevinduer med VFS-enumerering
- **Window Manager**: Dra, endre storrelse (med vekstboks), lagdeling, aktivering
- **Time Manager**: Noyaktig TSC-kalibrering, mikrosekunds presisjon, generasjonskontroll
- **Resource Manager**: O(log n) binaersok, LRU-hurtigbuffer, omfattende validering
- **Gestalt Manager**: Flerarkitekturs systeminformasjon med arkitekturdeteksjon
- **TextEdit Manager**: Komplett tekstredigering med utklippstavleintegrasjon
- **Scrap Manager**: Klassisk Mac OS-utklippstavle med stotte for flere formater
- **SimpleText-applikasjon**: Fullt utstyrt MDI-tekstredigerer med klipp/kopier/lim inn
- **List Manager**: System 7.1-kompatible listekontroller med tastaturnavigasjon
- **Control Manager**: Standard- og rullefelt-kontroller med CDEF-implementering
- **Dialog Manager**: Tastaturnavigasjon, fokusringer, hurtigtaster
- **Segment Loader**: Portabelt ISA-agnostisk 68K-segmentinnlastingssystem med relokering
- **M68K-tolk**: Full instruksjonsutsending med 84 opcode-handterere, alle 14 adresseringsmodi, unntak-/trap-rammeverk
- **Sound Manager**: Kommandoprosessering, MIDI-konvertering, kanaladministrasjon, tilbakeringing
- **Device Manager**: DCE-administrasjon, driverinstallering/-fjerning og I/O-operasjoner
- **Oppstartsskjerm**: Komplett oppstarts-UI med fremdriftssporing, faseadministrasjon og velkomstskjerm
- **Color Manager**: Fargetilstandsadministrasjon med QuickDraw-integrasjon

### Delvis implementert ⚠️

- **Applikasjonsintegrasjon**: M68K-tolk og segmentlaster ferdig; integrasjonstesting trengs for a verifisere at ekte applikasjoner kjorer
- **Window Definition Procedures (WDEF)**: Kjernestruktur pa plass, delvis utsending
- **Speech Manager**: API-rammeverk og lydgjennomgang kun; talesyntesemotor ikke implementert
- **Unntakshandtering (RTE)**: Retur fra unntak delvis implementert (stopper for oyeblikket i stedet for a gjenopprette kontekst)

### Ikke enna implementert ❌

- **Utskrift**: Inget utskriftssystem
- **Nettverk**: Ingen AppleTalk- eller nettverksfunksjonalitet
- **Skrivebordstilbehor**: Kun rammeverk
- **Avansert lyd**: Samplingavspilling, miksing (begrensning fra PC-hoyttaler)

### Delsystemer som ikke er kompilert 🔧

Folgende har kildekode, men er ikke integrert i kjernen:
- **AppleEventManager** (8 filer): Meldingsutveksling mellom applikasjoner; bevisst utelatt pa grunn av pthread-avhengigheter som er inkompatible med frittstaende miljo
- **FontResources** (kun headerfil): Typedefinisjoner for skriftressurser; faktisk skriftstotte leveres av kompilert FontResourceLoader.c

## 🏗️ Arkitektur

### Tekniske spesifikasjoner

- **Arkitektur**: Flerarkitektur via HAL (x86, ARM, PowerPC klar)
- **Oppstartsprotokoll**: Multiboot2 (x86), plattformspesifikke oppstartslastere
- **Grafikk**: VESA-rammebuffer, 800x600 @ 32-biters farge
- **Minneoppsett**: Kjernen lastes ved 1MB fysisk adresse (x86)
- **Tidtaking**: Arkitektur-agnostisk med mikrosekunds presisjon (RDTSC/timerregistre)
- **Ytelse**: Kald ressursbom <15us, hurtigbuffertreff <2us, tidsdrift <100ppm

### Kodebasestatistikk

- **225+ kildefiler** med ca. 57 500+ linjer kode
- **145+ headerfiler** fordelt pa 28+ delsystemer
- **69 ressurstyper** hentet fra System 7.1
- **Kompileringstid**: 3-5 sekunder pa moderne maskinvare
- **Kjernestorrelse**: ca. 4,16 MB
- **ISO-storrelse**: ca. 12,5 MB

## 🔨 Bygging

### Krav

- **GCC** med 32-biters stotte (`gcc-multilib` pa 64-bit)
- **GNU Make**
- **GRUB-verktoy**: `grub-mkrescue` (fra `grub2-common` eller `grub-pc-bin`)
- **QEMU** for testing (`qemu-system-i386`)
- **Python 3** for ressursprosessering
- **xxd** for binaerkonvertering
- *(Valgfritt)* **powerpc-linux-gnu** kryssverktoyrekke for PowerPC-bygg

### Ubuntu/Debian-installasjon

```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common
```

### Byggekommandoer

```bash
# Bygg kjernen (x86 som standard)
make

# Bygg for spesifikk plattform
make PLATFORM=x86
make PLATFORM=arm        # krever ARM bare-metal GCC
make PLATFORM=ppc        # eksperimentelt; krever PowerPC ELF-verktoyrekke

# Opprett oppstartbar ISO
make iso

# Bygg med alle sprak
make LOCALE_FR=1 LOCALE_DE=1 LOCALE_ES=1 LOCALE_JA=1 LOCALE_ZH=1 LOCALE_KO=1 LOCALE_RU=1 LOCALE_UK=1 LOCALE_PL=1 LOCALE_CS=1 LOCALE_SQ=1 LOCALE_BG=1 LOCALE_HR=1 LOCALE_DA=1 LOCALE_NL=1 LOCALE_ET=1 LOCALE_FI=1 LOCALE_EL=1 LOCALE_HU=1 LOCALE_IS=1 LOCALE_IT=1 LOCALE_LV=1 LOCALE_LT=1 LOCALE_MK=1 LOCALE_ME=1 LOCALE_NO=1 LOCALE_PT=1 LOCALE_RO=1 LOCALE_SK=1 LOCALE_SL=1 LOCALE_SV=1 LOCALE_TR=1 LOCALE_HI=1 LOCALE_TW=1 LOCALE_AR=1 LOCALE_BN=1 LOCALE_UR=1

# Bygg med ett enkelt tilleggssprak
make LOCALE_FR=1

# Bygg og kjor i QEMU
make run

# Rydd opp artefakter
make clean

# Vis byggestatistikk
make info
```

## 🚀 Kjoring

### Hurtigstart (QEMU)

```bash
# Standardkjoring med seriell logging
make run

# Manuelt med valg
qemu-system-i386 -cdrom system71.iso -serial file:/tmp/serial.log -display sdl -vga std -m 256M
```

### QEMU-alternativer

```bash
# Med seriell konsollutgang
qemu-system-i386 -cdrom system71.iso -serial stdio -display sdl -m 256M

# Hodelov (uten grafisk visning)
qemu-system-i386 -cdrom system71.iso -serial stdio -display none -m 256M

# Med GDB-feilsoking
make debug
# I en annen terminal: gdb kernel.elf -ex "target remote :1234"
```

## 📚 Dokumentasjon

### Komponentveiledninger
- **Control Manager**: `docs/components/ControlManager/`
- **Dialog Manager**: `docs/components/DialogManager/`
- **Font Manager**: `docs/components/FontManager/`
- **Seriell logging**: `docs/components/System/`
- **Event Manager**: `docs/components/EventManager.md`
- **Menu Manager**: `docs/components/MenuManager.md`
- **Window Manager**: `docs/components/WindowManager.md`
- **Resource Manager**: `docs/components/ResourceManager.md`

### Internasjonalisering
- **Locale Manager**: `include/LocaleManager/` -- kjoretidens sprakbytte, sprakvalg ved oppstart
- **Strengressurser**: `resources/strings/` -- per-sprak STR#-ressursfiler (34 sprak)
- **Utvidede skrifttyper**: `include/chicago_font_extended.h` -- Mac Roman 0x80-0xFF-glyffer for europeiske tegn
- **CJK-stotte**: `include/TextEncoding/CJKEncoding.h`, `include/FontManager/CJKFont.h` -- flerbytekodings- og skrifttypeinfrastruktur

### Implementeringsstatus
- **IMPLEMENTATION_PRIORITIES.md**: Planlagt arbeid og fullforingsgrad
- **IMPLEMENTATION_STATUS_AUDIT.md**: Detaljert revisjon av alle delsystemer

### Prosjektfilosofi

**Arkeologisk tilnaerming** med bevisbasert implementering:
1. Stottet av Inside Macintosh-dokumentasjon og MPW Universal Interfaces
2. Alle storre beslutninger merket med funn-IDer som refererer til stottende bevis
3. Mal: atferdsmessig paritet med det opprinnelige System 7, ikke modernisering
4. Ren-rom-implementering (ingen original Apple-kildekode)

## 🐛 Kjente problemer

1. **Ikondra-artefakter**: Sma visuelle artefakter under dra av skrivebordsikoner
2. **M68K-kjoring er stubbet**: Segmentlaster ferdig, kjoringslokke ikke implementert
3. **Ingen TrueType-stotte**: Kun bitmap-skrifttyper (Chicago)
4. **HFS skrivebeskyttet**: Virtuelt filsystem, ingen reell tilbakeskriving til disk
5. **Ingen stabilitetsgarantier**: Krasj og uventet oppforsel er vanlig

## 🤝 Bidra

Dette er primaert et laerings-/forskningsprosjekt:

1. **Feilrapporter**: Opprett saker med detaljerte reproduksjonssteg
2. **Testing**: Rapporter resultater pa ulik maskinvare/emulatorer
3. **Dokumentasjon**: Forbedre eksisterende dokumentasjon eller legg til nye veiledninger

## 📖 Viktige referanser

- **Inside Macintosh** (1992-1994): Offisiell Apple Toolbox-dokumentasjon
- **MPW Universal Interfaces 3.2**: Kanoniske headerfiler og struct-definisjoner
- **Guide to Macintosh Family Hardware**: Referanse for maskinvarearkitektur

### Nyttige verktoy

- **Mini vMac**: System 7-emulator for atferdsreferanse
- **ResEdit**: Ressursredigeringsverktoy for a studere System 7-ressurser
- **Ghidra/IDA**: For ROM-demonteringsanalyse

## ⚖️ Juridisk

Dette er en **ren-rom-reimplementering** for pedagogiske og bevaringsformaal:

- **Ingen Apple-kildekode** ble brukt
- Basert utelukkende pa offentlig dokumentasjon og svart-boks-analyse
- "System 7", "Macintosh", "QuickDraw" er varemerker tilhorende Apple Inc.
- Ikke tilknyttet, godkjent av eller sponset av Apple Inc.

**Det opprinnelige System 7-ROM og programvare forblir Apple Inc. sin eiendom.**

## 🙏 Takksigelser

- **Apple Computer, Inc.** for a ha skapt det opprinnelige System 7
- **Inside Macintosh-forfatterne** for omfattende dokumentasjon
- **Klassisk Mac-bevaringsmiljoet** for a holde plattformen i live
- **68k.news og Macintosh Garden** for ressursarkiver

## 📊 Utviklingsstatistikk

- **Kodelinjer**: Ca. 57 500+ (inkludert 2 500+ for segmentlaster)
- **Kompileringstid**: Ca. 3-5 sekunder
- **Kjernestorrelse**: Ca. 4,16 MB (kernel.elf)
- **ISO-storrelse**: Ca. 12,5 MB (system71.iso)
- **Feilreduksjon**: 94 % av kjernefunksjonaliteten fungerer
- **Hoveddelsystemer**: 28+ (Font, Window, Menu, Control, Dialog, TextEdit, osv.)

## 🔮 Fremtidig retning

**Planlagt arbeid**:

- Fullfor M68K-tolkens kjoringslokke
- Legg til TrueType-skrifttypestotte
- CJK bitmap-skriftressurser for japansk, kinesisk og koreansk gjengivelse
- Implementer ytterligere kontroller (tekstfelt, popup-menyer, glidebrytere)
- Tilbakeskriving til disk for HFS-filsystem
- Avanserte Sound Manager-funksjoner (miksing, sampling)
- Grunnleggende skrivebordstilbehor (Kalkulator, Notatblokk)

---

**Status**: Eksperimentell - Pedagogisk - Under utvikling

**Sist oppdatert**: November 2025 (Sound Manager-forbedringer ferdigstilt)

For sporsmal, problemer eller diskusjon, vennligst bruk GitHub Issues.
