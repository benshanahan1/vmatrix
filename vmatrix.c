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
float *history;
float *bins;
PointHistory *envelope;


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

	/* Allocate memory for 2-D history array. */
	if ((history = calloc(width * height, sizeof(float))) == NULL) {
		printf("Error allocating memory for history array.\n");
		exit(1);
	}

	/* Allocate array for creating amplitude envelope. */
	if ((envelope = calloc(width, sizeof(PointHistory))) == NULL) {
		printf("Error allocating memory for envelope array.\n");
		exit(1);
	}

	if ((fftr_cfg = kiss_fftr_alloc(N, 0, NULL, NULL)) == NULL) {
		printf("Error allocating memory for FFT.\n");
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

		/* Compute amplitude of frequency components. Since FFT has
		 * symmetric magnitude, we only need to take absolute value
		 * of the real component to get the amplitude. */
		float amplitudes[N_NYQUIST];
		for (int k = 0; k < N_NYQUIST; ++k) {
			amplitudes[k] = abs(out[k].r);
		}

		/* Update matrix display. */
		led_canvas_clear(canvas);
		bins = bin_amplitudes(amplitudes);

		bool show_envelope = true;
		bool fill_hist = true;
		histogram(bins, show_envelope, fill_hist);
		//scrolling_spectrogram(bins);

		/* Now, we swap the canvas. We give swap_on_vsync the buffer we
		 * just have drawn into, and wait until the next vsync happens.
		 * we get back the unused buffer to which we'll draw in the next
		 * iteration.
		 */
		canvas = led_matrix_swap_on_vsync(matrix, canvas);

	}

	clean_up();
	return 0;
}


/** Bin amplitudes from FFT. */
float *bin_amplitudes(float *amplitudes) {
	//double lower_edge = 0;

	int size = width;
	//int size = height;

	/* Allocate memory for binned amplitudes array. The `fmaxf` function
	 * finds the maximum of two floats.
	 */
	float *binarr;
	if ((binarr = calloc(size, sizeof(float))) == NULL) {
		printf("Error allocating memory for binned amplitude array.\n");
		exit(1);
	}

	float scaling = 1.0 / (FS / 3.0);

	for (int x = 0; x < size; ++x) {

		// First several amplitudes are too low too hear, so we offset
		// the amplitude indexing to skip them.
		binarr[x] = amplitudes[x + 2] * scaling;

		/*
		double max_freq = (MAX_FREQ < MAX_FREQ_CAP) ?
			MAX_FREQ : MAX_FREQ_CAP;

		// Bin the FFT buffer logarithmically.	
		//double upper_edge = logspace(MIN_FREQ, max_freq, x, size);
		//double index_scaling = FREQ_RES;
		//double scaling = 100;
	
		// Bin FFT linearly.
		//double upper_edge = linspace(MIN_FREQ, max_freq, x, size);
		//double index_scaling = FREQ_RES;
		//double scaling = FS;
	
		// Bin FFT linearly (method 2).
		//double upper_edge = 100 + 100 * x;
		//double index_scaling = 12.67;
		//double scaling = 10;	

		for (int i = 0; i < N_NYQUIST; ++i) {
			double f = (double) i * index_scaling;
			if (f >= lower_edge && f <= upper_edge) {
				binarr[x] += amplitudes[i] / scaling;
			} else if (f > upper_edge) {
				break;
			}
		}

		lower_edge = upper_edge;
		*/
	}

	// Return allocated pointer to binned amplitude values.
	return binarr;
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


/** A horizontally scrolling spectrogram. */
void scrolling_spectrogram(float *binarr) {
	/* Shift 2D history array. Since this 2D array is actually contiguous in
	 * memory, we can just shift all elements back by `width` (dropping the
	 * last `width` elements). Then, we can add our new `binarr` array to
	 * the front of the `history` array.
	 */
	int d = height;  // dimension that binning is over

	for (int i = (width * height) - 1; i >= d; --i)
		history[i] = history[i - d];

	for (int i = 0; i < d; ++i)
		history[i] = binarr[i];

	float s_min = 0.0;
	float s_max = 350.0;

	/* Since history is a contiguous 1D array (but we're using it to store
	 * 2D information) we can just incrementally step through it and we will
	 * retrieve the appropriate values for each column of the scrolling
	 * spectrogram matrix (we use the nested loops to get `x` and `y` for
	 * drawing to the canvas, but we use `ctr` to keep track of our index in
	 * `history` array). */
	int ctr = 0;
	for (int x = width - 1; x >= 0; --x) {
		for (int y = height - 1; y >= 0; --y) {
			int bin = history[ctr];

			// Normalize bin value.
			if (bin > s_max) bin = s_max;  // cap value of bin
			float normalized = (bin - s_min) / (s_max - s_min);
			float inverted = ((1.0 - normalized) / 0.25);
			int group = (int) inverted;
			int scale = (int) (255 * (inverted - group));

			// Map `group` and `scale` to RGB colormap.
			int r = 0, g = 0, b = 0;
			switch (group) {
				case 0:
					r = 255; b = scale; break;
				case 1:
					r = 255 - scale; b = 255; break;
				case 2:
					r = 0; b = 255; break;
				case 3:
					r = 0; b = 255 - scale; break;
				case 4:
					r = 0; b = 0; break;
			}

			led_canvas_set_pixel(canvas, x, y, r, g, b);
			ctr ++;
		}
	}
}


/** A basic spectrogram histogram visualization.
 *
 * If `fill_hist` is true, fill each histogram bin vertically.
 */
void histogram(float *binarr, bool show_envelope, bool fill_hist) {
	int y;
	float scaling = 1.0 / 20.0;

	for (int x = 0; x < width; ++x) {
		y = (height) - (int) (binarr[x] * scaling);
		if (y <= 0) y = 0;

		if (y > 0) {
			if (fill_hist == true) {
				for (int yy = height; yy >= y; --yy) {
					int r = yy;
					int g = 0;
					int b = yy * 7;
					led_canvas_set_pixel(canvas, x, yy, r, g, b);
				}
			} else {
				led_canvas_set_pixel(canvas, x, y, 0xff, 0, 0xff);
			}
		}

		// Update amplitude envelope.
		if (y < envelope[x].y) envelope[x].y = y;
		if (envelope[x].y > height) envelope[x].y = height;
		if (envelope[x].counter-- < 0) {
			envelope[x].y ++;
			envelope[x].counter = ENVELOPE_CTR;
		}
	}

	// Update envelope pixels on canvas.
	if (show_envelope) {
		for (int i = 0; i < width; ++i) {
			/* Don't set the pixels if they are on the bottom row of
			 * the canvas (this makes things look bad). */
			if (envelope[i].y != height) {
				int r = 0xcc;
				int g = 0;
				int b = 0x66;
				led_canvas_set_pixel(canvas, i, envelope[i].y, r, g, b);
			}
		}
	}
}


/** Clean up at the end of the process. */
void clean_up() {
	// Close sound device.
	snd_pcm_close(capture_handle);

	// Clean up kissfft.
	free(fftr_cfg);			
	kiss_fft_cleanup();

	// Free allocated arrays.
	free(history);
	free(envelope);
	free(bins);

	// Reset matrix display.
	led_matrix_delete(matrix);

	printf("Goodbye.\n");
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


/** Handle SIGINT, i.e. CTRL+C. */
void sigint_handler(int signo) {
	printf("Caught SIGINT. Cleaning up...\n");
	clean_up();
}
