tree_test_srcs=tree_test.c bitlib.c
tree_test_objs=$(tree_test_srcs:.c=.o)

pma_srcs=pma.c bitlib.c
pma_objs=$(pma_srcs:.c=.o)

CFLAGS=-g -Wall `pkg-config --cflags glib-2.0`

%.d: %.c
	@set -e; rm -f $@; \
	gcc -MM $(CPPFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

all: tree_test pma

-include $(tree_test_srcs:.c=.d)
-include $(pma_srcs:.c=.d)

tree_test: $(tree_test_objs)
	gcc -o tree_test $(tree_test_objs) `pkg-config --libs glib-2.0` -lrt

pma: $(pma_objs)
	gcc -o pma $(pma_objs) `pkg-config --libs glib-2.0` -lrt

clean:
	$(RM) bitmap *.o
