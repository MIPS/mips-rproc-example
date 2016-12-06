
TARGET = rproc-example-host

all: $(TARGET)

cflags += -O2

$(TARGET): $(TARGET).c
	$(CROSS_COMPILE)gcc $(cflags) $(arch_flags) -o $@ $^

clean:
	rm -f *.o $(TARGET)