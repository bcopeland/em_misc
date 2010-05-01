tree_test_srcs=tree_test.c bitlib.c
tree_test_objs=$(tree_test_srcs:.c=.o)

cobtree_srcs=cobtree.c vebtree.c pma.c bitlib.c
cobtree_objs=$(cobtree_srcs:.c=.o)

cobtree_sh_srcs=cobtree_sh.c veb_small_height.c bitlib.c
cobtree_sh_objs=$(cobtree_sh_srcs:.c=.o)

CFLAGS=-g -O2 -Wextra -Wall `pkg-config --cflags glib-2.0`

%.d: %.c
	@set -e; rm -f $@; \
	gcc -MM $(CPPFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

all: tree_test cobtree cobtree_sh

-include $(tree_test_srcs:.c=.d)
-include $(cobtree_srcs:.c=.d)

tree_test: $(tree_test_objs)
	gcc -o tree_test $(tree_test_objs) `pkg-config --libs glib-2.0` -lrt

cobtree: $(cobtree_objs)
	gcc -o cobtree $(cobtree_objs) `pkg-config --libs glib-2.0` -lrt

cobtree_sh: $(cobtree_sh_objs)
	gcc -o cobtree_sh $(cobtree_sh_objs) `pkg-config --libs glib-2.0` -lrt

clean:
	$(RM) tree_test cobtree *.o
