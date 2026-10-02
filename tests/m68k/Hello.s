| Hello - the smallest Macintosh application: start the managers, then quit.
|
| What every application of the period does first, so it is what the system
| has to get right first. Built by build.sh into CODE resources on a disk
| image; see README.md.

        .text
start:
        pea     -4(%a5)         | @thePort: QuickDraw's globals end at A5
        .short  0xA86E          | _InitGraf
        .short  0xA8FE          | _InitFonts
        .short  0xA912          | _InitWindows
        .short  0xA930          | _InitMenus
        .short  0xA9CC          | _TEInit
        clr.l   -(%sp)          | no resume procedure
        .short  0xA97B          | _InitDialogs
        .short  0xA850          | _InitCursor
        .short  0xA9F4          | _ExitToShell
