| The entry point CODE 1 starts at: run main, then quit as an application
| quits, whatever main did.
        .section .text.start
        .globl  _start
_start:
        bsr.w   main
        .short  0xA9F4          | _ExitToShell
        .section .note.GNU-stack,"",%progbits
