passthrough: cfst.c
	gcc -Wall cfst.c `pkg-config fuse3 --cflags --libs` -o cfs
