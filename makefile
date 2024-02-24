all: oss worker

oss: oss.c
        gcc -D_GNU_SOURCE -std=c99 -o oss oss.c

worker: worker.c
        gcc -D_GNU_SOURCE -std=c99 -o worker worker.c

clean:
        rm -f oss worker
