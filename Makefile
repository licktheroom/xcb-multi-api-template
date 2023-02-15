# Copyright (c) 2023 licktheroom

CC = clang
CFLAGS = -O2 -march=native -pipe -fomit-frame-pointer -Wall -Wextra -Wshadow \
		-Wdouble-promotion -fno-common -std=c11
CLIBS = -lxcb -lGL -lxcb -lX11 -lX11-xcb -lvulkan

files = main.o

all: shaders ${files}
	${CC} ${CFLAGS} ${CLIBS} ${files} -o build/xcb-multi
	rm -f *.o

main.o:
	${CC} ${CFLAGS} -c -o main.o src/main.c

shaders:
	mkdir -p build/shaders/
	glslc src/shaders/shader.frag -o build/shaders/frag.spv
	glslc src/shaders/shader.vert -o build/shaders/vert.spv