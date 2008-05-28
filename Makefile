CC=gcc
SPARSE=sparse

all:
	$(CC) `pkg-config gtk+-2.0 --cflags --libs` microbreaker.c -o microbreaker $(CFLAGS)

clean:
	rm -f microbreaker

check:
	$(SPARSE) `pkg-config gtk+-2.0 --cflags` microbreaker.c $(WFLAGS)
