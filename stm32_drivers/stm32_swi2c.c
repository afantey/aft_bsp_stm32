/**
 * Change Logs:
 * Date           Author          Notes
 * 2023-04-27     rgw             first version
 */

#include "sdk_board.h"
#include "sdk_swi2c.h"

//#define DBG_LVL DBG_LOG
#define LOG_TAG              "swi2c"
#include "sdk_log.h"


static void stm32_i2c_gpio_init(void)
{
    //Set sda and scl to OD and High
    LL_GPIO_SetOutputPin(SDA_MCU_GPIO_Port, SDA_MCU_Pin);
    LL_GPIO_SetOutputPin(SCL_MCU_GPIO_Port, SCL_MCU_Pin);
}

static void stm32_set_sda(void *data, int32_t state)
{
    if (state)
    {
        LL_GPIO_SetOutputPin(SDA_MCU_GPIO_Port, SDA_MCU_Pin);
    }
    else
    {
        LL_GPIO_ResetOutputPin(SDA_MCU_GPIO_Port, SDA_MCU_Pin);
    }
}

static void stm32_set_scl(void *data, int32_t state)
{
    if (state)
    {
        LL_GPIO_SetOutputPin(SCL_MCU_GPIO_Port, SCL_MCU_Pin);
    }
    else
    {
        LL_GPIO_ResetOutputPin(SCL_MCU_GPIO_Port, SCL_MCU_Pin);
    }
}

static int32_t stm32_get_sda(void *data)
{
    int ret = LL_GPIO_IsInputPinSet(SDA_MCU_GPIO_Port, SDA_MCU_Pin);
    return ret;
}

static int32_t stm32_get_scl(void *data)
{
    int ret = LL_GPIO_IsInputPinSet(SCL_MCU_GPIO_Port, SCL_MCU_Pin);
    return ret;
}

static void stm32_udelay(uint32_t us)
{
    sdk_hw_us_delay(us);
}

static sdk_err_t stm32_i2c_unlock(void)
{
    int32_t i = 0;
    int ret = stm32_get_sda(NULL);
    LOG_D("ret = %d", ret);
    if (0 == ret)
    {
        while (i++ < 9)
        {
            stm32_set_scl(NULL, 1);
            stm32_udelay(100);
            stm32_set_scl(NULL, 0);
            stm32_udelay(100);
        }
    }
    ret = stm32_get_sda(NULL);
    LOG_D("ret = %d", ret);
    if (0 == ret)
    {
        return -SDK_ERROR;
    }

    return SDK_OK;
}

static sdk_err_t stm32_i2c_init(void)
{
    sdk_err_t result;

    stm32_i2c_gpio_init();
    stm32_i2c_unlock();
    return SDK_OK;
}

static sdk_err_t stm32_i2c_deinit(void)
{
    return SDK_OK;
}

static struct sdk_swi2c_ops stm32_i2c_ops =
{
    .data     = NULL,
    .init     = stm32_i2c_init,
    .deinit   = stm32_i2c_deinit,
    .set_sda  = stm32_set_sda,
    .set_scl  = stm32_set_scl,
    .get_sda  = stm32_get_sda,
    .get_scl  = stm32_get_scl,
    .udelay   = stm32_udelay,
    .delay_us = 1,
    .timeout  = 100
};

sdk_swi2c_t swi2c1 = {
	.is_opened = 0,
	.ops = &stm32_i2c_ops,
};
