#include "driverlib.h"
#include "device.h"
#include "Myboard.h"
#include <stdio.h>


#define EPWM_TIMER_TBPRD 0000        // to be completed
#define ADC_BUF_LEN      0000       // to be completed
#define SAMPLING_FREQ    0000       // to be completed
#define ADC_ACQPS_TICKS  0000          // to be completed

uint16_t AdcBuf[ADC_BUF_LEN];


void main(void)
 {
    //to be completed

    while (1)
    {
        // Main loop stays empty; sampling and data transfer handled in ADC ISR
        asm(" NOP");
    }
}

// ================= ePWM1 ISR =================

//to be completed

// ================= ADC ISR =================

 // to be colpleted

// ================= NEW: Timer0 ISR (40 kHz triggers ADC) ==============
//to be completed

// ================= ePWM initialization =================


 // to be completed




// ================= NEW: Unified initialization of ADCA + Timer0 =================

//to be completed

