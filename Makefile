

CFLAGS=-std=c89 -pedantic 

all: master taxi source
	
master: master.o support.o
	gcc master.o support.o
	
master.o: master.c
	gcc -c $(CFLAGS) master.c
	
taxi: taxi.o support.o
	gcc -o  taxi  taxi.o support.o
		
taxi.o: taxi.c
	gcc -c $(CFLAGS) taxi.c	
	
source: source.o support.o
	gcc -D DENSE -o  source source.o support.o
	
source.o: source.c
	gcc -c $(CFLAGS) source.c

support.o: support.c
	gcc -c $(CFLAGS) support.c	
	
	



alld: masterd taxid sourced
	
masterd: masterd.o supportd.o
	gcc masterd.o supportd.o

masterd.o: master.c
	gcc -c $(CFLAGS) -D DENSE -o masterd.o master.c 

taxid: taxid.o supportd.o
	gcc -D DENSE -o  taxi taxid.o supportd.o
	
taxid.o: taxi.c
	gcc -c $(CFLAGS) -D DENSE -o taxid.o taxi.c

sourced: sourced.o supportd.o
	gcc -D DENSE -o  source sourced.o supportd.o
	
sourced.o: source.c
	gcc -c $(CFLAGS) -D DENSE -o  sourced.o source.c

supportd.o: support.c
	gcc -c $(CFLAGS) -D DENSE -o  supportd.o support.c
	

run: 
	./a.out
	

clean: 
	rm -f *.o master masterd support supportd taxi taxid support supportd
