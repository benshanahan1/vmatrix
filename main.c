/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *
 * Using the C-API of this library.
 *
 */
#include "led-matrix-c.h"
#include "kiss_fftr.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <alsa/asoundlib.h>

#define US       1000000.0             // microseconds per second

#define REFRESH  100.0                 // Hz, matrix panel refresh rate
#define FS       10000.0               // Hz, audio sampling rate
#define SAMP_DUR (1.0 / FS) * US       // usec, time between each sample
#define WIN_DUR  (1.0 / REFRESH) * US  // usec, window duration

#define N 3000 
#define N_NYQUIST (N / 2) + 1

int main(int argc, char *argv[]) {
	struct RGBLedMatrixOptions options;
	struct RGBLedMatrix *matrix;
	struct LedCanvas *offscreen_canvas;
	int width, height;
	int x, y;
	//int x, y, i;

	/* Configure ALSA for audio! */

	int err;
	short buf[N];
	snd_pcm_t *capture_handle;
	snd_pcm_hw_params_t *hw_params;

	if ((err = snd_pcm_open (&capture_handle, argv[1], SND_PCM_STREAM_CAPTURE, 0)) < 0) {
		fprintf (stderr, "cannot open audio device %s (%s)\n", 
			 argv[1],
			 snd_strerror (err));
		exit (1);
	}

	if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
		fprintf (stderr, "cannot allocate hardware parameter structure (%s)\n",
			 snd_strerror (err));
		exit (1);
	}
			 
	if ((err = snd_pcm_hw_params_any (capture_handle, hw_params)) < 0) {
		fprintf (stderr, "cannot initialize hardware parameter structure (%s)\n",
			 snd_strerror (err));
		exit (1);
	}

	if ((err = snd_pcm_hw_params_set_access (capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		fprintf (stderr, "cannot set access type (%s)\n",
			 snd_strerror (err));
		exit (1);
	}

	if ((err = snd_pcm_hw_params_set_format (capture_handle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
		fprintf (stderr, "cannot set sample format (%s)\n",
			 snd_strerror (err));
		exit (1);
	}

	unsigned int rate = 44100;
	if ((err = snd_pcm_hw_params_set_rate_near (capture_handle, hw_params, &rate, 0)) < 0) {
		fprintf (stderr, "cannot set sample rate (%s)\n",
			 snd_strerror (err));
		exit (1);
	}

	/*
	if ((err = snd_pcm_hw_params_set_channels (capture_handle, hw_params, 2)) < 0) {
		fprintf (stderr, "cannot set channel count (%s)\n",
			 snd_strerror (err));
		exit (1);
	}
	*/


	if ((err = snd_pcm_hw_params (capture_handle, hw_params)) < 0) {
		fprintf (stderr, "cannot set parameters (%s)\n",
			 snd_strerror (err));
		exit (1);
	}

	snd_pcm_hw_params_free (hw_params);

	if ((err = snd_pcm_prepare (capture_handle)) < 0) {
		fprintf (stderr, "cannot prepare audio interface for use (%s)\n",
			 snd_strerror (err));
		exit (1);
	}








	// Compute `N`, the number of samples taken per window.
	if (N <= 0) {
		printf("[ERROR]: N must be greater than 0 (N=%d)\n", N);
		exit(1);
	}

	kiss_fft_scalar in[N];
	kiss_fft_cpx out[N_NYQUIST];

	//char *line = NULL;
	//size_t size;
	
	memset(&options, 0, sizeof(options));
	options.rows = 32;
	options.cols = 64;
	options.chain_length = 1;

	/* This supports all the led commandline options. Try --led-help */
	matrix = led_matrix_create_from_options(&options, &argc, &argv);
	if (matrix == NULL)
		return 1;

	/* We use double-buffering: we have two buffers for the RGB matrix
	 * that we swap on each update. 
	 */
	offscreen_canvas = led_matrix_create_offscreen_canvas(matrix);

	led_canvas_get_size(offscreen_canvas, &width, &height);
	fprintf(stderr, "Size: %dx%d. Hardware gpio mapping: %s\n",
					width, height, options.hardware_mapping);

	/* FFT + RGB update loop. */
	for (;;) {
		
		/* Populate sample buffer for FFT. */

		// Read from sound card.
		if ((err = snd_pcm_readi(capture_handle, buf, N)) != N) {
			fprintf(stderr, "read from audio device failed (%s)\n", snd_strerror(err));
			exit(1);
		}
		for (int zzz = 0; zzz < N; ++zzz) {
			in[zzz] = buf[zzz];
		}

		/*
		for (int j = 0; j < N; ++j) {
			if (getline(&line, &size, stdin) == -1) {
				printf("--- NO LINE ---\n");
			} else {
				// convert line (string) to float
				//in[j] = (float) strtol(line, NULL, 10);
				float SCALE = 100000; // from generator.c, this will go away
				float sample = (float) strtol(line, NULL, 10);
				//printf("sample: %f\n", sample / SCALE);
				//in[j] = cos(j*2*M_PI/N);
				//printf("input: %f\n", in[j]);
				in[j] = sample / SCALE;
				usleep(SAMP_DUR);
			}
		}
		*/

		/* Do FFT on buffered data. */
		kiss_fftr_cfg fftr_cfg;
		if ((fftr_cfg = kiss_fftr_alloc(N, 0, NULL, NULL)) != NULL) {
			kiss_fftr(fftr_cfg, in, out);  // do FFT
			free(fftr_cfg);

			/*
			for (int k = 0; k < N; k++) {
				printf(" in[%2zu] = %+f    ", k, in[k]);
				if (k < N / 2 + 1)
					printf("out[%2zu] = %+f , %+f", k, out[k].r, out[k].i);
				printf("\n");
			}
			*/

		} else {
			printf("Out of memory?\n");
			exit(1);
		}

		/* Compute amplitude of frequency components. */
		float amplitudes[N_NYQUIST];
		for (int k = 0; k < N_NYQUIST; ++k) {
			amplitudes[k] = sqrt(out[k].r*out[k].r + out[k].i*out[k].i);
		}

		//for (int jj = 0; jj < N_NYQUIST; ++jj)
		//	printf("%f, ", amplitudes[jj]);
		//printf("\n");

		/* Update matrix display. */
		//int r, g, b;
		led_canvas_clear(offscreen_canvas);
		for (int a = 0; a < N_NYQUIST; a++) {
			x = a;
			y = (height - 1) - (int) (amplitudes[a] / 44100);
			
			if (y < 0) {
				y = 0;
			}

			// printf("%d ", y);
			for (int aa = height - 1; aa >= y; --aa) {
				led_canvas_set_pixel(offscreen_canvas, x, aa, 0xff, aa * 7, aa * 7);
			}
		}
		// printf("\n");

		/*
		for (y = 0; y < height; ++y) {
			for (x = 0; x < width; ++x) {
				r = 0;
				g = 0;
				b = 0;

				led_canvas_set_pixel(offscreen_canvas, x, y, r, g, b);
			}
		}
		*/

		/* Now, we swap the canvas. We give swap_on_vsync the buffer we
		 * just have drawn into, and wait until the next vsync happens.
		 * we get back the unused buffer to which we'll draw in the next
		 * iteration.
		 */
		offscreen_canvas = led_matrix_swap_on_vsync(matrix, offscreen_canvas);
	
	}

	/* Close sound device. */
	snd_pcm_close (capture_handle);

	/* Clean up kissfft. */
	kiss_fft_cleanup();

	/* Make sure to always call led_matrix_delete() in the end to reset the
	 * display. Installing signal handlers for defined exit is a good idea.
	 */
	led_matrix_delete(matrix);

	return 0;

}
