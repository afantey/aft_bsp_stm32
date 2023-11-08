/**
 * Change Logs:
 * Date           Author          Notes
 * 2023-06-16     rgw             first version
 */

#include "sdk_board.h"
#include "SEGGER_RTT.h"
#include "unibus_board.h"

void sdk_hw_console_output(const char *str)
{
    SEGGER_RTT_WriteString(0, str);
    if (g_WellRegMap[WELL_Ctrl_DebugInfoON] == 1 && bl_dev.status.con == 1)
    {
        sdk_uart_write(&uart_bl, (uint8_t *)str, strlen(str));
    }
}

void sdk_hw_console_putc(const int ch)
{
    SEGGER_RTT_PutChar(0, ch);
}

void sdk_hw_us_delay(uint32_t us)
{
    uint32_t ticks;
    uint32_t told, tnow, tcnt = 0;
    uint32_t reload = SysTick->LOAD;

    ticks = us * reload / (1000000 / SDK_SYSTICK_PER_SECOND);
    told = SysTick->VAL;
    while (1)
    {
        tnow = SysTick->VAL;
        if (tnow != told)
        {
            if (tnow < told)
            {
                tcnt += told - tnow;
            }
            else
            {
                tcnt += reload - tnow + told;
            }
            told = tnow;
            if (tcnt >= ticks)
            {
                break;
            }
        }
    }
}

void sdk_hw_interrupt_enable(void)
{
    __enable_irq();
}

void sdk_hw_interrupt_disable(void)
{
    __disable_irq();
}

extern volatile uint32_t systicks;
uint32_t sdk_hw_get_systick(void)
{
    return systicks;
}

void sdk_hw_system_reset(void)
{
    buzzer_error(APP_ERRNO_SYSTEM_RESET);
    NVIC_SystemReset();
}
