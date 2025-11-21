//#############################################################################
// File: DMD_Ping_Pong.c
// Author: Naim Dahnoun
// Chapter: DMA
// Code description: this code uses the Timer to trigger the DMA. The DMA is programmed to take 3 fixed locations,
// that can be mapped registers such as CMPSS Registers, and transfer the data to 3 memory locations.
// This example uses PING-PONG buffering, requiring 2* 3 blocks of memory.
// Testing: Open a graph and observe the memories:at location rData
// Open a graph window and import "rData_display.graphProp" to visualize the data.
//#############################################################################

#include "sysctl.h"
#include "driverlib.h"
#include "device.h"
#include <sys/types.h>
#include <time.h>
#include <stdio.h>



// Main function
void main(void) {
   

    while (1) {
        // Main loop: Application can add other tasks here
    }
}

// DMA Channel1 ISR





/* to be completed*/




// DMA Initialization




/*  to be completed*/





// Initialize CPU Timers



    // to be completed


// Configure CPU Timer



 //   to be completed



// CPU Timer 0 ISR


// CPU Timer 1 ISR


//#############################################################################
// End of File
//#############################################################################
