# System 7X Nanokernel - Detailed IRET Debug Script
# Single-steps through IRET to find the exact failure point

set pagination off
set disassembly-flavor intel
target remote localhost:1234
symbol-file kernel.elf

# Break right before IRET in irq0_stub
b *irq0_stub+56
commands
  silent
  printf "\n========== BEFORE IRET ==========\n"
  info registers
  printf "\n=== Stack dump (ESP=%p) ===\n", $esp
  x/8x $esp
  printf "\n=== Code at EIP ===\n"
  x/8i $eip
  printf "\n=== GDT Entry for CS=0x08 ===\n"
  x/2x ($gdtr+8)
  printf "\n=== About to single-step IRET ===\n"
  si
  printf "\n========== AFTER IRET ==========\n"
  info registers
  printf "\n=== Code at new EIP ===\n"
  x/8i $eip
  printf "\n=== Stack after IRET ===\n"
  x/8x $esp
  printf "\n=== Continuing... ===\n"
  continue
end

# Break on any exception
catch signal SIGSEGV
catch signal SIGILL
catch signal SIGTRAP

# Start execution
c
