/**
 * Change Logs:
 * Date           Author          Notes
 * 2023-06-28     rgw             first version
 */

#include "sdk_board.h"
#include "sdk_flash.h"
#include "stm32l0xx_ll_flash.h"

#define DBG_LVL DBG_LOG
#define DBG_TAG "mcu.flash"
#include "sdk_log.h"

#define ALIGN_DOWN(size, align)      ((size) & ~((align) - 1))

/**
  * @brief  Gets the page of a given address
  * @param  Addr: Address of the FLASH Memory
  * @retval The page of a given address
  */
static uint32_t GetPage(uint32_t addr)
{
    uint32_t page = 0;
    page = ALIGN_DOWN(addr - MCU_FLASH_START_ADRESS, MCU_FLASH_PAGE_SIZE) + MCU_FLASH_START_ADRESS;
    return page;
}

sdk_err_t stm32_flash_open(sdk_flash_t *flash)
{
    LL_FLASH_Unlock();
    return SDK_OK;
}

sdk_err_t stm32_flash_close(sdk_flash_t *flash)
{
    LL_FLASH_Lock();
    return SDK_OK;
}

int32_t stm32_flash_read(sdk_flash_t *flash, uint32_t addr, uint8_t *buf, size_t size)
{
    size_t i;

    if ((addr + size) > MCU_FLASH_END_ADDRESS)
    {
        LOG_E("read outrange flash size! addr is (0x%08x)", (void *)(addr + size));
        return -SDK_E_INVALID;
    }

    for (i = 0; i < size; i++, buf++, addr++)
    {
        *buf = *(uint8_t *) addr;
    }

    return size;
}

int32_t stm32_flash_write(sdk_flash_t *flash, uint32_t addr, const uint8_t *buf, size_t size)
{
    sdk_err_t result = SDK_OK;
    ErrorStatus status = ERROR;
    uint32_t end_addr = addr + size;
    uni_data_32_t data32 = {0};

    if (addr % 4 != 0)
    {
        LOG_E("write addr must be 4-byte alignment");
        return -SDK_E_INVALID;
    }

    if ((end_addr) > MCU_FLASH_END_ADDRESS)
    {
        LOG_E("write outrange flash size! addr is (0x%08x)", (void *)(addr + size));
        return -SDK_E_INVALID;
    }

    sdk_hw_interrupt_disable();
    LL_FLASH_Unlock();

    while (addr < end_addr)
    {
        for(int i = 0; i < 4; i++)
        {
            data32.data_8[i] = buf[i];
        }
        status = LL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, data32.data_32);
        if(status == SUCCESS)
        {
            if (*(uint32_t *)addr !=  data32.data_32)
            {
                LOG_E("ERROR: write read! addr is (0x%08x), write is %d, read is %d\n", (void *)(addr), *(uint32_t *)addr,  data32.data_32);
                result = -SDK_ERROR;
                break;
            }
            addr += 4;
            buf  += 4;
        }
        else
        {
            LOG_E("ERROR: write! addr is (0x%08x)\n", (void *)(addr));
            result = -SDK_ERROR;
            break;
        }
    }

    LL_FLASH_Lock();
    sdk_hw_interrupt_enable();

    if (result != SDK_OK)
    {
        return result;
    }

    return size;
}

sdk_err_t stm32_flash_erase(sdk_flash_t *flash, uint32_t addr, size_t size)
{
    sdk_err_t result = SDK_OK;

    if ((addr + size) > MCU_FLASH_END_ADDRESS)
    {
        LOG_E("ERROR: erase outrange flash size! addr is (0x%08x)\n", (void *)(addr + size));
        return -SDK_E_INVALID;
    }

    sdk_hw_interrupt_disable();
    LL_FLASH_Unlock();

    int NbPages = (size + MCU_FLASH_PAGE_SIZE - 1) / MCU_FLASH_PAGE_SIZE;
    uint32_t PageAddress = GetPage(addr);
    uint32_t PAGEError = 0;

    if (LL_FLASHEx_Erase(FLASH_TYPEERASE_PAGES, PageAddress, NbPages, &PAGEError) != SUCCESS)
    {
        result = -SDK_ERROR;
        goto __exit;
    }

__exit:
    LL_FLASH_Lock();
    sdk_hw_interrupt_enable();

    if (result != SDK_OK)
    {
        return result;
    }

    LOG_D("erase done: addr (0x%08x), size %d\n", (void *)addr, size);
    return size;
}

sdk_err_t stm32_flash_control(sdk_flash_t *flash, int32_t cmd, void *args)
{
    switch (cmd)
    {
    default:
        break;
    }

    return SDK_OK;
}

sdk_flash_t stm32_onchip_flash = 
{
    .ops.open = stm32_flash_open,
    .ops.close = stm32_flash_close,
    .ops.read = stm32_flash_read,
    .ops.write = stm32_flash_write,
    .ops.erase = stm32_flash_erase,
    .ops.control = stm32_flash_control,
};
