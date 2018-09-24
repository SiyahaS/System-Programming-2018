all:
	gcc -c main.c
	gcc -o multiprocess_DFT main.o -lm

debug:
	gcc -c main.c -DDEBUG
	gcc -o multiprocess_DFT main.o -lm

clean:
	rm main.o multiprocess_DFT stdout stderr consumer.log producer.log
