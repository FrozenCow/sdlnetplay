CC=g++

all: sdlnetplay.so

OBJECTS = \
	sdlnetplay.o \
	utils.o \
	protocol.o

%.o: %.c Makefile
	${CC} -D_GNU_SOURCE -std=c++11 -c -Wall -Werror -fPIC -D_GNU_SOURCE $<

sdlnetplay.so: $(OBJECTS)
	${CC} -shared -fPIC -rdynamic -o $@ $^ -ldl

clean:
	rm *.o sdlnetplay.so || true
