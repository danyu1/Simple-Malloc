all: test

test: test.c smalloc.c
        gcc -o test test.c smalloc.c -I. -Werror

clean:
        rm -f test
