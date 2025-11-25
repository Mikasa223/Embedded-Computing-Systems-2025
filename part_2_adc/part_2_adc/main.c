// Included Files
#include "driverlib.h"
#include "device.h"
#include "sysctl.h"
#include "Myboard.h"

#define rData_length 250
// conisder static next to variables ...

// ePWM period — gives 1 kHz PWM in up mode (period = TBPRD+1, i.e. counts from 0 to 2e6/1e3-1)
#define EPWM_TIMER_TBPRD (2e6/1e3 - 1)

// Clock configuration macro for setting up the PLL and system clock
#define MY_DEVICE_SETCLOCK_CFG \
    (SYSCTL_OSCSRC_XTAL |       /* Use external crystal oscillator */ \
     SYSCTL_IMULT(40) |         /* Integer multiplier = 40 (e.g., 10 MHz * 40 = 400 MHz VCO) */ \
     SYSCTL_FMULT_NONE |        /* No fractional multiplier */ \
     SYSCTL_SYSDIV(0) |         /* Divide-by-1 for SYSCLK = 200 MHz */ \
     SYSCTL_PLL_ENABLE)         /* Enable PLL */


// Globals
uint32_t sysClockFreq = 0; // Variable to store system clock frequency
uint16_t cpuTimer0IntCount;
uint16_t cpuTimer1IntCount;
uint16_t adcResultA;           // This will hold the result from ADCA (ADC0)

// why float & int? unnecessary wasted space -- use uint16 instead and modify how you view it accordingly
// float rData[rData_length];      // Raw ADC data buffer
// int   rData_int[rData_length];      // Raw ADC data buffer in 16-bit


uint16_t rawData[rData_length];
float32_t measuredVals[rData_length];


// ADC buffer index
uint16_t adcBufferIndex = 0;    // Current index for ADC data buffer

// Function Prototypes
__interrupt void cpuTimer0ISR(void);
__interrupt void adcA1ISR(void);
void initCPUTimers(void);
void configCPUTimer(uint32_t, float, float);
void configureADC(void);
void configureADCSOC(void);
void initEPWM(uint32_t base);

// Main
void main(void){
    int i = 0;
    Device_init(); // Initializes device clock and peripherals


    configureADC();     // Configure ADCA (ADC0)
    configureADCSOC();  // Configure SOC for ADCA

    Interrupt_initModule();     // Initializes PIE and clears PIE registers.
    Interrupt_initVectorTable(); // Initializes the PIE vector table

    Board_init();

    // Clock configuration — commented out, can be used to explicitly set PLL (last option in the macro) (apply the clk congif macro defined above)
    // SysCtl_setClock(MY_DEVICE_SETCLOCK_CFG);


    // Register ISRs for each CPU Timer interrupt
    //Interrupt_register(INT_TIMER0, &cpuTimer0ISR);
    //Interrupt_register(INT_ADCA1, &adcA1ISR); // Register ADC interrupt

    initCPUTimers();  // Initialize the Device Peripheral timers

    // Get the system clock frequency
    sysClockFreq = SysCtl_getClock(DEVICE_OSCSRC_FREQ);
    configCPUTimer(CPUTIMER0_BASE, sysClockFreq, 40000); // fs in hertz

    // Enable interrupts for CPU Timer and ADC
    CPUTimer_enableInterrupt(CPUTIMER0_BASE);

    // the below line allows for tmp interrupt propgation to pie then cpu, we don't want this, instead to feed directly to adc
    //Interrupt_enable(INT_TIMER0); 
    
    Interrupt_enable(INT_ADCA1);

    CPUTimer_startTimer(CPUTIMER0_BASE);

    ////////// PWM SECTION /////////////
    // // Disable time-base clock sync before configuring ePWM

    SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

    // Initialize ePWM settings
    initEPWM(EPWM1_BASE);

    // Explicitly set prescaler values: no division (1:1)
    EPWM_setClockPrescaler(EPWM1_BASE, EPWM_CLOCK_DIVIDER_1, EPWM_HSCLOCK_DIVIDER_1);

    // Re-enable time-base clock sync to start all ePWM counters
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);



     DEVICE_DELAY_US(100);  // Delay for 100 microseconds to settle the PLL


    // DEBUGGING 

    // // Read EPWM clock divider settings from the TBCTL register
    // uint16_t clkdiv = (HWREGH(EPWM1_BASE + EPWM_O_TBCTL) & EPWM_TBCTL_CLKDIV_M) >> EPWM_TBCTL_CLKDIV_S;
    // uint16_t hspclkdiv = (HWREGH(EPWM1_BASE + EPWM_O_TBCTL) & EPWM_TBCTL_HSPCLKDIV_M) >> EPWM_TBCTL_HSPCLKDIV_S;

    // // Assume SYSCLK = 200 MHz
    // uint32_t sysclk = 200000000U;

    // // Calculate TBCLK frequency: SYSCLK / CLKDIV / (2 × HSPCLKDIV if HSPCLKDIV > 0)
    // uint32_t tbclk = sysclk / (1 << clkdiv) / (hspclkdiv == 0 ? 1 : 2 * hspclkdiv);

    // // Calculate expected ePWM frequency in up-down count mode: TBCLK / (2 × TBPRD)
    // uint32_t pwm_freq = tbclk / (2 * EPWM_TIMER_TBPRD);






    // Initialize rData and Sdata arrays
    for (i = 0; i < rData_length; i++)
    {
        rawData[i] = 0;
        measuredVals[i]=0;
    }

    asm(" NOP");

    EINT;  // Enable Global interrupt INTM
    ERTM;  // Enable Global realtime interrupt DBGM

    while (1)
    {
        // Continuously check if buffer is filled for further processing
        if (adcBufferIndex >= rData_length)
        {
            asm(" NOP"); // debugging breakpoint
            // Reset buffer index after filling
            adcBufferIndex = 0;

            // Process the buffer here or trigger further processing

        }
    }
}

// Interrupt Service Routine for CPU Timer 0 ... since we've routed it directly to ADC, can i just remove it and disable TMR INT to reduce CPU overhead? 
__interrupt void cpuTimer0ISR(void)
{
    // Trigger ADC conversion
    //ADC_forceSOC(ADCA_BASE, ADC_SOC_NUMBER0);



    // Acknowledge this interrupt to receive more interrupts from group 1
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
}

// Interrupt Service Routine for ADC
__interrupt void INT_myADCA_1_ISR(void)
{

    // Store ADC result in buffer
    if (adcBufferIndex < rData_length)
    {
        static float32_t conversionFactor = (float32_t)3 / (4096 - 1);


        rawData[adcBufferIndex] = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER0);
        //rData_int[adcBufferIndex] = (int) (rData[adcBufferIndex] -40000);
        measuredVals[adcBufferIndex] =(rawData[adcBufferIndex] * conversionFactor);
        
        adcBufferIndex++;
    }

    // Toggle GPIO pin 0 to signal that ADC result has been read and processed
    //GPIO_togglePin(0);

    // Clear ADC interrupt flag
    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);

    // Acknowledge interrupt in PIE
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
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

    if (cpuTimer == CPUTIMER0_BASE) cpuTimer0IntCount = 0;
    else if (cpuTimer == CPUTIMER1_BASE) cpuTimer1IntCount = 0;
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

    cpuTimer0IntCount = 0;
}

// Configure ADC (Only ADCA is used here)
void configureADC(void)
{
    // resoltuion if N else ....

    // Set the prescaler to divide the ADC clock by 4
    ADC_setPrescaler(ADCA_BASE, ADC_CLK_DIV_4_0);

    
    // Set the resolution to 16-bit and mode to single-ended (NOT RECOMMENDED BT TI BTW)
    // you can't use 16 bit (differential) then ground one of the pins, you must adhere to the Vcm logic ... refer to perplixity covo nov 14th
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
    ADC_setupSOC(ADCA_BASE, ADC_SOC_NUMBER0, ADC_TRIGGER_CPU1_TINT0, ADC_CH_ADCIN0, 14);

    // Set interrupt source to end of SOC0
    ADC_setInterruptSource(ADCA_BASE, ADC_INT_NUMBER1, ADC_SOC_NUMBER0);

    // Enable interrupt for SOC0
    ADC_enableInterrupt(ADCA_BASE, ADC_INT_NUMBER1);

    // Clear the interrupt flag
    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
}


// Configure ePWM module
void initEPWM(uint32_t base)
{
    // Set PWM period for up-down counting
    EPWM_setTimeBasePeriod(base, EPWM_TIMER_TBPRD);

    // Disable phase shift
    EPWM_setPhaseShift(base, 0U);
    EPWM_disablePhaseShiftLoad(base);

    // Initialize counter to 0
    EPWM_setTimeBaseCounter(base, 0U);

    // Set compare values for A and B channels (DO I NEED TO CONFIG TWO CHANNELS, IS IT NOT POSSIBLE TO USE ONLY ONE AND SAVE PWR?)
    // EVEN IF SO AGAIN DON'T MISTAKEN CMPBA WITH CHANNELS BA, HERE NO POWER IS WASTED JUST UNNECESSARY CONFIGS, FIND A WAY TO DISABLE B
    EPWM_setCounterCompareValue(base, EPWM_COUNTER_COMPARE_A, (EPWM_TIMER_TBPRD+1) / 4 - 1);  // 25% duty
    EPWM_setCounterCompareValue(base, EPWM_COUNTER_COMPARE_B, (EPWM_TIMER_TBPRD+1) / 4 - 1);  // 25% duty

    // Set mode to up-down counting
    EPWM_setTimeBaseCounterMode(base, EPWM_COUNTER_MODE_UP);

    // Use shadow register, load on counter zero
    EPWM_setCounterCompareShadowLoadMode(base, EPWM_COUNTER_COMPARE_A, EPWM_COMP_LOAD_ON_CNTR_ZERO);
    EPWM_setCounterCompareShadowLoadMode(base, EPWM_COUNTER_COMPARE_B, EPWM_COMP_LOAD_ON_CNTR_ZERO);

    // === ePWM1A configuration ===
    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A,
                                  EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_ZERO);
    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A,
                                  EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);

    // === ePWM1B configuration (mirrors ePWM1A) ===
    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_B,
                                  EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_ZERO);
    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_B,
                                  EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPB);

    // Enable interrupt on time-base counter equals zero
    // EPWM_setInterruptSource(base, EPWM_INT_TBCTR_ZERO);
    // EPWM_enableInterrupt(base);
    // EPWM_setInterruptEventCount(base, 1U);
}