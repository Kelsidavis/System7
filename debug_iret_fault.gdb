# System 7X Nanokernel - IRET Fault Debug Script
# Usage:
#   qemu-system-i386 -cdrom system71.iso -s -S -no-reboot -no-shutdown \
#       -serial file:/tmp/serial.log -d int,cpu_reset,cpu
#   gdb -x debug_iret_fault.gdb

set pagination off
set disassembly-flavor intel
target remote localhost:1234
symbol-file kernel.elf

# Break when entering nk_switch_context_irq
b nk_switch_context_irq
commands
  silent
  printf "\n[BREAK] nk_switch_context_irq entered\n"
  info registers
  x/16i $eip
  continue
end

# Break immediately before iret
b *nk_switch_context_irq+0x70
commands
  silent
  printf "\n[BREAK] About to execute IRET\n"
  info registers
  x/32x $esp
  x/16i $eip
  continue
end

# Trap on triple fault reset (QEMU will signal CPU reset)
catch signal SIGTRAP

c
