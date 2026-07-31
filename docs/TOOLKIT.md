TOOLKIT design doc

We include placeholder, clean-room toolkit ROMs for PPC and 68k to provide a legacy Toolbox surface for emulation. These are NOT Apple code and are intended for compatibility testing only.

Files added:
 - src/Toolkit/ppc/toolkit.c         : PPC placeholder toolkit source
 - src/Toolkit/68k/toolkit.c        : 68k placeholder toolkit source
 - scripts/build_toolkits.sh        : build placeholders / attempt cross-compile
 - scripts/make_esp_with_toolkits.sh: create an ESP FAT image containing toolkits + PRAM

Next steps:
 - I will wire the loader to load the correct per-arch toolkit and pass BootInfo in the entry register.
 - Add kernel-side BootInfo consumer to map and use the toolkit region.
