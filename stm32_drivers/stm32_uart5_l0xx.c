/**
 * Change Logs:
 * Date           Author          Notes
 * 2023-06-16     rgw             first version
 */

#include "sdk_board.h"
#include "sdk_uart.h"

static int32_t stm32_uart5_open(sdk_uart_t *uart, int32_t baudrate, int32_t data_bit, char parity, int32_t stop_bit)
{
    LL_USART_InitTypeDef USART_InitStruct = {0};

    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Peripheral clock enable */
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART5);

    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB);
    /**USART5 GPIO Configuration
    PB3   ------> USART5_TX
    PB4   ------> USART5_RX
    */
    GPIO_InitStruct.Pin = LL_GPIO_PIN_3;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    GPIO_InitStruct.Alternate = LL_GPIO_AF_6;
    LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LL_GPIO_PIN_4;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    GPIO_InitStruct.Alternate = LL_GPIO_AF_6;
    LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

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
    LL_USART_Init(USART5, &USART_InitStruct);
    LL_USART_ConfigAsyncMode(USART5);

    LL_USART_Enable(USART5);

    while ((!(LL_USART_IsActiveFlag_TEACK(USART5))) || (!(LL_USART_IsActiveFlag_REACK(USART5))))
    {
    }

    return SDK_OK;
}

static int32_t stm32_uart5_close(sdk_uart_t *uart)
{
    LL_USART_DeInit(USART5);
    LL_USART_Disable(USART5);
    LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_3, LL_GPIO_MODE_ANALOG);
    LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_4, LL_GPIO_MODE_ANALOG);
    return SDK_OK;
}

static int32_t stm32_uart5_putc(sdk_uart_t *uart, int32_t ch)
{
    while(0 == LL_USART_IsActiveFlag_TXE(USART5));
    LL_USART_TransmitData8(USART5, (uint8_t) ch);
    return ch;
}

static int32_t stm32_uart5_getc(sdk_uart_t *uart)
{
    int ch = -1;

    if (LL_USART_IsActiveFlag_RXNE(USART5) != 0)
        ch = LL_USART_ReceiveData8(USART5);
    return ch;
}

static int32_t stm32_uart5_control(sdk_uart_t *uart, int32_t cmd, void *args)
{
    switch (cmd)
    {
    case SDK_CONTROL_UART_DISABLE_INT:
        LL_USART_DisableIT_RXNE(USART5);
        LL_USART_DisableIT_ERROR(USART5);
        NVIC_DisableIRQ(USART4_5_IRQn);
        break;
    case SDK_CONTROL_UART_ENABLE_INT:
        NVIC_SetPriority(USART4_5_IRQn, 1);
        NVIC_EnableIRQ(USART4_5_IRQn);
        LL_USART_EnableIT_RXNE(USART5);
        LL_USART_EnableIT_ERROR(USART5);
        break;
    case SDK_CONTROL_UART_INT_IDLE_ENABLE:
        /*wait IDLEF set and clear it*/
        while (0 == LL_USART_IsActiveFlag_IDLE(USART5))
        {
        }
        LL_USART_ClearFlag_IDLE(USART5);
        LL_USART_EnableIT_IDLE(USART5);
        break;
    case SDK_CONTROL_UART_INT_IDLE_DISABLE:
        LL_USART_DisableIT_IDLE(USART5);
        break;
    case SDK_CONTROL_UART_ENABLE_RX:
        LL_USART_EnableDirectionRx(USART5);
        break;
    case SDK_CONTROL_UART_DISABLE_RX:
        LL_USART_DisableDirectionRx(USART5);
        break;
    }

    return SDK_OK;
}

extern sdk_uart_t uart5;

void USART4_5_IRQHandler(void)
{
    if(LL_USART_IsActiveFlag_RXNE(USART5) && LL_USART_IsEnabledIT_RXNE(USART5))
    {
        sdk_uart_rx_isr(&uart5);
    }

    if(LL_USART_IsActiveFlag_IDLE(USART5) && LL_USART_IsEnabledIT_IDLE(USART5))
    {
        LL_USART_ClearFlag_IDLE(USART5);
        if(uart5.rx_idle_callback != NULL)
        {
            uart5.rx_idle_callback();
        }
    }
    if(LL_USART_IsActiveFlag_ORE(USART5))
    {
        LL_USART_ClearFlag_ORE(USART5);
    }
    if(LL_USART_IsActiveFlag_FE(USART5))
    {
        LL_USART_ClearFlag_FE(USART5);
    }
    if(LL_USART_IsActiveFlag_NE(USART5))
    {
        LL_USART_ClearFlag_NE(USART5);
    }
}

sdk_uart_t uart5 = 
{
    .ops.open = stm32_uart5_open,
    .ops.close = stm32_uart5_close,
    .ops.putc = stm32_uart5_putc,
    .ops.getc = stm32_uart5_getc,
    .ops.control = stm32_uart5_control,
    .rx_callback = NULL,
    .rx_idle_callback = NULL,
    .rx_rto_callback = NULL,
};
