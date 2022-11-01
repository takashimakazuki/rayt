CC=g++-12

rayt: rayt.cpp rayt.h
	$(CC) -o rayt rayt.cpp -fopenmp
