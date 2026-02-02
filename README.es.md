# System 7 - Reimplementacion Portable de Codigo Abierto

**[English](README.md)** | **[Fran&ccedil;ais](README.fr.md)** | **[Deutsch](README.de.md)** | **[Espa&ntilde;ol](README.es.md)** | **[日本語](README.ja.md)** | **[中文](README.zh.md)** | **[한국어](README.ko.md)**

<img width="793" height="657" alt="System 7 ejecutandose en hardware moderno" src="https://github.com/user-attachments/assets/be84b83e-191c-4f9d-a786-11d0bd04203b" />
<img width="801" height="662" alt="simpletextworks" src="https://github.com/user-attachments/assets/7c9ebe5b-22b4-4612-93a1-2076909d77cd" />
<img width="803" height="661" alt="macpaint" src="https://github.com/user-attachments/assets/cd3ed04a-fdde-4dd5-88ef-5b19b3a13a54" />

> **PRUEBA DE CONCEPTO** - Esta es una reimplementacion experimental y educativa del System 7 de Apple Macintosh. NO es un producto terminado y no debe considerarse software listo para produccion.

Una reimplementacion de codigo abierto del System 7 de Apple Macintosh para hardware x86 moderno, arrancable mediante GRUB2/Multiboot2. Este proyecto busca recrear la experiencia del Mac OS clasico, documentando al mismo tiempo la arquitectura del System 7 a traves del analisis de ingenieria inversa.

## Estado del Proyecto

**Estado actual**: Desarrollo activo con aproximadamente el 94% de la funcionalidad principal completada

### Ultimas Actualizaciones (Noviembre 2025)

#### Mejoras del Sound Manager -- COMPLETO
- **Conversion MIDI optimizada**: Funcion auxiliar compartida `SndMidiNoteToFreq()` con tabla de busqueda de 37 entradas (C3-B5) y respaldo basado en octavas para el rango MIDI completo (0-127)
- **Soporte de reproduccion asincrona**: Infraestructura completa de callbacks tanto para reproduccion de archivos (`FilePlayCompletionUPP`) como para ejecucion de comandos (`SndCallBackProcPtr`)
- **Enrutamiento de audio por canales**: Sistema de prioridad multinivel con controles de silencio y habilitacion
  - 4 niveles de prioridad de canales (0-3) para enrutamiento de salida de hardware
  - Controles independientes de silencio y habilitacion por canal
  - `SndGetActiveChannel()` devuelve el canal activo de mayor prioridad
  - Inicializacion correcta de canales con indicador de habilitacion por defecto
- **Implementacion de calidad de produccion**: Todo el codigo compila limpiamente, sin violaciones de malloc/free detectadas
- **Commits**: 07542c5 (optimizacion MIDI), 1854fe6 (callbacks asincronos), a3433c6 (enrutamiento de canales)

#### Logros de Sesiones Anteriores
- **Fase de Funciones Avanzadas**: Bucle de procesamiento de comandos del Sound Manager, serializacion de estilos multi-ejecucion, funciones extendidas de MIDI/sintesis
- **Sistema de Redimensionamiento de Ventanas**: Redimensionamiento interactivo con manejo adecuado del marco, caja de crecimiento y limpieza del escritorio
- **Traduccion de Teclado PS/2**: Mapeo completo de codigos de escaneo set 1 a codigos de tecla del Toolbox
- **HAL Multiplataforma**: Soporte para x86, ARM y PowerPC con abstraccion limpia

## Grado de Completitud del Proyecto

**Funcionalidad principal global**: aproximadamente el 94% completada (estimacion)

### Funcionalidades Completas

- **Capa de Abstraccion de Hardware (HAL)**: Abstraccion de plataforma completa para x86/ARM/PowerPC
- **Sistema de Arranque**: Arranca exitosamente mediante GRUB2/Multiboot2 en x86
- **Registro Serial**: Registro basado en modulos con filtrado en tiempo de ejecucion (Error/Warn/Info/Debug/Trace)
- **Base Grafica**: Framebuffer VESA (800x600x32) con primitivas QuickDraw incluyendo modo XOR
- **Renderizado del Escritorio**: Barra de menu del System 7 con logotipo arcoiris de Apple, iconos y patrones de escritorio
- **Tipografia**: Fuente bitmap Chicago con renderizado pixel-perfect y kerning adecuado, Mac Roman extendido (0x80-0xFF) para caracteres europeos acentuados
- **Internacionalizacion (i18n)**: Localizacion basada en recursos con 7 idiomas (ingles, frances, aleman, espanol, japones, chino, coreano), Locale Manager con seleccion de idioma en el arranque, infraestructura de codificacion multibyte CJK
- **Font Manager**: Soporte de multiples tamanos (9-24pt), sintesis de estilos, analisis FOND/NFNT, cache LRU
- **Sistema de Entrada**: Teclado y raton PS/2 con reenvio completo de eventos
- **Event Manager**: Multitarea cooperativa mediante WaitNextEvent con cola de eventos unificada
- **Memory Manager**: Asignacion basada en zonas con integracion del interprete 68K
- **Menu Manager**: Menus desplegables completos con seguimiento del raton y SaveBits/RestoreBits
- **Sistema de Archivos**: HFS con implementacion de arboles B, ventanas de carpetas con enumeracion VFS
- **Window Manager**: Arrastre, redimensionamiento (con caja de crecimiento), capas, activacion
- **Time Manager**: Calibracion TSC precisa, precision de microsegundos, verificacion de generacion
- **Resource Manager**: Busqueda binaria O(log n), cache LRU, validacion exhaustiva
- **Gestalt Manager**: Informacion del sistema multi-arquitectura con deteccion de arquitectura
- **TextEdit Manager**: Edicion de texto completa con integracion del portapapeles
- **Scrap Manager**: Portapapeles clasico de Mac OS con soporte de multiples formatos
- **Aplicacion SimpleText**: Editor de texto MDI completo con cortar/copiar/pegar
- **List Manager**: Controles de lista compatibles con System 7.1 con navegacion por teclado
- **Control Manager**: Controles estandar y barras de desplazamiento con implementacion CDEF
- **Dialog Manager**: Navegacion por teclado, anillos de foco, atajos de teclado
- **Segment Loader**: Sistema de carga de segmentos 68K portable e independiente de ISA con reubicacion
- **Interprete M68K**: Despacho completo de instrucciones con 84 manejadores de opcodes, los 14 modos de direccionamiento, marco de excepciones/traps
- **Sound Manager**: Procesamiento de comandos, conversion MIDI, gestion de canales, callbacks
- **Device Manager**: Gestion de DCE, instalacion/eliminacion de controladores y operaciones de E/S
- **Pantalla de Inicio**: Interfaz de arranque completa con seguimiento de progreso, gestion de fases y pantalla de bienvenida
- **Color Manager**: Gestion de estado de color con integracion QuickDraw

### Parcialmente Implementado

- **Integracion de Aplicaciones**: Interprete M68K y cargador de segmentos completos; se necesitan pruebas de integracion para verificar que las aplicaciones reales se ejecuten
- **Procedimientos de Definicion de Ventanas (WDEF)**: Estructura basica implementada, despacho parcial
- **Speech Manager**: Solo marco de API y paso de audio; motor de sintesis de voz no implementado
- **Manejo de Excepciones (RTE)**: Retorno de excepcion parcialmente implementado (actualmente se detiene en lugar de restaurar el contexto)

### Aun No Implementado

- **Impresion**: Sin sistema de impresion
- **Red**: Sin funcionalidad AppleTalk ni de red
- **Accesorios de Escritorio**: Solo el marco basico
- **Audio Avanzado**: Reproduccion de muestras, mezcla (limitacion del altavoz del PC)

### Subsistemas No Compilados

Los siguientes tienen codigo fuente pero no estan integrados en el kernel:
- **AppleEventManager** (8 archivos): Mensajeria entre aplicaciones; deliberadamente excluido debido a dependencias de pthread incompatibles con el entorno freestanding
- **FontResources** (solo encabezado): Definiciones de tipos de recursos de fuentes; el soporte real de fuentes lo proporciona FontResourceLoader.c compilado

## Arquitectura

### Especificaciones Tecnicas

- **Arquitectura**: Multi-arquitectura via HAL (x86, ARM, PowerPC listos)
- **Protocolo de Arranque**: Multiboot2 (x86), cargadores de arranque especificos por plataforma
- **Graficos**: Framebuffer VESA, 800x600 @ color de 32 bits
- **Disposicion de Memoria**: El kernel se carga en la direccion fisica 1MB (x86)
- **Temporizado**: Independiente de arquitectura con precision de microsegundos (RDTSC/registros de temporizador)
- **Rendimiento**: Fallo de cache de recursos en frio <15us, acierto de cache <2us, desviacion del temporizador <100ppm

### Estadisticas del Codigo Fuente

- **Mas de 225 archivos fuente** con mas de 57.500 lineas de codigo
- **Mas de 145 archivos de encabezado** en mas de 28 subsistemas
- **69 tipos de recursos** extraidos del System 7.1
- **Tiempo de compilacion**: 3-5 segundos en hardware moderno
- **Tamano del kernel**: ~4,16 MB
- **Tamano de la ISO**: ~12,5 MB

## Compilacion

### Requisitos

- **GCC** con soporte de 32 bits (`gcc-multilib` en sistemas de 64 bits)
- **GNU Make**
- **Herramientas GRUB**: `grub-mkrescue` (de `grub2-common` o `grub-pc-bin`)
- **QEMU** para pruebas (`qemu-system-i386`)
- **Python 3** para el procesamiento de recursos
- **xxd** para conversion binaria
- *(Opcional)* Toolchain cruzado **powerpc-linux-gnu** para compilaciones PowerPC

### Instalacion en Ubuntu/Debian

```bash
sudo apt-get install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86 python3 vim-common
```

### Comandos de Compilacion

```bash
# Compilar el kernel (x86 por defecto)
make

# Compilar para una plataforma especifica
make PLATFORM=x86
make PLATFORM=arm        # requiere GCC bare-metal para ARM
make PLATFORM=ppc        # experimental; requiere toolchain ELF para PowerPC

# Crear ISO arrancable
make iso

# Compilar con todos los idiomas (frances, aleman, espanol, japones, chino, coreano)
make LOCALE_FR=1 LOCALE_DE=1 LOCALE_ES=1 LOCALE_JA=1 LOCALE_ZH=1 LOCALE_KO=1

# Compilar con un solo idioma adicional
make LOCALE_FR=1

# Compilar y ejecutar en QEMU
make run

# Limpiar artefactos de compilacion
make clean

# Mostrar estadisticas de compilacion
make info
```

## Ejecucion

### Inicio Rapido (QEMU)

```bash
# Ejecucion estandar con registro serial
make run

# Manualmente con opciones
qemu-system-i386 -cdrom system71.iso -serial file:/tmp/serial.log -display sdl -vga std -m 256M
```

### Opciones de QEMU

```bash
# Con salida serial por consola
qemu-system-i386 -cdrom system71.iso -serial stdio -display sdl -m 256M

# Sin interfaz grafica (headless)
qemu-system-i386 -cdrom system71.iso -serial stdio -display none -m 256M

# Con depuracion GDB
make debug
# En otra terminal: gdb kernel.elf -ex "target remote :1234"
```

## Documentacion

### Guias de Componentes
- **Control Manager**: `docs/components/ControlManager/`
- **Dialog Manager**: `docs/components/DialogManager/`
- **Font Manager**: `docs/components/FontManager/`
- **Registro Serial**: `docs/components/System/`
- **Event Manager**: `docs/components/EventManager.md`
- **Menu Manager**: `docs/components/MenuManager.md`
- **Window Manager**: `docs/components/WindowManager.md`
- **Resource Manager**: `docs/components/ResourceManager.md`

### Internacionalizacion
- **Locale Manager**: `include/LocaleManager/` -- cambio de idioma en tiempo de ejecucion, seleccion de idioma en el arranque
- **Recursos de Cadenas**: `resources/strings/` -- archivos de recursos STR# por idioma (en, fr, de, es, ja, zh, ko)
- **Fuentes Extendidas**: `include/chicago_font_extended.h` -- glifos Mac Roman 0x80-0xFF para caracteres europeos
- **Soporte CJK**: `include/TextEncoding/CJKEncoding.h`, `include/FontManager/CJKFont.h` -- infraestructura de codificacion multibyte y fuentes

### Estado de Implementacion
- **IMPLEMENTATION_PRIORITIES.md**: Trabajo planificado y seguimiento de completitud
- **IMPLEMENTATION_STATUS_AUDIT.md**: Auditoria detallada de todos los subsistemas

### Filosofia del Proyecto

**Enfoque arqueologico** con implementacion basada en evidencia:
1. Respaldado por la documentacion de Inside Macintosh y las Universal Interfaces de MPW
2. Todas las decisiones importantes etiquetadas con identificadores de hallazgos que referencian evidencia de respaldo
3. Objetivo: paridad de comportamiento con el System 7 original, no modernizacion
4. Implementacion en sala limpia (sin codigo fuente original de Apple)

## Problemas Conocidos

1. **Artefactos al Arrastrar Iconos**: Artefactos visuales menores al arrastrar iconos del escritorio
2. **Ejecucion M68K Pendiente**: Cargador de segmentos completo, bucle de ejecucion no implementado
3. **Sin Soporte TrueType**: Solo fuentes bitmap (Chicago)
4. **HFS de Solo Lectura**: Sistema de archivos virtual, sin escritura a disco
5. **Sin Garantias de Estabilidad**: Los fallos y el comportamiento inesperado son frecuentes

## Contribuciones

Este es principalmente un proyecto de aprendizaje e investigacion:

1. **Reportes de Errores**: Cree issues con pasos de reproduccion detallados
2. **Pruebas**: Reporte resultados en diferentes configuraciones de hardware/emuladores
3. **Documentacion**: Mejore la documentacion existente o agregue nuevas guias

## Referencias Esenciales

- **Inside Macintosh** (1992-1994): Documentacion oficial del Toolbox de Apple
- **MPW Universal Interfaces 3.2**: Archivos de encabezado y definiciones de estructuras canonicas
- **Guide to Macintosh Family Hardware**: Referencia de arquitectura de hardware

### Herramientas Utiles

- **Mini vMac**: Emulador de System 7 para referencia de comportamiento
- **ResEdit**: Editor de recursos para estudiar los recursos del System 7
- **Ghidra/IDA**: Para analisis de desensamblado de ROM

## Aviso Legal

Esta es una **reimplementacion en sala limpia** con fines educativos y de preservacion:

- **No se utilizo codigo fuente de Apple**
- Basado unicamente en documentacion publica y analisis de caja negra
- "System 7", "Macintosh" y "QuickDraw" son marcas registradas de Apple Inc.
- No esta afiliado, respaldado ni patrocinado por Apple Inc.

**La ROM y el software originales del System 7 siguen siendo propiedad de Apple Inc.**

## Agradecimientos

- **Apple Computer, Inc.** por crear el System 7 original
- **Autores de Inside Macintosh** por la documentacion exhaustiva
- **Comunidad de preservacion del Mac clasico** por mantener viva la plataforma
- **68k.news y Macintosh Garden** por los archivos de recursos

## Estadisticas de Desarrollo

- **Lineas de Codigo**: mas de 57.500 (incluyendo mas de 2.500 para el cargador de segmentos)
- **Tiempo de Compilacion**: ~3-5 segundos
- **Tamano del Kernel**: ~4,16 MB (kernel.elf)
- **Tamano de la ISO**: ~12,5 MB (system71.iso)
- **Reduccion de Errores**: 94% de la funcionalidad principal operativa
- **Subsistemas Principales**: mas de 28 (Font, Window, Menu, Control, Dialog, TextEdit, etc.)

## Direccion Futura

**Trabajo planificado**:

- Completar el bucle de ejecucion del interprete M68K
- Agregar soporte de fuentes TrueType
- Recursos de fuentes bitmap CJK para renderizado en japones, chino y coreano
- Implementar controles adicionales (campos de texto, menus emergentes, deslizadores)
- Escritura a disco para el sistema de archivos HFS
- Funciones avanzadas del Sound Manager (mezcla, muestreo)
- Accesorios de escritorio basicos (Calculadora, Bloc de Notas)

---

**Estado**: Experimental - Educativo - En Desarrollo

**Ultima actualizacion**: Noviembre 2025 (Mejoras del Sound Manager Completas)

Para preguntas, problemas o discusion, utilice GitHub Issues.
