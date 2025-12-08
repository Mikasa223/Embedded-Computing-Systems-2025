//#############################################################################
// File: DMA_Timer_PINPONG.c
// Authors: Leonidas Vournas and Yuhang Du
// Chapter: DMA
// Code description: this code uses the CPU Timer 0 to trigger the DMA Transfer. The DMA is programmed to take 3 fixed locations,
// that can be mapped registers such as CMPSS Registers, and transfer the data to 3 memory locations.
// This example uses PING-PONG buffering, requiring 2*3 blocks of memory.
// Testing: Open a graph and observe the memories: at location rData
// Open a graph window and import "rData_display.graphProp" to visualize the data.
//#############################################################################

#include "sysctl.h"
#include "driverlib.h"
#include "device.h"
#include <sys/types.h>
#include <time.h>
#include <stdio.h>

//---------------------------------------------------------------------------
// DMA data sections
//---------------------------------------------------------------------------
// Place the TX and RX buffers in GS RAM so that DMA can access them
// independently of program/data RAM used by the CPU.
#pragma DATA_SECTION(sData, "ramgs0");  // Source buffer for DMA transfers
#pragma DATA_SECTION(rData, "ramgs1"); // Destination buffer for DMA transfers



//---------------------------------------------------------------------------
// DMA parameters
//---------------------------------------------------------------------------
// 3 words per "frame" (3 channels/measurements)
#define BURST          3          // Number of words in one logical frame
#define TRANSFER       152        // Number of frames per full DMA transfer

// Total number of words moved in one transfer (3 channels × TRANSFER frames)
#define DMA_Transfer   (TRANSFER * 3)

// Two frames (PING and PONG), each of size DMA_Transfer
#define rData_length   DMA_Transfer * 2

// For plotting convenience.
#define SAMPLES        TRANSFER

//---------------------------------------------------------------------------
// Globals
//---------------------------------------------------------------------------

// Source and destination buffers for DMA
uint16_t sData[100];
uint16_t rData[rData_length];

// Ping-pong flag (1 = update first half of rData)
int pingpong = 1;

// Timer debug counters: incremented in the timer ISRs
uint16_t cpuTimer0IntCount;

uint32_t sysClockFreq = 0;  // Will hold system clock frequency at run-time

// These pointers hold the base addresses for DMA source and destination
const void *destAddr1;
const void *destAddr2;
const void *srcAddr;

//---------------------------------------------------------------------------
// Function Prototypes
//---------------------------------------------------------------------------
void initDMA(void);
void initCPUTimers(void);
void configCPUTimer(uint32_t cpuTimer, float freq, float trigger_freq);
__interrupt void dmaCh1ISR(void);
__interrupt void cpuTimer0ISR(void);


//---------------------------------------------------------------------------
// Main
//---------------------------------------------------------------------------
void main(void)
{
    int i;

    // Map the C arrays to generic pointers expected by the DMA driverlib API
    srcAddr  = &sData[0];
    destAddr1 = &rData[0]; // Ping Region: 0 -> 455
    destAddr2 = &rData[456]; // Pong Region: 456 -> 911


    // Initialize the device
    Device_init();

    // Reset and configure the DMA controller
    initDMA();

    // Initialize and configure the CPU timer 0 -> generates DMA triggers
    initCPUTimers();
    sysClockFreq = SysCtl_getClock(DEVICE_OSCSRC_FREQ); // DEVICE_OSCSRC_FREQ = 20MHz (open declaration)

    // Configure Timer 0 at 8 kHz (trigger_freq argument is in Hz)
    configCPUTimer(CPUTIMER0_BASE, sysClockFreq, 8000.0f);


    // Initialize buffers:
    //  - sData is filled with 1s except of the measurements indices, sData[0], sData[30], sData[60]
    //  - rData is initially set to 0 and later filled by DMA
    for (i = 0; i < 912; i++)
    {
        rData[i] = 0;
    }

    for (i = 0; i < 100; i++)
        {
            sData[i] = 1;
        }

    sData[0] = 0;
    sData[30] = 30;
    sData[60] = 60;


    // Initialize PIE and CPU interrupt system
    Interrupt_initModule();
    Interrupt_initVectorTable();

    // Register DMA Channel 1 ISR.
    Interrupt_register(INT_DMA_CH1, &dmaCh1ISR);

    // Make sure DMA traffic goes through Peripheral Frame 2 bridge
    SysCtl_selectSecMaster(0, SYSCTL_SEC_MASTER_DMA);

    // Register ISRs for CPU Timer 0 and 1.
    Interrupt_register(INT_TIMER0, &cpuTimer0ISR);

    // Enable peripheral interrupts in the PIE.
    Interrupt_enable(INT_DMA_CH1);
    CPUTimer_enableInterrupt(CPUTIMER0_BASE);
    Interrupt_enable(INT_TIMER0);

    // Start CPU Timer 0
    CPUTimer_startTimer(CPUTIMER0_BASE);

    // Enable global interrupt mask (INTM) and real-time debug (DBGM).
    EINT;
    ERTM;

    // DMA Channel 1: responds to Timer 0 triggers.
    DMA_startChannel(DMA_CH1_BASE);

    while (1)
    {

    }
}


//---------------------------------------------------------------------------
// DMA Channel 1 ISR
//---------------------------------------------------------------------------
__interrupt void dmaCh1ISR(void)
{
    // Checks the value of the Ping-Pong flag

    if (pingpong == 1)
            {
                pingpong = 0; // Toggle the Ping-Pong flag
                DMA_configAddresses(DMA_CH1_BASE, destAddr2, srcAddr); // Change destination address of the buffer, to switch between the two halves of rData[]
            }
    else
            {
                pingpong = 1; // Toggle the Ping-Pong flag
                DMA_configAddresses(DMA_CH1_BASE, destAddr1, srcAddr); // Change destination address of the buffer, to switch between the two halves of rData[]

            }

    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP7);   // Clear PIE group 7 flag
    return;
}


//---------------------------------------------------------------------------
// DMA initialization and channel configuration
//---------------------------------------------------------------------------
void initDMA(void)
{
    // Perform a HARD reset of the DMA controller and return it to its power-up state (clears all channel configs).
    DMA_initController();

    // Configure source and destination base addresses for Channel 1.
    DMA_configAddresses(DMA_CH1_BASE, destAddr1, srcAddr);

    // Configure a transfer consisting of TRANSFER elements.
    // -- Burst: size = 3 , srcStep = 30, destSTep = 152
    // -- Transfer: size = 152, srcStep = 0, destStep = -303
    // -- Wrap: srcSize = 1 (all other parameters set to zero) -> wrap at the top of the sData after each burst, to continuously read the sData[0], sData[30] and sData[60]
    DMA_configBurst(DMA_CH1_BASE, BURST, 30, 152);
    DMA_configTransfer(DMA_CH1_BASE, TRANSFER, 0, -303);
    DMA_configWrap(DMA_CH1_BASE, 1, 0, 0, 0);


    // Configure DMA triggering and behavior:
    //  - Trigger source: Timer 0 interrupt (TINT0)
    //  - ONESHOT disabled: DMA runs continuously (restarts automatically)
    //  - CONTINUOUS enabled: keeps transferring on every trigger
    //  - SIZE_16BIT: moves 16-bit words.
    DMA_configMode(DMA_CH1_BASE,
                   DMA_TRIGGER_TINT0,
                   DMA_CFG_ONESHOT_DISABLE |
                   DMA_CFG_CONTINUOUS_ENABLE |
                   DMA_CFG_SIZE_16BIT);

    // Generate DMA interrupt at the end of the transfer.
    DMA_setInterruptMode(DMA_CH1_BASE, DMA_INT_AT_END);

    // Allow DMA Channel 1 to respond to triggers.
    DMA_enableTrigger(DMA_CH1_BASE);

    // Enable DMA Channel 1 interrupt in the controller.
    DMA_enableInterrupt(DMA_CH1_BASE);
}


//---------------------------------------------------------------------------
// Initialise CPU timers with default settings
//---------------------------------------------------------------------------
void initCPUTimers(void)
{
    // Set maximum period initially (will be re-configured later).
    CPUTimer_setPeriod(CPUTIMER0_BASE, 0xFFFFFFFF);

    // Prescalers = 0 -> timer clock = SYSCLK.
    CPUTimer_setPreScaler(CPUTIMER0_BASE, 0);

    // Ensure timers are stopped and counters reloaded.
    CPUTimer_stopTimer(CPUTIMER0_BASE);
    CPUTimer_reloadTimerCounter(CPUTIMER0_BASE);
}


//---------------------------------------------------------------------------
// Configure an individual CPU timer
//  - freq: system clock frequency in Hz
//  - trigger_freq: the desired triggering frequency of the design
//---------------------------------------------------------------------------
void configCPUTimer(uint32_t cpuTimer, float freq, float trigger_freq)
{

    // Convert triggering frequency to period (s)
    float period = 1 / trigger_freq;

    // Convert desired period (s) to timer ticks.
    uint32_t temp = (uint32_t)(freq * period);

    CPUTimer_setPeriod(cpuTimer, temp);
    CPUTimer_setPreScaler(cpuTimer, 0);
    CPUTimer_stopTimer(cpuTimer);
    CPUTimer_reloadTimerCounter(cpuTimer);

    // When halted by debugger, timer stops after next decrement.
    CPUTimer_setEmulationMode(cpuTimer,
                              CPUTIMER_EMULATIONMODE_STOPAFTERNEXTDECREMENT);
}


//---------------------------------------------------------------------------
// CPU Timer 0 ISR
//---------------------------------------------------------------------------
// Executed at 8 kHz.
__interrupt void cpuTimer0ISR(void)
{
    cpuTimer0IntCount++;  // Software counter
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);    // Clear PIE group 1 flag
}


//#############################################################################
// End of File
//#############################################################################
