CFILES := $(wildcard *.c)

%.exe : %.c
	gcc -Wall -O2 -std=gnu11 -static $< -o $@

all: $(CFILES:.c=.exe)
