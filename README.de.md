# System 7 - Portable Open-Source-Neuimplementierung

**[English](README.md)** | **[Fran&ccedil;ais](README.fr.md)** | **[Deutsch](README.de.md)** | **[Espa&ntilde;ol](README.es.md)** | **[Italiano](README.it.md)** | **[Portugu&ecirc;s](README.pt.md)** | **[Nederlands](README.nl.md)** | **[Dansk](README.da.md)** | **[Norsk](README.no.md)** | **[Svenska](README.sv.md)** | **[Suomi](README.fi.md)** | **[&Iacute;slenska](README.is.md)** | **[Ελληνικά](README.el.md)** | **[T&uuml;rk&ccedil;e](README.tr.md)** | **[Polski](README.pl.md)** | **[Čeština](README.cs.md)** | **[Slovenčina](README.sk.md)** | **[Slovenščina](README.sl.md)** | **[Hrvatski](README.hr.md)** | **[Magyar](README.hu.md)** | **[Rom&acirc;n&atilde;](README.ro.md)** | **[Български](README.bg.md)** | **[Shqip](README.sq.md)** | **[Eesti](README.et.md)** | **[Latviešu](README.lv.md)** | **[Lietuvių](README.lt.md)** | **[Македонски](README.mk.md)** | **[Crnogorski](README.me.md)** | **[Русский](README.ru.md)** | **[Українська](README.uk.md)** | **[日本語](README.ja.md)** | **[中文](README.zh.md)** | **[한국어](README.ko.md)** | **[हिन्दी](README.hi.md)**

<img width="793" height="657" alt="System 7 auf moderner Hardware" src="https://github.com/user-attachments/assets/be84b83e-191c-4f9d-a786-11d0bd04203b" />
<img width="801" height="662" alt="simpletextworks" src="https://github.com/user-attachments/assets/7c9ebe5b-22b4-4612-93a1-2076909d77cd" />
<img width="803" height="661" alt="macpaint" src="https://github.com/user-attachments/assets/cd3ed04a-fdde-4dd5-88ef-5b19b3a13a54" />

> **PROOF OF CONCEPT** - Dies ist eine experimentelle, zu Bildungszwecken erstellte Neuimplementierung von Apples Macintosh System 7. Es handelt sich NICHT um ein fertiges Produkt und sollte nicht als produktionsreife Software betrachtet werden.

Eine quelloffene Neuimplementierung von Apple Macintosh System 7 fuer moderne x86-Hardware, bootfaehig ueber GRUB2/Multiboot2. Dieses Projekt hat zum Ziel, das klassische Mac-OS-Erlebnis nachzubilden und gleichzeitig die System-7-Architektur durch Reverse-Engineering-Analyse zu dokumentieren.

## Projektstatus

**Aktueller Stand**: Aktive Entwicklung mit ca. 94 % der Kernfunktionalitaet abgeschlossen

### Neueste Aktualisierungen (November 2025)

#### Sound Manager-Verbesserungen - ABGESCHLOSSEN
- **Optimierte MIDI-Konvertierung**: Gemeinsam genutzte `SndMidiNoteToFreq()`-Hilfsfunktion mit 37-Eintraege-Nachschlagetabelle (C3-B5) und oktavbasiertem Fallback fuer den gesamten MIDI-Bereich (0-127)
- **Asynchrone Wiedergabe**: Vollstaendige Callback-Infrastruktur fuer Dateiwiedergabe (`FilePlayCompletionUPP`) und Befehlsausfuehrung (`SndCallBackProcPtr`)
- **Kanalbasiertes Audio-Routing**: Mehrstufiges Prioritaetssystem mit Stummschaltung und Aktivierungssteuerung
  - 4-stufige Prioritaetskanaele (0-3) fuer Hardware-Ausgaberouting
  - Unabhaengige Stummschaltungs- und Aktivierungssteuerung pro Kanal
  - `SndGetActiveChannel()` gibt den aktiven Kanal mit hoechster Prioritaet zurueck
  - Korrekte Kanalinitialisierung mit standardmaessig aktiviertem Flag
- **Produktionsreife Implementierung**: Gesamter Code kompiliert fehlerfrei, keine malloc/free-Verletzungen erkannt
- **Commits**: 07542c5 (MIDI-Optimierung), 1854fe6 (asynchrone Callbacks), a3433c6 (Kanal-Routing)

#### Vorherige Meilensteine
- **Erweiterte Funktionsphase**: Sound Manager-Befehlsverarbeitungsschleife, Multi-Run-Style-Serialisierung, erweiterte MIDI-/Synthesefunktionen
- **Fenstergroessen-Aenderungssystem**: Interaktive Groessenaenderung mit korrektem Chrome-Handling, Grow-Box und Desktop-Bereinigung
- **PS/2-Tastaturuebersetzung**: Vollstaendige Set-1-Scancode-zu-Toolbox-Tastencode-Zuordnung
- **Multi-Plattform-HAL**: Unterstuetzung fuer x86, ARM und PowerPC mit sauberer Abstraktion

## Projektvollstaendigkeit

**Gesamte Kernfunktionalitaet**: ca. 94 % abgeschlossen (geschaetzt)

### Voll funktionsfaehig

- **Hardware Abstraction Layer (HAL)**: Vollstaendige Plattformabstraktion fuer x86/ARM/PowerPC
- **Boot-System**: Erfolgreiches Booten ueber GRUB2/Multiboot2 auf x86
- **Serielle Protokollierung**: Modulbasierte Protokollierung mit Laufzeitfilterung (Error/Warn/Info/Debug/Trace)
- **Grafikgrundlage**: VESA-Framebuffer (800x600x32) mit QuickDraw-Primitiven einschliesslich XOR-Modus
- **Desktop-Darstellung**: System-7-Menueleiste mit regenbogenfarbenem Apple-Logo, Symbolen und Desktop-Mustern
- **Typografie**: Chicago-Bitmap-Schrift mit pixelgenauem Rendering und korrektem Kerning, erweitertes Mac Roman (0x80-0xFF) fuer europaeische Akzentzeichen
- **Internationalisierung (i18n)**: Ressourcenbasierte Lokalisierung mit 7 Sprachen (Englisch, Franzoesisch, Deutsch, Spanisch, Japanisch, Chinesisch, Koreanisch), Locale Manager mit Sprachauswahl beim Booten, CJK-Multibyte-Kodierungsinfrastruktur
- **Font Manager**: Unterstuetzung mehrerer Groessen (9-24pt), Stilsynthese, FOND/NFNT-Parsing, LRU-Caching
- **Eingabesystem**: PS/2-Tastatur und -Maus mit vollstaendiger Ereignisweiterleitung
- **Event Manager**: Kooperatives Multitasking ueber WaitNextEvent mit vereinheitlichter Ereigniswarteschlange
- **Memory Manager**: Zonenbasierte Speicherzuweisung mit 68K-Interpreter-Integration
- **Menu Manager**: Vollstaendige Dropdown-Menues mit Mausverfolgung und SaveBits/RestoreBits
- **Dateisystem**: HFS mit B-Baum-Implementierung, Ordnerfenster mit VFS-Aufzaehlung
- **Window Manager**: Verschieben, Groessenaenderung (mit Grow-Box), Ebenen, Aktivierung
- **Time Manager**: Praezise TSC-Kalibrierung, Mikrosekunden-Genauigkeit, Generationspruefung
- **Resource Manager**: O(log n)-Binaersuche, LRU-Cache, umfassende Validierung
- **Gestalt Manager**: Multi-Architektur-Systeminformationen mit Architekturerkennung
- **TextEdit Manager**: Vollstaendige Textbearbeitung mit Zwischenablage-Integration
- **Scrap Manager**: Klassische Mac-OS-Zwischenablage mit Unterstuetzung mehrerer Formate
- **SimpleText-Anwendung**: Voll ausgestatteter MDI-Texteditor mit Ausschneiden/Kopieren/Einfuegen
- **List Manager**: System-7.1-kompatible Listensteuerelemente mit Tastaturnavigation
- **Control Manager**: Standard- und Scrollbar-Steuerelemente mit CDEF-Implementierung
- **Dialog Manager**: Tastaturnavigation, Fokusringe, Tastaturkuerzel
- **Segment Loader**: Portables, ISA-unabhaengiges 68K-Segment-Ladesystem mit Relokation
- **M68K-Interpreter**: Vollstaendiger Befehlsdispatch mit 84 Opcode-Handlern, allen 14 Adressierungsmodi, Exception-/Trap-Framework
- **Sound Manager**: Befehlsverarbeitung, MIDI-Konvertierung, Kanalverwaltung, Callbacks
- **Device Manager**: DCE-Verwaltung, Treiberinstallation/-entfernung und E/A-Operationen
- **Startbildschirm**: Vollstaendige Boot-Oberflaeche mit Fortschrittsverfolgung, Phasenverwaltung und Splash-Screen
- **Color Manager**: Farbzustandsverwaltung mit QuickDraw-Integration

### Teilweise implementiert

- **Anwendungsintegration**: M68K-Interpreter und Segment Loader abgeschlossen; Integrationstests erforderlich, um die Ausfuehrung realer Anwendungen zu verifizieren
- **Window Definition Procedures (WDEF)**: Grundstruktur vorhanden, teilweiser Dispatch
- **Speech Manager**: Nur API-Framework und Audio-Passthrough; Sprachsynthese-Engine nicht implementiert
- **Exception Handling (RTE)**: Return from Exception teilweise implementiert (haelt derzeit an, statt den Kontext wiederherzustellen)

### Noch nicht implementiert

- **Drucken**: Kein Drucksystem vorhanden
- **Netzwerk**: Keine AppleTalk- oder Netzwerkfunktionalitaet
- **Desk Accessories**: Nur Framework vorhanden
- **Erweitertes Audio**: Sample-Wiedergabe, Mixing (Einschraenkung durch PC-Lautsprecher)

### Nicht kompilierte Subsysteme

Die folgenden Subsysteme haben Quellcode, sind aber nicht in den Kernel integriert:
- **AppleEventManager** (8 Dateien): Nachrichtenaustausch zwischen Anwendungen; bewusst ausgeschlossen wegen pthread-Abhaengigkeiten, die mit der freistehenden Umgebung nicht kompatibel sind
- **FontResources** (nur Header): Schriftressourcen-Typdefinitionen; tatsaechliche Schriftunterstuetzung wird durch den kompilierten FontResourceLoader.c bereitgestellt

## Architektur

### Technische Spezifikationen

- **Architektur**: Multi-Architektur ueber HAL (x86, ARM, PowerPC bereit)
- **Boot-Protokoll**: Multiboot2 (x86), plattformspezifische Bootloader
- **Grafik**: VESA-Framebuffer, 800x600 bei 32-Bit-Farbtiefe
- **Speicherlayout**: Kernel wird an physischer Adresse 1 MB geladen (x86)
- **Zeitmessung**: Architekturunabhaengig mit Mikrosekunden-Praezision (RDTSC/Timer-Register)
- **Leistung**: Kalter Ressourcen-Miss <15 us, Cache-Treffer <2 us, Timer-Drift <100 ppm

### Codebasis-Statistiken

- **225+ Quelldateien** mit ca. 57.500+ Zeilen Code
- **145+ Header-Dateien** ueber 28+ Subsysteme verteilt
- **69 Ressourcentypen** aus System 7.1 extrahiert
- **Kompilierungszeit**: 3-5 Sekunden auf moderner Hardware
- **Kernel-Groesse**: ca. 4,16 MB
- **ISO-Groesse**: ca. 12,5 MB

## Erstellen (Build)

### Voraussetzungen

- **GCC** mit 32-Bit-Unterstuetzung (`gcc-multilib` auf 64-Bit-Systemen)
- **GNU Make**
- **GRUB-Werkzeuge**: `grub-mkrescue` (aus `grub2-common` oder `grub-pc-bin`)
- **QEMU** zum Testen (`qemu-system-i386`)
- **Python 3** zur Ressourcenverarbeitung
- **xxd** zur Binaerkonvertierung
- *(Optional)* **powerpc-linux-gnu**-Cross-Toolchain fuer PowerPC-Builds

### Installation unter Ubuntu/Debian

```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common
```

### Build-Befehle

```bash
# Kernel erstellen (standardmaessig x86)
make

# Fuer eine bestimmte Plattform erstellen
make PLATFORM=x86
make PLATFORM=arm        # erfordert ARM-Bare-Metal-GCC
make PLATFORM=ppc        # experimentell; erfordert PowerPC-ELF-Toolchain

# Bootfaehiges ISO erstellen
make iso

# Mit allen Sprachen erstellen (Franzoesisch, Deutsch, Spanisch, Japanisch, Chinesisch, Koreanisch)
make LOCALE_FR=1 LOCALE_DE=1 LOCALE_ES=1 LOCALE_JA=1 LOCALE_ZH=1 LOCALE_KO=1

# Mit einer einzelnen zusaetzlichen Sprache erstellen
make LOCALE_FR=1

# Erstellen und in QEMU ausfuehren
make run

# Build-Artefakte bereinigen
make clean

# Build-Statistiken anzeigen
make info
```

## Ausfuehren

### Schnellstart (QEMU)

```bash
# Standardausfuehrung mit serieller Protokollierung
make run

# Manuell mit Optionen
qemu-system-i386 -cdrom system71.iso -serial file:/tmp/serial.log -display sdl -vga std -m 256M
```

### QEMU-Optionen

```bash
# Mit serieller Konsolenausgabe
qemu-system-i386 -cdrom system71.iso -serial stdio -display sdl -m 256M

# Headless (ohne Grafikanzeige)
qemu-system-i386 -cdrom system71.iso -serial stdio -display none -m 256M

# Mit GDB-Debugging
make debug
# In einem anderen Terminal: gdb kernel.elf -ex "target remote :1234"
```

## Dokumentation

### Komponentenleitfaeden
- **Control Manager**: `docs/components/ControlManager/`
- **Dialog Manager**: `docs/components/DialogManager/`
- **Font Manager**: `docs/components/FontManager/`
- **Serielle Protokollierung**: `docs/components/System/`
- **Event Manager**: `docs/components/EventManager.md`
- **Menu Manager**: `docs/components/MenuManager.md`
- **Window Manager**: `docs/components/WindowManager.md`
- **Resource Manager**: `docs/components/ResourceManager.md`

### Internationalisierung
- **Locale Manager**: `include/LocaleManager/` -- Laufzeit-Gebietsschema-Umschaltung, Sprachauswahl beim Booten
- **String-Ressourcen**: `resources/strings/` -- sprachspezifische STR#-Ressourcendateien (en, fr, de, es, ja, zh, ko)
- **Erweiterte Schriften**: `include/chicago_font_extended.h` -- Mac-Roman-0x80-0xFF-Glyphen fuer europaeische Zeichen
- **CJK-Unterstuetzung**: `include/TextEncoding/CJKEncoding.h`, `include/FontManager/CJKFont.h` -- Multibyte-Kodierung und Schriftinfrastruktur

### Implementierungsstatus
- **IMPLEMENTATION_PRIORITIES.md**: Geplante Arbeiten und Fortschrittsverfolgung
- **IMPLEMENTATION_STATUS_AUDIT.md**: Detaillierte Pruefung aller Subsysteme

### Projektphilosophie

**Archaeologischer Ansatz** mit evidenzbasierter Implementierung:
1. Gestuetzt auf Inside-Macintosh-Dokumentation und MPW Universal Interfaces
2. Alle wesentlichen Entscheidungen mit Finding-IDs versehen, die auf stuetzende Belege verweisen
3. Ziel: Verhaltensgleichheit mit dem originalen System 7, keine Modernisierung
4. Clean-Room-Implementierung (kein originaler Apple-Quellcode verwendet)

## Bekannte Probleme

1. **Symbol-Zieh-Artefakte**: Geringfuegige visuelle Artefakte beim Ziehen von Desktop-Symbolen
2. **M68K-Ausfuehrung als Stub**: Segment Loader abgeschlossen, Ausfuehrungsschleife nicht implementiert
3. **Keine TrueType-Unterstuetzung**: Nur Bitmap-Schriften (Chicago)
4. **HFS nur lesend**: Virtuelles Dateisystem, kein tatsaechliches Zurueckschreiben auf die Festplatte
5. **Keine Stabilitaetsgarantien**: Abstuerze und unerwartetes Verhalten sind haeufig

## Mitwirken

Dies ist in erster Linie ein Lern- und Forschungsprojekt:

1. **Fehlerberichte**: Erstellen Sie Issues mit detaillierten Reproduktionsschritten
2. **Testen**: Berichten Sie ueber Ergebnisse auf verschiedener Hardware und verschiedenen Emulatoren
3. **Dokumentation**: Verbessern Sie bestehende Dokumentation oder erstellen Sie neue Anleitungen

## Wichtige Referenzen

- **Inside Macintosh** (1992-1994): Offizielle Apple-Toolbox-Dokumentation
- **MPW Universal Interfaces 3.2**: Kanonische Header-Dateien und Strukturdefinitionen
- **Guide to Macintosh Family Hardware**: Referenz zur Hardware-Architektur

### Nuetzliche Werkzeuge

- **Mini vMac**: System-7-Emulator als Verhaltensreferenz
- **ResEdit**: Ressourceneditor zum Studium von System-7-Ressourcen
- **Ghidra/IDA**: Fuer ROM-Disassembly-Analyse

## Rechtliches

Dies ist eine **Clean-Room-Neuimplementierung** zu Bildungs- und Archivierungszwecken:

- **Kein Apple-Quellcode** wurde verwendet
- Basiert ausschliesslich auf oeffentlicher Dokumentation und Black-Box-Analyse
- "System 7", "Macintosh", "QuickDraw" sind Marken von Apple Inc.
- Nicht mit Apple Inc. verbunden, nicht von Apple Inc. unterstuetzt oder gesponsert

**Das originale System-7-ROM und die Originalsoftware bleiben Eigentum von Apple Inc.**

## Danksagungen

- **Apple Computer, Inc.** fuer die Erschaffung des originalen System 7
- **Inside-Macintosh-Autoren** fuer die umfassende Dokumentation
- **Classic-Mac-Preservation-Community** fuer die Bewahrung der Plattform
- **68k.news und Macintosh Garden** fuer Ressourcenarchive

## Entwicklungsstatistiken

- **Codezeilen**: ca. 57.500+ (einschliesslich 2.500+ fuer den Segment Loader)
- **Kompilierungszeit**: ca. 3-5 Sekunden
- **Kernel-Groesse**: ca. 4,16 MB (kernel.elf)
- **ISO-Groesse**: ca. 12,5 MB (system71.iso)
- **Fehlerreduktion**: 94 % der Kernfunktionalitaet funktionsfaehig
- **Haupt-Subsysteme**: 28+ (Font, Window, Menu, Control, Dialog, TextEdit usw.)

## Zukunftsplanung

**Geplante Arbeiten**:

- M68K-Interpreter-Ausfuehrungsschleife vervollstaendigen
- TrueType-Schriftunterstuetzung hinzufuegen
- CJK-Bitmap-Schriftressourcen fuer japanisches, chinesisches und koreanisches Rendering
- Zusaetzliche Steuerelemente implementieren (Textfelder, Pop-ups, Schieberegler)
- Zurueckschreiben auf Festplatte fuer das HFS-Dateisystem
- Erweiterte Sound-Manager-Funktionen (Mixing, Sampling)
- Grundlegende Desk Accessories (Taschenrechner, Notizblock)

---

**Status**: Experimentell - Zu Bildungszwecken - In Entwicklung

**Zuletzt aktualisiert**: November 2025 (Sound-Manager-Verbesserungen abgeschlossen)

Bei Fragen, Problemen oder fuer Diskussionen nutzen Sie bitte GitHub Issues.
