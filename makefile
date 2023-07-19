UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
  CC=gcc
  CFLAGS=-I. -framework ApplicationServices -ldelegate -L./delegate/target/release
  SRC = main.c
  EXECUTABLE=kl
else
  $(error This program can only be compiled and run on macOS)
endif

$(EXECUTABLE): $(SRC)
	cd delegate && cargo build --release
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f $(EXECUTABLE)

	