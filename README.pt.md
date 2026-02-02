# System 7 - Reimplementacao Portatil de Codigo Aberto

**[English](README.md)** | **[Fran&ccedil;ais](README.fr.md)** | **[Deutsch](README.de.md)** | **[Espa&ntilde;ol](README.es.md)** | **[Italiano](README.it.md)** | **[Portugu&ecirc;s](README.pt.md)** | **[Nederlands](README.nl.md)** | **[Dansk](README.da.md)** | **[Norsk](README.no.md)** | **[Svenska](README.sv.md)** | **[Suomi](README.fi.md)** | **[&Iacute;slenska](README.is.md)** | **[Ελληνικά](README.el.md)** | **[T&uuml;rk&ccedil;e](README.tr.md)** | **[Polski](README.pl.md)** | **[Čeština](README.cs.md)** | **[Slovenčina](README.sk.md)** | **[Slovenščina](README.sl.md)** | **[Hrvatski](README.hr.md)** | **[Magyar](README.hu.md)** | **[Rom&acirc;n&atilde;](README.ro.md)** | **[Български](README.bg.md)** | **[Shqip](README.sq.md)** | **[Eesti](README.et.md)** | **[Latviešu](README.lv.md)** | **[Lietuvių](README.lt.md)** | **[Македонски](README.mk.md)** | **[Crnogorski](README.me.md)** | **[Русский](README.ru.md)** | **[Українська](README.uk.md)** | **[العربية](README.ar.md)** | **[日本語](README.ja.md)** | **[简体中文](README.zh.md)** | **[繁體中文](README.tw.md)** | **[한국어](README.ko.md)** | **[हिन्दी](README.hi.md)** | **[বাংলা](README.bn.md)** | **[اردو](README.ur.md)**

<img width="793" height="657" alt="System 7 a funcionar em hardware moderno" src="https://github.com/user-attachments/assets/be84b83e-191c-4f9d-a786-11d0bd04203b" />
<img width="801" height="662" alt="simpletextworks" src="https://github.com/user-attachments/assets/7c9ebe5b-22b4-4612-93a1-2076909d77cd" />
<img width="803" height="661" alt="macpaint" src="https://github.com/user-attachments/assets/cd3ed04a-fdde-4dd5-88ef-5b19b3a13a54" />

> ⚠️ **PROVA DE CONCEITO** - Esta e uma reimplementacao experimental e educacional do Apple Macintosh System 7. NAO e um produto acabado e nao deve ser considerado software pronto para producao.

Uma reimplementacao de codigo aberto do Apple Macintosh System 7 para hardware x86 moderno, inicializavel via GRUB2/Multiboot2. Este projeto visa recriar a experiencia classica do Mac OS enquanto documenta a arquitetura do System 7 atraves de analise de engenharia reversa.

## 🎯 Estado do Projeto

**Estado Atual**: Desenvolvimento ativo com ~94% da funcionalidade principal concluida

### Ultimas Atualizacoes (Novembro de 2025)

#### Melhorias no Sound Manager ✅ CONCLUIDO
- **Conversao MIDI otimizada**: Funcao auxiliar partilhada `SndMidiNoteToFreq()` com tabela de consulta de 37 entradas (C3-B5) e fallback baseado em oitavas para a gama MIDI completa (0-127)
- **Suporte a reproducao assincrona**: Infraestrutura completa de callbacks tanto para reproducao de ficheiros (`FilePlayCompletionUPP`) como para execucao de comandos (`SndCallBackProcPtr`)
- **Encaminhamento de audio baseado em canais**: Sistema de prioridade multi-nivel com controlos de silencio e ativacao
  - 4 niveis de prioridade de canais (0-3) para encaminhamento de saida de hardware
  - Controlos independentes de silencio e ativacao por canal
  - `SndGetActiveChannel()` retorna o canal ativo de maior prioridade
  - Inicializacao adequada de canais com flag de ativacao por defeito
- **Implementacao com qualidade de producao**: Todo o codigo compila sem avisos, nenhuma violacao de malloc/free detetada
- **Commits**: 07542c5 (otimizacao MIDI), 1854fe6 (callbacks assincronos), a3433c6 (encaminhamento de canais)

#### Realizacoes de Sessoes Anteriores
- ✅ **Fase de Funcionalidades Avancadas**: Ciclo de processamento de comandos do Sound Manager, serializacao de estilos multi-run, funcionalidades estendidas de MIDI/sintese
- ✅ **Sistema de Redimensionamento de Janelas**: Redimensionamento interativo com tratamento adequado do chrome, grow box e limpeza do ambiente de trabalho
- ✅ **Traducao de Teclado PS/2**: Mapeamento completo de scancodes do conjunto 1 para codigos de tecla do Toolbox
- ✅ **HAL Multi-plataforma**: Suporte para x86, ARM e PowerPC com abstracao limpa

## 📊 Grau de Conclusao do Projeto

**Funcionalidade Principal Global**: ~94% concluida (estimativa)

### O Que Funciona Totalmente ✅

- **Camada de Abstracao de Hardware (HAL)**: Abstracao de plataforma completa para x86/ARM/PowerPC
- **Sistema de Arranque**: Arranca com sucesso via GRUB2/Multiboot2 em x86
- **Registo via Porta Serie**: Registo baseado em modulos com filtragem em tempo de execucao (Error/Warn/Info/Debug/Trace)
- **Base Grafica**: Framebuffer VESA (800x600x32) com primitivas QuickDraw incluindo modo XOR
- **Renderizacao do Ambiente de Trabalho**: Barra de menus do System 7 com logotipo arco-iris da Apple, icones e padroes do ambiente de trabalho
- **Tipografia**: Fonte bitmap Chicago com renderizacao pixel-perfect e kerning adequado, Mac Roman estendido (0x80-0xFF) para caracteres europeus acentuados
- **Internacionalizacao (i18n)**: Localizacao baseada em recursos com 38 idiomas (Ingles, Frances, Alemao, Espanhol, Italiano, Portugues, Neerlandes, Dinamarques, Noruegues, Sueco, Finlandes, Islandes, Grego, Turco, Polaco, Checo, Eslovaco, Esloveno, Croata, Hungaro, Romeno, Bulgaro, Albanes, Estoniano, Letao, Lituano, Macedonio, Montenegrino, Russo, Ucraniano, Arabe, Japones, Chines Simplificado, Chines Tradicional, Coreano, Hindi, Bengali, Urdu), Locale Manager com selecao de idioma no arranque, infraestrutura de codificacao multi-byte CJK
- **Font Manager**: Suporte multi-tamanho (9-24pt), sintese de estilos, analise FOND/NFNT, cache LRU
- **Sistema de Entrada**: Teclado e rato PS/2 com encaminhamento completo de eventos
- **Event Manager**: Multitarefa cooperativa via WaitNextEvent com fila de eventos unificada
- **Memory Manager**: Alocacao baseada em zonas com integracao do interpretador 68K
- **Menu Manager**: Menus suspensos completos com rastreamento do rato e SaveBits/RestoreBits
- **Sistema de Ficheiros**: HFS com implementacao de B-tree, janelas de pastas com enumeracao VFS
- **Window Manager**: Arrastar, redimensionar (com grow box), camadas, ativacao
- **Time Manager**: Calibracao TSC precisa, precisao ao microssegundo, verificacao de geracao
- **Resource Manager**: Pesquisa binaria O(log n), cache LRU, validacao abrangente
- **Gestalt Manager**: Informacao de sistema multi-arquitetura com detecao de arquitetura
- **TextEdit Manager**: Edicao de texto completa com integracao de area de transferencia
- **Scrap Manager**: Area de transferencia classica do Mac OS com suporte a multiplos formatos
- **Aplicacao SimpleText**: Editor de texto MDI completo com cortar/copiar/colar
- **List Manager**: Controlos de lista compativeis com System 7.1 com navegacao por teclado
- **Control Manager**: Controlos standard e barras de deslocamento com implementacao CDEF
- **Dialog Manager**: Navegacao por teclado, aneis de foco, atalhos de teclado
- **Segment Loader**: Sistema de carregamento de segmentos 68K portatil e agnositco de ISA com realocacao
- **Interpretador M68K**: Despacho completo de instrucoes com 84 manipuladores de opcodes, todos os 14 modos de enderecamento, framework de excecoes/traps
- **Sound Manager**: Processamento de comandos, conversao MIDI, gestao de canais, callbacks
- **Device Manager**: Gestao de DCE, instalacao/remocao de drivers e operacoes de E/S
- **Ecra de Arranque**: Interface de arranque completa com acompanhamento de progresso, gestao de fases e ecra de abertura
- **Color Manager**: Gestao de estado de cor com integracao QuickDraw

### Parcialmente Implementado ⚠️

- **Integracao de Aplicacoes**: Interpretador M68K e segment loader completos; testes de integracao necessarios para verificar se aplicacoes reais executam
- **Procedimentos de Definicao de Janela (WDEF)**: Estrutura base implementada, despacho parcial
- **Speech Manager**: Framework de API e passthrough de audio apenas; motor de sintese de voz nao implementado
- **Tratamento de Excecoes (RTE)**: Retorno de excecao parcialmente implementado (atualmente para em vez de restaurar o contexto)

### Ainda Nao Implementado ❌

- **Impressao**: Sem sistema de impressao
- **Rede**: Sem AppleTalk ou funcionalidade de rede
- **Acessorios de Secretaria**: Apenas framework
- **Audio Avancado**: Reproducao de amostras, mistura (limitacao do speaker do PC)

### Subsistemas Nao Compilados 🔧

Os seguintes possuem codigo-fonte mas nao estao integrados no kernel:
- **AppleEventManager** (8 ficheiros): Mensagens entre aplicacoes; deliberadamente excluido devido a dependencias de pthread incompativeis com ambiente freestanding
- **FontResources** (apenas cabecalho): Definicoes de tipos de recursos de fontes; o suporte real a fontes e fornecido pelo FontResourceLoader.c compilado

## 🏗️ Arquitetura

### Especificacoes Tecnicas

- **Arquitetura**: Multi-arquitetura via HAL (x86, ARM, PowerPC preparados)
- **Protocolo de Arranque**: Multiboot2 (x86), bootloaders especificos por plataforma
- **Graficos**: Framebuffer VESA, 800x600 @ cor de 32 bits
- **Disposicao de Memoria**: Kernel carregado no endereco fisico 1MB (x86)
- **Temporizacao**: Agnostica de arquitetura com precisao ao microssegundo (RDTSC/registos de temporizador)
- **Desempenho**: Falha de cache de recursos <15us, acerto de cache <2us, desvio de temporizador <100ppm

### Estatisticas da Base de Codigo

- **225+ ficheiros-fonte** com ~57.500+ linhas de codigo
- **145+ ficheiros de cabecalho** em 28+ subsistemas
- **69 tipos de recursos** extraidos do System 7.1
- **Tempo de compilacao**: 3-5 segundos em hardware moderno
- **Tamanho do kernel**: ~4,16 MB
- **Tamanho do ISO**: ~12,5 MB

## 🔨 Compilacao

### Requisitos

- **GCC** com suporte a 32 bits (`gcc-multilib` em sistemas de 64 bits)
- **GNU Make**
- **Ferramentas GRUB**: `grub-mkrescue` (de `grub2-common` ou `grub-pc-bin`)
- **QEMU** para testes (`qemu-system-i386`)
- **Python 3** para processamento de recursos
- **xxd** para conversao binaria
- *(Opcional)* Toolchain cruzado **powerpc-linux-gnu** para compilacoes PowerPC

### Instalacao em Ubuntu/Debian

```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common
```

### Comandos de Compilacao

```bash
# Compilar kernel (x86 por defeito)
make

# Compilar para uma plataforma especifica
make PLATFORM=x86
make PLATFORM=arm        # requer GCC bare-metal para ARM
make PLATFORM=ppc        # experimental; requer toolchain ELF para PowerPC

# Criar ISO inicializavel
make iso

# Compilar com todos os idiomas
make LOCALE_FR=1 LOCALE_DE=1 LOCALE_ES=1 LOCALE_JA=1 LOCALE_ZH=1 LOCALE_KO=1 LOCALE_RU=1 LOCALE_UK=1 LOCALE_PL=1 LOCALE_CS=1 LOCALE_SQ=1 LOCALE_BG=1 LOCALE_HR=1 LOCALE_DA=1 LOCALE_NL=1 LOCALE_ET=1 LOCALE_FI=1 LOCALE_EL=1 LOCALE_HU=1 LOCALE_IS=1 LOCALE_IT=1 LOCALE_LV=1 LOCALE_LT=1 LOCALE_MK=1 LOCALE_ME=1 LOCALE_NO=1 LOCALE_PT=1 LOCALE_RO=1 LOCALE_SK=1 LOCALE_SL=1 LOCALE_SV=1 LOCALE_TR=1 LOCALE_HI=1 LOCALE_TW=1 LOCALE_AR=1 LOCALE_BN=1 LOCALE_UR=1

# Compilar com um unico idioma adicional
make LOCALE_FR=1

# Compilar e executar no QEMU
make run

# Limpar artefactos
make clean

# Mostrar estatisticas de compilacao
make info
```

## 🚀 Execucao

### Inicio Rapido (QEMU)

```bash
# Execucao padrao com registo via porta serie
make run

# Manualmente com opcoes
qemu-system-i386 -cdrom system71.iso -serial file:/tmp/serial.log -display sdl -vga std -m 256M
```

### Opcoes do QEMU

```bash
# Com saida serie na consola
qemu-system-i386 -cdrom system71.iso -serial stdio -display sdl -m 256M

# Sem interface grafica (headless)
qemu-system-i386 -cdrom system71.iso -serial stdio -display none -m 256M

# Com depuracao GDB
make debug
# Noutro terminal: gdb kernel.elf -ex "target remote :1234"
```

## 📚 Documentacao

### Guias de Componentes
- **Control Manager**: `docs/components/ControlManager/`
- **Dialog Manager**: `docs/components/DialogManager/`
- **Font Manager**: `docs/components/FontManager/`
- **Registo via Porta Serie**: `docs/components/System/`
- **Event Manager**: `docs/components/EventManager.md`
- **Menu Manager**: `docs/components/MenuManager.md`
- **Window Manager**: `docs/components/WindowManager.md`
- **Resource Manager**: `docs/components/ResourceManager.md`

### Internacionalizacao
- **Locale Manager**: `include/LocaleManager/` — comutacao de locale em tempo de execucao, selecao de idioma no arranque
- **Recursos de Strings**: `resources/strings/` — ficheiros de recursos STR# por idioma (34 idiomas)
- **Fontes Estendidas**: `include/chicago_font_extended.h` — glifos Mac Roman 0x80-0xFF para caracteres europeus
- **Suporte CJK**: `include/TextEncoding/CJKEncoding.h`, `include/FontManager/CJKFont.h` — infraestrutura de codificacao multi-byte e fontes

### Estado da Implementacao
- **IMPLEMENTATION_PRIORITIES.md**: Trabalho planeado e acompanhamento do grau de conclusao
- **IMPLEMENTATION_STATUS_AUDIT.md**: Auditoria detalhada de todos os subsistemas

### Filosofia do Projeto

**Abordagem Arqueologica** com implementacao baseada em evidencias:
1. Apoiada na documentacao Inside Macintosh e nas MPW Universal Interfaces
2. Todas as decisoes importantes marcadas com IDs de descobertas que referenciam evidencias de suporte
3. Objetivo: paridade comportamental com o System 7 original, nao modernizacao
4. Implementacao em sala limpa (sem codigo-fonte original da Apple)

## 🐛 Problemas Conhecidos

1. **Artefactos ao Arrastar Icones**: Artefactos visuais menores ao arrastar icones no ambiente de trabalho
2. **Execucao M68K com Stubs**: Segment loader completo, ciclo de execucao nao implementado
3. **Sem Suporte TrueType**: Apenas fontes bitmap (Chicago)
4. **HFS Apenas Leitura**: Sistema de ficheiros virtual, sem escrita real em disco
5. **Sem Garantias de Estabilidade**: Crashes e comportamentos inesperados sao comuns

## 🤝 Contribuir

Este e principalmente um projeto de aprendizagem/investigacao:

1. **Relatorios de Bugs**: Submeta issues com passos de reproducao detalhados
2. **Testes**: Comunique resultados em diferente hardware/emuladores
3. **Documentacao**: Melhore documentacao existente ou adicione novos guias

## 📖 Referencias Essenciais

- **Inside Macintosh** (1992-1994): Documentacao oficial do Apple Toolbox
- **MPW Universal Interfaces 3.2**: Ficheiros de cabecalho e definicoes de estruturas canonicas
- **Guide to Macintosh Family Hardware**: Referencia de arquitetura de hardware

### Ferramentas Uteis

- **Mini vMac**: Emulador de System 7 para referencia comportamental
- **ResEdit**: Editor de recursos para estudo dos recursos do System 7
- **Ghidra/IDA**: Para analise de desassemblagem de ROM

## ⚖️ Aspetos Legais

Esta e uma **reimplementacao em sala limpa** para fins educacionais e de preservacao:

- **Nenhum codigo-fonte da Apple** foi utilizado
- Baseado apenas em documentacao publica e analise de caixa negra
- "System 7", "Macintosh", "QuickDraw" sao marcas registadas da Apple Inc.
- Nao afiliado, endossado ou patrocinado pela Apple Inc.

**A ROM e o software originais do System 7 permanecem propriedade da Apple Inc.**

## 🙏 Agradecimentos

- **Apple Computer, Inc.** por criar o System 7 original
- **Autores do Inside Macintosh** pela documentacao abrangente
- **Comunidade de preservacao do Mac classico** por manter a plataforma viva
- **68k.news e Macintosh Garden** pelos arquivos de recursos

## 📊 Estatisticas de Desenvolvimento

- **Linhas de Codigo**: ~57.500+ (incluindo 2.500+ para o segment loader)
- **Tempo de Compilacao**: ~3-5 segundos
- **Tamanho do Kernel**: ~4,16 MB (kernel.elf)
- **Tamanho do ISO**: ~12,5 MB (system71.iso)
- **Reducao de Erros**: 94% da funcionalidade principal a funcionar
- **Subsistemas Principais**: 28+ (Font, Window, Menu, Control, Dialog, TextEdit, etc.)

## 🔮 Direcao Futura

**Trabalho Planeado**:

- Concluir o ciclo de execucao do interpretador M68K
- Adicionar suporte a fontes TrueType
- Recursos de fontes bitmap CJK para renderizacao de Japones, Chines e Coreano
- Implementar controlos adicionais (campos de texto, pop-ups, sliders)
- Escrita em disco para o sistema de ficheiros HFS
- Funcionalidades avancadas do Sound Manager (mistura, amostragem)
- Acessorios de secretaria basicos (Calculadora, Bloco de Notas)

---

**Estado**: Experimental - Educacional - Em Desenvolvimento

**Ultima Atualizacao**: Novembro de 2025 (Melhorias no Sound Manager Concluidas)

Para questoes, problemas ou discussao, por favor utilize os GitHub Issues.
