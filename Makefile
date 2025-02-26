OUTPUT = ./bin
TEST = ./test
SRC = ./src
OBJECTS = lapi.o lauxlib.o ldo.o lmem.o lobject.o lstate.o lua.o lgc.o lstring.o

lapi.o: $(SRC)/lapi.c $(SRC)/lapi.h $(SRC)/lstate.h
	gcc -c -g $(SRC)/lapi.c -o $(OUTPUT)/lapi.o

lauxlib.o: $(SRC)/lauxlib.c $(SRC)/lauxlib.h $(SRC)/ldo.h
	gcc -c -g $(SRC)/lauxlib.c -o $(OUTPUT)/lauxlib.o

ldo.o: $(SRC)/ldo.c $(SRC)/ldo.h $(SRC)/lmem.h
	gcc -c -g $(SRC)/ldo.c -o $(OUTPUT)/ldo.o

lmem.o: $(SRC)/lmem.c $(SRC)/lmem.h $(SRC)/ldo.h
	gcc -c -g $(SRC)/lmem.c -o $(OUTPUT)/lmem.o

lobject.o: $(SRC)/lobject.c $(SRC)/lobject.h
	gcc -c -g $(SRC)/lobject.c -o $(OUTPUT)/lobject.o

lstate.o: $(SRC)/lstate.c $(SRC)/lstate.h $(SRC)/llimits.h $(SRC)/lmem.h
	gcc -c -g $(SRC)/lstate.c -o $(OUTPUT)/lstate.o

lgc.o: $(SRC)/lgc.c $(SRC)/lgc.h $(SRC)/lmem.h
	gcc -c -g $(SRC)/lgc.c -o $(OUTPUT)/lgc.o

lstring.o: $(SRC)/lstring.c $(SRC)/lstring.h $(SRC)/lmem.h
	gcc -c -g $(SRC)/lstring.c -o $(OUTPUT)/lstring.o

lua.o: $(SRC)/lua.c $(SRC)/lua.h
	gcc -c -g $(SRC)/lua.c -o $(OUTPUT)/lua.o

all: test 

test: $(OBJECTS) test.o
	gcc -g -o $(OUTPUT)/test $(OUTPUT)/test.o $(OUTPUT)/lapi.o $(OUTPUT)/lauxlib.o $(OUTPUT)/ldo.o $(OUTPUT)/lmem.o $(OUTPUT)/lobject.o $(OUTPUT)/lgc.o $(OUTPUT)/lstate.o $(OUTPUT)/lua.o

test2: $(OBJECTS) test2.o
	gcc -g -o $(OUTPUT)/test2 $(OUTPUT)/test2.o $(OUTPUT)/lapi.o $(OUTPUT)/lauxlib.o $(OUTPUT)/ldo.o $(OUTPUT)/lmem.o $(OUTPUT)/lobject.o $(OUTPUT)/lgc.o $(OUTPUT)/lstate.o $(OUTPUT)/lua.o

test3: $(OBJECTS) test3.o
	gcc -g -o $(OUTPUT)/test3 $(OUTPUT)/test3.o $(OUTPUT)/lapi.o $(OUTPUT)/lauxlib.o $(OUTPUT)/ldo.o $(OUTPUT)/lmem.o $(OUTPUT)/lobject.o $(OUTPUT)/lgc.o $(OUTPUT)/lstate.o $(OUTPUT)/lua.o $(OUTPUT)/lstring.o

test.o: $(TEST)/test.c
	gcc -c -g $(TEST)/test.c -o $(OUTPUT)/test.o

test2.o: $(TEST)/test2.c
	gcc -c -g $(TEST)/test2.c -o $(OUTPUT)/test2.o

test3.o: $(TEST)/test3.c
	gcc -c -g $(TEST)/test3.c -o $(OUTPUT)/test3.o

clean:
	rm -rf $(OUTPUT)/*

# ///////////////////////////////////////////////////////////
# 测试例子
setjmp: setjmp.o
	gcc -g -o $(OUTPUT)/setjmp $(OUTPUT)/setjmp.o

setjmp.o: $(TEST)/setjmp.c
	gcc -c -g $(TEST)/setjmp.c -o $(OUTPUT)/setjmp.o
