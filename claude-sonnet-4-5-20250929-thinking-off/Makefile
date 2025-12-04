CC = gcc
CFLAGS = -O3 -march=native -mtune=native -flto -funroll-loops \
         -fno-trapping-math -fassociative-math \
         -freciprocal-math -msse4.2 -mavx2 \
         -Wall -Wextra

TARGET = 1brc
SRC = 1brc.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

test: $(TARGET)
	./$(TARGET) ../data/measurements.txt > output.txt
	diff output.txt ../data/expected_output.txt

benchmark: $(TARGET)
	time ./$(TARGET) ../data/measurements.txt > /dev/null

.PHONY: all clean test benchmark
