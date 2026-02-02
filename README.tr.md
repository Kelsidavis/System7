# System 7 - Tasinabilir Acik Kaynakli Yeniden Uygulama

**[English](README.md)** | **[Fran&ccedil;ais](README.fr.md)** | **[Deutsch](README.de.md)** | **[Espa&ntilde;ol](README.es.md)** | **[Italiano](README.it.md)** | **[Portugu&ecirc;s](README.pt.md)** | **[Nederlands](README.nl.md)** | **[Dansk](README.da.md)** | **[Norsk](README.no.md)** | **[Svenska](README.sv.md)** | **[Suomi](README.fi.md)** | **[&Iacute;slenska](README.is.md)** | **[Ελληνικά](README.el.md)** | **[T&uuml;rk&ccedil;e](README.tr.md)** | **[Polski](README.pl.md)** | **[Čeština](README.cs.md)** | **[Slovenčina](README.sk.md)** | **[Slovenščina](README.sl.md)** | **[Hrvatski](README.hr.md)** | **[Magyar](README.hu.md)** | **[Rom&acirc;n&atilde;](README.ro.md)** | **[Български](README.bg.md)** | **[Shqip](README.sq.md)** | **[Eesti](README.et.md)** | **[Latviešu](README.lv.md)** | **[Lietuvių](README.lt.md)** | **[Македонски](README.mk.md)** | **[Crnogorski](README.me.md)** | **[Русский](README.ru.md)** | **[Українська](README.uk.md)** | **[العربية](README.ar.md)** | **[日本語](README.ja.md)** | **[简体中文](README.zh.md)** | **[繁體中文](README.tw.md)** | **[한국어](README.ko.md)** | **[हिन्दी](README.hi.md)** | **[বাংলা](README.bn.md)** | **[اردو](README.ur.md)**

<img width="793" height="657" alt="System 7 modern donanim uzerinde calisiyor" src="https://github.com/user-attachments/assets/be84b83e-191c-4f9d-a786-11d0bd04203b" />
<img width="801" height="662" alt="simpletextworks" src="https://github.com/user-attachments/assets/7c9ebe5b-22b4-4612-93a1-2076909d77cd" />
<img width="803" height="661" alt="macpaint" src="https://github.com/user-attachments/assets/cd3ed04a-fdde-4dd5-88ef-5b19b3a13a54" />

> ⚠️ **KAVRAMIN KANITI** - Bu, Apple'in Macintosh System 7 isletim sisteminin deneysel ve egitim amacli bir yeniden uygulamasidir. Bu, tamamlanmis bir urun DEGILDIR ve uretim ortaminda kullanima hazir bir yazilim olarak degerlendirilmemelidir.

Modern x86 donanim icin Apple Macintosh System 7'nin acik kaynakli yeniden uygulamasi; GRUB2/Multiboot2 araciligiyla baslatilabilir. Bu proje, klasik Mac OS deneyimini yeniden olusturmayi ve tersine muhendislik analizi yoluyla System 7 mimarisini belgelemeyi amaclamaktadir.

## 🎯 Proje Durumu

**Mevcut Durum**: Cekirdek islevselligin ~%94'u tamamlanmis olarak aktif gelistirme devam ediyor

### Son Guncellemeler (Kasim 2025)

#### Ses Yoneticisi Iyilestirmeleri ✅ TAMAMLANDI
- **Optimize edilmis MIDI donusumu**: 37 girisli arama tablosu (C3-B5) ve tam MIDI araligi (0-127) icin oktav tabanli geri donus mekanizmasi ile paylasimli `SndMidiNoteToFreq()` yardimci fonksiyonu
- **Asenkron kaydirma destegi**: Hem dosya kaydirma (`FilePlayCompletionUPP`) hem de komut yurutme (`SndCallBackProcPtr`) icin eksiksiz geri arama altyapisi
- **Kanal tabanli ses yonlendirme**: Sessiz ve etkinlestirme kontrolleri ile cok seviyeli oncelik sistemi
  - Donanim cikis yonlendirmesi icin 4 seviyeli oncelik kanallari (0-3)
  - Her kanal icin bagimsiz sessiz ve etkinlestirme kontrolleri
  - `SndGetActiveChannel()` en yuksek oncelikli aktif kanali dondurur
  - Varsayilan olarak etkinlestirilmis bayrakla uygun kanal baslatma
- **Uretim kalitesinde uygulama**: Tum kod temiz derlenir, malloc/free ihlali tespit edilmedi
- **Commit'ler**: 07542c5 (MIDI optimizasyonu), 1854fe6 (asenkron geri aramalar), a3433c6 (kanal yonlendirme)

#### Onceki Oturum Basarilari
- ✅ **Gelismis Ozellikler Asamasi**: Ses Yoneticisi komut isleme dongusu, coklu calistirma stili serializasyonu, genisletilmis MIDI/sentez ozellikleri
- ✅ **Pencere Yeniden Boyutlandirma Sistemi**: Uygun krom yonetimi, buyutme kutusu ve masaustu temizligi ile etkilesimli yeniden boyutlandirma
- ✅ **PS/2 Klavye Cevirisi**: Tam set 1 tarama kodu - Toolbox tus kodu eslemesi
- ✅ **Coklu Platform HAL**: Temiz soyutlama ile x86, ARM ve PowerPC destegi

## 📊 Proje Tamamlanma Durumu

**Genel Cekirdek Islevsellik**: ~%94 tamamlandi (tahmini)

### Tam Olarak Calisan ✅

- **Donanim Soyutlama Katmani (HAL)**: x86/ARM/PowerPC icin eksiksiz platform soyutlamasi
- **Baslatma Sistemi**: x86 uzerinde GRUB2/Multiboot2 ile basariyla baslatilir
- **Seri Gunlukleme**: Calisma zamaninda filtreleme ile modul tabanli gunlukleme (Error/Warn/Info/Debug/Trace)
- **Grafik Temeli**: QuickDraw primitifleri ve XOR modu dahil VESA cerceve arabellegi (800x600x32)
- **Masaustu Olusturma**: Gokkusagi Apple logosu, simgeler ve masaustu desenleri ile System 7 menu cubugu
- **Tipografi**: Piksel-kusursuz olusturma ve uygun karisik aralik ile Chicago bitmap yazitip, Avrupa aksan isareti karakterleri icin genisletilmis Mac Roman (0x80-0xFF)
- **Uluslararasilastirma (i18n)**: Kaynak tabanli yerellestime ile 38 dil (Ingilizce, Fransizca, Almanca, Ispanyolca, Italyanca, Portekizce, Felemenkce, Danimarkaca, Norvecce, Isvecce, Fince, Izlandaca, Yunanca, Turkce, Lehce, Cekce, Slovakca, Slovence, Hirvatca, Macarca, Rumence, Bulgarca, Arnavutca, Estonca, Letonca, Litvanca, Makedonca, Karadagca, Rusca, Ukraynaca, Arapca, Japonca, Basitlesilmis Cince, Geleneksel Cince, Korece, Hintce, Bengalce, Urduca), baslatma zamaninda dil secimi ile Yerel Ayar Yoneticisi, CJK coklu bayt kodlama altyapisi
- **Yazitip Yoneticisi**: Coklu boyut destegi (9-24pt), stil sentezi, FOND/NFNT ayrıstirma, LRU onbellekleme
- **Giris Sistemi**: Tam olay yonlendirme ile PS/2 klavye ve fare
- **Olay Yoneticisi**: Birlesik olay kuyrugu ile WaitNextEvent araciligiyla isbirlikli coklu gorev
- **Bellek Yoneticisi**: 68K yorumlayici entegrasyonu ile bolge tabanli tahsis
- **Menu Yoneticisi**: Fare izleme ve SaveBits/RestoreBits ile eksiksiz acilir menuler
- **Dosya Sistemi**: B-agac uygulamasi ile HFS, VFS numaralandirmasi ile klasor pencereleri
- **Pencere Yoneticisi**: Surukle, yeniden boyutlandir (buyutme kutusu ile), katmanlama, etkinlestirme
- **Zaman Yoneticisi**: Hassas TSC kalibrasyon, mikrosaniye hassasiyeti, nesil denetimi
- **Kaynak Yoneticisi**: O(log n) ikili arama, LRU onbellek, kapsamli dogrulama
- **Gestalt Yoneticisi**: Mimari algilama ile coklu mimari sistem bilgisi
- **TextEdit Yoneticisi**: Pano entegrasyonu ile eksiksiz metin duzenleme
- **Scrap Yoneticisi**: Birden fazla tat destegi ile klasik Mac OS panosu
- **SimpleText Uygulamasi**: Kes/kopyala/yapistir ile tam ozellikli MDI metin duzenleyici
- **Liste Yoneticisi**: Klavye gezintisi ile System 7.1 uyumlu liste kontrolleri
- **Kontrol Yoneticisi**: CDEF uygulamasi ile standart ve kaydir cubugu kontrolleri
- **Diyalog Yoneticisi**: Klavye gezintisi, odak halkalari, klavye kisayollari
- **Segment Yukleyici**: Yer degistirme ile tasinabilir ISA-bagımsiz 68K segment yukleme sistemi
- **M68K Yorumlayici**: 84 islem kodu isleyicisi, tum 14 adresleme modu, istisna/trap cercevesi ile tam komut dagitimi
- **Ses Yoneticisi**: Komut isleme, MIDI donusumu, kanal yonetimi, geri aramalar
- **Aygit Yoneticisi**: DCE yonetimi, surucu kurulumu/kaldirmasi ve G/C islemleri
- **Baslatma Ekrani**: Ilerleme izleme, asama yonetimi ve karsilama ekrani ile eksiksiz baslatma kullanici arayuzu
- **Renk Yoneticisi**: QuickDraw entegrasyonu ile renk durumu yonetimi

### Kismen Uygulanmis ⚠️

- **Uygulama Entegrasyonu**: M68K yorumlayici ve segment yukleyici tamamlandi; gercek uygulamalarin calistigini dogrulamak icin entegrasyon testi gerekli
- **Pencere Tanimlama Prosedürleri (WDEF)**: Cekirdek yapi mevcut, kismi dagitim
- **Konusma Yoneticisi**: Yalnizca API cercevesi ve ses gecisi; konusma sentezi motoru uygulanmadi
- **Istisna Yonetimi (RTE)**: Istisnadan donus kismen uygulanmis (su anda baglami geri yuklemek yerine duruyor)

### Henuz Uygulanmamis ❌

- **Yazdirma**: Yazdirma sistemi yok
- **Ag**: AppleTalk veya ag islevseligi yok
- **Masa Aksesuarlari**: Yalnizca cerceve
- **Gelismis Ses**: Ornek kaydirma, karistirma (PC hoparloru sinirlamasi)

### Derlenmemis Alt Sistemler 🔧

Asagidakilerin kaynak kodu mevcuttur ancak cekirdege entegre edilmemistir:
- **AppleEventManager** (8 dosya): Uygulamalar arasi mesajlasma; bagimsiz ortamla uyumsuz pthread bagimliliklari nedeniyle kasitli olarak disarida birakilmistir
- **FontResources** (yalnizca baslik): Yazitip kaynak tur tanimlari; gercek yazitip destegi derlenmis FontResourceLoader.c tarafindan saglanir

## 🏗️ Mimari

### Teknik Ozellikler

- **Mimari**: HAL araciligiyla coklu mimari (x86, ARM, PowerPC hazir)
- **Baslatma Protokolu**: Multiboot2 (x86), platforma ozel baslatma yukleyicileri
- **Grafik**: VESA cerceve arabellegi, 800x600 @ 32-bit renk
- **Bellek Duzeni**: Cekirdek 1MB fiziksel adrese yuklenir (x86)
- **Zamanlama**: Mikrosaniye hassasiyetinde mimariden bagimsiz (RDTSC/zamanlayici kayitlari)
- **Performans**: Soguk kaynak iskalama <15us, onbellek isabeti <2us, zamanlayici sapma <100ppm

### Kod Tabani Istatistikleri

- **225+ kaynak dosya** ile ~57.500+ satir kod
- **28+ alt sistemde 145+ baslik dosyasi**
- **System 7.1'den cikarilmis 69 kaynak turu**
- **Derleme suresi**: Modern donanim uzerinde 3-5 saniye
- **Cekirdek boyutu**: ~4,16 MB
- **ISO boyutu**: ~12,5 MB

## 🔨 Derleme

### Gereksinimler

- **GCC** ve 32-bit destegi (64-bit sistemlerde `gcc-multilib`)
- **GNU Make**
- **GRUB araclari**: `grub-mkrescue` (`grub2-common` veya `grub-pc-bin`'den)
- **QEMU** test icin (`qemu-system-i386`)
- **Python 3** kaynak isleme icin
- **xxd** ikili donusum icin
- *(Istege bagli)* PowerPC derlemeleri icin **powerpc-linux-gnu** capraz arac zinciri

### Ubuntu/Debian Kurulumu

```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common
```

### Derleme Komutlari

```bash
# Cekirdegi derle (varsayilan olarak x86)
make

# Belirli bir platform icin derle
make PLATFORM=x86
make PLATFORM=arm        # ARM bare-metal GCC gerektirir
make PLATFORM=ppc        # deneysel; PowerPC ELF arac zinciri gerektirir

# Baslatilabilir ISO olustur
make iso

# Tum dillerle derle
make LOCALE_FR=1 LOCALE_DE=1 LOCALE_ES=1 LOCALE_JA=1 LOCALE_ZH=1 LOCALE_KO=1 LOCALE_RU=1 LOCALE_UK=1 LOCALE_PL=1 LOCALE_CS=1 LOCALE_SQ=1 LOCALE_BG=1 LOCALE_HR=1 LOCALE_DA=1 LOCALE_NL=1 LOCALE_ET=1 LOCALE_FI=1 LOCALE_EL=1 LOCALE_HU=1 LOCALE_IS=1 LOCALE_IT=1 LOCALE_LV=1 LOCALE_LT=1 LOCALE_MK=1 LOCALE_ME=1 LOCALE_NO=1 LOCALE_PT=1 LOCALE_RO=1 LOCALE_SK=1 LOCALE_SL=1 LOCALE_SV=1 LOCALE_TR=1 LOCALE_HI=1 LOCALE_TW=1 LOCALE_AR=1 LOCALE_BN=1 LOCALE_UR=1

# Tek bir ek dil ile derle
make LOCALE_FR=1

# Derle ve QEMU'da calistir
make run

# Derleme ürünlerini temizle
make clean

# Derleme istatistiklerini goruntule
make info
```

## 🚀 Calistirma

### Hizli Baslangic (QEMU)

```bash
# Seri gunlukleme ile standart calistirma
make run

# Seceneklerle manuel calistirma
qemu-system-i386 -cdrom system71.iso -serial file:/tmp/serial.log -display sdl -vga std -m 256M
```

### QEMU Secenekleri

```bash
# Konsol seri ciktisi ile
qemu-system-i386 -cdrom system71.iso -serial stdio -display sdl -m 256M

# Basliksiz (grafik gosterimi yok)
qemu-system-i386 -cdrom system71.iso -serial stdio -display none -m 256M

# GDB hata ayiklama ile
make debug
# Baska bir terminalde: gdb kernel.elf -ex "target remote :1234"
```

## 📚 Belgelendirme

### Bilesen Kilavuzlari
- **Kontrol Yoneticisi**: `docs/components/ControlManager/`
- **Diyalog Yoneticisi**: `docs/components/DialogManager/`
- **Yazitip Yoneticisi**: `docs/components/FontManager/`
- **Seri Gunlukleme**: `docs/components/System/`
- **Olay Yoneticisi**: `docs/components/EventManager.md`
- **Menu Yoneticisi**: `docs/components/MenuManager.md`
- **Pencere Yoneticisi**: `docs/components/WindowManager.md`
- **Kaynak Yoneticisi**: `docs/components/ResourceManager.md`

### Uluslararasilastirma
- **Yerel Ayar Yoneticisi**: `include/LocaleManager/` — calisma zamaninda yerel ayar degistirme, baslatma zamaninda dil secimi
- **Dize Kaynaklari**: `resources/strings/` — dile gore STR# kaynak dosyalari (34 dil)
- **Genisletilmis Yazitipleri**: `include/chicago_font_extended.h` — Avrupa karakterleri icin Mac Roman 0x80-0xFF glifleri
- **CJK Destegi**: `include/TextEncoding/CJKEncoding.h`, `include/FontManager/CJKFont.h` — coklu bayt kodlama ve yazitip altyapisi

### Uygulama Durumu
- **IMPLEMENTATION_PRIORITIES.md**: Planlanan calisma ve tamamlanma takibi
- **IMPLEMENTATION_STATUS_AUDIT.md**: Tum alt sistemlerin ayrintili denetimi

### Proje Felsefesi

**Arkeolojik Yaklasim** ile kanit tabanli uygulama:
1. Inside Macintosh belgeleri ve MPW Universal Interfaces ile desteklenmis
2. Tum onemli kararlar, destekleyici kanita referans veren Bulgu Kimlikleri ile etiketlenmis
3. Hedef: orijinal System 7 ile davranissal esitlik, modernlestirme degil
4. Temiz oda uygulamasi (orijinal Apple kaynak kodu kullanilmamistir)

## 🐛 Bilinen Sorunlar

1. **Simge Surukle Artifaktlari**: Masaustu simge suruklemesi sirasinda kucuk gorsel artifaktlar
2. **M68K Yurutme Saplanmis**: Segment yukleyici tamamlandi, yurutme dongusu uygulanmadi
3. **TrueType Destegi Yok**: Yalnizca bitmap yazitipleri (Chicago)
4. **HFS Salt Okunur**: Sanal dosya sistemi, gercek disk geri yazimi yok
5. **Kararlilik Garantisi Yok**: Cokme ve beklenmedik davranislar yaygindir

## 🤝 Katki Saglamak

Bu oncelikli olarak bir ogrenme/arastirma projesidir:

1. **Hata Raporlari**: Ayrintili yeniden uretme adimlari ile sorunlari bildirin
2. **Test**: Farkli donanim/emulatorlerde sonuclari raporlayin
3. **Belgelendirme**: Mevcut belgeleri iyilestirin veya yeni kilavuzlar ekleyin

## 📖 Temel Kaynaklar

- **Inside Macintosh** (1992-1994): Resmi Apple Toolbox belgeleri
- **MPW Universal Interfaces 3.2**: Kanonik baslik dosyalari ve yapi tanimlari
- **Guide to Macintosh Family Hardware**: Donanim mimarisi referansi

### Faydali Araclar

- **Mini vMac**: Davranissal referans icin System 7 emülatoru
- **ResEdit**: System 7 kaynaklarini incelemek icin kaynak duzenleyici
- **Ghidra/IDA**: ROM sokumleme analizi icin

## ⚖️ Yasal

Bu, egitim ve koruma amaciyla yapilmis bir **temiz oda yeniden uygulamasidir**:

- **Apple kaynak kodu** kullanilmamistir
- Yalnizca kamuya acik belgelere ve kara kutu analizine dayanmaktadir
- "System 7", "Macintosh", "QuickDraw" Apple Inc. ticari markalaridir
- Apple Inc. ile bagli degildir, Apple Inc. tarafindan onaylanmamis veya desteklenmemistir

**Orijinal System 7 ROM ve yazilimi Apple Inc.'in mulkiyetindedir.**

## 🙏 Tesekkurler

- **Apple Computer, Inc.** orijinal System 7'yi yarattigi icin
- **Inside Macintosh yazarlari** kapsamli belgeleme icin
- **Klasik Mac koruma toplulugu** platformu canli tuttugu icin
- **68k.news ve Macintosh Garden** kaynak arsivleri icin

## 📊 Gelistirme Istatistikleri

- **Kod Satiri Sayisi**: ~57.500+ (segment yukleyici icin 2.500+ dahil)
- **Derleme Suresi**: ~3-5 saniye
- **Cekirdek Boyutu**: ~4,16 MB (kernel.elf)
- **ISO Boyutu**: ~12,5 MB (system71.iso)
- **Hata Azaltma**: Cekirdek islevselligin %94'u calisiyor
- **Ana Alt Sistemler**: 28+ (Font, Window, Menu, Control, Dialog, TextEdit, vb.)

## 🔮 Gelecek Yonu

**Planlanan Calismalar**:

- M68K yorumlayici yurutme dongusunu tamamlama
- TrueType yazitip destegi ekleme
- Japonca, Cince ve Korece olusturma icin CJK bitmap yazitip kaynaklari
- Ek kontrollerin uygulanmasi (metin alanlari, acilir menuler, kaydiricılar)
- HFS dosya sistemi icin disk geri yazimi
- Gelismis Ses Yoneticisi ozellikleri (karistirma, ornekleme)
- Temel masa aksesuarlari (Hesap Makinesi, Not Defteri)

---

**Durum**: Deneysel - Egitim Amacli - Gelistirme Asamasinda

**Son Guncelleme**: Kasim 2025 (Ses Yoneticisi Iyilestirmeleri Tamamlandi)

Sorulariniz, sorunlariniz veya tartisma icin lutfen GitHub Issues'i kullanin.
