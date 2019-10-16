CC= gcc
CFLAGS= -g
LIBOBJS = myalloc.o
LIB=myalloc
LIBFILE=lib$(LIB).a
TESTS = test
all: $(TESTS) 

%.o: %.c
	$(CC) $(CFLAGS) -c $< 

test : test.o $(LIB)
	$(CC) test.o $(CFLAGS) -o test -L. -l$(LIB) -lpthread

$(LIB) : $(LIBOBJS)
	ar -cvr $(LIBFILE) $(LIBOBJS)
	#ranlib $(LIBFILE) # may be needed on some systems
	ar -t $(LIBFILE)

clean:
	/bin/rm -f *.o $(TESTS) $(LIBFILE)
