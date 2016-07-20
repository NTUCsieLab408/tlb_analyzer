CFLAGS=-Wall -Wextra

all: libtlb_analyzer.a tlb_sim docs

tlb_trace.o: tlb_trace.c tlb_trace.h
	$(CC) $(CFLAGS) -c tlb_trace.c

tlb_sim.o: tlb_sim.c tlb_sim.h
	$(CC) $(CFLAGS) -c tlb_sim.c

tlb_sim: main.c libtlb_analyzer.a
	$(CC) $(CFLAGS) -o tlb_sim main.c -L. -ltlb_analyzer $(LDFLAGS)

libtlb_analyzer.a: tlb_trace.o tlb_sim.o
	$(AR) rcs libtlb_analyzer.a tlb_trace.o tlb_sim.o

docs: tlb_analyzer.cfg mainpage.dox tlb_trace.h tlb_sim.h
	rm -rf docs
	doxygen tlb_analyzer.cfg

clean:
	rm -f libtlb_analyzer.a *.o tlb_sim
	rm -rf docs
