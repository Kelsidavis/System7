# Debug what happens after STI

set pagination off
set disassembly-flavor intel
target remote localhost:1234
symbol-file kernel.elf

# Break right after STI instruction
b nk_test_threads_run
commands
  silent
  # Continue to the STI instruction
  until 98
  printf "\n=== About to execute STI ===\n"
  info registers
  si
  printf "\n=== After STI, before serial_puts ===\n"
  info registers
  x/10i $eip
  # Try to continue
  continue
end

# Set timeout breakpoint
b serial_puts
commands
  silent
  printf "\n=== In serial_puts, caller EIP: %p ===\n", *(void**)$esp
  x/10i *(void**)$esp
  continue
end

c
