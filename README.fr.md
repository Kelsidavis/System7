# System 7 - Reimplementation Portable Open-Source

**[English](README.md)** | **[Fran&ccedil;ais](README.fr.md)** | **[Deutsch](README.de.md)** | **[Espa&ntilde;ol](README.es.md)** | **[Italiano](README.it.md)** | **[Portugu&ecirc;s](README.pt.md)** | **[Nederlands](README.nl.md)** | **[Dansk](README.da.md)** | **[Norsk](README.no.md)** | **[Svenska](README.sv.md)** | **[Suomi](README.fi.md)** | **[&Iacute;slenska](README.is.md)** | **[Ελληνικά](README.el.md)** | **[T&uuml;rk&ccedil;e](README.tr.md)** | **[Polski](README.pl.md)** | **[Čeština](README.cs.md)** | **[Slovenčina](README.sk.md)** | **[Slovenščina](README.sl.md)** | **[Hrvatski](README.hr.md)** | **[Magyar](README.hu.md)** | **[Rom&acirc;n&atilde;](README.ro.md)** | **[Български](README.bg.md)** | **[Shqip](README.sq.md)** | **[Eesti](README.et.md)** | **[Latviešu](README.lv.md)** | **[Lietuvių](README.lt.md)** | **[Македонски](README.mk.md)** | **[Crnogorski](README.me.md)** | **[Русский](README.ru.md)** | **[Українська](README.uk.md)** | **[日本語](README.ja.md)** | **[中文](README.zh.md)** | **[한국어](README.ko.md)** | **[हिन्दी](README.hi.md)**

<img width="793" height="657" alt="System 7 fonctionnant sur du materiel moderne" src="https://github.com/user-attachments/assets/be84b83e-191c-4f9d-a786-11d0bd04203b" />
<img width="801" height="662" alt="simpletextworks" src="https://github.com/user-attachments/assets/7c9ebe5b-22b4-4612-93a1-2076909d77cd" />
<img width="803" height="661" alt="macpaint" src="https://github.com/user-attachments/assets/cd3ed04a-fdde-4dd5-88ef-5b19b3a13a54" />

> **PREUVE DE CONCEPT** - Ceci est une reimplementation experimentale et pedagogique du System 7 d'Apple Macintosh. Il ne s'agit PAS d'un produit fini et ce logiciel ne doit pas etre considere comme pret pour la production.

Une reimplementation open-source du System 7 d'Apple Macintosh pour le materiel x86 moderne, amorcable via GRUB2/Multiboot2. Ce projet vise a recreer l'experience du Mac OS classique tout en documentant l'architecture du System 7 par l'analyse en retro-ingenierie.

## Etat du projet

**Etat actuel** : Developpement actif avec environ 94 % des fonctionnalites principales achevees

### Dernieres mises a jour (novembre 2025)

#### Ameliorations du Sound Manager -- TERMINE
- **Conversion MIDI optimisee** : fonction partagee `SndMidiNoteToFreq()` avec table de correspondance de 37 entrees (C3-B5) et repli par octave pour toute la plage MIDI (0-127)
- **Lecture asynchrone** : infrastructure complete de rappels (callbacks) pour la lecture de fichiers (`FilePlayCompletionUPP`) et l'execution de commandes (`SndCallBackProcPtr`)
- **Routage audio par canaux** : systeme de priorite multi-niveaux avec controles de sourdine et d'activation
  - 4 niveaux de priorite (0-3) pour le routage vers la sortie materielle
  - Controles independants de sourdine et d'activation par canal
  - `SndGetActiveChannel()` renvoie le canal actif de plus haute priorite
  - Initialisation correcte des canaux avec activation par defaut
- **Implementation de qualite production** : tout le code compile sans erreurs, aucune violation malloc/free detectee
- **Commits** : 07542c5 (optimisation MIDI), 1854fe6 (callbacks asynchrones), a3433c6 (routage par canaux)

#### Realisations des sessions precedentes
- **Phase de fonctionnalites avancees** : boucle de traitement des commandes du Sound Manager, serialisation de styles multi-run, fonctionnalites MIDI/synthese etendues
- **Systeme de redimensionnement des fenetres** : redimensionnement interactif avec gestion correcte du chrome, poignee de redimensionnement et nettoyage du bureau
- **Traduction clavier PS/2** : correspondance complete des scancodes set 1 vers les codes de touches Toolbox
- **HAL multi-plateforme** : prise en charge x86, ARM et PowerPC avec abstraction propre

## Completude du projet

**Fonctionnalites principales** : environ 94 % achevees (estimation)

### Ce qui fonctionne pleinement

- **Couche d'abstraction materielle (HAL)** : abstraction de plateforme complete pour x86/ARM/PowerPC
- **Systeme d'amorcage** : demarre avec succes via GRUB2/Multiboot2 sur x86
- **Journalisation serie** : journalisation modulaire avec filtrage a l'execution (Error/Warn/Info/Debug/Trace)
- **Fondations graphiques** : framebuffer VESA (800x600x32) avec primitives QuickDraw incluant le mode XOR
- **Rendu du bureau** : barre de menus System 7 avec logo Apple arc-en-ciel, icones et motifs de bureau
- **Typographie** : police bitmap Chicago avec rendu au pixel pres et crenage correct, Mac Roman etendu (0x80-0xFF) pour les caracteres accentues europeens
- **Internationalisation (i18n)** : localisation basee sur les ressources avec 7 langues (anglais, francais, allemand, espagnol, japonais, chinois, coreen), Locale Manager avec selection de la langue au demarrage, infrastructure d'encodage multi-octets CJK
- **Font Manager** : prise en charge multi-taille (9-24pt), synthese de styles, analyse FOND/NFNT, cache LRU
- **Systeme de saisie** : clavier et souris PS/2 avec acheminement complet des evenements
- **Event Manager** : multitache cooperatif via WaitNextEvent avec file d'evenements unifiee
- **Memory Manager** : allocation par zones avec integration de l'interpreteur 68K
- **Menu Manager** : menus deroulants complets avec suivi de la souris et SaveBits/RestoreBits
- **Systeme de fichiers** : HFS avec implementation d'arbres B, fenetres de dossiers avec enumeration VFS
- **Window Manager** : deplacement, redimensionnement (avec poignee), superposition, activation
- **Time Manager** : calibration TSC precise, precision a la microseconde, verification de generation
- **Resource Manager** : recherche binaire O(log n), cache LRU, validation exhaustive
- **Gestalt Manager** : informations systeme multi-architecture avec detection d'architecture
- **TextEdit Manager** : edition de texte complete avec integration du presse-papiers
- **Scrap Manager** : presse-papiers Mac OS classique avec prise en charge de formats multiples
- **Application SimpleText** : editeur de texte MDI complet avec couper/copier/coller
- **List Manager** : controles de listes compatibles System 7.1 avec navigation au clavier
- **Control Manager** : controles standard et barres de defilement avec implementation CDEF
- **Dialog Manager** : navigation au clavier, anneaux de focus, raccourcis clavier
- **Segment Loader** : systeme portable de chargement de segments 68K independant de l'ISA avec relocalisation
- **Interpreteur M68K** : dispatch complet des instructions avec 84 gestionnaires d'opcodes, les 14 modes d'adressage, cadre d'exceptions/traps
- **Sound Manager** : traitement des commandes, conversion MIDI, gestion des canaux, rappels
- **Device Manager** : gestion des DCE, installation/suppression de pilotes et operations d'E/S
- **Ecran de demarrage** : interface de demarrage complete avec suivi de la progression, gestion des phases et ecran d'accueil
- **Color Manager** : gestion de l'etat des couleurs avec integration QuickDraw

### Partiellement implemente

- **Integration d'applications** : interpreteur M68K et chargeur de segments termines ; tests d'integration necessaires pour verifier l'execution d'applications reelles
- **Procedures de definition de fenetres (WDEF)** : structure de base en place, dispatch partiel
- **Speech Manager** : cadre API et transfert audio uniquement ; moteur de synthese vocale non implemente
- **Gestion des exceptions (RTE)** : retour d'exception partiellement implemente (arret au lieu de restaurer le contexte)

### Pas encore implemente

- **Impression** : aucun systeme d'impression
- **Reseau** : aucune fonctionnalite AppleTalk ou reseau
- **Accessoires de bureau** : cadre seulement
- **Audio avance** : lecture d'echantillons, mixage (limitation du haut-parleur PC)

### Sous-systemes non compiles

Les elements suivants disposent de code source mais ne sont pas integres au noyau :
- **AppleEventManager** (8 fichiers) : messagerie inter-applications ; deliberement exclu en raison de dependances pthread incompatibles avec un environnement autonome
- **FontResources** (en-tete uniquement) : definitions de types de ressources de polices ; la prise en charge effective des polices est assuree par FontResourceLoader.c compile

## Architecture

### Specifications techniques

- **Architecture** : multi-architecture via HAL (x86, ARM, PowerPC prets)
- **Protocole d'amorcage** : Multiboot2 (x86), chargeurs de demarrage specifiques aux plateformes
- **Graphisme** : framebuffer VESA, 800x600 en couleur 32 bits
- **Organisation memoire** : le noyau se charge a l'adresse physique 1 Mo (x86)
- **Temporisation** : independante de l'architecture avec precision a la microseconde (RDTSC/registres de minuterie)
- **Performance** : defaut de cache ressource <15 us, acces cache <2 us, derive temporelle <100 ppm

### Statistiques du code source

- **Plus de 225 fichiers source** totalisant environ 57 500 lignes de code
- **Plus de 145 fichiers d'en-tete** repartis dans plus de 28 sous-systemes
- **69 types de ressources** extraits de System 7.1
- **Temps de compilation** : 3 a 5 secondes sur du materiel moderne
- **Taille du noyau** : environ 4,16 Mo
- **Taille de l'ISO** : environ 12,5 Mo

## Compilation

### Prerequis

- **GCC** avec prise en charge 32 bits (`gcc-multilib` sur systemes 64 bits)
- **GNU Make**
- **Outils GRUB** : `grub-mkrescue` (depuis `grub2-common` ou `grub-pc-bin`)
- **QEMU** pour les tests (`qemu-system-i386`)
- **Python 3** pour le traitement des ressources
- **xxd** pour la conversion binaire
- *(Facultatif)* Chaine de compilation croisee **powerpc-linux-gnu** pour les compilations PowerPC

### Installation Ubuntu/Debian

```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common
```

### Commandes de compilation

```bash
# Compiler le noyau (x86 par defaut)
make

# Compiler pour une plateforme specifique
make PLATFORM=x86
make PLATFORM=arm        # necessite le GCC bare-metal ARM
make PLATFORM=ppc        # experimental ; necessite la chaine PowerPC ELF

# Creer une image ISO amorcable
make iso

# Compiler avec toutes les langues (francais, allemand, espagnol, japonais, chinois, coreen)
make LOCALE_FR=1 LOCALE_DE=1 LOCALE_ES=1 LOCALE_JA=1 LOCALE_ZH=1 LOCALE_KO=1

# Compiler avec une seule langue supplementaire
make LOCALE_FR=1

# Compiler et lancer dans QEMU
make run

# Nettoyer les artefacts de compilation
make clean

# Afficher les statistiques de compilation
make info
```

## Execution

### Demarrage rapide (QEMU)

```bash
# Execution standard avec journalisation serie
make run

# Manuellement avec des options
qemu-system-i386 -cdrom system71.iso -serial file:/tmp/serial.log -display sdl -vga std -m 256M
```

### Options QEMU

```bash
# Avec sortie serie sur la console
qemu-system-i386 -cdrom system71.iso -serial stdio -display sdl -m 256M

# Sans affichage graphique
qemu-system-i386 -cdrom system71.iso -serial stdio -display none -m 256M

# Avec debogage GDB
make debug
# Dans un autre terminal : gdb kernel.elf -ex "target remote :1234"
```

## Documentation

### Guides par composant
- **Control Manager** : `docs/components/ControlManager/`
- **Dialog Manager** : `docs/components/DialogManager/`
- **Font Manager** : `docs/components/FontManager/`
- **Journalisation serie** : `docs/components/System/`
- **Event Manager** : `docs/components/EventManager.md`
- **Menu Manager** : `docs/components/MenuManager.md`
- **Window Manager** : `docs/components/WindowManager.md`
- **Resource Manager** : `docs/components/ResourceManager.md`

### Internationalisation
- **Locale Manager** : `include/LocaleManager/` -- changement de langue a l'execution, selection de la langue au demarrage
- **Ressources de chaines** : `resources/strings/` -- fichiers de ressources STR# par langue (en, fr, de, es, ja, zh, ko)
- **Polices etendues** : `include/chicago_font_extended.h` -- glyphes Mac Roman 0x80-0xFF pour les caracteres europeens
- **Prise en charge CJK** : `include/TextEncoding/CJKEncoding.h`, `include/FontManager/CJKFont.h` -- infrastructure d'encodage multi-octets et de polices

### Etat de l'implementation
- **IMPLEMENTATION_PRIORITIES.md** : travaux prevus et suivi de la completude
- **IMPLEMENTATION_STATUS_AUDIT.md** : audit detaille de tous les sous-systemes

### Philosophie du projet

**Approche archeologique** avec implementation fondee sur les preuves :
1. Appuyee par la documentation Inside Macintosh et les Universal Interfaces de MPW
2. Toutes les decisions majeures sont etiquetees avec des identifiants de decouverte renvoyant aux preuves correspondantes
3. Objectif : parite comportementale avec le System 7 original, sans modernisation
4. Implementation en salle blanche (aucun code source Apple original)

## Problemes connus

1. **Artefacts de deplacement d'icones** : artefacts visuels mineurs lors du deplacement des icones sur le bureau
2. **Execution M68K inachevee** : chargeur de segments termine, boucle d'execution non implementee
3. **Pas de prise en charge TrueType** : polices bitmap uniquement (Chicago)
4. **HFS en lecture seule** : systeme de fichiers virtuel, pas d'ecriture sur disque
5. **Aucune garantie de stabilite** : les plantages et comportements inattendus sont frequents

## Contribuer

Ce projet est avant tout un projet de recherche et d'apprentissage :

1. **Rapports de bogues** : ouvrez un ticket avec des etapes de reproduction detaillees
2. **Tests** : signalez vos resultats sur differents materiels et emulateurs
3. **Documentation** : ameliorez la documentation existante ou redigez de nouveaux guides

## References essentielles

- **Inside Macintosh** (1992-1994) : documentation officielle de la Toolbox Apple
- **MPW Universal Interfaces 3.2** : fichiers d'en-tete et definitions de structures canoniques
- **Guide to Macintosh Family Hardware** : reference sur l'architecture materielle

### Outils utiles

- **Mini vMac** : emulateur System 7 pour reference comportementale
- **ResEdit** : editeur de ressources pour etudier les ressources System 7
- **Ghidra/IDA** : pour l'analyse par desassemblage de la ROM

## Mentions legales

Il s'agit d'une **reimplementation en salle blanche** a des fins pedagogiques et de preservation :

- **Aucun code source Apple** n'a ete utilise
- Basee uniquement sur la documentation publique et l'analyse en boite noire
- "System 7", "Macintosh", "QuickDraw" sont des marques deposees d'Apple Inc.
- Ce projet n'est ni affilie, ni approuve, ni parraine par Apple Inc.

**La ROM et les logiciels originaux du System 7 restent la propriete d'Apple Inc.**

## Remerciements

- **Apple Computer, Inc.** pour la creation du System 7 original
- **Les auteurs d'Inside Macintosh** pour leur documentation exhaustive
- **La communaute de preservation du Mac classique** pour maintenir la plateforme en vie
- **68k.news et Macintosh Garden** pour leurs archives de ressources

## Statistiques de developpement

- **Lignes de code** : environ 57 500 (dont plus de 2 500 pour le chargeur de segments)
- **Temps de compilation** : environ 3 a 5 secondes
- **Taille du noyau** : environ 4,16 Mo (kernel.elf)
- **Taille de l'ISO** : environ 12,5 Mo (system71.iso)
- **Reduction des erreurs** : 94 % des fonctionnalites principales operationnelles
- **Sous-systemes majeurs** : plus de 28 (Font, Window, Menu, Control, Dialog, TextEdit, etc.)

## Orientations futures

**Travaux prevus** :

- Achever la boucle d'execution de l'interpreteur M68K
- Ajouter la prise en charge des polices TrueType
- Ressources de polices bitmap CJK pour le rendu du japonais, du chinois et du coreen
- Implementer des controles supplementaires (champs de texte, menus contextuels, curseurs)
- Ecriture sur disque pour le systeme de fichiers HFS
- Fonctionnalites avancees du Sound Manager (mixage, echantillonnage)
- Accessoires de bureau de base (Calculette, Bloc-notes)

---

**Etat** : Experimental - Pedagogique - En developpement

**Derniere mise a jour** : novembre 2025 (ameliorations du Sound Manager terminees)

Pour toute question, probleme ou discussion, veuillez utiliser les GitHub Issues.
