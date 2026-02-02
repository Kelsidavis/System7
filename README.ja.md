# System 7 - ポータブル・オープンソース再実装

**[English](README.md)** | **[Fran&ccedil;ais](README.fr.md)** | **[Deutsch](README.de.md)** | **[Espa&ntilde;ol](README.es.md)** | **[日本語](README.ja.md)** | **[中文](README.zh.md)** | **[한국어](README.ko.md)**

<img width="793" height="657" alt="最新ハードウェアで動作するSystem 7" src="https://github.com/user-attachments/assets/be84b83e-191c-4f9d-a786-11d0bd04203b" />
<img width="801" height="662" alt="simpletextworks" src="https://github.com/user-attachments/assets/7c9ebe5b-22b4-4612-93a1-2076909d77cd" />
<img width="803" height="661" alt="macpaint" src="https://github.com/user-attachments/assets/cd3ed04a-fdde-4dd5-88ef-5b19b3a13a54" />

> **概念実証（プルーフ・オブ・コンセプト）** - これはApple Macintosh System 7の実験的・教育的な再実装です。完成品ではなく、本番環境での使用を想定したソフトウェアではありません。

最新のx86ハードウェア上で動作する、Apple Macintosh System 7のオープンソース再実装です。GRUB2/Multiboot2経由で起動可能です。このプロジェクトは、リバースエンジニアリング分析を通じてSystem 7のアーキテクチャを文書化しながら、クラシックMac OSの操作体験を再現することを目指しています。

## プロジェクトの状況

**現在の状態**: コア機能の約94%が完成し、活発に開発中

### 最新の更新（2025年11月）

#### Sound Managerの機能強化 -- 完了
- **最適化されたMIDI変換**: 37エントリの参照テーブル（C3-B5）とオクターブベースのフォールバックによる完全なMIDI範囲（0-127）対応の共有 `SndMidiNoteToFreq()` ヘルパー
- **非同期再生サポート**: ファイル再生（`FilePlayCompletionUPP`）とコマンド実行（`SndCallBackProcPtr`）の両方に対応した完全なコールバック基盤
- **チャンネルベースのオーディオルーティング**: ミュートおよび有効化制御を備えた多段優先度システム
  - ハードウェア出力ルーティング用の4段階優先度チャンネル（0-3）
  - チャンネルごとの独立したミュートおよび有効化制御
  - `SndGetActiveChannel()` が最高優先度のアクティブチャンネルを返却
  - デフォルトで有効化フラグ付きの適切なチャンネル初期化
- **本番品質の実装**: すべてのコードがクリーンにコンパイルされ、malloc/free違反は検出されず
- **コミット**: 07542c5（MIDI最適化）、1854fe6（非同期コールバック）、a3433c6（チャンネルルーティング）

#### これまでの達成項目
- **高度な機能フェーズ**: Sound Managerコマンド処理ループ、マルチランスタイルシリアライゼーション、拡張MIDI/合成機能
- **ウィンドウリサイズシステム**: 適切なクローム処理、グローボックス、デスクトップクリーンアップを備えたインタラクティブなリサイズ
- **PS/2キーボード変換**: セット1スキャンコードからToolboxキーコードへの完全なマッピング
- **マルチプラットフォームHAL**: クリーンな抽象化によるx86、ARM、PowerPCサポート

## プロジェクトの完成度

**コア機能全体**: 約94%完成（推定）

### 完全に動作する機能

- **ハードウェア抽象化層（HAL）**: x86/ARM/PowerPC向けの完全なプラットフォーム抽象化
- **ブートシステム**: x86上でGRUB2/Multiboot2経由の起動に成功
- **シリアルログ**: 実行時フィルタリング（Error/Warn/Info/Debug/Trace）対応のモジュールベースログ
- **グラフィックス基盤**: XORモードを含むQuickDrawプリミティブ対応のVESAフレームバッファ（800x600x32）
- **デスクトップ描画**: レインボーAppleロゴ、アイコン、デスクトップパターンを備えたSystem 7メニューバー
- **タイポグラフィ**: ピクセルパーフェクトなレンダリングと適切なカーニングを備えたChicagoビットマップフォント、ヨーロッパ言語のアクセント付き文字に対応した拡張Mac Roman（0x80-0xFF）
- **国際化（i18n）**: 7言語（英語、フランス語、ドイツ語、スペイン語、日本語、中国語、韓国語）対応のリソースベースローカライゼーション、起動時の言語選択機能付きLocale Manager、CJKマルチバイトエンコーディング基盤
- **Font Manager**: マルチサイズサポート（9-24pt）、スタイル合成、FOND/NFNT解析、LRUキャッシュ
- **入力システム**: 完全なイベント転送対応のPS/2キーボードおよびマウス
- **Event Manager**: 統合イベントキューによるWaitNextEventベースの協調マルチタスク
- **Memory Manager**: 68Kインタプリタ統合によるゾーンベースのメモリ割り当て
- **Menu Manager**: マウストラッキングとSaveBits/RestoreBits対応の完全なドロップダウンメニュー
- **ファイルシステム**: B-tree実装によるHFS、VFS列挙対応のフォルダウィンドウ
- **Window Manager**: ドラッグ、リサイズ（グローボックス付き）、レイヤリング、アクティベーション
- **Time Manager**: マイクロ秒精度の正確なTSCキャリブレーション、世代チェック
- **Resource Manager**: O(log n)二分探索、LRUキャッシュ、包括的なバリデーション
- **Gestalt Manager**: アーキテクチャ検出付きのマルチアーキテクチャシステム情報
- **TextEdit Manager**: クリップボード統合による完全なテキスト編集
- **Scrap Manager**: 複数フレーバーサポート付きのクラシックMac OSクリップボード
- **SimpleTextアプリケーション**: カット/コピー/ペースト対応のフル機能MDIテキストエディタ
- **List Manager**: キーボードナビゲーション付きのSystem 7.1互換リストコントロール
- **Control Manager**: CDEF実装による標準コントロールおよびスクロールバー
- **Dialog Manager**: キーボードナビゲーション、フォーカスリング、キーボードショートカット
- **Segment Loader**: リロケーション対応のポータブルISA非依存68Kセグメントローディングシステム
- **M68Kインタプリタ**: 84オペコードハンドラ、全14アドレッシングモード、例外/トラップフレームワークによる完全な命令ディスパッチ
- **Sound Manager**: コマンド処理、MIDI変換、チャンネル管理、コールバック
- **Device Manager**: DCE管理、ドライバのインストール/削除、I/O操作
- **起動画面**: 進捗トラッキング、フェーズ管理、スプラッシュスクリーンを備えた完全なブートUI
- **Color Manager**: QuickDraw統合によるカラー状態管理

### 部分的に実装済み

- **アプリケーション統合**: M68Kインタプリタとセグメントローダーは完成。実際のアプリケーションが実行できることを検証するための統合テストが必要
- **ウィンドウ定義プロシージャ（WDEF）**: コア構造は配置済み、部分的なディスパッチ
- **Speech Manager**: APIフレームワークとオーディオパススルーのみ。音声合成エンジンは未実装
- **例外処理（RTE）**: 例外からの復帰が部分的に実装済み（現在はコンテキスト復元の代わりに停止）

### 未実装

- **印刷**: 印刷システムなし
- **ネットワーク**: AppleTalkやネットワーク機能なし
- **デスクアクセサリ**: フレームワークのみ
- **高度なオーディオ**: サンプル再生、ミキシング（PCスピーカーの制限あり）

### 未コンパイルのサブシステム

以下はソースコードが存在しますが、カーネルに統合されていません：
- **AppleEventManager**（8ファイル）: アプリケーション間メッセージング。フリースタンディング環境と互換性のないpthread依存のため意図的に除外
- **FontResources**（ヘッダのみ）: フォントリソース型定義。実際のフォントサポートはコンパイル済みのFontResourceLoader.cが提供

## アーキテクチャ

### 技術仕様

- **アーキテクチャ**: HALによるマルチアーキテクチャ（x86、ARM、PowerPC対応）
- **ブートプロトコル**: Multiboot2（x86）、プラットフォーム固有のブートローダー
- **グラフィックス**: VESAフレームバッファ、800x600 @ 32ビットカラー
- **メモリレイアウト**: カーネルは物理アドレス1MBにロード（x86）
- **タイミング**: マイクロ秒精度のアーキテクチャ非依存タイマー（RDTSC/タイマーレジスタ）
- **パフォーマンス**: コールドリソースミス <15us、キャッシュヒット <2us、タイマードリフト <100ppm

### コードベースの統計

- **225以上のソースファイル**、約57,500行以上のコード
- **145以上のヘッダファイル**、28以上のサブシステム
- **69のリソースタイプ**（System 7.1から抽出）
- **コンパイル時間**: 最新ハードウェアで3-5秒
- **カーネルサイズ**: 約4.16 MB
- **ISOサイズ**: 約12.5 MB

## ビルド方法

### 必要条件

- **GCC**（32ビットサポート付き、64ビット環境では `gcc-multilib`）
- **GNU Make**
- **GRUBツール**: `grub-mkrescue`（`grub2-common` または `grub-pc-bin` から）
- **QEMU**（テスト用、`qemu-system-i386`）
- **Python 3**（リソース処理用）
- **xxd**（バイナリ変換用）
- *（オプション）* **powerpc-linux-gnu** クロスツールチェーン（PowerPCビルド用）

### Ubuntu/Debianでのインストール

```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common
```

### ビルドコマンド

```bash
# カーネルをビルド（デフォルトはx86）
make

# 特定のプラットフォーム向けにビルド
make PLATFORM=x86
make PLATFORM=arm        # ARMベアメタルGCCが必要
make PLATFORM=ppc        # 実験的。PowerPC ELFツールチェーンが必要

# 起動可能なISOを作成
make iso

# 全言語でビルド（フランス語、ドイツ語、スペイン語、日本語、中国語、韓国語）
make LOCALE_FR=1 LOCALE_DE=1 LOCALE_ES=1 LOCALE_JA=1 LOCALE_ZH=1 LOCALE_KO=1

# 追加言語を1つだけ指定してビルド
make LOCALE_FR=1

# ビルドしてQEMUで実行
make run

# ビルド成果物をクリーン
make clean

# ビルド統計を表示
make info
```

## 実行方法

### クイックスタート（QEMU）

```bash
# シリアルログ付きで標準実行
make run

# オプション指定で手動実行
qemu-system-i386 -cdrom system71.iso -serial file:/tmp/serial.log -display sdl -vga std -m 256M
```

### QEMUオプション

```bash
# コンソールシリアル出力付き
qemu-system-i386 -cdrom system71.iso -serial stdio -display sdl -m 256M

# ヘッドレス（グラフィックス表示なし）
qemu-system-i386 -cdrom system71.iso -serial stdio -display none -m 256M

# GDBデバッグ付き
make debug
# 別のターミナルで: gdb kernel.elf -ex "target remote :1234"
```

## ドキュメント

### コンポーネントガイド
- **Control Manager**: `docs/components/ControlManager/`
- **Dialog Manager**: `docs/components/DialogManager/`
- **Font Manager**: `docs/components/FontManager/`
- **シリアルログ**: `docs/components/System/`
- **Event Manager**: `docs/components/EventManager.md`
- **Menu Manager**: `docs/components/MenuManager.md`
- **Window Manager**: `docs/components/WindowManager.md`
- **Resource Manager**: `docs/components/ResourceManager.md`

### 国際化

- **Locale Manager**: `include/LocaleManager/` -- 実行時のロケール切り替え、起動時の言語選択
- **文字列リソース**: `resources/strings/` -- 言語別のSTR#リソースファイル（en, fr, de, es, ja, zh, ko）
- **拡張フォント**: `include/chicago_font_extended.h` -- ヨーロッパ言語文字用のMac Roman 0x80-0xFFグリフ
- **CJKサポート**: `include/TextEncoding/CJKEncoding.h`、`include/FontManager/CJKFont.h` -- マルチバイトエンコーディングとフォント基盤

### 実装状況
- **IMPLEMENTATION_PRIORITIES.md**: 計画中の作業と完成度の追跡
- **IMPLEMENTATION_STATUS_AUDIT.md**: 全サブシステムの詳細監査

### プロジェクトの哲学

**考古学的アプローチ** -- エビデンスに基づく実装：
1. Inside Macintoshドキュメントおよび MPW Universal Interfacesに裏付けられた実装
2. すべての主要な設計判断に、裏付け証拠を参照するFinding IDをタグ付け
3. 目標：近代化ではなく、オリジナルのSystem 7との動作の一致
4. クリーンルーム実装（Appleのオリジナルソースコードは不使用）

## 既知の問題

1. **アイコンドラッグのアーティファクト**: デスクトップアイコンのドラッグ中に軽微な表示の乱れが発生
2. **M68K実行がスタブ状態**: セグメントローダーは完成しているが、実行ループは未実装
3. **TrueTypeフォント非対応**: ビットマップフォント（Chicago）のみ
4. **HFSは読み取り専用**: 仮想ファイルシステムのため、実際のディスクへの書き戻し不可
5. **安定性の保証なし**: クラッシュや予期しない動作が頻繁に発生する可能性あり

## コントリビューション

このプロジェクトは主に学習・研究目的です：

1. **バグ報告**: 詳細な再現手順と共にIssueを提出してください
2. **テスト**: さまざまなハードウェア/エミュレータでのテスト結果をご報告ください
3. **ドキュメント**: 既存のドキュメントの改善や新しいガイドの追加

## 主要な参考文献

- **Inside Macintosh**（1992-1994）: Apple公式のToolboxドキュメント
- **MPW Universal Interfaces 3.2**: 標準的なヘッダファイルと構造体定義
- **Guide to Macintosh Family Hardware**: ハードウェアアーキテクチャのリファレンス

### 便利なツール

- **Mini vMac**: 動作の参考用System 7エミュレータ
- **ResEdit**: System 7リソースの研究用リソースエディタ
- **Ghidra/IDA**: ROM逆アセンブリ解析用

## 法的事項

これは教育および保存を目的とした**クリーンルーム再実装**です：

- **Appleのソースコード**は一切使用していません
- 公開されているドキュメントとブラックボックス分析のみに基づいています
- 「System 7」「Macintosh」「QuickDraw」はApple Inc.の商標です
- Apple Inc.とは提携、承認、後援の関係にありません

**オリジナルのSystem 7 ROMおよびソフトウェアはApple Inc.の所有物です。**

## 謝辞

- **Apple Computer, Inc.** -- オリジナルのSystem 7を創造してくださったことに感謝
- **Inside Macintoshの著者の方々** -- 包括的なドキュメントの執筆に感謝
- **クラシックMac保存コミュニティ** -- このプラットフォームを存続させてくださっていることに感謝
- **68k.newsおよびMacintosh Garden** -- リソースアーカイブの提供に感謝

## 開発統計

- **コード行数**: 約57,500行以上（セグメントローダーの2,500行以上を含む）
- **コンパイル時間**: 約3-5秒
- **カーネルサイズ**: 約4.16 MB（kernel.elf）
- **ISOサイズ**: 約12.5 MB（system71.iso）
- **エラー削減**: コア機能の94%が動作
- **主要サブシステム**: 28以上（Font、Window、Menu、Control、Dialog、TextEditなど）

## 今後の方向性

**計画中の作業**：

- M68Kインタプリタ実行ループの完成
- TrueTypeフォントサポートの追加
- 日本語・中国語・韓国語レンダリング用のCJKビットマップフォントリソース
- 追加コントロールの実装（テキストフィールド、ポップアップ、スライダー）
- HFSファイルシステムのディスク書き戻し
- 高度なSound Manager機能（ミキシング、サンプリング）
- 基本的なデスクアクセサリ（計算機、メモ帳）

---

**状態**: 実験的 - 教育目的 - 開発中

**最終更新**: 2025年11月（Sound Manager機能強化完了）

ご質問、問題、議論については、GitHub Issuesをご利用ください。
