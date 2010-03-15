srcs=bitmap.c bitlib.c
objs=$(srcs:.c=.o)
encode_srcs=tree_test.c bitlib.c
encode_objs=$(encode_srcs:.c=.o)

CFLAGS=-g -O2 `pkg-config --cflags glib-2.0`

all: bitmap encode

bitmap: $(objs)
	gcc -o bitmap $(objs)

encode: $(encode_objs)
	gcc -o encode $(encode_objs) `pkg-config --libs glib-2.0` -lrt

clean:
	$(RM) bitmap *.o
