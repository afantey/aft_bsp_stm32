/**
 * Change Logs:
 * Date           Author          Notes
 * 2023-06-15     rgw             first version
 */

#include "sdk_rtc.h"
#include "aft_sdk.h"
#include "sdk_board.h"
#include "stm32l0xx_ll_lptim.h"

#define DBG_TAG "bsp.rtc"
#define DBG_LVL DBG_LOG
#include "sdk_log.h"

#define RTC_ERROR_NONE    0
#define RTC_ERROR_TIMEOUT 1

#if !defined(SDK_RTC_CLOCK_SELECT_LSI) && \
    !defined(SDK_RTC_CLOCK_SELECT_LSE)
#define SDK_RTC_CLOCK_SELECT_LSI
#endif

#ifdef SDK_RTC_CLOCK_SELECT_LSI
/* ck_apre=LSIFreq/(ASYNC prediv + 1) with LSIFreq=37 kHz RC */
#define RTC_ASYNCH_PREDIV          ((uint32_t)0x7F)
/* ck_spre=ck_apre/(SYNC prediv + 1) = 1 Hz */
#define RTC_SYNCH_PREDIV           ((uint32_t)0x122)
#endif

#ifdef SDK_RTC_CLOCK_SELECT_LSE
/* ck_apre=LSEFreq/(ASYNC prediv + 1) = 256Hz with LSEFreq=32768Hz */
#define RTC_ASYNCH_PREDIV          ((uint32_t)0x7F)
/* ck_spre=ck_apre/(SYNC prediv + 1) = 1 Hz */
#define RTC_SYNCH_PREDIV           ((uint32_t)0x00FF)
#endif

#define RTC_BKP_DATE_TIME_UPDTATED ((uint32_t)0x32F2)

void     Configure_RTC_Clock(void);
void     Configure_RTC(void);
void     Configure_RTC_Calendar(void);
uint32_t Enter_RTC_InitMode(void);
uint32_t Exit_RTC_InitMode(void);
uint32_t WaitForSynchro_RTC(void);

static uint32_t lsi_freq = 0;

static void MX_LPTIM1_Init(void)
{
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_LPTIM1);

  LL_LPTIM_SetClockSource(LPTIM1, LL_LPTIM_CLK_SOURCE_INTERNAL);
  LL_LPTIM_SetPrescaler(LPTIM1, LL_LPTIM_PRESCALER_DIV1);
  LL_LPTIM_SetPolarity(LPTIM1, LL_LPTIM_OUTPUT_POLARITY_REGULAR);
  LL_LPTIM_SetUpdateMode(LPTIM1, LL_LPTIM_UPDATE_MODE_IMMEDIATE);
  LL_LPTIM_SetCounterMode(LPTIM1, LL_LPTIM_COUNTER_MODE_INTERNAL);
  LL_LPTIM_TrigSw(LPTIM1);

  LL_RCC_SetLPTIMClockSource(LL_RCC_LPTIM1_CLKSOURCE_LSI);
}

static void stm32lx_lptim_calibration(void)
{
    uint32_t delta_time = 0;
    uint32_t lptim_freq = 0;

    MX_LPTIM1_Init();
    
    LL_LPTIM_EnableTimeout(LPTIM1);
    LL_LPTIM_Enable(LPTIM1);
    LL_LPTIM_ClearFlag_ARROK(LPTIM1);
    LL_LPTIM_SetAutoReload(LPTIM1, 0xFFFF); /* 定时1秒,reload = 37000 / 32 = 1156 */
    while (!LL_LPTIM_IsActiveFlag_ARROK(LPTIM1))
        ;

    LL_LPTIM_ClearFlag_CMPOK(LPTIM1);
    LL_LPTIM_SetCompare(LPTIM1, 37000);
    while (!LL_LPTIM_IsActiveFlag_CMPOK(LPTIM1))
        ;
    LL_LPTIM_StartCounter(LPTIM1, LL_LPTIM_OPERATING_MODE_ONESHOT);
 
    delta_time  = clock();
    
    while (!LL_LPTIM_IsActiveFlag_CMPM(LPTIM1))
        ;
    delta_time = clock() - delta_time;
 
    /* 计算lptim的实际频率 */
    lptim_freq = (37000) * SDK_SYSTICK_PER_SECOND / delta_time;
    lsi_freq = lptim_freq;
    //disable
    LL_LPTIM_Disable(LPTIM1);
    LOG_D("lsi_freq = %d, delta_time = %d", lsi_freq, delta_time);
}

static time_t get_rtc_timestamp(void)
{
    struct tm tm_new;

    tm_new.tm_sec  = bcd2dec(LL_RTC_TIME_GetSecond(RTC));
    tm_new.tm_min  = bcd2dec(LL_RTC_TIME_GetMinute(RTC));
    tm_new.tm_hour = bcd2dec(LL_RTC_TIME_GetHour(RTC));
    tm_new.tm_mday = bcd2dec(LL_RTC_DATE_GetDay(RTC));
    tm_new.tm_mon  = bcd2dec(LL_RTC_DATE_GetMonth(RTC)) - 1;
    tm_new.tm_year = bcd2dec(LL_RTC_DATE_GetYear(RTC)) + 100;

    return mktime(&tm_new);
}

static int32_t set_rtc_timestamp(time_t time_stamp)
{
    struct tm *p_tm;

    p_tm = localtime(&time_stamp);
    if (p_tm->tm_year < 100)
    {
        return -SDK_ERROR;
    }

    LL_RTC_TimeTypeDef RTC_TimeStruct = {0};
    LL_RTC_DateTypeDef RTC_DateStruct = {0};

    RTC_TimeStruct.TimeFormat = LL_RTC_TIME_FORMAT_AM_OR_24;
    RTC_TimeStruct.Hours = dec2bcd(p_tm->tm_hour);
    RTC_TimeStruct.Minutes = dec2bcd(p_tm->tm_min);
    RTC_TimeStruct.Seconds = dec2bcd(p_tm->tm_sec);
    LL_RTC_TIME_Init(RTC, LL_RTC_FORMAT_BCD, &RTC_TimeStruct);

    if (dec2bcd(p_tm->tm_wday) == 0)
        RTC_DateStruct.WeekDay = 7;
    else
        RTC_DateStruct.WeekDay = dec2bcd(p_tm->tm_wday);
    RTC_DateStruct.Month = dec2bcd(p_tm->tm_mon + 1);
    RTC_DateStruct.Day = dec2bcd(p_tm->tm_mday);
    RTC_DateStruct.Year = dec2bcd(p_tm->tm_year - 100);
    LL_RTC_DATE_Init(RTC, LL_RTC_FORMAT_BCD, &RTC_DateStruct);

    return SDK_OK;
}
#define RTC_TIMEOUT_VALUE          ((uint32_t)1000)  /* 1 s */
static sdk_err_t set_wakeup_autoload_value(uint32_t value)
{
#if USE_TIMEOUT
    uint32_t Timeout = RTC_TIMEOUT_VALUE; /* Variable used for Timeout management */
#endif /* USE_TIMEOUT */
    LL_RTC_DisableWriteProtection(RTC);
    LL_RTC_WAKEUP_Disable(RTC);
    while (LL_RTC_IsActiveFlag_WUTW(RTC) != 1)
    {
#if USE_TIMEOUT
        if (LL_SYSTICK_IsActiveCounterFlag())
        {
            Timeout--;
        }
        if (Timeout == 0)
        {
            return SDK_E_TIMEOUT;
        }
#endif /* USE_TIMEOUT */
    }
    if(value < 0x10000) // 0 to 18 hour
    {
        LL_RTC_WAKEUP_SetClock(RTC, LL_RTC_WAKEUPCLOCK_CKSPRE);
    }
    else if (value < 0x20000)// for 18 to 36 hour
    {
        value -= 0xFFFF;
        LL_RTC_WAKEUP_SetClock(RTC, LL_RTC_WAKEUPCLOCK_CKSPRE_WUT);
    }
    else
    {
        return SDK_E_INVALID;
    }
    LL_RTC_WAKEUP_SetAutoReload(RTC, value);
    LL_RTC_EnableIT_WUT(RTC);
    LL_RTC_WAKEUP_Enable(RTC);
    LL_RTC_EnableWriteProtection(RTC);
    LL_RTC_ClearFlag_WUT(RTC);
    
    return SDK_OK;
}

static sdk_err_t stm32_rtc_control(sdk_rtc_t *rtc, int32_t cmd, void *args)
{
    sdk_err_t result = SDK_OK;
#if defined(RT_USING_ALARM)
    struct rt_rtc_wkalarm *wkalarm;
    rtc_alarm_struct rtc_alarm;
#endif

    switch (cmd)
    {
    case SDK_CONTROL_RTC_GET_TIME:
        *(uint32_t *)args = get_rtc_timestamp();
        break;

    case SDK_CONTROL_RTC_SET_TIME:
        if (set_rtc_timestamp(*(uint32_t *)args))
        {
            result = -SDK_ERROR;
        }
        
#ifdef RT_USING_ALARM
        rt_alarm_dump();
        rt_alarm_update(dev, 1);
        rt_alarm_dump();
#endif
        break;
#if defined(RT_USING_ALARM)
    case SDK_CONTROL_RTC_GET_ALARM:
        wkalarm = (struct rt_rtc_wkalarm *)args;
        rtc_alarm_get(RTC_ALARM0, &rtc_alarm);

        wkalarm->tm_hour = bcd2dec(rtc_alarm.alarm_hour);
        wkalarm->tm_min = bcd2dec(rtc_alarm.alarm_minute);
        wkalarm->tm_sec = bcd2dec(rtc_alarm.alarm_second);
        LOG_D("RTC: get rtc_alarm time : hour: %d , min: %d , sec:  %d \n",
              wkalarm->tm_hour,
              wkalarm->tm_min,
              wkalarm->tm_sec);
        break;

    case SDK_CONTROL_RTC_SET_ALARM:
        wkalarm = (struct rt_rtc_wkalarm *)args;

        rtc_alarm_disable(RTC_ALARM0);
        rtc_alarm.alarm_mask = RTC_ALARM_DATE_MASK;
        rtc_alarm.weekday_or_date = RTC_ALARM_DATE_SELECTED;
        rtc_alarm.alarm_day = 0x31;
        rtc_alarm.am_pm = RTC_AM;

        rtc_alarm.alarm_hour = dec2bcd(wkalarm->tm_hour);
        rtc_alarm.alarm_minute = dec2bcd(wkalarm->tm_min);
        rtc_alarm.alarm_second = dec2bcd(wkalarm->tm_sec);

        rtc_alarm_config(RTC_ALARM0, &rtc_alarm);

        rtc_interrupt_enable(RTC_INT_ALARM0);
        rtc_alarm_enable(RTC_ALARM0);
        nvic_irq_enable(RTC_Alarm_IRQn, 0U);
        LOG_D("RTC: set rtc_alarm time : hour: %d , min: %d , sec:  %d \n",
              wkalarm->tm_hour,
              wkalarm->tm_min,
              wkalarm->tm_sec);
        break;
#endif
    case SDK_CONTROL_RTC_SET_WAKEUP:
    {
        uint32_t wut = *(uint32_t *)args;
        set_wakeup_autoload_value(wut);
        break;
    }
    case SDK_CONTROL_RTC_CALIBRATION:
#ifdef SDK_RTC_CLOCK_SELECT_LSI
        stm32lx_lptim_calibration();
        Configure_RTC();
#endif
        break;
    default:
        return -(SDK_E_INVALID);
    }

    return result;
}

/**
  * @brief  Configure RTC.
  * @note   Peripheral configuration is minimal configuration from reset values.
  *         Thus, some useless LL unitary functions calls below are provided as
  *         commented examples - setting is default configuration from reset.
  * @param  None
  * @retval None
  */
void Configure_RTC(void)
{
  /*##-1- Enable RTC peripheral Clocks #######################################*/
  /* Enable RTC Clock */ 
  LL_RCC_EnableRTC();

  /*##-2- Disable RTC registers write protection ##############################*/
  LL_RTC_DisableWriteProtection(RTC);

  /*##-3- Enter in initialization mode #######################################*/
  if (Enter_RTC_InitMode() != RTC_ERROR_NONE)   
  {
    /* Initialization Error */
    // LED_Blinking(LED_BLINK_ERROR);
  }

  /*##-4- Configure RTC ######################################################*/
  /* Configure RTC prescaler and RTC data registers */
  /* Set Hour Format */
  LL_RTC_SetHourFormat(RTC, LL_RTC_HOURFORMAT_24HOUR);

  if(lsi_freq > 0)
  {/* ck_apre=LSIFreq/(ASYNC prediv + 1) with LSIFreq=37 kHz RC */
/* ck_spre=ck_apre/(SYNC prediv + 1) = 1 Hz */
      uint32_t async_prediv = 0x7F;
      uint32_t sync_prediv = lsi_freq / (async_prediv + 1) - 1;
      /* Set Asynch Prediv (value according to source clock) */
      LL_RTC_SetAsynchPrescaler(RTC, async_prediv);
      /* Set Synch Prediv (value according to source clock) */
      LL_RTC_SetSynchPrescaler(RTC, sync_prediv);
  }
  else
  {
      /* Set Asynch Prediv (value according to source clock) */
      LL_RTC_SetAsynchPrescaler(RTC, RTC_ASYNCH_PREDIV);
      /* Set Synch Prediv (value according to source clock) */
      LL_RTC_SetSynchPrescaler(RTC, RTC_SYNCH_PREDIV);
  }

  /* Set OutPut */
  /* Reset value is LL_RTC_ALARMOUT_DISABLE */
  //LL_RTC_SetAlarmOutEvent(RTC, LL_RTC_ALARMOUT_DISABLE);
  /* Set OutPutPolarity */
  /* Reset value is LL_RTC_OUTPUTPOLARITY_PIN_HIGH */
  //LL_RTC_SetOutputPolarity(RTC, LL_RTC_OUTPUTPOLARITY_PIN_HIGH);
  /* Set OutPutType */
  /* Reset value is LL_RTC_ALARM_OUTPUTTYPE_OPENDRAIN */
  //LL_RTC_SetAlarmOutputType(RTC, LL_RTC_ALARM_OUTPUTTYPE_OPENDRAIN);

  /*##-5- Exit of initialization mode #######################################*/
  Exit_RTC_InitMode();
  
  /*##-6- Enable RTC registers write protection #############################*/
  LL_RTC_EnableWriteProtection(RTC);
}

/**
  * @brief  Configure the current time and date.
  * @param  None
  * @retval None
  */
void Configure_RTC_Calendar(void)
{
  /*##-1- Disable RTC registers write protection ############################*/
  LL_RTC_DisableWriteProtection(RTC);

  /*##-2- Enter in initialization mode ######################################*/
  if (Enter_RTC_InitMode() != RTC_ERROR_NONE)   
  {
    /* Initialization Error */
    // LED_Blinking(LED_BLINK_ERROR);
  }

  /*##-3- Configure the Date ################################################*/
  /* Note: __LL_RTC_CONVERT_BIN2BCD helper macro can be used if user wants to*/
  /*       provide directly the decimal value:                               */
  /*       LL_RTC_DATE_Config(RTC, LL_RTC_WEEKDAY_MONDAY,                    */
  /*                          __LL_RTC_CONVERT_BIN2BCD(31), (...))           */
  /* Set Date: Monday March 31th 2015 */
  LL_RTC_DATE_Config(RTC, LL_RTC_WEEKDAY_MONDAY, 0x24, LL_RTC_MONTH_APRIL, 0x23);
  
  /*##-4- Configure the Time ################################################*/
  LL_RTC_TIME_Config(RTC, LL_RTC_TIME_FORMAT_AM_OR_24, 0x00, 0x00, 0x00);
  
  /*##-5- Exit of initialization mode #######################################*/
  if (Exit_RTC_InitMode() != RTC_ERROR_NONE)   
  {
    /* Initialization Error */
    // LED_Blinking(LED_BLINK_ERROR);
  }
   
  /*##-6- Enable RTC registers write protection #############################*/
  LL_RTC_EnableWriteProtection(RTC);

  /*##-8- Writes a data in a RTC Backup data Register1 #######################*/
  LL_RTC_BAK_SetRegister(RTC, LL_RTC_BKP_DR1, RTC_BKP_DATE_TIME_UPDTATED);
}

/**
  * @brief  Enter in initialization mode
  * @note In this mode, the calendar counter is stopped and its value can be updated
  * @param  None
  * @retval RTC_ERROR_NONE if no error
  */
uint32_t Enter_RTC_InitMode(void)
{
  /* Set Initialization mode */
  LL_RTC_EnableInitMode(RTC);
  
#if (USE_TIMEOUT == 1)
    Timeout = RTC_TIMEOUT_VALUE;
#endif /* USE_TIMEOUT */

  /* Check if the Initialization mode is set */
  while (LL_RTC_IsActiveFlag_INIT(RTC) != 1)
  {
#if (USE_TIMEOUT == 1)
      if (LL_SYSTICK_IsActiveCounterFlag())
    {
        Timeout --;
    }
      if (Timeout == 0)
    {
      return RTC_ERROR_TIMEOUT;
    }  
#endif /* USE_TIMEOUT */
  }
  
  return RTC_ERROR_NONE;
}

/**
  * @brief  Exit Initialization mode 
  * @param  None
  * @retval RTC_ERROR_NONE if no error
  */
uint32_t Exit_RTC_InitMode(void)
{
  LL_RTC_DisableInitMode(RTC);
  
  /* Wait for synchro */
  /* Note: Needed only if Shadow registers is enabled           */
  /*       LL_RTC_IsShadowRegBypassEnabled function can be used */
  return (WaitForSynchro_RTC());
}

/**
  * @brief  Wait until the RTC Time and Date registers (RTC_TR and RTC_DR) are
  *         synchronized with RTC APB clock.
  * @param  None
  * @retval RTC_ERROR_NONE if no error (RTC_ERROR_TIMEOUT will occur if RTC is 
  *         not synchronized)
  */
uint32_t WaitForSynchro_RTC(void)
{
  /* Clear RSF flag */
  LL_RTC_ClearFlag_RS(RTC);

#if (USE_TIMEOUT == 1)
    Timeout = RTC_TIMEOUT_VALUE;
#endif /* USE_TIMEOUT */

  /* Wait the registers to be synchronised */
  while(LL_RTC_IsActiveFlag_RS(RTC) != 1)
  {
#if (USE_TIMEOUT == 1)
      if (LL_SYSTICK_IsActiveCounterFlag())
    {
        Timeout --;
    }
      if (Timeout == 0)
    {
      return RTC_ERROR_TIMEOUT;
    }  
#endif /* USE_TIMEOUT */
  }
  return RTC_ERROR_NONE;
}

#if defined(RT_USING_ALARM)
void RTC_Alarm_IRQHandler(void)
{
    if (RESET != rtc_flag_get(RTC_FLAG_ALARM0))
    {
        rtc_flag_clear(RTC_FLAG_ALARM0);
        exti_flag_clear(EXTI_17);
        
        rt_alarm_update(&g_stm32_rtc_dev.rtc_dev, 1);
    }
}
#endif

static sdk_err_t stm32_rtc_open(sdk_rtc_t *rtc)
{
    if (LL_RTC_BAK_GetRegister(RTC, LL_RTC_BKP_DR1) != RTC_BKP_DATE_TIME_UPDTATED)
    {
#ifdef SDK_RTC_CLOCK_SELECT_LSI
        stm32lx_lptim_calibration();
#endif
        Configure_RTC();
        Configure_RTC_Calendar();
        LOG_D("rtc_setup....\n\r");
    }
    else
    {
        LOG_D("no need to configure RTC....\n\r");
#if defined(RT_USING_ALARM)
        rtc_flag_clear(RTC_STAT_ALRM0F);
        exti_flag_clear(EXTI_17);
#endif
    }
#if defined(RT_USING_ALARM)
    /* RTC alarm interrupt configuration */
    exti_init(EXTI_17, EXTI_INTERRUPT, EXTI_TRIG_RISING);
    nvic_irq_enable(RTC_Alarm_IRQn, 0);
#endif

    return SDK_OK;
}

static sdk_err_t stm32_rtc_close(sdk_rtc_t *rtc)
{
    return SDK_OK;
}

__WEAK void sdk_rtc_wakeup_callback(void)
{
    LOG_D("\n");
}


// void RTC_WKUP_IRQHandler(void)
// {
//     if (rtc_flag_get(RTC_FLAG_WT) != RESET)
//     {
//         exti_interrupt_flag_clear(EXTI_20);
//         rtc_flag_clear(RTC_FLAG_WT);

//         //INT callback
//         sdk_rtc_wakeup_callback();
        
//     }
// }

sdk_rtc_t rtc = 
{
    .ops.open = stm32_rtc_open,
    .ops.close = stm32_rtc_close,
    .ops.control = stm32_rtc_control,
};
