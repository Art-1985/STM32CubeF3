/**
  ******************************************************************************
  * @file    Examples_LL/ADC/ADC_AnalogWatchdog/Src/main.c
  * @author  MCD Application Team
  * @brief   This example describes how to use a ADC peripheral
  *          with ADC analog watchdog to monitor a channel and detect 
  *          when the corresponding conversion data is out of window thresholds.
  *          This example is based on the STM32F3xx ADC LL API;
  *          Peripheral initialization done using LL unitary services functions.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2016 STMicroelectronics</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/** @addtogroup STM32F3xx_LL_Examples
  * @{
  */

/** @addtogroup ADC_AnalogWatchdog
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

/* Definitions of ADC hardware constraints delays */
/* Note: Only ADC IP HW delays are defined in ADC LL driver driver,           */
/*       not timeout values:                                                  */
/*       Timeout values for ADC operations are dependent to device clock      */
/*       configuration (system clock versus ADC clock),                       */
/*       and therefore must be defined in user application.                   */
/*       Refer to @ref ADC_LL_EC_HW_DELAYS for description of ADC timeout     */
/*       values definition.                                                   */

  /* Timeout values for ADC operations. */
  /* (calibration, enable settling time, disable settling time, ...)          */
  /* Values defined to be higher than worst cases: low clock frequency,       */
  /* maximum prescalers.                                                      */
  /* Example of profile very low frequency : ADC clock frequency 0.14MHz      */
  /* prescaler 256, sampling time 601.5 ADC clock cycles, resolution 12 bits. */
  /*  - ADC calibration time: On STM32F3 ADC, maximum delay is 112/fADC,      */
  /*    resulting in a maximum delay of 800us                                 */
  /*    (refer to device datasheet, parameter "tCAL")                         */
  /*  - ADC enable time: maximum delay is 1us.                                */
  /*    (refer to device datasheet, parameter "tSTAB")                        */
  /*  - ADC disable time: maximum delay should be a few ADC clock cycles      */
  /*  - ADC stop conversion time: maximum delay should be a few ADC clock     */
  /*    cycles                                                                */
  /*  - ADC conversion time: with this hypothesis of clock settings, maximum  */
  /*    delay will be 1123ms.                                                 */
  /*    (refer to device reference manual, section "Timing")                  */
  /* Unit: ms                                                                 */
  #define ADC_CALIBRATION_TIMEOUT_MS       ((uint32_t)   1)
  #define ADC_ENABLE_TIMEOUT_MS            ((uint32_t)   1)
  #define ADC_DISABLE_TIMEOUT_MS           ((uint32_t)   1)
  #define ADC_STOP_CONVERSION_TIMEOUT_MS   ((uint32_t)   1)
  #define ADC_CONVERSION_TIMEOUT_MS        ((uint32_t)1150)
  
  /* Delay between ADC end of calibration and ADC enable.                     */
  /* Delay estimation in CPU cycles: Case of ADC enable done                  */
  /* immediately after ADC calibration, ADC clock setting slow                */
  /* (LL_ADC_CLOCK_ASYNC_DIV32). Use a higher delay if ratio                  */
  /* (CPU clock / ADC clock) is above 32.                                     */
  #define ADC_DELAY_CALIB_ENABLE_CPU_CYCLES  (LL_ADC_DELAY_CALIB_ENABLE_ADC_CYCLES * 32)
  

/* Definitions of environment analog values */
  /* Value of analog reference voltage (Vref+), connected to analog voltage   */
  /* supply Vdda (unit: mV).                                                  */
  #define VDDA_APPLI                       ((uint32_t)3300)

/* Definitions of data related to this example */
  /* Definition of ADCx analog watchdog window thresholds */
  /* Value of ADC analog watchdog threshold high */
  #define ADC_AWD_THRESHOLD_HIGH           (__LL_ADC_DIGITAL_SCALE(LL_ADC_RESOLUTION_12B)/2)
  /* Value of ADC analog watchdog threshold low  */
  #define ADC_AWD_THRESHOLD_LOW            ((uint32_t)   0)

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

/* Variable to report status of ADC analog watchdog 1:                        */
/*  0: ADC conversion data into AWD window                                    */
/*  1: ADC conversion data out of AWD window                                  */
__IO   uint8_t ubAnalogWatchdog1Status = 0; /* Variable set into analog watchdog 1 interruption callback */

/* Private function prototypes -----------------------------------------------*/
void     SystemClock_Config(void);
void     Configure_ADC(void);
void     Activate_ADC(void);
void     LED_Init(void);
void     LED_On(void);
void     LED_Off(void);
void     LED_Blinking(uint32_t Period);
void     UserButton_Init(void);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Main program
  * @param  None
  * @retval None
  */
int main(void)
{
  /* Configure the system clock to 64 MHz */
  SystemClock_Config();
  
  /* Initialize LED2 */
  LED_Init();
  
  /* Initialize button in EXTI mode */
  UserButton_Init();
  
  /* Configure ADC */
  /* Note: This function configures the ADC but does not enable it.           */
  /*       To enable it, use function "Activate_ADC()".                       */
  /*       This is intended to optimize power consumption:                    */
  /*       1. ADC configuration can be done once at the beginning             */
  /*          (ADC disabled, minimal power consumption)                       */
  /*       2. ADC enable (higher power consumption) can be done just before   */
  /*          ADC conversions needed.                                         */
  /*          Then, possible to perform successive "Activate_ADC()",          */
  /*          "Deactivate_ADC()", ..., without having to set again            */
  /*          ADC configuration.                                              */
  Configure_ADC();
  
  /* Activate ADC */
  /* Perform ADC activation procedure to make it ready to convert. */
  Activate_ADC();
  
  /* Start ADC group regular conversion */
  /* Note: Hardware constraint (refer to description of the functions         */
  /*       below):                                                            */
  /*       On this STM32 serie, setting of this feature is conditioned to     */
  /*       ADC state:                                                         */
  /*       ADC must be enabled without conversion on going on group regular,  */
  /*       without ADC disable command on going.                              */
  /* Note: In this example, all these checks are not necessary but are        */
  /*       implemented anyway to show the best practice usages                */
  /*       corresponding to reference manual procedure.                       */
  /*       Software can be optimized by removing some of these checks, if     */
  /*       they are not relevant considering previous settings and actions    */
  /*       in user application.                                               */
  if ((LL_ADC_IsEnabled(ADC2) == 1)               &&
      (LL_ADC_IsDisableOngoing(ADC2) == 0)        &&
      (LL_ADC_REG_IsConversionOngoing(ADC2) == 0)   )
  {
    LL_ADC_REG_StartConversion(ADC2);
  }
  else
  {
    /* Error: ADC conversion start could not be performed */
    LED_Blinking(LED_BLINK_ERROR);
  }
  
  /* Infinite loop */
  while (1)
  {
    /* Note: LED state depending on ADC analog watchdog 1 status              */
    /*       and status variable "ubAnalogWatchdog1Status"                    */
    /*       are set into ADC analog watchdog 1 IRQ handler,                  */
    /*       refer to function "AdcAnalogWatchdog1_Callback()".               */
    /*       After analog watchdog interruption, press on push button         */
    /*       to rearm ADC analog watchdog to be ready for another trig,       */
    /*       refer to function "UserButton_Callback()".                       */
    
  }
}

/**
  * @brief  Configure ADC (ADC instance: ADC2) and GPIO used by ADC channels.
  * @note   In case re-use of this function outside of this example:
  *         This function includes checks of ADC hardware constraints before
  *         executing some configuration functions.
  *         - In this example, all these checks are not necessary but are
  *           implemented anyway to show the best practice usages
  *           corresponding to reference manual procedure.
  *           (On some STM32 series, setting of ADC features are not
  *           conditioned to ADC state. However, in order to be compliant with
  *           other STM32 series and to show the best practice usages,
  *           ADC state is checked anyway with same constraints).
  *           Software can be optimized by removing some of these checks,
  *           if they are not relevant considering previous settings and actions
  *           in user application.
  *         - If ADC is not in the appropriate state to modify some parameters,
  *           the setting of these parameters is bypassed without error
  *           reporting:
  *           it can be the expected behavior in case of recall of this 
  *           function to update only a few parameters (which update fullfills
  *           the ADC state).
  *           Otherwise, it is up to the user to set the appropriate error 
  *           reporting in user application.
  * @note   Peripheral configuration is minimal configuration from reset values.
  *         Thus, some useless LL unitary functions calls below are provided as
  *         commented examples - setting is default configuration from reset.
  * @param  None
  * @retval None
  */
void Configure_ADC(void)
{
  /*## Configuration of GPIO used by ADC channels ############################*/
  
  /* Note: On this STM32 device, ADC2 channel 1 is mapped on GPIO pin PA.04 */ 
  
  /* Enable GPIO Clock */
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
  
  /* Configure GPIO in analog mode to be used as ADC input */
  LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_4, LL_GPIO_MODE_ANALOG);
  
  /*## Configuration of NVIC #################################################*/
  /* Configure NVIC to enable ADC2 interruptions */
  NVIC_SetPriority(ADC1_2_IRQn, 0);
  NVIC_EnableIRQ(ADC1_2_IRQn);
  
  /*## Configuration of ADC ##################################################*/
  
  /*## Configuration of ADC hierarchical scope: common to several ADC ########*/
  
  /* Enable ADC clock (core clock) */
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_ADC12);
  
  /* Note: Hardware constraint (refer to description of the functions         */
  /*       below):                                                            */
  /*       On this STM32 serie, setting of these features is conditioned to   */
  /*       ADC state:                                                         */
  /*       All ADC instances of the ADC common group must be disabled.        */
  /* Note: In this example, all these checks are not necessary but are        */
  /*       implemented anyway to show the best practice usages                */
  /*       corresponding to reference manual procedure.                       */
  /*       Software can be optimized by removing some of these checks, if     */
  /*       they are not relevant considering previous settings and actions    */
  /*       in user application.                                               */
  if(__LL_ADC_IS_ENABLED_ALL_COMMON_INSTANCE() == 0)
  {
    /* Note: Call of the functions below are commented because they are       */
    /*       useless in this example:                                         */
    /*       setting corresponding to default configuration from reset state. */
    
    /* Set ADC clock (conversion clock) common to several ADC instances */
    LL_ADC_SetCommonClock(__LL_ADC_COMMON_INSTANCE(ADC2), LL_ADC_CLOCK_SYNC_PCLK_DIV2);
    
    /* Set ADC measurement path to internal channels */
    // LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC2), LL_ADC_PATH_INTERNAL_NONE);
    
    
  /*## Configuration of ADC hierarchical scope: multimode ####################*/
  
    /* Set ADC multimode configuration */
    // LL_ADC_SetMultimode(__LL_ADC_COMMON_INSTANCE(ADC2), LL_ADC_MULTI_INDEPENDENT);
    
    /* Set ADC multimode DMA transfer */
    // LL_ADC_SetMultiDMATransfer(__LL_ADC_COMMON_INSTANCE(ADC2), LL_ADC_MULTI_REG_DMA_EACH_ADC);
    
    /* Set ADC multimode: delay between 2 sampling phases */
    // LL_ADC_SetMultiTwoSamplingDelay(__LL_ADC_COMMON_INSTANCE(ADC2), LL_ADC_MULTI_TWOSMP_DELAY_1CYCLE);
    
  }
  
  
  /*## Configuration of ADC hierarchical scope: ADC instance #################*/
  
  /* Note: Hardware constraint (refer to description of the functions         */
  /*       below):                                                            */
  /*       On this STM32 serie, setting of these features is conditioned to   */
  /*       ADC state:                                                         */
  /*       ADC must be disabled.                                              */
  if (LL_ADC_IsEnabled(ADC2) == 0)
  {
    /* Note: Call of the functions below are commented because they are       */
    /*       useless in this example:                                         */
    /*       setting corresponding to default configuration from reset state. */
    
    /* Set ADC data resolution */
    // LL_ADC_SetResolution(ADC2, LL_ADC_RESOLUTION_12B);
    
    /* Set ADC conversion data alignment */
    // LL_ADC_SetResolution(ADC2, LL_ADC_DATA_ALIGN_RIGHT);
    
    /* Set ADC low power mode */
    // LL_ADC_SetLowPowerMode(ADC2, LL_ADC_LP_MODE_NONE);
    
    /* Set ADC selected offset number: channel and offset level */
    // LL_ADC_SetOffset(ADC2, LL_ADC_OFFSET_1, LL_ADC_CHANNEL_1, 0x000);
    
  }
  
  
  /*## Configuration of ADC hierarchical scope: ADC group regular ############*/
  
  /* Note: Hardware constraint (refer to description of the functions         */
  /*       below):                                                            */
  /*       On this STM32 serie, setting of these features is conditioned to   */
  /*       ADC state:                                                         */
  /*       ADC must be disabled or enabled without conversion on going        */
  /*       on group regular.                                                  */
  if ((LL_ADC_IsEnabled(ADC2) == 0)               ||
      (LL_ADC_REG_IsConversionOngoing(ADC2) == 0)   )
  {
    /* Set ADC group regular trigger source */
    LL_ADC_REG_SetTriggerSource(ADC2, LL_ADC_REG_TRIG_SOFTWARE);
    
    /* Set ADC group regular trigger polarity */
    // LL_ADC_REG_SetTriggerEdge(ADC2, LL_ADC_REG_TRIG_EXT_RISING);
    
    /* Set ADC group regular continuous mode */
    LL_ADC_REG_SetContinuousMode(ADC2, LL_ADC_REG_CONV_CONTINUOUS);
    
    /* Set ADC group regular conversion data transfer */
    // LL_ADC_REG_SetDMATransfer(ADC2, LL_ADC_REG_DMA_TRANSFER_NONE);
    
    /* Set ADC group regular overrun behavior */
    LL_ADC_REG_SetOverrun(ADC2, LL_ADC_REG_OVR_DATA_OVERWRITTEN);
    
    /* Set ADC group regular sequencer */
    /* Note: On this STM32 serie, ADC group regular sequencer is              */
    /*       fully configurable: sequencer length and each rank               */
    /*       affectation to a channel are configurable.                       */
    /*       Refer to description of function                                 */
    /*       "LL_ADC_REG_SetSequencerLength()".                               */
    
    /* Set ADC group regular sequencer length and scan direction */
    LL_ADC_REG_SetSequencerLength(ADC2, LL_ADC_REG_SEQ_SCAN_DISABLE);
    
    /* Set ADC group regular sequencer discontinuous mode */
    // LL_ADC_REG_SetSequencerDiscont(ADC2, LL_ADC_REG_SEQ_DISCONT_DISABLE);
    
    /* Set ADC group regular sequence: channel on the selected sequence rank. */
    LL_ADC_REG_SetSequencerRanks(ADC2, LL_ADC_REG_RANK_1, LL_ADC_CHANNEL_1);
  }
  
  
  /*## Configuration of ADC hierarchical scope: ADC group injected ###########*/
  
  /* Note: Hardware constraint (refer to description of the functions         */
  /*       below):                                                            */
  /*       On this STM32 serie, setting of these features is conditioned to   */
  /*       ADC state:                                                         */
  /*       ADC must be disabled or enabled without conversion on going        */
  /*       on group injected.                                                 */
  if ((LL_ADC_IsEnabled(ADC2) == 0)               ||
      (LL_ADC_INJ_IsConversionOngoing(ADC2) == 0)   )
  {
    /* Note: Call of the functions below are commented because they are       */
    /*       useless in this example:                                         */
    /*       setting corresponding to default configuration from reset state. */
    
    /* Set ADC group injected trigger source */
    // LL_ADC_INJ_SetTriggerSource(ADC2, LL_ADC_INJ_TRIG_SOFTWARE);
    
    /* Set ADC group injected trigger polarity */
    // LL_ADC_INJ_SetTriggerEdge(ADC2, LL_ADC_INJ_TRIG_EXT_RISING);
    
    /* Set ADC group injected conversion trigger  */
    // LL_ADC_INJ_SetTrigAuto(ADC2, LL_ADC_INJ_TRIG_INDEPENDENT);
    
    /* Set ADC group injected contexts queue mode */
    /* Note: If ADC group injected contexts queue are enabled, configure      */
    /*       contexts using function "LL_ADC_INJ_ConfigQueueContext()".       */
    // LL_ADC_INJ_SetQueueMode(ADC2, LL_ADC_INJ_QUEUE_DISABLE);
    
    /* Set ADC group injected sequencer */
    /* Note: On this STM32 serie, ADC group injected sequencer is             */
    /*       fully configurable: sequencer length and each rank               */
    /*       affectation to a channel are configurable.                       */
    /*       Refer to description of function                                 */
    /*       "LL_ADC_INJ_SetSequencerLength()".                               */
    
    /* Set ADC group injected sequencer length and scan direction */
    // LL_ADC_INJ_SetSequencerLength(ADC2, LL_ADC_INJ_SEQ_SCAN_DISABLE);
    
    /* Set ADC group injected sequencer discontinuous mode */
    // LL_ADC_INJ_SetSequencerDiscont(ADC2, LL_ADC_INJ_SEQ_DISCONT_DISABLE);
    
    /* Set ADC group injected sequence: channel on the selected sequence rank. */
    // LL_ADC_INJ_SetSequencerRanks(ADC2, LL_ADC_INJ_RANK_1, LL_ADC_CHANNEL_1);
  }
  
  
  /*## Configuration of ADC hierarchical scope: channels #####################*/
  
  /* Note: Hardware constraint (refer to description of the functions         */
  /*       below):                                                            */
  /*       On this STM32 serie, setting of these features is conditioned to   */
  /*       ADC state:                                                         */
  /*       ADC must be disabled or enabled without conversion on going        */
  /*       on either groups regular or injected.                              */
  if ((LL_ADC_IsEnabled(ADC2) == 0)                    ||
      ((LL_ADC_REG_IsConversionOngoing(ADC2) == 0) &&
       (LL_ADC_INJ_IsConversionOngoing(ADC2) == 0)   )   )
  {
    /* Set ADC channels sampling time */
    /* Note: Considering interruption occurring after each ADC conversion     */
    /*       when ADC conversion is out of the analog watchdog window         */
    /*       selected (IT from ADC analog watchdog),                          */
    /*       select sampling time and ADC clock with sufficient               */
    /*       duration to not create an overhead situation in IRQHandler.      */
    LL_ADC_SetChannelSamplingTime(ADC2, LL_ADC_CHANNEL_1, LL_ADC_SAMPLINGTIME_181CYCLES_5);
    
    /* Set mode single-ended or differential input of the selected            */
    /* ADC channel.                                                           */
    // LL_ADC_SetChannelSingleDiff(ADC2, LL_ADC_CHANNEL_1, LL_ADC_SINGLE_ENDED);
  }
  
  
  /*## Configuration of ADC transversal scope: analog watchdog ###############*/
  
  /* Set ADC analog watchdog channels to be monitored */
  LL_ADC_SetAnalogWDMonitChannels(ADC2, LL_ADC_AWD1, LL_ADC_AWD_ALL_CHANNELS_REG);
  
  /* Set ADC analog watchdog thresholds */
  LL_ADC_ConfigAnalogWDThresholds(ADC2, LL_ADC_AWD1, ADC_AWD_THRESHOLD_HIGH, ADC_AWD_THRESHOLD_LOW);
  
  
  /*## Configuration of ADC transversal scope: oversampling ##################*/
  
  /* Note: Feature not available on this STM32 serie */
  
  
  /*## Configuration of ADC interruptions ####################################*/
  /* Enable ADC analog watchdog 1 interruption */
  LL_ADC_EnableIT_AWD1(ADC2);
  
}

/**
  * @brief  Perform ADC activation procedure to make it ready to convert
  *         (ADC instance: ADC2).
  * @note   Operations:
  *         - ADC instance
  *           - Enable internal voltage regulator
  *           - Run ADC self calibration
  *           - Enable ADC
  *         - ADC group regular
  *           none: ADC conversion start-stop to be performed
  *                 after this function
  *         - ADC group injected
  *           none: ADC conversion start-stop to be performed
  *                 after this function
  * @param  None
  * @retval None
  */
void Activate_ADC(void)
{
  __IO uint32_t wait_loop_index = 0;
  #if (USE_TIMEOUT == 1)
  uint32_t Timeout = 0; /* Variable used for timeout management */
  #endif /* USE_TIMEOUT */
  
  /*## Operation on ADC hierarchical scope: ADC instance #####################*/
  
  /* Note: Hardware constraint (refer to description of the functions         */
  /*       below):                                                            */
  /*       On this STM32 serie, setting of these features is conditioned to   */
  /*       ADC state:                                                         */
  /*       ADC must be disabled.                                              */
  /* Note: In this example, all these checks are not necessary but are        */
  /*       implemented anyway to show the best practice usages                */
  /*       corresponding to reference manual procedure.                       */
  /*       Software can be optimized by removing some of these checks, if     */
  /*       they are not relevant considering previous settings and actions    */
  /*       in user application.                                               */
  if (LL_ADC_IsEnabled(ADC2) == 0)
  {
    /* Enable ADC internal voltage regulator */
    LL_ADC_EnableInternalRegulator(ADC2);
    
    /* Delay for ADC internal voltage regulator stabilization.                */
    /* Compute number of CPU cycles to wait for, from delay in us.            */
    /* Note: Variable divided by 2 to compensate partially                    */
    /*       CPU processing cycles (depends on compilation optimization).     */
    /* Note: If system core clock frequency is below 200kHz, wait time        */
    /*       is only a few CPU processing cycles.                             */
    wait_loop_index = ((LL_ADC_DELAY_INTERNAL_REGUL_STAB_US * (SystemCoreClock / (100000 * 2))) / 10);
    while(wait_loop_index != 0)
    {
      wait_loop_index--;
    }
    
    /* Run ADC self calibration */
    LL_ADC_StartCalibration(ADC2, LL_ADC_SINGLE_ENDED);
    
    /* Poll for ADC effectively calibrated */
    #if (USE_TIMEOUT == 1)
    Timeout = ADC_CALIBRATION_TIMEOUT_MS;
    #endif /* USE_TIMEOUT */
    
    while (LL_ADC_IsCalibrationOnGoing(ADC2) != 0)
    {
    #if (USE_TIMEOUT == 1)
      /* Check Systick counter flag to decrement the time-out value */
      if (LL_SYSTICK_IsActiveCounterFlag())
      {
        if(Timeout-- == 0)
        {
        /* Time-out occurred. Set LED to blinking mode */
        LED_Blinking(LED_BLINK_ERROR);
        }
      }
    #endif /* USE_TIMEOUT */
    }
    
    /* Delay between ADC end of calibration and ADC enable.                   */
    /* Note: Variable divided by 2 to compensate partially                    */
    /*       CPU processing cycles (depends on compilation optimization).     */
    wait_loop_index = (ADC_DELAY_CALIB_ENABLE_CPU_CYCLES >> 1);
    while(wait_loop_index != 0)
    {
      wait_loop_index--;
    }
    
    /* Enable ADC */
    LL_ADC_Enable(ADC2);
    
    /* Poll for ADC ready to convert */
    #if (USE_TIMEOUT == 1)
    Timeout = ADC_ENABLE_TIMEOUT_MS;
    #endif /* USE_TIMEOUT */
    
    while (LL_ADC_IsActiveFlag_ADRDY(ADC2) == 0)
    {
    #if (USE_TIMEOUT == 1)
      /* Check Systick counter flag to decrement the time-out value */
      if (LL_SYSTICK_IsActiveCounterFlag())
      {
        if(Timeout-- == 0)
        {
        /* Time-out occurred. Set LED to blinking mode */
        LED_Blinking(LED_BLINK_ERROR);
        }
      }
    #endif /* USE_TIMEOUT */
    }
    
    /* Note: ADC flag ADRDY is not cleared here to be able to check ADC       */
    /*       status afterwards.                                               */
    /*       This flag should be cleared at ADC Deactivation, before a new    */
    /*       ADC activation, using function "LL_ADC_ClearFlag_ADRDY()".       */
  }
  
  /*## Operation on ADC hierarchical scope: ADC group regular ################*/
  /* Note: No operation on ADC group regular performed here.                  */
  /*       ADC group regular conversions to be performed after this function  */
  /*       using function:                                                    */
  /*       "LL_ADC_REG_StartConversion();"                                    */
  
  /*## Operation on ADC hierarchical scope: ADC group injected ###############*/
  /* Note: No operation on ADC group injected performed here.                 */
  /*       ADC group injected conversions to be performed after this function */
  /*       using function:                                                    */
  /*       "LL_ADC_INJ_StartConversion();"                                    */
  
}

/**
  * @brief  Initialize LED2.
  * @param  None
  * @retval None
  */
void LED_Init(void)
{
  /* Enable the LED2 Clock */
  LED2_GPIO_CLK_ENABLE();

  /* Configure IO in output push-pull mode to drive external LED2 */
  LL_GPIO_SetPinMode(LED2_GPIO_PORT, LED2_PIN, LL_GPIO_MODE_OUTPUT);
  /* Reset value is LL_GPIO_OUTPUT_PUSHPULL */
  //LL_GPIO_SetPinOutputType(LED2_GPIO_PORT, LED2_PIN, LL_GPIO_OUTPUT_PUSHPULL);
  /* Reset value is LL_GPIO_SPEED_FREQ_LOW */
  //LL_GPIO_SetPinSpeed(LED2_GPIO_PORT, LED2_PIN, LL_GPIO_SPEED_FREQ_LOW);
  /* Reset value is LL_GPIO_PULL_NO */
  //LL_GPIO_SetPinPull(LED2_GPIO_PORT, LED2_PIN, LL_GPIO_PULL_NO);
}

/**
  * @brief  Turn-on LED2.
  * @param  None
  * @retval None
  */
void LED_On(void)
{
  /* Turn LED2 on */
  LL_GPIO_SetOutputPin(LED2_GPIO_PORT, LED2_PIN);
}

/**
  * @brief  Turn-off LED2.
  * @param  None
  * @retval None
  */
void LED_Off(void)
{
  /* Turn LED2 off */
  LL_GPIO_ResetOutputPin(LED2_GPIO_PORT, LED2_PIN);
}

/**
  * @brief  Set LED2 to Blinking mode for an infinite loop (toggle period based on value provided as input parameter).
  * @param  Period : Period of time (in ms) between each toggling of LED
  *   This parameter can be user defined values. Pre-defined values used in that example are :
  *     @arg LED_BLINK_FAST : Fast Blinking
  *     @arg LED_BLINK_SLOW : Slow Blinking
  *     @arg LED_BLINK_ERROR : Error specific Blinking
  * @retval None
  */
void LED_Blinking(uint32_t Period)
{
  /* Turn LED2 on */
  LL_GPIO_SetOutputPin(LED2_GPIO_PORT, LED2_PIN);
  
  /* Toggle IO in an infinite loop */
  while (1)
  {
    LL_GPIO_TogglePin(LED2_GPIO_PORT, LED2_PIN);  
    LL_mDelay(Period);
  }
}

/**
  * @brief  Configures User push-button in EXTI Line Mode.
  * @param  None
  * @retval None
  */
void UserButton_Init(void)
{
  /* Enable the BUTTON Clock */
  USER_BUTTON_GPIO_CLK_ENABLE();
  
  /* Configure GPIO for BUTTON */
  LL_GPIO_SetPinMode(USER_BUTTON_GPIO_PORT, USER_BUTTON_PIN, LL_GPIO_MODE_INPUT);
  LL_GPIO_SetPinPull(USER_BUTTON_GPIO_PORT, USER_BUTTON_PIN, LL_GPIO_PULL_NO);
  
  /* if(Button_Mode == BUTTON_MODE_EXTI) */
  {
    /* Connect External Line to the GPIO */
    USER_BUTTON_SYSCFG_SET_EXTI();
    
    /* Enable a rising trigger EXTI line 13 Interrupt */
    USER_BUTTON_EXTI_LINE_ENABLE();
    USER_BUTTON_EXTI_FALLING_TRIG_ENABLE();
    
    /* Configure NVIC for USER_BUTTON_EXTI_IRQn */
    NVIC_EnableIRQ(USER_BUTTON_EXTI_IRQn); 
    NVIC_SetPriority(USER_BUTTON_EXTI_IRQn,0x03);  
  }
}

/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow : 
  *            System Clock source            = PLL (HSI)
  *            SYSCLK(Hz)                     = 64000000
  *            HCLK(Hz)                       = 64000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 2
  *            APB2 Prescaler                 = 1
  *            PLLMUL                         = 16
  *            Flash Latency(WS)              = 2
  * @param  None
  * @retval None
  */
void SystemClock_Config(void)
{
  /* Set FLASH latency */ 
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_2);

  /* Enable HSI if not already activated*/
  if (LL_RCC_HSI_IsReady() == 0)
  {
    /* Enable HSI and wait for activation*/
    LL_RCC_HSI_Enable(); 
    while(LL_RCC_HSI_IsReady() != 1)
    {
    };
  }

  /* Main PLL configuration and activation */
  LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI_DIV_2, LL_RCC_PLL_MUL_16);
  
  LL_RCC_PLL_Enable();
  while(LL_RCC_PLL_IsReady() != 1) 
  {
  };
  
  /* Sysclk activation on the main PLL */
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL)
  {
  };
  
  /* Set APB1 & APB2 prescaler*/
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_2);
  LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);
  
  /* Set systick to 1ms in using frequency set to 64MHz */
  /* This frequency can be calculated through LL RCC macro */
  /* ex: __LL_RCC_CALC_PLLCLK_FREQ ((HSI_VALUE / 2), LL_RCC_PLL_MUL_16) */
  LL_Init1msTick(64000000);
  
  /* Update CMSIS variable (which can be updated also through SystemCoreClockUpdate function) */
  LL_SetSystemCoreClock(64000000);
}
/******************************************************************************/
/*   USER IRQ HANDLER TREATMENT                                               */
/******************************************************************************/

/**
  * @brief  Function to manage IRQ Handler
  * @param  None
  * @retval None
  */
void UserButton_Callback(void)
{
  /* Rearm ADC analog watchdog to be ready for another trig */

  /* Turn LED2 off */
  LED_Off();
  
  /* Reset status variable of ADC analog watchdog 1 */
  ubAnalogWatchdog1Status = 0;
  
  /* Clear flag ADC analog watchdog 1 */
  LL_ADC_ClearFlag_AWD1(ADC2);
  
  /* Enable ADC analog watchdog 1 interruption */
  LL_ADC_EnableIT_AWD1(ADC2);
}

/**
  * @brief  ADC analog watchdog 1 interruption callback
  * @note   This function is executed when the ADC conversion data is 
  *         out of analog watchdog 1 window thresholds.
  * @retval None
  */
void AdcAnalogWatchdog1_Callback()
{
  /* Disable ADC analog watchdog 1 interruption */
  LL_ADC_DisableIT_AWD1(ADC2);
  
  /* Update status variable of ADC analog watchdog 1 */
  ubAnalogWatchdog1Status = 1;
  
  /* Set LED depending on ADC analog watchdog status */
  /* - Turn-on if voltage is out of AWD window */
  LED_On();
}

#ifdef  USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(char *file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/