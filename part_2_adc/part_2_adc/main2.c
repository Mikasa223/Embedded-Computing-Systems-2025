// How to calculate ADC_ACQPS_TICKS
// Uising up-down PWM
//The oscilloscope gives the right waveform
// CSS Graph for rData gives the correct sample for 25% dutyc cycle. Can double check.
// Values should be rolling but need to double check.
// Would be nice to do something about this line //rData_int[adcBufferIndex] = (int) (rData[adcBufferIndex] - 2048);

//Included
#include "driverlib.h"
#include "device.h"
#include "Myboard.h"
#include <stdio.h>
#include "sysctl.h"

// Clock configuration macro for setting up the PLL and system clock
#define MY_DEVICE_SETCLOCK_CFG \
    (SYSCTL_OSCSRC_XTAL |       /* Use external crystal oscillator */ \
     SYSCTL_IMULT(40) |         /* Integer multiplier = 40 (e.g., 10 MHz * 40 = 400 MHz VCO) */ \
     SYSCTL_FMULT_NONE |        /* No fractional multiplier */ \
     SYSCTL_SYSDIV(0) |         /* Divide-by-1 for SYSCLK = 200 MHz */ \
     SYSCTL_PLL_ENABLE)         /* Enable PLL */

//Definitions
#define EPWM_TIMER_TBPRD 6250 // ePWM period — gives 1 kHz PWM in up-down mode (period = TBPRD)
#define SAMPLING_FREQ    40000
#define ADC_ACQPS_TICKS  30          // to be completed
#define ADC_BUF_LEN 250

// Variables used
uint32_t sysClockFreq = 0; // Variable to store system clock frequency
uint16_t cpuTimer0IntCount;
uint16_t rData[ADC_BUF_LEN];      // Raw ADC data buffer
int   rData_int[ADC_BUF_LEN];      // Raw ADC data buffer in 16-bit
uint16_t adcBufferIndex = 0;    // Current index for ADC data buffer

// ADC Forward declarations
__interrupt void INT_myADCA_1_ISR(void);
void initCPUTimers(void);
void configCPUTimer(uint32_t, float, float);
void configureADC(void);
void configureADCSOC(void);

// PWM Forward declarations
void initEPWM(uint32_t base);
__interrupt void epwm1ISR(void);


void main(void)
 {
    //Initialization
    int i = 0;
    // Initialize system control: watchdog, PLL, peripheral clocks
    Device_init();
    // Initialize GPIO (peripheral pins)

    // Initialize PIE and CPU vector table
    Interrupt_initModule();
    Interrupt_initVectorTable();

    Device_initGPIO();
    GPIO_setPadConfig(DEVICE_GPIO_PIN_LED1, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(DEVICE_GPIO_PIN_LED1, GPIO_DIR_MODE_OUT);

    GPIO_setPadConfig(0, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(0, GPIO_DIR_MODE_OUT);

    // Register the interrupt service routine for ePWM1
    Interrupt_register(INT_EPWM1, &epwm1ISR);

    Board_init();

    //ADC
    //Allows for ADC to power up
    configureADC();     // Configure ADCA (ADC0)
    configureADCSOC();  // Configure SOC fors ADCA
    // Register ISRs for ADC
    Interrupt_register(INT_ADCA1, &INT_myADCA_1_ISR); // Register ADC interrupt
    Interrupt_enable(INT_ADCA1);

    //Time
    initCPUTimers();
    //Configures timers to set the ADC sampling frequenct.
    //sysClockFreq = SysCtl_getClock(DEVICE_OSCSRC_FREQ);
    sysClockFreq = SysCtl_getClock(DEVICE_OSCSRC_FREQ);
    configCPUTimer(CPUTIMER0_BASE, sysClockFreq, SAMPLING_FREQ); // fs in hertz
    CPUTimer_enableInterrupt(CPUTIMER0_BASE);

    //PWM
    // Disable time-base clock sync before configuring ePWM
    SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);
    // Initialize ePWM settings
    initEPWM(myEPWM1_BASE);
    // Re-enable time-base clock sync to start all ePWM counters
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);
    // Explicitly set prescaler values: no division (1:1)
    EPWM_setClockPrescaler(myEPWM1_BASE, EPWM_CLOCK_DIVIDER_16, EPWM_HSCLOCK_DIVIDER_1);
    SysCtl_setClock(MY_DEVICE_SETCLOCK_CFG);
    DEVICE_DELAY_US(100);  // Delay for 100 microseconds to settle the PLL
    // Read EPWM clock divider settings from the TBCTL register
    uint16_t clkdiv = (HWREGH(EPWM1_BASE + EPWM_O_TBCTL) & EPWM_TBCTL_CLKDIV_M) >> EPWM_TBCTL_CLKDIV_S;
    uint16_t hspclkdiv = (HWREGH(EPWM1_BASE + EPWM_O_TBCTL) & EPWM_TBCTL_HSPCLKDIV_M) >> EPWM_TBCTL_HSPCLKDIV_S;
    // Assume SYSCLK = 200 MHz
    uint32_t sysclk = 200000000U;
    // Calculate TBCLK frequency: SYSCLK / CLKDIV / (2 × HSPCLKDIV if HSPCLKDIV > 0)
    //1 << clkdiv 2^clkdiv
    uint32_t tbclk = sysclk / (1 << clkdiv) / (hspclkdiv == 0 ? 1 : 2 * hspclkdiv);
    // Calculate expected ePWM frequency in up-down count mode: TBCLK / (2 × TBPRD)
    uint32_t pwm_freq = tbclk / (2 * EPWM_TIMER_TBPRD);
    // Enable ePWM interrupt in the PIE
    Interrupt_enable(INT_EPWM1);

    CPUTimer_startTimer(CPUTIMER0_BASE);

    // Initialize rData and Sdata arrays
    for (i = 0; i < ADC_BUF_LEN; i++)
    {
        rData[i] = 0;
        rData_int[i] =0;
    }

    // Enable global and real-time interrupts
    EINT;  // Enable global interrupt mask
    ERTM;  // Enable real-time interrupt mask

    while (1)
    {
        // Continuously check if buffer is filled for further processing
                if (adcBufferIndex >= ADC_BUF_LEN)
                {
                    // Reset buffer index after filling
                    adcBufferIndex = 0;

                }
    }
}
// ================= ePWM ISR =================
__interrupt void epwm1ISR(void)
{
    // Clear ePWM interrupt flag
    EPWM_clearEventTriggerInterruptFlag(myEPWM1_BASE);

    // Acknowledge the PIE group so more interrupts can be taken
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP3);
}

// ================= ePWM initialization =================
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

    // Set compare values for A
    EPWM_setCounterCompareValue(base, EPWM_COUNTER_COMPARE_A, EPWM_TIMER_TBPRD*3 /4 );  // 50% duty

    // Set mode to up-down counting
    EPWM_setTimeBaseCounterMode(base, EPWM_COUNTER_MODE_UP_DOWN);

    // Use shadow register, load on counter zero
    EPWM_setCounterCompareShadowLoadMode(base, EPWM_COUNTER_COMPARE_A, EPWM_COMP_LOAD_ON_CNTR_ZERO);

    // === ePWM1A configuration ===
    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A,
                                  EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_ZERO);
    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A,
                                  EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A,
                                      EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_DOWN_CMPA);

    // Enable interrupt on time-base counter equals zero
    EPWM_setInterruptSource(base, EPWM_INT_TBCTR_ZERO);
    EPWM_enableInterrupt(base);
    EPWM_setInterruptEventCount(base, 1U);
}

// ================= ADC ISR =================

__interrupt void INT_myADCA_1_ISR(void)
{
    // Store ADC result in buffer
    if (adcBufferIndex < ADC_BUF_LEN)
    {
        rData[adcBufferIndex] = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER0);
        //rData_int[adcBufferIndex] = (int) (rData[adcBufferIndex] - 2048);
        adcBufferIndex++;

    }

    // Toggle GPIO pin 0 to signal that ADC result has been read and processed
    //GPIO_togglePin(0);

    // Clear ADC interrupt flag
    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);

    // Acknowledge interrupt in PIE
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
}

// ================= NEW: Timer0 ISR (40 kHz triggers ADC) ==============
// Configure CPU Timer
void configCPUTimer(uint32_t cpuTimer, float freq, float fs)
{
    uint32_t temp = (uint32_t)(freq / fs);

    // Initialize timer period
    //Timer couts 4999 to 0 so 50000
    CPUTimer_setPeriod(cpuTimer, temp-1);

    // Set pre-scale counter to divide by 1 (SYSCLKOUT)
    CPUTimer_setPreScaler(cpuTimer, 0);

    // Initializes timer control register. The timer is stopped, reloaded,
    // free run disabled, and interrupt enabled.
    CPUTimer_stopTimer(cpuTimer);
    CPUTimer_reloadTimerCounter(cpuTimer);
    CPUTimer_setEmulationMode(cpuTimer, CPUTIMER_EMULATIONMODE_STOPAFTERNEXTDECREMENT);
    CPUTimer_enableInterrupt(cpuTimer);

    if (cpuTimer == CPUTIMER0_BASE) cpuTimer0IntCount = 0;
}


// ================= NEW: Unified initialization of ADCA + Timer0 =================

// Configure ADC (Only ADCA is used here)
void configureADC(void)
{
    // Set the prescaler to divide the ADC clock by 4
    ADC_setPrescaler(ADCA_BASE, ADC_CLK_DIV_4_0);

    // Set the resolution to 16-bit and mode to single-ended
 //   ADC_setMode(ADCA_BASE, ADC_RESOLUTION_16BIT, ADC_MODE_SINGLE_ENDED);
    ADC_setMode(ADCA_BASE, ADC_RESOLUTION_12BIT, ADC_MODE_SINGLE_ENDED);
    // Set pulse positions to late
    ADC_setInterruptPulseMode(ADCA_BASE, ADC_PULSE_END_OF_CONV);

    // Power up the ADC module
    ADC_enableConverter(ADCA_BASE);

    // Delay for 1 ms to allow the ADC to power up
    DEVICE_DELAY_US(1000);
}


// Configure SOC for ADCA conversion which we want that is ADCINA0
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
