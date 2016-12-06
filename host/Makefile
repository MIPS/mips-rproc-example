
SUBDIRS = case_invert

clean_SUBDIRS=$(addprefix clean_,$(SUBDIRS))

all: $(SUBDIRS)
clean: $(clean_SUBDIRS)

.PHONY: force

$(SUBDIRS): force
	make -C $@
$(clean_SUBDIRS): force
	make -C $(patsubst clean_%,%,$@) clean

