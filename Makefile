srcs=bitmap.c bitlib.c
objs=$(srcs:.c=.o)
tree_test_srcs=tree_test.c bitlib.c
tree_test_objs=$(tree_test_srcs:.c=.o)

pma_srcs=pma.c bitlib.c
pma_objs=$(pma_srcs:.c=.o)

CFLAGS=-g `pkg-config --cflags glib-2.0`

all: bitmap tree_test pma

bitmap: $(objs)
	gcc -o bitmap $(objs)

tree_test: $(tree_test_objs)
	gcc -o tree_test $(tree_test_objs) `pkg-config --libs glib-2.0` -lrt

pma: $(pma_objs)
	gcc -o pma $(pma_objs) `pkg-config --libs glib-2.0` -lrt

clean:
	$(RM) bitmap *.o
