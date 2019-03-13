#include <alsa/asoundlib.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "led-matrix-c.h"
#include "kiss_fftr.h"


#define AUDIO_DEVICE "hw:1"    // audio device to read from
#define MATRIX_ROWS 32         // matrix default row count
#define MATRIX_COLS 64         // matrix default column count
#define AUDIO_FS 44100         // Hz, audio sampling rate
#define N 750                  // audio sample buffer size 
#define N_NYQUIST (N / 2) + 1


void histogram(float *amplitudes);
void sigint_handler(int signo);
void clean_up();
void alsa_config_hw_params();
