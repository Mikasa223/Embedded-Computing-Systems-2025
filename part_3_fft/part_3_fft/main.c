#include "i_cmplx.h"  /* Definition of the complex type */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <string.h> // For initializing the complex array COMPLEX fft_input[NUM_SAMPLES]

#define SAMPLING_RATE 8000 // Sampling rate in Hz
#define FREQ1 600          // Frequency of the first sine wave in Hz
#define FREQ2 200          // Frequency of the second sine wave in Hz

#define NUM_SAMPLES 1024   // Number of samples

#define AMPLITUDE 32767    // Maximum amplitude for 16-bit signed integers
#define PI 3.14159265358979323846

extern void fft(Complex *Y, int N);
extern int magnitude(int real, int imag);

// Generate sine wave samples


int main() {
    // Allocate memory for FFT input array
    Complex *signal = malloc(NUM_SAMPLES * sizeof(Complex));

    // Generate the signal


    // Perform FFT
    fft(signal, NUM_SAMPLES);

    // Compute magnitudes


    }





