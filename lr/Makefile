all:
	#gcc main.c -o lr -lSDL -ldl -I/usr/include/SDL
	mipsel-linux-gcc main.c -o lr -lSDL -ldl -lshake -I/opt/gcw0-toolchain/usr/mipsel-gcw0-linux-uclibc/sysroot/usr/include/SDL
	mipsel-linux-strip lr

clean:
	rm -rf lr
