all: main.c
	cc -O2 -Wall -mavx -std=gnu99 main.c TinyMT/tinymt/tinymt32.c -o lottery
