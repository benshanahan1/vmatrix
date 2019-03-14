/** VMATRIX 
 *
 * Real-time audio spectrogram on an RGB LED matrix.
 */

#include "vmatrix.h"


/* Define global variables. */
struct RGBLedMatrixOptions options;
struct RGBLedMatrix *matrix;
struct LedCanvas *canvas;
int width, height;
snd_pcm_t *capture_handle;
snd_pcm_hw_params_t *hw_params;
kiss_fftr_cfg fftr_cfg;


int main(int argc, char *argv[]) {

	// Install SIGINT handler.
	signal(SIGINT, sigint_handler);

	char *device = AUDIO_DEVICE;

	kiss_fft_scalar in[N];
	kiss_fft_cpx out[N_NYQUIST];

	memset(&options, 0, sizeof(options));
	options.rows = MATRIX_ROWS;
	options.cols = MATRIX_COLS;
	options.chain_length = 1;

	/* This supports all the led commandline options. Try --led-help */
	matrix = led_matrix_create_from_options(&options, &argc, &argv);
	if (matrix == NULL)
		return 1;

	/* Configure ALSA for audio! */
	int err;
	short buf[N];

	err = snd_pcm_open(&capture_handle, device, SND_PCM_STREAM_CAPTURE, 0);
	if (err < 0) {
		fprintf(stderr, "cannot open audio device %s (%s)\n", device,
				snd_strerror (err));
		exit(1);
	}
	
	// Configure ALSA hardware parameters.
	alsa_config_hw_params();

	snd_pcm_hw_params_free (hw_params);

	err = snd_pcm_prepare (capture_handle);
	if (err < 0) {
		fprintf(stderr, "cannot prepare audio interface (%s)\n",
				snd_strerror(err));
		exit(1);
	}

	/* We use double-buffering: we have two buffers for the RGB matrix
	 * that we swap on each update. 
	 */
	canvas = led_matrix_create_offscreen_canvas(matrix);

	led_canvas_get_size(canvas, &width, &height);
	fprintf(stderr, "Size: %dx%d. Hardware gpio mapping: %s\n",
			width, height, options.hardware_mapping);

	if ((fftr_cfg = kiss_fftr_alloc(N, 0, NULL, NULL)) == NULL) {
		printf("Failed to allocate memory for FFT.\n");
		exit(1);
	}

	/* FFT + RGB update loop. */
	for (;;) {
		
		// Read from sound card.
		if ((err = snd_pcm_readi(capture_handle, buf, N)) != N) {
			fprintf(stderr, "read from audio device failed (%s)\n",
					snd_strerror(err));
			exit(1);
		}
		for (int g = 0; g < N; ++g) in[g] = (kiss_fft_scalar) buf[g];

		/* Do FFT on buffered data. */
		kiss_fftr(fftr_cfg, in, out);

		/* Compute amplitude of frequency components. */
		float amplitudes[N_NYQUIST];
		for (int k = 0; k < N_NYQUIST; ++k) {
			float squared = out[k].r*out[k].r + out[k].i*out[k].i;
			amplitudes[k] = sqrt(squared);
		}

		/* Update matrix display. */
		led_canvas_clear(canvas);
		histogram(amplitudes);

		/* Now, we swap the canvas. We give swap_on_vsync the buffer we
		 * just have drawn into, and wait until the next vsync happens.
		 * we get back the unused buffer to which we'll draw in the next
		 * iteration.
		 */
		canvas = led_matrix_swap_on_vsync(matrix,
				canvas);
	
	}

	clean_up();
	return 0;

}


/** Configure ALSA hardware parameters. */
void alsa_config_hw_params() {
	int err;
	unsigned int rate = FS;

	err = snd_pcm_hw_params_malloc(&hw_params);
	if (err < 0) {
		fprintf(stderr, "cannot allocate hw parameter struct (%s)\n",
				snd_strerror(err));
		exit(1);
	}

	err = snd_pcm_hw_params_any(capture_handle, hw_params);		 
	if (err < 0) {
		fprintf(stderr, "cannot initialize hw parameter struct (%s)\n",
				snd_strerror(err));
		exit(1);
	}

	err = snd_pcm_hw_params_set_access(capture_handle, hw_params,
			SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0) {
		fprintf(stderr, "cannot set access type (%s)\n",
				snd_strerror(err));
		exit(1);
	}
	
	err = snd_pcm_hw_params_set_format(capture_handle, hw_params,
			SND_PCM_FORMAT_S16_LE);
	if (err < 0) {
		fprintf(stderr, "cannot set sample format (%s)\n",
				snd_strerror (err));
		exit(1);
	}

	err = snd_pcm_hw_params_set_rate_near(capture_handle, hw_params,
			&rate, 0);
	if (err < 0) {
		fprintf(stderr, "cannot set sample rate (%s)\n",
				snd_strerror(err));
		exit(1);
	}

	err = snd_pcm_hw_params(capture_handle, hw_params);
	if (err < 0) {
		fprintf(stderr, "cannot set parameters (%s)\n",
				snd_strerror(err));
		exit(1);
	}
}


/** Return bin edge in linear-space for given values. */
double linspace(double min, double max, int i, int n) {
	double diff = (max - min) / (double) n;
	return min + (diff * (double) i);
}


/** Return bin edge in logarithmic-space for given values. */
double logspace(double min, double max, int i, int n) {
	double lin = linspace(log(min), log(max), i, n);
	return exp(lin);
}


/** Clean up at the end of the process. */
void clean_up() {
	// Close sound device.
	snd_pcm_close(capture_handle);

	// Clean up kissfft.
	free(fftr_cfg);			
	kiss_fft_cleanup();

	// Reset matrix display.
	led_matrix_delete(matrix);

	printf("Goodbye.\n");
}


/** Handle SIGINT, i.e. CTRL+C. */
void sigint_handler(int signo) {
	printf("Caught SIGINT. Cleaning up...\n");
	clean_up();
}


/** A basic spectrogram histogram visualization. */
void histogram(float *amplitudes) {
	//int x
	int y;
	double lower_edge = 0;

	//amplitudes[N_NYQUIST-1] = 10000000;

	int w = width;
	for (int x = 0; x < w; ++x) {

		//double max_freq = (MAX_FREQ < MAX_FREQ_CAP) ?
		//	MAX_FREQ : MAX_FREQ_CAP;

		// Bin the FFT buffer logarithmically.
		/*	
		double upper_edge = logspace(MIN_FREQ, max_freq, x, w);
		double data = 0;
		int ctr = 0;
		for (int i = 0; i < N_NYQUIST; ++i) {
			double f = (double) i * FREQ_RES;
			if (f >= lower_edge && f <= upper_edge) {
				data += amplitudes[i];
				ctr ++;
			} else if (f > upper_edge) {
				break;
			}
		}
		*/

		// Bin the FFT buffer linearly
		/*
		double upper_edge = linspace(MIN_FREQ, max_freq, x, w);
		double data = 0;
		int ctr = 0;
		for (int i = 0; i < N_NYQUIST; ++i) {
			double f = (double) i * FREQ_RES;
			if (f >= lower_edge && f <= upper_edge) {
				data += amplitudes[i] / 10;
				ctr ++;
			} else if (f > upper_edge) {
				break;
			}
		}
		*/

		// Bin the FFT linearly.
		double upper_edge = 100 + 100 * x;
		double data = 0;
		//int ctr = 0;
		for (int i = 0; i < N_NYQUIST; ++i) {
			float f = i * 12.67;  // scale so bins are distributed
			if (f >= lower_edge && f <= upper_edge) {
				data += amplitudes[i] / 10;
				//ctr ++;
			} else if (f > upper_edge) {
				break;
			}
		}

		//printf("%d ", ctr);

		y = (height - 1) - (int) (data / FS);
	
		if (y < 0) y = 0;

		for (int aa = height - 1; aa >= y; --aa) {
			led_canvas_set_pixel(canvas, x, aa, 0xff, aa*7, aa*7);
		}

		lower_edge = upper_edge;
	}
	//printf("\n");

}
