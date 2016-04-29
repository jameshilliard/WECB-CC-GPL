# Makefile to build the clink API into a library
#
#SRC?=../ClinkD/Src

# Go with the flow in SnapGear
TARGET = clnkapi.a
OBJ    = ../ClinkD/Src/ClnkPswdSeed.o ClnkCtl.o ClnkNms.o clnkethapilnx.o 
GCCFLAGS += -DCLINK_LINUX
CXXFLAGS += -I ../ClinkD/Inc  $(ENTROPIC_COMMON_CFLAGS)
CXXFLAGS += -Wall
CFLAGS += -I ../ClinkD/Inc -I ../ClinkD/Src $(ENTROPIC_COMMON_CFLAGS) -Wall -lrt

all: $(TARGET)

$(TARGET): $(OBJ)
	@echo Creating $@ library
	$(AR) $(ARFLAGS) $@ $(OBJ)

romfs:
	# -- nothing to install --

clean:
	-rm -f *.o *.a $(OBJ)



