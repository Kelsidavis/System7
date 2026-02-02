# System 7 - 可移植开源重新实现

**[English](README.md)** | **[Fran&ccedil;ais](README.fr.md)** | **[Deutsch](README.de.md)** | **[Espa&ntilde;ol](README.es.md)** | **[日本語](README.ja.md)** | **[中文](README.zh.md)** | **[한국어](README.ko.md)**

<img width="793" height="657" alt="在现代硬件上运行的System 7" src="https://github.com/user-attachments/assets/be84b83e-191c-4f9d-a786-11d0bd04203b" />
<img width="801" height="662" alt="simpletextworks" src="https://github.com/user-attachments/assets/7c9ebe5b-22b4-4612-93a1-2076909d77cd" />
<img width="803" height="661" alt="macpaint" src="https://github.com/user-attachments/assets/cd3ed04a-fdde-4dd5-88ef-5b19b3a13a54" />

> **概念验证** - 这是一个实验性、教育性的 Apple Macintosh System 7 重新实现。这不是一个成品，不应被视为生产级软件。

面向现代 x86 硬件的 Apple Macintosh System 7 开源重新实现，可通过 GRUB2/Multiboot2 引导启动。本项目旨在重现经典 Mac OS 体验，同时通过逆向工程分析记录 System 7 的架构。

## 项目状态

**当前状态**：积极开发中，核心功能约完成 ~94%

### 最新更新（2025 年 11 月）

#### Sound Manager 增强 -- 已完成
- **优化的 MIDI 转换**：共享 `SndMidiNoteToFreq()` 辅助函数，内含 37 条目查找表（C3-B5）和基于八度的回退机制，覆盖完整 MIDI 范围（0-127）
- **异步播放支持**：完整的回调基础设施，支持文件播放（`FilePlayCompletionUPP`）和命令执行（`SndCallBackProcPtr`）
- **基于通道的音频路由**：多级优先级系统，具有静音和启用控制
  - 4 级优先级通道（0-3），用于硬件输出路由
  - 每通道独立的静音和启用控制
  - `SndGetActiveChannel()` 返回最高优先级的活动通道
  - 通道初始化时默认启用
- **生产级实现**：所有代码编译无警告，未检测到 malloc/free 违规
- **提交记录**：07542c5（MIDI 优化）、1854fe6（异步回调）、a3433c6（通道路由）

#### 先前阶段成果
- **高级功能阶段**：Sound Manager 命令处理循环、多运行样式序列化、扩展 MIDI/合成功能
- **窗口调整大小系统**：交互式调整大小，支持正确的窗口边框处理、拖拽框和桌面清理
- **PS/2 键盘翻译**：完整的 Set 1 扫描码到 Toolbox 键码映射
- **多平台 HAL**：支持 x86、ARM 和 PowerPC，具有清晰的抽象层

## 项目完成度

**整体核心功能**：约 ~94% 完成（估计）

### 已完全实现

- **硬件抽象层（HAL）**：面向 x86/ARM/PowerPC 的完整平台抽象
- **引导系统**：在 x86 上通过 GRUB2/Multiboot2 成功引导
- **串行日志**：基于模块的日志系统，支持运行时过滤（Error/Warn/Info/Debug/Trace）
- **图形基础**：VESA 帧缓冲区（800x600x32），支持 QuickDraw 图形原语（包括 XOR 模式）
- **桌面渲染**：System 7 菜单栏，含彩虹 Apple 标志、图标和桌面图案
- **排版**：Chicago 位图字体，具有像素级精确渲染和正确的字距调整，扩展 Mac Roman 字符集（0x80-0xFF）支持欧洲语言重音字符
- **国际化（i18n）**：基于资源的本地化，支持 7 种语言（英语、法语、德语、西班牙语、日语、中文、韩语），Locale Manager 支持启动时语言选择，CJK 多字节编码基础设施
- **字体管理器**：多尺寸支持（9-24pt）、样式合成、FOND/NFNT 解析、LRU 缓存
- **输入系统**：PS/2 键盘和鼠标，完整事件转发
- **事件管理器**：通过 WaitNextEvent 实现协作式多任务，统一事件队列
- **内存管理器**：基于区域的分配，集成 68K 解释器
- **菜单管理器**：完整的下拉菜单，支持鼠标追踪和 SaveBits/RestoreBits
- **文件系统**：HFS，含 B-tree 实现、文件夹窗口和 VFS 枚举
- **窗口管理器**：拖拽、调整大小（含拖拽框）、图层管理、激活
- **时间管理器**：精确的 TSC 校准、微秒精度、代次检查
- **资源管理器**：O(log n) 二分查找、LRU 缓存、全面验证
- **Gestalt 管理器**：多架构系统信息查询与架构检测
- **TextEdit 管理器**：完整的文本编辑功能，集成剪贴板
- **Scrap 管理器**：经典 Mac OS 剪贴板，支持多种数据格式
- **SimpleText 应用程序**：功能完整的 MDI 文本编辑器，支持剪切/复制/粘贴
- **列表管理器**：兼容 System 7.1 的列表控件，支持键盘导航
- **控件管理器**：标准控件和滚动条控件，含 CDEF 实现
- **对话框管理器**：键盘导航、焦点环、键盘快捷键
- **段加载器**：可移植的、与 ISA 无关的 68K 段加载系统，支持重定位
- **M68K 解释器**：完整的指令分发，含 84 个操作码处理器、全部 14 种寻址模式、异常/陷阱框架
- **Sound Manager**：命令处理、MIDI 转换、通道管理、回调
- **设备管理器**：DCE 管理、驱动程序安装/卸载和 I/O 操作
- **启动画面**：完整的启动界面，含进度跟踪、阶段管理和闪屏
- **颜色管理器**：颜色状态管理，集成 QuickDraw

### 部分实现

- **应用程序集成**：M68K 解释器和段加载器已完成；需要集成测试以验证实际应用程序的执行
- **窗口定义过程（WDEF）**：核心结构就绪，部分分发已实现
- **语音管理器**：仅有 API 框架和音频直通；语音合成引擎未实现
- **异常处理（RTE）**：从异常返回仅部分实现（当前执行暂停而非恢复上下文）

### 尚未实现

- **打印**：无打印系统
- **网络**：无 AppleTalk 或网络功能
- **桌面附件**：仅有框架
- **高级音频**：采样播放、混音（受 PC 扬声器限制）

### 未编译的子系统

以下子系统拥有源代码，但未集成到内核中：
- **AppleEventManager**（8 个文件）：应用程序间消息传递；由于 pthread 依赖与独立运行环境不兼容，已被刻意排除
- **FontResources**（仅头文件）：字体资源类型定义；实际字体支持由已编译的 FontResourceLoader.c 提供

## 架构

### 技术规格

- **架构**：通过 HAL 支持多架构（x86、ARM、PowerPC 就绪）
- **引导协议**：Multiboot2（x86）、平台特定的引导加载器
- **图形**：VESA 帧缓冲区，800x600 @ 32 位色彩
- **内存布局**：内核加载于物理地址 1MB 处（x86）
- **计时**：架构无关，微秒精度（RDTSC/定时器寄存器）
- **性能**：资源冷缺失 <15us，缓存命中 <2us，定时器漂移 <100ppm

### 代码库统计

- **225+ 源文件**，约 ~57,500+ 行代码
- **145+ 头文件**，跨 28+ 个子系统
- **69 种资源类型**，提取自 System 7.1
- **编译时间**：在现代硬件上约 3-5 秒
- **内核大小**：约 4.16 MB
- **ISO 大小**：约 12.5 MB

## 构建

### 系统要求

- **GCC**，需 32 位支持（64 位系统上需安装 `gcc-multilib`）
- **GNU Make**
- **GRUB 工具**：`grub-mkrescue`（来自 `grub2-common` 或 `grub-pc-bin`）
- **QEMU** 用于测试（`qemu-system-i386`）
- **Python 3** 用于资源处理
- **xxd** 用于二进制转换
- *（可选）* **powerpc-linux-gnu** 交叉编译工具链，用于 PowerPC 构建

### Ubuntu/Debian 安装

```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common
```

### 构建命令

```bash
# 构建内核（默认为 x86）
make

# 为特定平台构建
make PLATFORM=x86
make PLATFORM=arm        # 需要 ARM 裸机 GCC
make PLATFORM=ppc        # 实验性；需要 PowerPC ELF 工具链

# 创建可引导 ISO
make iso

# 构建时包含所有语言（法语、德语、西班牙语、日语、中文、韩语）
make LOCALE_FR=1 LOCALE_DE=1 LOCALE_ES=1 LOCALE_JA=1 LOCALE_ZH=1 LOCALE_KO=1

# 构建时包含单一额外语言
make LOCALE_FR=1

# 构建并在 QEMU 中运行
make run

# 清理构建产物
make clean

# 显示构建统计信息
make info
```

## 运行

### 快速开始（QEMU）

```bash
# 标准运行，启用串行日志
make run

# 手动运行并指定选项
qemu-system-i386 -cdrom system71.iso -serial file:/tmp/serial.log -display sdl -vga std -m 256M
```

### QEMU 选项

```bash
# 控制台串行输出
qemu-system-i386 -cdrom system71.iso -serial stdio -display sdl -m 256M

# 无头模式（无图形显示）
qemu-system-i386 -cdrom system71.iso -serial stdio -display none -m 256M

# 使用 GDB 调试
make debug
# 在另一个终端中：gdb kernel.elf -ex "target remote :1234"
```

## 文档

### 组件指南
- **控件管理器**：`docs/components/ControlManager/`
- **对话框管理器**：`docs/components/DialogManager/`
- **字体管理器**：`docs/components/FontManager/`
- **串行日志**：`docs/components/System/`
- **事件管理器**：`docs/components/EventManager.md`
- **菜单管理器**：`docs/components/MenuManager.md`
- **窗口管理器**：`docs/components/WindowManager.md`
- **资源管理器**：`docs/components/ResourceManager.md`

### 国际化
- **Locale Manager**：`include/LocaleManager/` -- 运行时区域设置切换、启动时语言选择
- **字符串资源**：`resources/strings/` -- 按语言分类的 STR# 资源文件（en、fr、de、es、ja、zh、ko）
- **扩展字体**：`include/chicago_font_extended.h` -- Mac Roman 0x80-0xFF 字形，用于欧洲语言字符
- **CJK 支持**：`include/TextEncoding/CJKEncoding.h`、`include/FontManager/CJKFont.h` -- 多字节编码和字体基础设施

### 实现状态
- **IMPLEMENTATION_PRIORITIES.md**：计划中的工作和完成度跟踪
- **IMPLEMENTATION_STATUS_AUDIT.md**：所有子系统的详细审计

### 项目理念

**考古式方法**，基于证据的实现：
1. 以 Inside Macintosh 文档和 MPW Universal Interfaces 为依据
2. 所有重大决策均标记有 Finding ID，引用支持性证据
3. 目标：与原始 System 7 实现行为一致性，而非现代化改造
4. 洁净室实现（未使用任何 Apple 原始源代码）

## 已知问题

1. **图标拖拽伪影**：桌面图标拖拽时存在轻微视觉伪影
2. **M68K 执行为桩代码**：段加载器已完成，执行循环未实现
3. **无 TrueType 支持**：仅支持位图字体（Chicago）
4. **HFS 为只读**：虚拟文件系统，不支持真实磁盘回写
5. **无稳定性保证**：崩溃和意外行为较为常见

## 贡献

本项目主要用于学习和研究：

1. **错误报告**：提交 issue 并附上详细的复现步骤
2. **测试**：报告在不同硬件/模拟器上的测试结果
3. **文档**：改进现有文档或添加新指南

## 重要参考资料

- **Inside Macintosh**（1992-1994）：Apple 官方 Toolbox 文档
- **MPW Universal Interfaces 3.2**：权威头文件和结构体定义
- **Guide to Macintosh Family Hardware**：硬件架构参考

### 实用工具

- **Mini vMac**：System 7 模拟器，用于行为参考
- **ResEdit**：资源编辑器，用于研究 System 7 资源
- **Ghidra/IDA**：用于 ROM 反汇编分析

## 法律声明

本项目是出于教育和保存目的的**洁净室重新实现**：

- **未使用任何 Apple 源代码**
- 仅基于公开文档和黑箱分析
- "System 7"、"Macintosh"、"QuickDraw" 是 Apple Inc. 的商标
- 本项目与 Apple Inc. 无关联、未获其认可或赞助

**原始 System 7 ROM 和软件仍为 Apple Inc. 的财产。**

## 致谢

- **Apple Computer, Inc.** 创造了原始的 System 7
- **Inside Macintosh 作者团队** 编写了全面详尽的文档
- **经典 Mac 保存社区** 使这个平台得以延续
- **68k.news 和 Macintosh Garden** 提供了资源档案

## 开发统计

- **代码行数**：约 ~57,500+（其中段加载器约 2,500+）
- **编译时间**：约 3-5 秒
- **内核大小**：约 4.16 MB（kernel.elf）
- **ISO 大小**：约 12.5 MB（system71.iso）
- **错误率降低**：94% 的核心功能已正常运行
- **主要子系统**：28+（字体、窗口、菜单、控件、对话框、TextEdit 等）

## 未来方向

**计划中的工作**：

- 完成 M68K 解释器执行循环
- 添加 TrueType 字体支持
- 为日语、中文和韩语渲染添加 CJK 位图字体资源
- 实现额外控件（文本框、弹出菜单、滑块）
- HFS 文件系统磁盘回写
- 高级 Sound Manager 功能（混音、采样）
- 基本桌面附件（计算器、备忘录）

---

**状态**：实验性 - 教育性 - 开发中

**最后更新**：2025 年 11 月（Sound Manager 增强已完成）

如有问题、议题或讨论，请使用 GitHub Issues。
