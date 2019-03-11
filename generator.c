/** Generator.
 * 
 * Generate waveform data and write it to `stdout`.
 */

#include <stdio.h>
#include <math.h>
#include <unistd.h>

#define FS 10000.0  // simulated sampling rate
#define SCALE 10000.0  // max (and min) value output value
#define SLEEP_INTERVAL (1 / FS)*1000000

/** Generate data point. */
long generate_sine(long x) {
	double zzz = sin(((double) x) / FS);
	double yyy = cos((((double) x) * 10) / FS);

	// Scale generated value 
	return (long) ((zzz + yyy) * SCALE);
}

int main(int argc, char *argv[]) {
	long counter = 0;

	for (;;) {
		fprintf(stdout, "%ld\n", generate_sine(counter));
		counter ++;
		usleep(SLEEP_INTERVAL);
	}

	return 0;
}
