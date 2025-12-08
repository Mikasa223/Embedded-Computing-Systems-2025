/****************** 

Authors: Abdalrahim Naser & Lady Payan
Emails: fk22040@bristol.ac.uk, lnicole.pc.2022@bristol.ac.uk
Description: ePWM-ADC loopback example with a PWM frequency of 1khz and sampling frequency of 40khz

******************/ 

// Included Files
#include "driverlib.h"
#include "device.h"
#include "sysctl.h"
#include "Myboard.h"

// Macros
#define ADC_BUF_LEN  250
#define PWM_FREQ 1000
#define SAMPLING_FREQ 40e3
#define ADC_ACQPS_TICKS 14
#define TBCLK 100e6 // 200mhz / 2 (default divider value, reference: https://dev.ti.com/tirex/explore/node?node=A__ASXXwGbQ.ubt5o3S3jXEvA__C28X-ACADEMY__1sbHxUB__LATEST)
#define TBCLK_DIVIDER 4


// Variables
uint16_t rawData[ADC_BUF_LEN];
float32_t measuredVals[ADC_BUF_LEN];
uint16_t adcBufferIndex = 0;    // Current index for ADC data buffer
uint32_t sysClockFreq = 0;

// Function Prototypes
__interrupt void adcA1ISR(void);
void initCPUTimers(void);
void configCPUTimer(uint32_t, float, float);
void configureADC(void);
void configureADCSOC(void);
void initEPWM(uint32_t base);


#define MY_DEVICE_SETCLOCK_CFG \
    (SYSCTL_OSCSRC_XTAL |       /* Use external crystal oscillator */ \
     SYSCTL_IMULT(20) |         /* Integer multiplier = 20 (e.g., 10 MHz * 20 = 200 MHz VCO) */ \
     SYSCTL_FMULT_NONE |        /* No fractional multiplier */ \
     SYSCTL_SYSDIV(0) |         /* Divide-by-1 for SYSCLK = 200 MHz */ \
     SYSCTL_PLL_ENABLE)         /* Enable PLL */



// Main
void main(void){ 
    int i = 0;                                                                                            
    Device_init(); // Initializes device clock and peripherals
    Interrupt_initModule();     // Initializes PIE and clears PIE registers.
    Interrupt_initVectorTable(); // Initializes the PIE vector table

    SysCtl_setClock(MY_DEVICE_SETCLOCK_CFG);
    DEVICE_DELAY_US(1000);  // Delay for 1s to settle the PLL



    // TIMER
    initCPUTimers();  // Initialize the Device Peripheral timers
    configCPUTimer(CPUTIMER0_BASE, DEVICE_SYSCLK_FREQ, SAMPLING_FREQ); // fs in hertz
    CPUTimer_enableInterrupt(CPUTIMER0_BASE); // interrupt to trigger ADC conversions

    // ADC
    configureADC();     // Configure ADCA (ADC0)
    configureADCSOC();  // Configure SOC for ADCA
    Interrupt_register(INT_ADCA1, &adcA1ISR);
    Interrupt_enable(INT_ADCA1);

    // PWM
    SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);
    PinMux_init();
    SYNC_init();
    initEPWM(EPWM1_BASE);     // Initialize ePWM settings
    EPWM_setClockPrescaler(EPWM1_BASE, EPWM_CLOCK_DIVIDER_4, EPWM_HSCLOCK_DIVIDER_1); // Explicitly set prescaler values: no division (1:1)
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);     // Re-enable time-base clock sync to start all ePWM counters
    DEVICE_DELAY_US(100);  // Delay for 100 microseconds to settle the PLL

    // start the timer
    CPUTimer_startTimer(CPUTIMER0_BASE);

    // Initialize rawData and measuredVals arrays
    for (i = 0; i < ADC_BUF_LEN; i++)
    {
        rawData[i] = 0;
        measuredVals[i]=0;
    }

    EINT;  // Enable Global interrupt INTM
    ERTM;  // Enable Global realtime interrupt DBGM

    // main loop
    while (1)
    {
        // Continuously check if buffer is filled for further processing
        if (adcBufferIndex >= ADC_BUF_LEN)
        {
            asm(" NOP"); // debugging breakpoint
            adcBufferIndex = 0;  // Reset buffer index after filling
        }
    }
}


// Initialize CPU Timers to a known state
void initCPUTimers(void)
{
    // Initialize timer period to maximum
    CPUTimer_setPeriod(CPUTIMER0_BASE, 0xFFFFFFFF);

    // Initialize pre-scale counter to divide by 1 (SYSCLKOUT)
    CPUTimer_setPreScaler(CPUTIMER0_BASE, 0);

    // Make sure timer is stopped
    CPUTimer_stopTimer(CPUTIMER0_BASE);

    // Reload all counter register with period value
    CPUTimer_reloadTimerCounter(CPUTIMER0_BASE);
}

// Configure CPU Timer
void configCPUTimer(uint32_t cpuTimer, float freq, float fs)
{
    uint32_t temp = (uint32_t)(freq / fs);

    // Initialize timer period
    CPUTimer_setPeriod(cpuTimer, temp);

    // Set pre-scale counter to divide by 1 (SYSCLKOUT)
    CPUTimer_setPreScaler(cpuTimer, 0);

    // Initializes timer control register. The timer is stopped, reloaded,
    // free run disabled, and interrupt enabled.
    CPUTimer_stopTimer(cpuTimer);
    CPUTimer_reloadTimerCounter(cpuTimer);
    CPUTimer_setEmulationMode(cpuTimer, CPUTIMER_EMULATIONMODE_STOPAFTERNEXTDECREMENT);
    CPUTimer_enableInterrupt(cpuTimer);
}


// Configure ADC (Only ADCA is used here)
void configureADC(void)
{
    // Set the prescaler to divide the ADC clock by 4
    ADC_setPrescaler(ADCA_BASE, ADC_CLK_DIV_4_0);

    ADC_setMode(ADCA_BASE, ADC_RESOLUTION_12BIT, ADC_MODE_SINGLE_ENDED);

    // Set pulse positions to late
    ADC_setInterruptPulseMode(ADCA_BASE, ADC_PULSE_END_OF_CONV);

    // Power up the ADC module
    ADC_enableConverter(ADCA_BASE);

    // Delay for 1 ms to allow the ADC to power up
    DEVICE_DELAY_US(1000);
}

// Configure SOC for ADCA conversion
void configureADCSOC(void)
{
    // Configure SOC0 of ADCA to sample on ADC channel A0 (ADCINA0)
    ADC_setupSOC(ADCA_BASE, ADC_SOC_NUMBER0, ADC_TRIGGER_CPU1_TINT0, ADC_CH_ADCIN0, ADC_ACQPS_TICKS);

    // Set interrupt source to end of SOC0
    ADC_setInterruptSource(ADCA_BASE, ADC_INT_NUMBER1, ADC_SOC_NUMBER0);

    // Enable interrupt for SOC0
    ADC_enableInterrupt(ADCA_BASE, ADC_INT_NUMBER1);

    // Clear the interrupt flag
    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
}


// Interrupt Service Routine for ADC
__interrupt void adcA1ISR(void)
{

    // Store ADC result in buffer
    if (adcBufferIndex < ADC_BUF_LEN)
    {
        static float32_t conversionFactor = 3.0 / (4096 - 1); // 12 bits digital to 3->0 linear scale (VREFHI = 3V, SRC: LaunchPad XL datasheet)
        
        rawData[adcBufferIndex] = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER0);
        measuredVals[adcBufferIndex] =(rawData[adcBufferIndex] * conversionFactor);

        adcBufferIndex++;
    }

    // Clear ADC interrupt flag
    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);

    // Acknowledge interrupt in PIE
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
}



// Configure ePWM module
void initEPWM(uint32_t base)
{
    uint32_t tbclk = TBCLK / TBCLK_DIVIDER;      // e.g. 100 MHz / 4 = 25 MHz
    uint16_t tbprd = (tbclk / (2 * PWM_FREQ)) - 1;

    EPWM_setTimeBasePeriod(base, tbprd);
    EPWM_setPhaseShift(base, 0);
    EPWM_disablePhaseShiftLoad(base);
    EPWM_setTimeBaseCounter(base, 0);

    float duty = 0.25f;                 // 25%
    uint16_t cmpa = (uint16_t)((1.0f - duty) * (tbprd + 1.0f) + 0.5f);

    EPWM_setCounterCompareValue(base, EPWM_COUNTER_COMPARE_A, cmpa);

    EPWM_setTimeBaseCounterMode(base, EPWM_COUNTER_MODE_UP_DOWN);

    EPWM_setCounterCompareShadowLoadMode(base,
                                         EPWM_COUNTER_COMPARE_A,
                                         EPWM_COMP_LOAD_ON_CNTR_ZERO);
    //
    // Action-qualifier mapping (center-aligned PWM)
    //
    // When counter goes up and reaches CMPA → output HIGH
    // When counter goes down and reaches CMPA → output LOW
    //
    EPWM_setActionQualifierAction(base,
                                  EPWM_AQ_OUTPUT_A,
                                  EPWM_AQ_OUTPUT_HIGH,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);

    EPWM_setActionQualifierAction(base,
                                  EPWM_AQ_OUTPUT_A,
                                  EPWM_AQ_OUTPUT_LOW,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_DOWN_CMPA);
}
