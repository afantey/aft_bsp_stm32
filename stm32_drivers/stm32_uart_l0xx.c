/**
 * Change Logs:
 * Date           Author          Notes
 * 2023-06-16     rgw             first version
 */

#include "sdk_board.h"
#include "sdk_uart.h"

extern sdk_uart_t uart1;
extern sdk_uart_t uart2;
extern sdk_uart_t uart4;
extern sdk_uart_t uart5;

__WEAK int stm32_uart_msp_init(sdk_uart_t *uart)
{
    return -SDK_ERROR;
}

__WEAK int stm32_uart_msp_deinit(sdk_uart_t *uart)
{
    return -SDK_ERROR;
}

static int32_t stm32_uart_open(sdk_uart_t *uart, int32_t baudrate, int32_t data_bit, char parity, int32_t stop_bit)
{
    LL_USART_InitTypeDef USART_InitStruct = {0};

    // msp init
    if (stm32_uart_msp_init(uart) != SDK_OK)
    {
        return -SDK_ERROR;
    }

    if(uart->instance == USART1)
    {
        /* Peripheral clock enable */
        LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1);
    }
    else if(uart->instance == USART2)
    {
        /* Peripheral clock enable */
        LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART2);
    }
    else if(uart->instance == USART4)
    {
        /* Peripheral clock enable */
        LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART4);
    }
    else if(uart->instance == USART5)
    {
        /* Peripheral clock enable */
        LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART5);
    }
    else
    {
        return -SDK_ERROR;
    }

    USART_InitStruct.BaudRate = baudrate;

    switch (data_bit)
    {
    case 9:
        USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_9B;
        break;
    case 8:
    default:
        USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
        break;
    }

    switch (parity)
    {
    case 'e':
    case 'E':
        USART_InitStruct.Parity = LL_USART_PARITY_EVEN;
        break;
    case 'o':
    case 'O':
        USART_InitStruct.Parity = LL_USART_PARITY_ODD;
        break;
    case 'n':
    case 'N':
    default:
        USART_InitStruct.Parity = LL_USART_PARITY_NONE;
        break;
    }

    switch (stop_bit)
    {
    case 2:
        USART_InitStruct.StopBits = LL_USART_STOPBITS_2;
        break;
    case 1:
    default:
        USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
        break;
    }

    USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
    USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
    LL_USART_Init(uart->instance, &USART_InitStruct);
    LL_USART_ConfigAsyncMode(uart->instance);

    LL_USART_Enable(uart->instance);

    while((!(LL_USART_IsActiveFlag_TEACK(uart->instance))) || (!(LL_USART_IsActiveFlag_REACK(uart1.instance))))
    {
    }

    return SDK_OK;
}

static int32_t stm32_uart_close(sdk_uart_t *uart)
{
    LL_USART_DeInit(uart->instance);
    LL_USART_Disable(uart->instance);
    // msp deinit
    if (stm32_uart_msp_deinit(uart) != SDK_OK)
    {
        return -SDK_ERROR;
    }

    return SDK_OK;
}

static int32_t stm32_uart_putc(sdk_uart_t *uart, int32_t ch)
{
    while(0 == LL_USART_IsActiveFlag_TXE(uart->instance));
    LL_USART_TransmitData8(uart->instance, (uint8_t) ch);
    return ch;
}

static int32_t stm32_uart_getc(sdk_uart_t *uart)
{
    int ch = -1;

    if (LL_USART_IsActiveFlag_RXNE(uart->instance) != 0)
        ch = LL_USART_ReceiveData8(uart->instance);
    return ch;
}

static int32_t stm32_uart_control(sdk_uart_t *uart, int32_t cmd, void *args)
{
    switch (cmd)
    {
    case SDK_CONTROL_UART_DISABLE_INT:
        LL_USART_DisableIT_RXNE(uart->instance);
        LL_USART_DisableIT_ERROR(uart->instance);
        NVIC_DisableIRQ(uart->irq);
        break;
    case SDK_CONTROL_UART_ENABLE_INT:
        NVIC_SetPriority(uart->irq, uart->irq_prio);
        NVIC_EnableIRQ(uart->irq);
        LL_USART_EnableIT_RXNE(uart->instance);
        LL_USART_EnableIT_ERROR(uart->instance);
        break;
    case SDK_CONTROL_UART_INT_IDLE_ENABLE:
        /*wait IDLEF set and clear it*/
        while (0 == LL_USART_IsActiveFlag_IDLE(uart->instance))
        {
        }
        LL_USART_ClearFlag_IDLE(uart->instance);
        LL_USART_EnableIT_IDLE(uart->instance);
        break;
    case SDK_CONTROL_UART_INT_IDLE_DISABLE:
        LL_USART_DisableIT_IDLE(uart->instance);
        break;
    case SDK_CONTROL_UART_ENABLE_RX:
        LL_USART_EnableDirectionRx(uart->instance);
        break;
    case SDK_CONTROL_UART_DISABLE_RX:
        LL_USART_DisableDirectionRx(uart->instance);
        break;
    }

    return SDK_OK;
}

void USART1_IRQHandler(void)
{
    if(LL_USART_IsActiveFlag_RXNE(uart1.instance) && LL_USART_IsEnabledIT_RXNE(uart1.instance))
    {
        sdk_uart_rx_isr(&uart1);
    }

    if(LL_USART_IsActiveFlag_IDLE(uart1.instance) && LL_USART_IsEnabledIT_IDLE(uart1.instance))
    {
        LL_USART_ClearFlag_IDLE(uart1.instance);
        if(uart1.rx_idle_callback != NULL)
        {
            uart1.rx_idle_callback();
        }
    }
    if(LL_USART_IsActiveFlag_ORE(uart1.instance))
    {
        LL_USART_ClearFlag_ORE(uart1.instance);
    }
    if(LL_USART_IsActiveFlag_FE(uart1.instance))
    {
        LL_USART_ClearFlag_FE(uart1.instance);
    }
        if(LL_USART_IsActiveFlag_NE(uart1.instance))
    {
        LL_USART_ClearFlag_NE(uart1.instance);
    }
}

void USART2_IRQHandler(void)
{
    if(LL_USART_IsActiveFlag_RXNE(USART2) && LL_USART_IsEnabledIT_RXNE(USART2))
    {
        sdk_uart_rx_isr(&uart2);
    }

    if(LL_USART_IsActiveFlag_IDLE(USART2) && LL_USART_IsEnabledIT_IDLE(USART2))
    {
        LL_USART_ClearFlag_IDLE(USART2);
        if(uart2.rx_idle_callback != NULL)
        {
            uart2.rx_idle_callback();
        }
    }
    if(LL_USART_IsActiveFlag_ORE(USART2))
    {
        LL_USART_ClearFlag_ORE(USART2);
    }
    if(LL_USART_IsActiveFlag_FE(USART2))
    {
        LL_USART_ClearFlag_FE(USART2);
    }
        if(LL_USART_IsActiveFlag_NE(USART2))
    {
        LL_USART_ClearFlag_NE(USART2);
    }
}

void USART4_5_IRQHandler(void)
{
    if(LL_USART_IsActiveFlag_RXNE(USART5) && LL_USART_IsEnabledIT_RXNE(USART5))
    {
        sdk_uart_rx_isr(&uart5);
    }
    else if(LL_USART_IsActiveFlag_RXNE(USART4) && LL_USART_IsEnabledIT_RXNE(USART4))
    {
        sdk_uart_rx_isr(&uart4);
    }

    if(LL_USART_IsActiveFlag_IDLE(USART5) && LL_USART_IsEnabledIT_IDLE(USART5))
    {
        LL_USART_ClearFlag_IDLE(USART5);
        if(uart5.rx_idle_callback != NULL)
        {
            uart5.rx_idle_callback();
        }
    }
    else if(LL_USART_IsActiveFlag_IDLE(USART4) && LL_USART_IsEnabledIT_IDLE(USART4))
    {
        LL_USART_ClearFlag_IDLE(USART4);
        if(uart4.rx_idle_callback != NULL)
        {
            uart4.rx_idle_callback();
        }
    }

    if(LL_USART_IsActiveFlag_ORE(USART5))
    {
        LL_USART_ClearFlag_ORE(USART5);
    }
    else if(LL_USART_IsActiveFlag_ORE(USART4))
    {
        LL_USART_ClearFlag_ORE(USART4);
    }

    if(LL_USART_IsActiveFlag_FE(USART5))
    {
        LL_USART_ClearFlag_FE(USART5);
    }
    else if(LL_USART_IsActiveFlag_FE(USART4))
    {
        LL_USART_ClearFlag_FE(USART4);
    }

    if(LL_USART_IsActiveFlag_NE(USART5))
    {
        LL_USART_ClearFlag_NE(USART5);
    }
    else if(LL_USART_IsActiveFlag_NE(USART4))
    {
        LL_USART_ClearFlag_NE(USART4);
    }
}

sdk_uart_t uart1 = 
{
    .instance = USART1,
    .irq = USART1_IRQn,
    .irq_prio = 1,
    .ops.open = stm32_uart_open,
    .ops.close = stm32_uart_close,
    .ops.putc = stm32_uart_putc,
    .ops.getc = stm32_uart_getc,
    .ops.control = stm32_uart_control,
    .rx_callback = NULL,
    .rx_idle_callback = NULL,
    .rx_rto_callback = NULL,
};

sdk_uart_t uart2 = 
{
    .instance = USART2,
    .irq = USART2_IRQn,
    .irq_prio = 1,
    .ops.open = stm32_uart_open,
    .ops.close = stm32_uart_close,
    .ops.putc = stm32_uart_putc,
    .ops.getc = stm32_uart_getc,
    .ops.control = stm32_uart_control,
    .rx_callback = NULL,
    .rx_idle_callback = NULL,
    .rx_rto_callback = NULL,
};

sdk_uart_t uart4 = 
{
    .instance = USART4,
    .irq = USART4_5_IRQn,
    .irq_prio = 1,
    .ops.open = stm32_uart_open,
    .ops.close = stm32_uart_close,
    .ops.putc = stm32_uart_putc,
    .ops.getc = stm32_uart_getc,
    .ops.control = stm32_uart_control,
    .rx_callback = NULL,
    .rx_idle_callback = NULL,
    .rx_rto_callback = NULL,
};

sdk_uart_t uart5 = 
{
    .instance = USART5,
    .irq = USART4_5_IRQn,
    .irq_prio = 1,
    .ops.open = stm32_uart_open,
    .ops.close = stm32_uart_close,
    .ops.putc = stm32_uart_putc,
    .ops.getc = stm32_uart_getc,
    .ops.control = stm32_uart_control,
    .rx_callback = NULL,
    .rx_idle_callback = NULL,
    .rx_rto_callback = NULL,
};
