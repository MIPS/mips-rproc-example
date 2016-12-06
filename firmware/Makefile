
SUBDIRS = case_invert ws2812

clean_SUBDIRS=$(addprefix clean_,$(SUBDIRS))

all: $(SUBDIRS)
clean: $(clean_SUBDIRS)

.PHONY: force

$(SUBDIRS): force
	make -C $@
$(clean_SUBDIRS): force
	make -C $(patsubst clean_%,%,$@) clean

