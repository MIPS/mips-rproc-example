
TARGET_FW = rproc-example-firmware

all: $(TARGET_FW)

COMMON := ../common

s_objs += head.o
c_objs += main.o printf.o trace.o vring.o

vpath %.S $(COMMON)
vpath %.c $(COMMON)

includes += -I$(COMMON)/include

cflags += -g -nostdlib -fno-exceptions -fno-builtin -nostartfiles -nodefaultlibs -fno-stack-protector -mno-abicalls
cflags += -O2

$(s_objs): %.o: %.S
	$(CROSS_COMPILE)gcc $(includes) $(cflags) $(arch_flags) -c -o $@ $<

$(c_objs): %.o: %.c
	$(CROSS_COMPILE)gcc $(includes) $(cflags) $(arch_flags) -c -o $@ $<

$(TARGET_FW): $(s_objs) $(c_objs)
	$(CROSS_COMPILE)ld -T $(COMMON)/$(TARGET_FW).ld $(arch_flags) -o $@ $^

clean:
	rm -f *.o $(TARGET_FW)
