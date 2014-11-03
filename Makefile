all: sdlnetplay.so

sdlnetplay.o: sdlnetplay.c
	gcc -c -Wall -Werror -fPIC -std=gnu99 -D_GNU_SOURCE sdlnetplay.c

sdlnetplay.so: sdlnetplay.o
	gcc -shared -fPIC -rdynamic -o sdlnetplay.so sdlnetplay.o -ldl

clean:
	rm sdlnetplay.so || true
