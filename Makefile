cc=gcc
test : leptjson.o test.o
	cc -o test leptjson.o test.o
leptjson.o : leptjson.h
	cc -c leptjson.c
test.o : leptjson.o
	cc -c test.c
clean:
	rm test leptjson.o test.o