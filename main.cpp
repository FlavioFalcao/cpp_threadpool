#include <stdlib.h>
#include <stdio.h>
#include <cstdlib>
#include <unistd.h>
#include "threadpool.h"

#define SAMPLES 1000000000UL
#define NWORKERS 100
#define SAMPLES_PER_WORKER SAMPLES/NWORKERS

// Results array
unsigned long results[NWORKERS];

// Thread Local: random number generator state
__thread struct drand48_data rand_state;

/**
 * Utility function to find out the number of cores
 */
inline int num_cores() {
	return sysconf( _SC_NPROCESSORS_ONLN );
}

/** 
 * Init random seed
 */
void mc_seed_init(unsigned long pos)
{
	srand48_r(pos, &rand_state);
}

/** 
 * Monte Carlo Pi aproximation
 * tries `samples` times and returns the number of positives
 */
unsigned long mc_pi(unsigned long samples)
{
	unsigned long in = 0;
	for (unsigned long i = 0; i < samples; i++)
	{
		double x;
		double y;

		drand48_r(&rand_state, &x);
		drand48_r(&rand_state, &y);

		if ((x*x+y*y) < 1e0) in++;
	}

	return in;
}

class MyWorker : public ThreadPoolWorker
{
private:
	unsigned long pos;		// Worker ID
	unsigned long samples;  // Number of samples to test
public:
	MyWorker(unsigned long _pos, unsigned long _samples) : pos(_pos), samples(_samples) { }
	void operator()()
	{
		mc_seed_init(pos);
		results[pos] = mc_pi(samples);
	}
};

int main( int argc, char *argv[]) {

	// Define the threadpool with 1 thread per core
	ThreadPool pool = ThreadPool(num_cores());

	// Enqueue work
	for (unsigned long i = 0; i < NWORKERS; i++) {
		pool.enqueueWork(new MyWorker(i, SAMPLES_PER_WORKER));
	}

	// Wait for all workers to finish
	pool.waitEnd();

	// Aggregate results
	unsigned long in = 0;
	for (unsigned long i = 0; i < NWORKERS; i++) {
		in += results[i];
	}

	// Finish MC aproximation
	double pi =  (in*4e0)/SAMPLES;

	printf("Total number of samples: %lu", SAMPLES);
	printf(" of those, fell inside: %lu\n\n", in);

	printf("PI: %16.15f\n", pi);

	return 0;
}

