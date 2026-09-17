TARGET ?= libnccl-net-mrc.so

all:
	$(MAKE) -C src OUTPUT=../$(TARGET)

clean:
	$(MAKE) -C src OUTPUT=../$(TARGET) clean

.PHONY: all clean
