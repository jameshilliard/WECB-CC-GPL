CC = mips-linux-gcc
AR = mips-linux-ar
OBJS=hnap_ssdp.o
TARGET=libhnap_ssdp.a

all: $(OBJS)
	$(AR) -cruv $(TARGET) $(OBJS)
clean:
	rm -f *.o
	rm -f $(TARGET)

romfs:
	$(ROMFSINST) libhnap_ssdp.a /lib/ 

.c.o:
	$(CC) -c -o $@ $(CFLAGS) $<
