server: server.o utils.o
	gcc -g utils.o server.o -o server -Wall
server.o: server.c
	gcc -c -g server.c -o server.o -Wall
utils.o: utils.c
	gcc -c -g utils.c -o utils.o -Wall

.PHONY:clean
clean:
	rm -rf *.o


