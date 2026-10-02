# ARM64 QEMU virt build wrapper
#
# The complete System 7 kernel is built by the repository Makefile. Keeping a
# second source list here caused this entry point to drift until it referenced
# files that no longer existed. Delegate to the maintained build instead.

ROOT_DIR := ../../..
KERNEL := $(ROOT_DIR)/kernel.elf

.PHONY: all clean qemu qemu-virt qemu-build

all:
	$(MAKE) -C $(ROOT_DIR) PLATFORM=arm64 all

qemu-build: all

qemu qemu-virt: qemu-build
	qemu-system-aarch64 \
		-M virt \
		-cpu cortex-a53 \
		-m 1G \
		-kernel $(KERNEL) \
		-serial stdio \
		-display none

clean:
	$(MAKE) -C $(ROOT_DIR) clean
