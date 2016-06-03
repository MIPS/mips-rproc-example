
ifeq ($(ARCH), R6)
	arch_flags = -mips32r6
else
	arch_flags = -mips32r2
endif

ifeq ($(ENDIAN), BIG)
	arch_flags += -EB
else
	arch_flags += -EL
endif

export arch_flags

SUBDIRS = host firmware
clean_SUBDIRS=$(addprefix clean_,$(SUBDIRS))

all: $(SUBDIRS)
clean: $(clean_SUBDIRS)

.PHONY: force

$(SUBDIRS): force
	make -C $@
$(clean_SUBDIRS): force
	make -C $(patsubst clean_%,%,$@) clean

