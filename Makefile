CC := mpicc
TARGET := mpi_orch
SRC := mpi_orch.c

PKG_CONFIG ?= pkg-config
GLIB_CFLAGS := $(shell $(PKG_CONFIG) --cflags glib-2.0)
GLIB_LIBS := $(shell $(PKG_CONFIG) --libs glib-2.0)

CFLAGS := -std=c11 -O0 -g3 -fno-omit-frame-pointer -pipe \
          -Wall -Wextra -Wpedantic -Wformat=2 -Wshadow \
          -Wstrict-prototypes -Wmissing-prototypes -Wwrite-strings \
          -Wconversion -Wsign-conversion -Wvla \
          -fopenmp

CPPFLAGS := -D_GNU_SOURCE $(GLIB_CFLAGS)
LDLIBS := $(GLIB_LIBS)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@ $(LDLIBS)

clean:
	rm -f $(TARGET)