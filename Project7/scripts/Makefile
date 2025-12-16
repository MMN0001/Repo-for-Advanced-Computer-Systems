CC = gcc

CFLAGS = -O3 -march=native -pthread -Wall -Wextra

BASELINE = hash_baseline
FINE     = hash_fine_grained

BASELINE_SRC = hash_baseline.c
FINE_SRC     = hash_fine_grained.c

all: $(BASELINE) $(FINE)

$(BASELINE): $(BASELINE_SRC)
	$(CC) $(CFLAGS) -o $@ $<

$(FINE): $(FINE_SRC)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(BASELINE) $(FINE) *.o *.csv

baseline: $(BASELINE)
fine: $(FINE)

rebuild: clean all
