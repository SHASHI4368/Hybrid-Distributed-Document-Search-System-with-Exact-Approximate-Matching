CC = mpicc
CFLAGS = -fopenmp -Wall
OBJS = main.o file_utils.o matcher.o exact_match.o approx_match.o

docsearch: $(OBJS)
	$(CC) -o docsearch $(OBJS) -fopenmp -lm

clean:
	rm -f *.o docsearch
