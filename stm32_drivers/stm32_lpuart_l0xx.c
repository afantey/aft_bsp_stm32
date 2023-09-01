/**
 * Change Logs:
 * Date           Author          Notes
 * 2023-06-15     rgw             first version
 */

#include "sdk_board.h"
#include "sdk_uart.h"

#define DBG_TAG "bsp.lpuart"
#define DBG_LVL DBG_LOG
#include "sdk_log.h"

extern sdk_uart_t lpuart;

__WEAK int stm32_lpuart_msp_init(sdk_uart_t *uart)
{
    return -SDK_ERROR;
}

__WEAK int stm32_lpuart_msp_deinit(sdk_uart_t *uart)
{
    return -SDK_ERROR;
}

static int32_t stm32_lpuart_open(sdk_uart_t *uart, int32_t baudrate, int32_t data_bit, char parity, int32_t stop_bit)
{
    LL_LPUART_InitTypeDef LPUART_InitStruct = {0};

    // msp init
    if (stm32_lpuart_msp_init(uart) != SDK_OK)
    {
        return -SDK_ERROR;
    }

    if(uart->instance == LPUART1)
    {
        /* Peripheral clock enable */
        LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_LPUART1);
    }
    else
    {
        return -SDK_ERROR;
    }

    LPUART_InitStruct.BaudRate = baudrate;

    switch (data_bit)
    {
    case 9:
        LPUART_InitStruct.DataWidth = LL_LPUART_DATAWIDTH_9B;
        break;
    case 8:
    default:
        LPUART_InitStruct.DataWidth = LL_LPUART_DATAWIDTH_8B;
        break;
    }

    switch (parity)
    {
    case 'e':
    case 'E':
        LPUART_InitStruct.Parity = LL_LPUART_PARITY_EVEN;
        break;
    case 'o':
    case 'O':
        LPUART_InitStruct.Parity = LL_LPUART_PARITY_ODD;
        break;
    case 'n':
    case 'N':
    default:
        LPUART_InitStruct.Parity = LL_LPUART_PARITY_NONE;
        break;
    }

    switch (stop_bit)
    {
    case 2:
        LPUART_InitStruct.StopBits = LL_LPUART_STOPBITS_2;
        break;
    case 1:
    default:
        LPUART_InitStruct.StopBits = LL_LPUART_STOPBITS_1;
        break;
    }

    LPUART_InitStruct.TransferDirection = LL_LPUART_DIRECTION_TX_RX;
    LPUART_InitStruct.HardwareFlowControl = LL_LPUART_HWCONTROL_NONE;
    LL_LPUART_Init(uart->instance, &LPUART_InitStruct);


    LL_LPUART_Enable(uart->instance);

    return SDK_OK;
}

static int32_t stm32_lpuart_close(sdk_uart_t *uart)
{
    LL_LPUART_DeInit(uart->instance);
    LL_LPUART_Disable(uart->instance);

    // msp deinit
    if (stm32_lpuart_msp_deinit(uart) != SDK_OK)
    {
        return -SDK_ERROR;
    }
    return SDK_OK;
}

static int32_t stm32_lpuart_putc(sdk_uart_t *uart, int32_t ch)
{
    while(0 == LL_LPUART_IsActiveFlag_TXE(uart->instance));
    LL_LPUART_TransmitData8(uart->instance, (uint8_t) ch);
    return ch;
}

static int32_t stm32_lpuart_getc(sdk_uart_t *uart)
{
    int ch;

    ch = -1;
    if (LL_LPUART_IsActiveFlag_RXNE(uart->instance) != 0)
        ch = LL_LPUART_ReceiveData8(uart->instance);
    return ch;
}

static int32_t stm32_lpuart_control(sdk_uart_t *uart, int32_t cmd, void *args)
{
    switch (cmd)
    {
    case SDK_CONTROL_UART_DISABLE_INT:
        LL_LPUART_DisableIT_RXNE(uart->instance);
        LL_LPUART_DisableIT_ERROR(uart->instance);
        NVIC_DisableIRQ(uart->irq);
        break;
    case SDK_CONTROL_UART_ENABLE_INT:
        NVIC_SetPriority(uart->irq, uart->irq_prio);
        NVIC_EnableIRQ(uart->irq);
        LL_LPUART_EnableIT_RXNE(uart->instance);
        LL_LPUART_EnableIT_ERROR(uart->instance);
        break;
    case SDK_CONTROL_UART_INT_IDLE_ENABLE:
        /*wait IDLEF set and clear it*/
        while (0 == LL_LPUART_IsActiveFlag_IDLE(uart->instance))
        {
        }
        LL_LPUART_ClearFlag_IDLE(uart->instance);
        LL_LPUART_EnableIT_IDLE(uart->instance);
        break;
    case SDK_CONTROL_UART_INT_IDLE_DISABLE:
        LL_LPUART_DisableIT_IDLE(uart->instance);
        break;
    case SDK_CONTROL_UART_ENABLE_RX:
        LL_LPUART_EnableDirectionRx(uart->instance);
        break;
    case SDK_CONTROL_UART_DISABLE_RX:
        LL_LPUART_DisableDirectionRx(uart->instance);
        break;
    }

    return SDK_OK;
}

void LPUART1_IRQHandler(void)
{
    if(LL_LPUART_IsActiveFlag_RXNE(LPUART1) && LL_LPUART_IsEnabledIT_RXNE(LPUART1))
    {
        sdk_uart_rx_isr(&lpuart);
    }

    if(LL_LPUART_IsActiveFlag_IDLE(LPUART1) && LL_LPUART_IsEnabledIT_IDLE(LPUART1))
    {
        LL_LPUART_ClearFlag_IDLE(LPUART1);
        if(lpuart.rx_idle_callback != NULL)
        {
            lpuart.rx_idle_callback();
        }
    }
    if(LL_LPUART_IsActiveFlag_ORE(LPUART1))
    {
        LL_LPUART_ClearFlag_ORE(LPUART1);
    }
    if(LL_LPUART_IsActiveFlag_FE(LPUART1))
    {
        LL_LPUART_ClearFlag_FE(LPUART1);
    }
        if(LL_LPUART_IsActiveFlag_NE(LPUART1))
    {
        LL_LPUART_ClearFlag_NE(LPUART1);
    }
}

__WEAK void lpuart_wakeup_callback(void)
{
    LOG_D("\n");
}

sdk_uart_t lpuart = 
{
    .instance = LPUART1,
    .irq = LPUART1_IRQn,
    .irq_prio = 1,
    .ops.open = stm32_lpuart_open,
    .ops.close = stm32_lpuart_close,
    .ops.putc = stm32_lpuart_putc,
    .ops.getc = stm32_lpuart_getc,
    .ops.control = stm32_lpuart_control,
    .rx_callback = NULL,
    .rx_idle_callback = NULL,
    .rx_rto_callback = NULL,
};
