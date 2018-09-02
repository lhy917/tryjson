cc=gcc
test : myjson.o test.o
	cc -o test myjson.o test.o
leptjson.o : myjson.h
	cc -c myjson.c
test.o : myjson.o
	cc -c test.c
clean:
	rm test myjson.o test.o