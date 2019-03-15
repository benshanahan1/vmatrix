#include <alsa/asoundlib.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "led-matrix-c.h"
#include "kiss_fftr.h"


/* Definitions. */
#define AUDIO_DEVICE "hw:1"  // audio device to read from
#define MATRIX_ROWS 32       // matrix default row count
#define MATRIX_COLS 64       // matrix default column count
#define FS 44100             // Hz, audio sampling rate
#define N 1600               // audio sample buffer size 


/* Computed definitions. */
#define N_NYQUIST (N / 2) + 1      // Nyquist frequency
#define FREQ_RES (FS / N)          // FFT frequency resolution
#define MIN_FREQ FREQ_RES          // freq. of lowest FFT bin
#define MAX_FREQ FREQ_RES * (N/2)  // freq. of highest FFT bin
#define MAX_FREQ_CAP 16000         // max freq. for visualization purposes


/* Function declarations. */
void sigint_handler(int signo);
void clean_up();
void alsa_config_hw_params();
double linspace(double min, double max, int i, int n);
double logspace(double min, double max, int i, int n);
float *bin_amplitudes(float *amplitudes);
void histogram(float *amplitudes);
void scrolling_spectrogram(float *binarr);
