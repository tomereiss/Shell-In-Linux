all: myshell mypipe looper mypipeline

myshell: myshell.o LineParser.o
	gcc -g -m32 -Wall -o myshell myshell.o LineParser.o
           
mypipe: mypipe.o
	gcc -g -m32 -Wall -o mypipe mypipe.o
	          
looper: looper.o
	gcc -g -m32 -Wall -o looper looper.o

mypipeline: mypipeline.o
	gcc -g -m32 -Wall -o mypipeline mypipeline.o

myshell.o : myshell.c LineParser.h 
	gcc -g -m32 -Wall -c -o myshell.o myshell.c 

mypipe.o: mypipe.c 
	gcc -g -m32 -Wall -c -o mypipe.o mypipe.c 

LineParser.o: LineParser.c LineParser.h
	gcc -g -m32 -Wall -c -o LineParser.o LineParser.c

looper.o : looper.c
	gcc -g -m32 -Wall -c -o looper.o looper.c

mypipeline.o : mypipeline.c
	gcc -g -m32 -Wall -c -o mypipeline.o mypipeline.c

.PHONY: clean

clean:
	rm -f *.o myshell mypipe looper mypipeline