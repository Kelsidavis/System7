# System 7 - Siirrettava avoimen lahdekoodin uudelleentoteutus

**[English](README.md)** | **[Fran&ccedil;ais](README.fr.md)** | **[Deutsch](README.de.md)** | **[Espa&ntilde;ol](README.es.md)** | **[Italiano](README.it.md)** | **[Portugu&ecirc;s](README.pt.md)** | **[Nederlands](README.nl.md)** | **[Dansk](README.da.md)** | **[Norsk](README.no.md)** | **[Svenska](README.sv.md)** | **[Suomi](README.fi.md)** | **[&Iacute;slenska](README.is.md)** | **[Ελληνικά](README.el.md)** | **[T&uuml;rk&ccedil;e](README.tr.md)** | **[Polski](README.pl.md)** | **[Čeština](README.cs.md)** | **[Slovenčina](README.sk.md)** | **[Slovenščina](README.sl.md)** | **[Hrvatski](README.hr.md)** | **[Magyar](README.hu.md)** | **[Rom&acirc;n&atilde;](README.ro.md)** | **[Български](README.bg.md)** | **[Shqip](README.sq.md)** | **[Eesti](README.et.md)** | **[Latviešu](README.lv.md)** | **[Lietuvių](README.lt.md)** | **[Македонски](README.mk.md)** | **[Crnogorski](README.me.md)** | **[Русский](README.ru.md)** | **[Українська](README.uk.md)** | **[日本語](README.ja.md)** | **[中文](README.zh.md)** | **[한국어](README.ko.md)** | **[हिन्दी](README.hi.md)**

<img width="793" height="657" alt="System 7 toiminnassa nykyaikaisella laitteistolla" src="https://github.com/user-attachments/assets/be84b83e-191c-4f9d-a786-11d0bd04203b" />
<img width="801" height="662" alt="simpletextworks" src="https://github.com/user-attachments/assets/7c9ebe5b-22b4-4612-93a1-2076909d77cd" />
<img width="803" height="661" alt="macpaint" src="https://github.com/user-attachments/assets/cd3ed04a-fdde-4dd5-88ef-5b19b3a13a54" />

> ⚠️ **KONSEPTITODISTUS** - Tama on kokeellinen, opetuksellinen uudelleentoteutus Applen Macintosh System 7:sta. Tama EI ole valmis tuote eika sita tule pitaa tuotantovalmiina ohjelmistona.

Avoimen lahdekoodin uudelleentoteutus Apple Macintosh System 7:sta nykyaikaiselle x86-laitteistolle, kaynnistettavissa GRUB2/Multiboot2:n kautta. Taman projektin tavoitteena on luoda uudelleen klassinen Mac OS -kokemus ja samalla dokumentoida System 7:n arkkitehtuuri kaanteistekniikka-analyysin avulla.

## 🎯 Projektin tila

**Nykyinen tila**: Aktiivinen kehitys, noin 94 % ydintoiminnallisuudesta valmis

### Viimeisimmat paivitykset (marraskuu 2025)

#### Sound Manager -parannukset ✅ VALMIS
- **Optimoitu MIDI-muunnos**: Jaettu `SndMidiNoteToFreq()`-apufunktio 37 alkion hakutaululla (C3-B5) ja oktaavipohjainen varatoteutus koko MIDI-alueelle (0-127)
- **Asynkroninen toisto**: Taysi takaisinkutsuinfrastruktuuri seka tiedostojen toistolle (`FilePlayCompletionUPP`) etta kasky jen suoritukselle (`SndCallBackProcPtr`)
- **Kanavapohjainen aanireititys**: Monitasoinen prioriteettijarjestelma mykistys- ja kayttoonsaannilla
  - 4-tasoinen prioriteettikanavajarjestelma (0-3) laitteistolahtoreitityksen
  - Itsenaiset mykistys- ja kayttoonsaannit kanavaa kohti
  - `SndGetActiveChannel()` palauttaa korkeimman prioriteetin aktiivisen kanavan
  - Asianmukainen kanavan alustus oletusarvoisesti kaytossa
- **Tuotantolaatuinen toteutus**: Kaikki koodi kaantyy puhtaasti, malloc/free-rikkomuksia ei havaittu
- **Commitit**: 07542c5 (MIDI-optimointi), 1854fe6 (asynkroniset takaisinkutsut), a3433c6 (kanavareititys)

#### Aiemmat saavutukset
- ✅ **Edistyneiden ominaisuuksien vaihe**: Sound Manager -kasky jen kasittelysilmukka, monikertaisen tyylin serialisointi, laajennetut MIDI-/synteesiominaisuudet
- ✅ **Ikkunan koon muutosjarjestelma**: Vuorovaikutteinen koon muutos asianmukaisella kehysten kasittelylla, kasvulaatikolla ja tyopoydalla siivouksella
- ✅ **PS/2-nappaimiston kaannos**: Taysi sarjan 1 skannauskoodi-Toolbox-nappainkoodi-vastaavuus
- ✅ **Monialustainen HAL**: x86-, ARM- ja PowerPC-tuki puhtaalla abstraktiolla

## 📊 Projektin valmiusaste

**Ydintoiminnallisuus kokonaisuutena**: noin 94 % valmis (arvio)

### Taysin toimivat ✅

- **Laitteistoabstraktiokerros (HAL)**: Taysi alusta-abstraktio x86/ARM/PowerPC-alustoille
- **Kaynnistysjarjestelma**: Kaynnistyy onnistuneesti GRUB2/Multiboot2:n kautta x86-alustalla
- **Sarjamuotoinen lokitus**: Moduulipohjainen lokitus ajonaikaisella suodatuksella (Error/Warn/Info/Debug/Trace)
- **Grafiikkapohja**: VESA-kehyspuskuri (800x600x32) QuickDraw-primitiiveilla mukaan lukien XOR-tila
- **Tyopoydanpiirto**: System 7 -valikkorivi sateenkaari-Apple-logolla, kuvakkeilla ja tyopoytakuvioilla
- **Typografia**: Chicago-bittikarttafontti pikselitarkalla piirtamisella ja oikealla merkkivalistyksen saadolla, laajennettu Mac Roman (0x80-0xFF) eurooppalaisille aksentoituille merkeille
- **Kansainvalistyminen (i18n)**: Resurssipohjainen lokalisointi 34 kielella (englanti, ranska, saksa, espanja, italia, portugali, hollanti, tanska, norja, ruotsi, suomi, islanti, kreikka, turkki, puola, tsekki, slovakki, sloveeni, kroatia, unkari, romania, bulgaria, albania, viro, latvia, liettua, makedonia, montenegro, venaja, ukraina, japani, kiina, korea, hindi), Locale Manager kaynnistyksenaikaisella kielen valinnalla, CJK-monitavukoodausinfrastruktuuri
- **Font Manager**: Monikokotuki (9-24pt), tyylien synteesi, FOND/NFNT-jasennys, LRU-vaalimuisti
- **Syottosysteemi**: PS/2-nappaimisto ja -hiiri taydellisella tapahtumien valityksella
- **Event Manager**: Yhteistoiminnallinen moniajo WaitNextEvent-funktiolla ja yhtenaisella tapahtumajonolla
- **Memory Manager**: Vyohykepohjainen muistinvaraus 68K-tulkin integroinnilla
- **Menu Manager**: Taydelliset pudotusvalikot hiiren seurannalla ja SaveBits/RestoreBits-toiminnolla
- **Tiedostojarjestelma**: HFS B-puutoteutuksella, kansioikkunat VFS-luetteloinnilla
- **Window Manager**: Raahaaminen, koon muutos (kasvulaatikolla), kerrostus, aktivointi
- **Time Manager**: Tarkka TSC-kalibrointi, mikrosekuntitarkkuus, sukupolvitarkistus
- **Resource Manager**: O(log n) -binaarihaku, LRU-valimuisti, kattava validointi
- **Gestalt Manager**: Moniarkkitehtuurinen jarjestelmatiedot arkkitehtuurin tunnistuksella
- **TextEdit Manager**: Taysi tekstinmuokkaus leikepoydan integroinnilla
- **Scrap Manager**: Klassinen Mac OS -leikepoyta monikohteisella muototuella
- **SimpleText-sovellus**: Taysipiirteinen MDI-tekstieditori leikkaa/kopioi/liita-toiminnoilla
- **List Manager**: System 7.1 -yhteensopivat listaelementit nappaimistonavigoinnilla
- **Control Manager**: Vakio- ja vierityspalkki-elementit CDEF-toteutuksella
- **Dialog Manager**: Nappaimistonavigointi, kohdistuskehykset, nappainkomennot
- **Segment Loader**: Siirrettava ISA-riippumaton 68K-segmenttien latausjarjestelma uudelleensijoituksella
- **M68K-tulkki**: Taysi kasky jen valitys 84 operaatiokoodinkasittelijalla, kaikki 14 osoitustapaa, poikkeus-/trap-kehys
- **Sound Manager**: Kasky jen kasittely, MIDI-muunnos, kanavanhallinta, takaisinkutsut
- **Device Manager**: DCE-hallinta, ajurien asennus/poisto ja I/O-operaatiot
- **Kaynnistysruutu**: Taysi kaynnistys-UI edistymisen seurannalla, vaiheiden hallinnalla ja kaynnistyslogolla
- **Color Manager**: Varitilojen hallinta QuickDraw-integroinnilla

### Osittain toteutettu ⚠️

- **Sovellusintegraatio**: M68K-tulkki ja segmenttien lataaja valmiit; integrointitestaus tarvitaan todellisten sovellusten suorituksen vahvistamiseksi
- **Ikkunanmaarittelyproseduurit (WDEF)**: Ydinrakenne paikallaan, osittainen valitys
- **Speech Manager**: API-kehys ja aanilapikulku ainoastaan; puhesynteesimoottoria ei ole toteutettu
- **Poikkeuskasittely (RTE)**: Poikkeuksesta paluu osittain toteutettu (talla hetkella pysahtyy kontekstin palauttamisen sijaan)

### Ei viela toteutettu ❌

- **Tulostus**: Ei tulostusjarjestelmaa
- **Verkko**: Ei AppleTalk- tai verkkotoiminnallisuutta
- **Tyopoytalisaosat**: Ainoastaan kehys
- **Edistynyt aani**: Naytteiden toisto, miksaus (PC-kaiuttimen rajoitus)

### Alijos jarjestelmat, joita ei kaanneta 🔧

Seuraavilla on lahdekoodia, mutta niita ei ole integroitu kerneliin:
- **AppleEventManager** (8 tiedostoa): Sovellusten valinen viestinta; tarkoituksella jatettya pois pthread-riippuvuuksien vuoksi, jotka eivat ole yhteensopivia itsenaisesti toimivan ympariston kanssa
- **FontResources** (vain otsaketiedosto): Fonttiresurssien tyyppimarittelyt; varsinainen fonttituki tarjotaan kaannetyn FontResourceLoader.c-tiedoston kautta

## 🏗️ Arkkitehtuuri

### Tekniset ominaisuudet

- **Arkkitehtuuri**: Moniarkkitehtuurinen HAL:n kautta (x86, ARM, PowerPC valmius)
- **Kaynnistysprotokolla**: Multiboot2 (x86), alustakohtaiset kaynnistyslataimet
- **Grafiikka**: VESA-kehyspuskuri, 800x600 @ 32-bittinen vari
- **Muistiasettelu**: Kerneli ladataan fyysiseen osoitteeseen 1 Mt (x86)
- **Ajastus**: Arkkitehtuuririippumaton mikrosekuntitarkkuudella (RDTSC/ajastinrekisterit)
- **Suorituskyky**: Kylmaresurssihuti <15 us, valimuistiosuma <2 us, ajastinpoikkeama <100 ppm

### Koodikannan tilastot

- **225+ lahdekooditiedostoa** ja noin 57 500+ koodirivia
- **145+ otsaketiedostoa** yli 28 alijarjestelmassa
- **69 resurssityyppia** poimittu System 7.1:sta
- **Kaannosaikal**: 3-5 sekuntia nykyaikaisella laitteistolla
- **Kernelin koko**: noin 4,16 Mt
- **ISO-koko**: noin 12,5 Mt

## 🔨 Kaaminen

### Vaatimukset

- **GCC** 32-bittisella tuella (`gcc-multilib` 64-bittisilla jarjestelmilla)
- **GNU Make**
- **GRUB-tyokalut**: `grub-mkrescue` (paketista `grub2-common` tai `grub-pc-bin`)
- **QEMU** testausta varten (`qemu-system-i386`)
- **Python 3** resurssien kasittelyyn
- **xxd** binaarimuunnokseen
- *(Valinnainen)* **powerpc-linux-gnu** -ristikaannostyokaluketju PowerPC-kaannoksia varten

### Ubuntu/Debian-asennus

```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common
```

### Kaannoskomennot

```bash
# Kaanna kerneli (oletuksena x86)
make

# Kaanna tietylle alustalle
make PLATFORM=x86
make PLATFORM=arm        # vaatii ARM bare-metal GCC:n
make PLATFORM=ppc        # kokeellinen; vaatii PowerPC ELF -tyokaluketjun

# Luo kaynnistettava ISO
make iso

# Kaanna kaikilla kielilla
make LOCALE_FR=1 LOCALE_DE=1 LOCALE_ES=1 LOCALE_JA=1 LOCALE_ZH=1 LOCALE_KO=1 LOCALE_RU=1 LOCALE_UK=1 LOCALE_PL=1 LOCALE_CS=1 LOCALE_SQ=1 LOCALE_BG=1 LOCALE_HR=1 LOCALE_DA=1 LOCALE_NL=1 LOCALE_ET=1 LOCALE_FI=1 LOCALE_EL=1 LOCALE_HU=1 LOCALE_IS=1 LOCALE_IT=1 LOCALE_LV=1 LOCALE_LT=1 LOCALE_MK=1 LOCALE_ME=1 LOCALE_NO=1 LOCALE_PT=1 LOCALE_RO=1 LOCALE_SK=1 LOCALE_SL=1 LOCALE_SV=1 LOCALE_TR=1 LOCALE_HI=1

# Kaanna yhdella lisakielella
make LOCALE_FR=1

# Kaanna ja suorita QEMU:ssa
make run

# Siivoa kaannostuotteet
make clean

# Naytta kaannostilastot
make info
```

## 🚀 Suorittaminen

### Pikakaynnistys (QEMU)

```bash
# Vakiosuoritus sarjalokituksella
make run

# Manuaalisesti valinnaisilla asetuksilla
qemu-system-i386 -cdrom system71.iso -serial file:/tmp/serial.log -display sdl -vga std -m 256M
```

### QEMU-asetukset

```bash
# Konsolin sarjatulosteella
qemu-system-i386 -cdrom system71.iso -serial stdio -display sdl -m 256M

# Paaton (ilman grafiikkanayttoa)
qemu-system-i386 -cdrom system71.iso -serial stdio -display none -m 256M

# GDB-virheenkorjauksella
make debug
# Toisessa paatetssa: gdb kernel.elf -ex "target remote :1234"
```

## 📚 Dokumentaatio

### Komponenttioppaat
- **Control Manager**: `docs/components/ControlManager/`
- **Dialog Manager**: `docs/components/DialogManager/`
- **Font Manager**: `docs/components/FontManager/`
- **Sarjalokitus**: `docs/components/System/`
- **Event Manager**: `docs/components/EventManager.md`
- **Menu Manager**: `docs/components/MenuManager.md`
- **Window Manager**: `docs/components/WindowManager.md`
- **Resource Manager**: `docs/components/ResourceManager.md`

### Kansainvalistyminen
- **Locale Manager**: `include/LocaleManager/` -- ajonaikainen kielen vaihto, kaynnistyksenaikainen kielen valinta
- **Merkkijonoresurssit**: `resources/strings/` -- kielikohtaiset STR#-resurssitiedostot (34 kielta)
- **Laajennetut fontit**: `include/chicago_font_extended.h` -- Mac Roman 0x80-0xFF -kuvamerkit eurooppalaisille merkeille
- **CJK-tuki**: `include/TextEncoding/CJKEncoding.h`, `include/FontManager/CJKFont.h` -- monitavukoodaus ja fonttiinfrastruktuuri

### Toteutuksen tila
- **IMPLEMENTATION_PRIORITIES.md**: Suunniteltu tyo ja valmiusasteen seuranta
- **IMPLEMENTATION_STATUS_AUDIT.md**: Yksityiskohtainen auditointi kaikista alijarjestelmista

### Projektin filosofia

**Arkeologinen lahestymistapa** nayttoon perustuvalla toteutuksella:
1. Perustuu Inside Macintosh -dokumentaatioon ja MPW Universal Interfaces -rajapintoihin
2. Kaikki merkittavat paatokset merkitty loyto-ID:illa, jotka viittaavat tukevaan nayttoon
3. Tavoite: kaytostavastaa alkuperaista System 7:aa, ei modernisoida
4. Puhdas huone -toteutus (alkuperaista Applen lahdekoodia ei kaytetty)

## 🐛 Tunnetut ongelmat

1. **Kuvakkeiden raahausartefaktit**: Pieniia visuaalisia virheita tyopoytakuvakkeiden raahauksen aikana
2. **M68K-suoritus tynkana**: Segmenttien lataaja valmis, suoritussilmukkaa ei ole toteutettu
3. **Ei TrueType-tukea**: Ainoastaan bittikarttafontit (Chicago)
4. **HFS vain luku**: Virtuaalinen tiedostojarjestelma, ei todellista levykirjoitusta
5. **Ei vakaustakuita**: Kaatumiset ja odottamaton kayttaytyminen ovat yleisia

## 🤝 Osallistuminen

Tama on ensisijaisesti oppimis-/tutkimusprojekti:

1. **Vikailmoitukset**: Lahetä ongelmailmoituksia yksityiskohtaisilla toistamisohjeilla
2. **Testaus**: Raportoi tuloksia eri laitteistoilla/emulaattoreilla
3. **Dokumentaatio**: Paranna olemassa olevia ohjeita tai lisaa uusia oppaita

## 📖 Olennaiset lahteet

- **Inside Macintosh** (1992-1994): Applen virallinen Toolbox-dokumentaatio
- **MPW Universal Interfaces 3.2**: Viralliset otsaketiedostot ja tietuerakennemaarittelyt
- **Guide to Macintosh Family Hardware**: Laitteistoarkkitehtuurin viiteteos

### Hyodylliset tyokalut

- **Mini vMac**: System 7 -emulaattori kayttaytymisvertailua varten
- **ResEdit**: Resurssieditori System 7 -resurssien tutkimiseen
- **Ghidra/IDA**: ROM-purkuanalyysiin

## ⚖️ Oikeudellinen

Tama on **puhdas huone -uudelleentoteutus** opetus- ja sailytystarkoituksiin:

- **Applen lahdekoodia ei kaytetty**
- Perustuu ainoastaan julkiseen dokumentaatioon ja mustan laatikon analyysiin
- "System 7", "Macintosh", "QuickDraw" ovat Apple Inc.:n tavaramerkkeja
- Ei ole Apple Inc.:n liittyma, tukema tai rahoittama

**Alkuperainen System 7 ROM ja ohjelmisto ovat edelleen Apple Inc.:n omaisuutta.**

## 🙏 Kiitokset

- **Apple Computer, Inc.** alkuperaisen System 7:n luomisesta
- **Inside Macintosh -kirjoittajat** kattavasta dokumentaatiosta
- **Klassisen Macin sailytysyhteiso** alustan pitamisesta elossa
- **68k.news ja Macintosh Garden** resurssiarkistoista

## 📊 Kehitystilastot

- **Koodirivit**: noin 57 500+ (mukaan lukien 2 500+ segmenttien lataajalle)
- **Kaannosaikal**: noin 3-5 sekuntia
- **Kernelin koko**: noin 4,16 Mt (kernel.elf)
- **ISO-koko**: noin 12,5 Mt (system71.iso)
- **Virheiden vahennys**: 94 % ydintoiminnallisuudesta toimii
- **Paaalijarjestelmat**: 28+ (Font, Window, Menu, Control, Dialog, TextEdit jne.)

## 🔮 Tulevaisuuden suunta

**Suunniteltu tyo**:

- M68K-tulkin suoritussilmukan viimeistely
- TrueType-fonttituen lisaaminen
- CJK-bittikarttafonttien resurssit japanin, kiinan ja korean piirtamiseen
- Lisakontrollien toteutus (tekstikentat, ponnahdusvalikot, liukusaadimet)
- Levykirjoitus HFS-tiedostojarjestelmaan
- Edistyneet Sound Manager -ominaisuudet (miksaus, nayteistaminen)
- Perustvopoytalisaosat (Calculator, Note Pad)

---

**Tila**: Kokeellinen - Opetuksellinen - Kehityksessa

**Viimeksi paivitetty**: marraskuu 2025 (Sound Manager -parannukset valmiit)

Kysymyksiin, ongelmiin tai keskusteluun kayta GitHub Issues -palvelua.
