CFS: main.c main.h 369.o 391.o 394.o
    gcc -Wall main.c main.h 369.o 391.o 394.o `pkg-config fuse3 --cflags --libs` -o CFS

369.o: 369.c 369.h
    gcc -c 369.c 369.h

391.o: 391.c 391.h
    gcc -c 391.c 391.h

394.o: 394.c 394.h
    gcc -c 394.c 394.h
