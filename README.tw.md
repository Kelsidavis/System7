# System 7 - 可攜式開源重新實作

**[English](README.md)** | **[Fran&ccedil;ais](README.fr.md)** | **[Deutsch](README.de.md)** | **[Espa&ntilde;ol](README.es.md)** | **[Italiano](README.it.md)** | **[Portugu&ecirc;s](README.pt.md)** | **[Nederlands](README.nl.md)** | **[Dansk](README.da.md)** | **[Norsk](README.no.md)** | **[Svenska](README.sv.md)** | **[Suomi](README.fi.md)** | **[&Iacute;slenska](README.is.md)** | **[Ελληνικά](README.el.md)** | **[T&uuml;rk&ccedil;e](README.tr.md)** | **[Polski](README.pl.md)** | **[Čeština](README.cs.md)** | **[Slovenčina](README.sk.md)** | **[Slovenščina](README.sl.md)** | **[Hrvatski](README.hr.md)** | **[Magyar](README.hu.md)** | **[Rom&acirc;n&atilde;](README.ro.md)** | **[Български](README.bg.md)** | **[Shqip](README.sq.md)** | **[Eesti](README.et.md)** | **[Latviešu](README.lv.md)** | **[Lietuvių](README.lt.md)** | **[Македонски](README.mk.md)** | **[Crnogorski](README.me.md)** | **[Русский](README.ru.md)** | **[Українська](README.uk.md)** | **[العربية](README.ar.md)** | **[日本語](README.ja.md)** | **[简体中文](README.zh.md)** | **[繁體中文](README.tw.md)** | **[한국어](README.ko.md)** | **[हिन्दी](README.hi.md)** | **[বাংলা](README.bn.md)** | **[اردو](README.ur.md)**

<img width="793" height="657" alt="System 7 在現代硬體上執行" src="https://github.com/user-attachments/assets/be84b83e-191c-4f9d-a786-11d0bd04203b" />
<img width="801" height="662" alt="simpletextworks" src="https://github.com/user-attachments/assets/7c9ebe5b-22b4-4612-93a1-2076909d77cd" />
<img width="803" height="661" alt="macpaint" src="https://github.com/user-attachments/assets/cd3ed04a-fdde-4dd5-88ef-5b19b3a13a54" />

> ⚠️ **概念驗證** - 這是一個實驗性、教育性質的 Apple Macintosh System 7 重新實作。這不是一個完成品，也不應被視為可用於正式環境的軟體。

一個針對現代 x86 硬體的 Apple Macintosh System 7 開源重新實作，可透過 GRUB2/Multiboot2 開機。本專案旨在重現經典 Mac OS 體驗，同時透過逆向工程分析來記錄 System 7 的架構。

## 🎯 專案狀態

**目前狀態**：積極開發中，核心功能約完成 ~94%

### 最新更新（2025 年 11 月）

#### Sound Manager 增強功能 ✅ 已完成
- **最佳化 MIDI 轉換**：共用 `SndMidiNoteToFreq()` 輔助函式，包含 37 個項目的查找表（C3-B5）及基於八度音階的備援機制，支援完整 MIDI 範圍（0-127）
- **非同步播放支援**：完整的回呼基礎架構，支援檔案播放（`FilePlayCompletionUPP`）和命令執行（`SndCallBackProcPtr`）
- **基於通道的音訊路由**：多層級優先權系統，具備靜音和啟用控制
  - 4 層級優先權通道（0-3），用於硬體輸出路由
  - 每個通道獨立的靜音和啟用控制
  - `SndGetActiveChannel()` 回傳最高優先權的作用中通道
  - 通道初始化時預設為啟用狀態
- **正式品質實作**：所有程式碼編譯乾淨，未偵測到 malloc/free 違規
- **提交紀錄**：07542c5（MIDI 最佳化）、1854fe6（非同步回呼）、a3433c6（通道路由）

#### 先前工作階段成果
- ✅ **進階功能階段**：Sound Manager 命令處理迴圈、多重執行樣式序列化、擴充 MIDI/合成功能
- ✅ **視窗調整大小系統**：互動式調整大小，含適當的視窗外框處理、拖曳調整區塊和桌面清理
- ✅ **PS/2 鍵盤轉譯**：完整的 Set 1 掃描碼到 Toolbox 按鍵碼對應
- ✅ **多平台 HAL**：支援 x86、ARM 和 PowerPC，具備乾淨的抽象層

## 📊 專案完成度

**整體核心功能**：約完成 ~94%（估計值）

### 已完整運作 ✅

- **硬體抽象層（HAL）**：完整的 x86/ARM/PowerPC 平台抽象
- **開機系統**：透過 GRUB2/Multiboot2 在 x86 上成功開機
- **序列埠日誌**：基於模組的日誌記錄，支援執行期篩選（Error/Warn/Info/Debug/Trace）
- **圖形基礎**：VESA 框架緩衝區（800x600x32），含 QuickDraw 繪圖基元，包括 XOR 模式
- **桌面渲染**：System 7 選單列，含彩虹 Apple 標誌、圖示和桌面圖案
- **字型排版**：Chicago 點陣字型，具備像素完美渲染和適當的字距調整，擴充 Mac Roman（0x80-0xFF）支援歐洲重音字元
- **國際化（i18n）**：基於資源的在地化，支援 38 種語言（英文、法文、德文、西班牙文、義大利文、葡萄牙文、荷蘭文、丹麥文、挪威文、瑞典文、芬蘭文、冰島文、希臘文、土耳其文、波蘭文、捷克文、斯洛伐克文、斯洛維尼亞文、克羅埃西亞文、匈牙利文、羅馬尼亞文、保加利亞文、阿爾巴尼亞文、愛沙尼亞文、拉脫維亞文、立陶宛文、馬其頓文、蒙特內哥羅文、俄文、烏克蘭文、阿拉伯文、日文、簡體中文、繁體中文、韓文、印地文、孟加拉文、烏爾都文），Locale Manager 支援開機時語言選擇，CJK 多位元組編碼基礎架構
- **Font Manager**：多尺寸支援（9-24pt）、樣式合成、FOND/NFNT 解析、LRU 快取
- **輸入系統**：PS/2 鍵盤和滑鼠，含完整的事件轉發
- **Event Manager**：透過 WaitNextEvent 實現協作式多工，具備統一事件佇列
- **Memory Manager**：基於區域的記憶體配置，整合 68K 直譯器
- **Menu Manager**：完整的下拉式選單，含滑鼠追蹤和 SaveBits/RestoreBits
- **檔案系統**：HFS，含 B-tree 實作，資料夾視窗支援 VFS 列舉
- **Window Manager**：拖曳、調整大小（含拖曳調整區塊）、圖層管理、啟用
- **Time Manager**：精確的 TSC 校準、微秒精度、世代檢查
- **Resource Manager**：O(log n) 二分搜尋、LRU 快取、完整驗證
- **Gestalt Manager**：多架構系統資訊，含架構偵測
- **TextEdit Manager**：完整的文字編輯，含剪貼簿整合
- **Scrap Manager**：經典 Mac OS 剪貼簿，支援多種資料格式
- **SimpleText 應用程式**：功能完整的 MDI 文字編輯器，含剪下/複製/貼上
- **List Manager**：相容 System 7.1 的列表控制項，含鍵盤導覽
- **Control Manager**：標準控制項和捲軸控制項，含 CDEF 實作
- **Dialog Manager**：鍵盤導覽、焦點環、鍵盤快速鍵
- **Segment Loader**：可攜式、與 ISA 無關的 68K 區段載入系統，含重新定位
- **M68K 直譯器**：完整的指令分派，含 84 個運算碼處理器、全部 14 種定址模式、例外/陷阱框架
- **Sound Manager**：命令處理、MIDI 轉換、通道管理、回呼
- **Device Manager**：DCE 管理、驅動程式安裝/移除和 I/O 操作
- **啟動畫面**：完整的開機 UI，含進度追蹤、階段管理和啟動畫面
- **Color Manager**：色彩狀態管理，整合 QuickDraw

### 部分實作 ⚠️

- **應用程式整合**：M68K 直譯器和區段載入器已完成；需要整合測試以驗證真實應用程式能否執行
- **視窗定義程序（WDEF）**：核心結構已就位，部分分派
- **Speech Manager**：僅有 API 框架和音訊直通；語音合成引擎尚未實作
- **例外處理（RTE）**：從例外返回僅部分實作（目前會暫停而非還原上下文）

### 尚未實作 ❌

- **列印**：無列印系統
- **網路**：無 AppleTalk 或網路功能
- **桌面配件**：僅有框架
- **進階音訊**：取樣播放、混音（受限於 PC 蜂鳴器）

### 未編譯的子系統 🔧

以下子系統具有原始碼但尚未整合至核心：
- **AppleEventManager**（8 個檔案）：應用程式間訊息傳遞；因 pthread 依賴與獨立環境不相容而刻意排除
- **FontResources**（僅標頭檔）：字型資源型別定義；實際字型支援由已編譯的 FontResourceLoader.c 提供

## 🏗️ 架構

### 技術規格

- **架構**：透過 HAL 實現多架構支援（x86、ARM、PowerPC 就緒）
- **開機協定**：Multiboot2（x86）、平台特定開機載入器
- **圖形**：VESA 框架緩衝區，800x600 @ 32 位元色彩
- **記憶體配置**：核心載入於 1MB 實體位址（x86）
- **計時**：與架構無關，具備微秒精度（RDTSC/計時器暫存器）
- **效能**：資源快取未命中 <15µs、快取命中 <2µs、計時器漂移 <100ppm

### 程式碼庫統計

- **225+ 個原始碼檔案**，約 57,500+ 行程式碼
- **145+ 個標頭檔**，橫跨 28+ 個子系統
- **69 種資源類型**，從 System 7.1 擷取
- **編譯時間**：在現代硬體上 3-5 秒
- **核心大小**：約 4.16 MB
- **ISO 大小**：約 12.5 MB

## 🔨 建置

### 需求

- 支援 32 位元的 **GCC**（64 位元系統需安裝 `gcc-multilib`）
- **GNU Make**
- **GRUB 工具**：`grub-mkrescue`（來自 `grub2-common` 或 `grub-pc-bin`）
- **QEMU** 用於測試（`qemu-system-i386`）
- **Python 3** 用於資源處理
- **xxd** 用於二進位轉換
- *（選用）* **powerpc-linux-gnu** 交叉編譯工具鏈，用於 PowerPC 建置

### Ubuntu/Debian 安裝

```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common
```

### 建置指令

```bash
# 建置核心（預設為 x86）
make

# 為特定平台建置
make PLATFORM=x86
make PLATFORM=arm        # 需要 ARM 裸機 GCC
make PLATFORM=ppc        # 實驗性；需要 PowerPC ELF 工具鏈

# 建立可開機 ISO
make iso

# 建置含所有語言
make LOCALE_FR=1 LOCALE_DE=1 LOCALE_ES=1 LOCALE_JA=1 LOCALE_ZH=1 LOCALE_KO=1 LOCALE_RU=1 LOCALE_UK=1 LOCALE_PL=1 LOCALE_CS=1 LOCALE_SQ=1 LOCALE_BG=1 LOCALE_HR=1 LOCALE_DA=1 LOCALE_NL=1 LOCALE_ET=1 LOCALE_FI=1 LOCALE_EL=1 LOCALE_HU=1 LOCALE_IS=1 LOCALE_IT=1 LOCALE_LV=1 LOCALE_LT=1 LOCALE_MK=1 LOCALE_ME=1 LOCALE_NO=1 LOCALE_PT=1 LOCALE_RO=1 LOCALE_SK=1 LOCALE_SL=1 LOCALE_SV=1 LOCALE_TR=1 LOCALE_HI=1 LOCALE_TW=1 LOCALE_AR=1 LOCALE_BN=1 LOCALE_UR=1

# 建置含單一額外語言
make LOCALE_FR=1

# 建置並在 QEMU 中執行
make run

# 清除建置產物
make clean

# 顯示建置統計資訊
make info
```

## 🚀 執行

### 快速開始（QEMU）

```bash
# 標準執行，含序列埠日誌
make run

# 手動指定選項
qemu-system-i386 -cdrom system71.iso -serial file:/tmp/serial.log -display sdl -vga std -m 256M
```

### QEMU 選項

```bash
# 含主控台序列埠輸出
qemu-system-i386 -cdrom system71.iso -serial stdio -display sdl -m 256M

# 無頭模式（無圖形顯示）
qemu-system-i386 -cdrom system71.iso -serial stdio -display none -m 256M

# 含 GDB 除錯
make debug
# 在另一個終端機中：gdb kernel.elf -ex "target remote :1234"
```

## 📚 文件

### 元件指南
- **Control Manager**：`docs/components/ControlManager/`
- **Dialog Manager**：`docs/components/DialogManager/`
- **Font Manager**：`docs/components/FontManager/`
- **序列埠日誌**：`docs/components/System/`
- **Event Manager**：`docs/components/EventManager.md`
- **Menu Manager**：`docs/components/MenuManager.md`
- **Window Manager**：`docs/components/WindowManager.md`
- **Resource Manager**：`docs/components/ResourceManager.md`

### 國際化
- **Locale Manager**：`include/LocaleManager/` — 執行期語言切換、開機時語言選擇
- **字串資源**：`resources/strings/` — 各語言 STR# 資源檔案（34 種語言）
- **擴充字型**：`include/chicago_font_extended.h` — Mac Roman 0x80-0xFF 字符，用於歐洲字元
- **CJK 支援**：`include/TextEncoding/CJKEncoding.h`、`include/FontManager/CJKFont.h` — 多位元組編碼和字型基礎架構

### 實作狀態
- **IMPLEMENTATION_PRIORITIES.md**：計畫工作和完成度追蹤
- **IMPLEMENTATION_STATUS_AUDIT.md**：所有子系統的詳細稽核

### 專案理念

**考古式方法**，以證據為基礎的實作：
1. 以 Inside Macintosh 文件和 MPW Universal Interfaces 為依據
2. 所有重大決策均標記 Finding ID，引用支援證據
3. 目標：與原始 System 7 達成行為一致性，而非現代化
4. 潔淨室實作（未使用任何 Apple 原始碼）

## 🐛 已知問題

1. **圖示拖曳殘影**：桌面圖示拖曳時有輕微視覺殘影
2. **M68K 執行為空殼**：區段載入器已完成，執行迴圈尚未實作
3. **不支援 TrueType**：僅有點陣字型（Chicago）
4. **HFS 唯讀**：虛擬檔案系統，無法實際寫回磁碟
5. **無穩定性保證**：當機和非預期行為時有發生

## 🤝 貢獻

本專案主要為學習/研究用途：

1. **錯誤回報**：提交 Issue 並附上詳細的重現步驟
2. **測試**：回報在不同硬體/模擬器上的測試結果
3. **文件**：改善現有文件或新增指南

## 📖 重要參考資料

- **Inside Macintosh**（1992-1994）：Apple 官方 Toolbox 文件
- **MPW Universal Interfaces 3.2**：標準標頭檔和結構體定義
- **Guide to Macintosh Family Hardware**：硬體架構參考

### 實用工具

- **Mini vMac**：System 7 模擬器，用於行為參考
- **ResEdit**：資源編輯器，用於研究 System 7 資源
- **Ghidra/IDA**：用於 ROM 反組譯分析

## ⚖️ 法律聲明

本專案為**潔淨室重新實作**，用於教育和保存目的：

- **未使用任何 Apple 原始碼**
- 僅基於公開文件和黑箱分析
- 「System 7」、「Macintosh」、「QuickDraw」為 Apple Inc. 的商標
- 與 Apple Inc. 無任何關聯、背書或贊助關係

**原始 System 7 ROM 和軟體仍為 Apple Inc. 的財產。**

## 🙏 致謝

- **Apple Computer, Inc.** 創造了原始的 System 7
- **Inside Macintosh 作者群** 提供了完整的文件
- **經典 Mac 保存社群** 持續維護此平台的活力
- **68k.news 和 Macintosh Garden** 提供資源典藏

## 📊 開發統計

- **程式碼行數**：約 57,500+（含 2,500+ 行用於區段載入器）
- **編譯時間**：約 3-5 秒
- **核心大小**：約 4.16 MB（kernel.elf）
- **ISO 大小**：約 12.5 MB（system71.iso）
- **錯誤減少**：94% 的核心功能已運作
- **主要子系統**：28+（Font、Window、Menu、Control、Dialog、TextEdit 等）

## 🔮 未來方向

**計畫工作**：

- 完成 M68K 直譯器執行迴圈
- 新增 TrueType 字型支援
- 為日文、中文和韓文渲染新增 CJK 點陣字型資源
- 實作額外控制項（文字欄位、彈出式選單、滑桿）
- HFS 檔案系統磁碟寫回
- 進階 Sound Manager 功能（混音、取樣）
- 基本桌面配件（計算機、記事本）

---

**狀態**：實驗性 - 教育性 - 開發中

**最後更新**：2025 年 11 月（Sound Manager 增強功能完成）

如有問題、議題或討論，請使用 GitHub Issues。
