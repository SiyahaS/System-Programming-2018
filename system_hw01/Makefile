
all:
	gcc -c main.c tiff.c
	gcc -o tiffprocessor main.o tiff.o 

debug:
	gcc -c main.c tiff.c -DDEBUG
	gcc -o tiffprocessor main.o tiff.o -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer

clean:
	rm main.o tiff.o
