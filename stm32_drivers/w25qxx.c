/**
 ******************************************************************************
 * @file           : w25qxx.h
 * @brief          : Minimal W25Qxx Library Source
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2022, 2023 Lars Boegild Thomsen <lbthomsen@gmail.com>
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 * Notice!  The library does _not_ bother to check that sectors have been erased
 * before writing.
 *
 ******************************************************************************
 */

#include "main.h"
#include "w25qxx.h"
#include "sdk_board.h"
#include "spi.h"

#define DBG_TAG "w25q"
#define DBG_LVL DBG_NONE
#include "sdk_log.h"

#define W25_DBG LOG_D

#define SPI_RX_BUFFER_SIZE 32*4 + 4
uint8_t aRxBuffer[SPI_RX_BUFFER_SIZE] = {0};
__IO uint8_t ubReceiveIndex = 0;
#define SPI_TIMEOUT 1000
#define HAL_MAX_DELAY      0xFFFFFFFF

static inline void cs_on(W25QXX_HandleTypeDef *w25qxx)
{
    board_spi_cs_low();
}

static inline void cs_off(W25QXX_HandleTypeDef *w25qxx)
{
    board_spi_cs_high();
}

W25QXX_result_t w25qxx_transmit(W25QXX_HandleTypeDef *w25qxx, uint8_t *buf, uint32_t len)
{
    W25QXX_result_t ret = W25QXX_Err;

    volatile uint16_t timeout;

    for (uint16_t i = 0; i < len; i++)
    {
        LL_SPI_TransmitData8(SPI1, buf[i]);
        timeout = SPI_TIMEOUT;
        while (!LL_SPI_IsActiveFlag_TXE(SPI1) && timeout--)
            ;
        if(timeout == 0)
            return W25QXX_Err;
        timeout = SPI_TIMEOUT;
        while (LL_SPI_IsActiveFlag_BSY(SPI1) && timeout--)
            ;
        if(timeout == 0)
            return W25QXX_Err;
        LL_SPI_ClearFlag_OVR(SPI1);
    }
    ret = W25QXX_Ok;
    return ret;
}

void SPI1_Rx_Callback(void)
{
  /* Read character in Data register.
  RXNE flag is cleared by reading data in DR register */
  aRxBuffer[ubReceiveIndex++] = LL_SPI_ReceiveData8(SPI1); //LL_SPI_ReceiveData16(SPI1) >> 8;

  if (ubReceiveIndex >= SPI_RX_BUFFER_SIZE)
  {
    ubReceiveIndex = 0;
  }
}
W25QXX_result_t w25qxx_receive(W25QXX_HandleTypeDef *w25qxx, uint8_t *buf, uint32_t len)
{
    W25QXX_result_t ret = W25QXX_Err;
    volatile uint16_t timeout;

    // if (LL_SPI_IsActiveFlag_RXNE(SPI1))
    // {
    //     /* Call function Slave Reception Callback */
    //     SPI1_Rx_Callback();
    // }
    /* Enable RXNE  Interrupt             */
    for (uint16_t i = 0; i < len; i++)
    {
        LL_SPI_TransmitData8(SPI1, 0x00);
        timeout = SPI_TIMEOUT;
        while (!LL_SPI_IsActiveFlag_RXNE(SPI1) && timeout--)
            ;
        if(timeout == 0)
            return W25QXX_Err;
        *buf = LL_SPI_ReceiveData8(SPI1);
        buf++;
    }

    // memcpy(buf, aRxBuffer, len);
    // ubReceiveIndex = 0;

    ret = W25QXX_Ok;

    return ret;
}



uint32_t w25qxx_read_id(W25QXX_HandleTypeDef *w25qxx) {
    uint32_t ret = 0;
    uint8_t buf[3];
    cs_on(w25qxx);
    buf[0] = W25QXX_GET_ID;
    if (w25qxx_transmit(w25qxx, buf, 1) == W25QXX_Ok) {
        if (w25qxx_receive(w25qxx, buf, 3) == W25QXX_Ok) {
            ret = (uint32_t) ((buf[0] << 16) | (buf[1] << 8) | (buf[2]));
        }
    }
    cs_off(w25qxx);
    return ret;
}

uint8_t w25qxx_get_status(W25QXX_HandleTypeDef *w25qxx) {
    uint8_t ret = 0;
    uint8_t buf = W25QXX_READ_REGISTER_1;
    cs_on(w25qxx);
    if (w25qxx_transmit(w25qxx, &buf, 1) == W25QXX_Ok) {
        if (w25qxx_receive(w25qxx, &buf, 1) == W25QXX_Ok) {
            ret = buf;
        }
    }
    cs_off(w25qxx);
    return ret;
}

static W25QXX_result_t w25qxx_write_enable(W25QXX_HandleTypeDef *w25qxx) {
    W25_DBG("w25qxx_write_enable");
    W25QXX_result_t ret = W25QXX_Err;
    uint8_t buf[1];
    cs_on(w25qxx);
    buf[0] = W25QXX_WRITE_ENABLE;
    if (w25qxx_transmit(w25qxx, buf, 1) == W25QXX_Ok) {
        ret = W25QXX_Ok;
    }
    cs_off(w25qxx);
    return ret;
}

static W25QXX_result_t w25qxx_wait_for_ready(W25QXX_HandleTypeDef *w25qxx, uint32_t timeout) {
    W25QXX_result_t ret = W25QXX_Ok;
    uint32_t begin = sdk_hw_get_systick();
    uint32_t now = sdk_hw_get_systick();
    while ((now - begin <= timeout) && (w25qxx_get_status(w25qxx) && 0x01 == 0x01)) {
        now = sdk_hw_get_systick();
    }
    if (now - begin == timeout)
        ret = W25QXX_Timeout;
    return ret;
}

#ifdef W25QXX_QSPI
W25QXX_result_t w25qxx_init(W25QXX_HandleTypeDef *w25qxx, QSPI_HandleTypeDef *qhspi) {

}
#else
// W25QXX_result_t w25qxx_init(W25QXX_HandleTypeDef *w25qxx, SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_port, uint16_t cs_pin)
W25QXX_result_t w25qxx_init(W25QXX_HandleTypeDef *w25qxx) 
{
    W25QXX_result_t result = W25QXX_Ok;

    MX_SPI1_Init();
    LL_SPI_Enable(SPI1);

    W25_DBG("w25qxx_init");

    // w25qxx->spiHandle = hspi;
    // w25qxx->cs_port = cs_port;
    // w25qxx->cs_pin = cs_pin;

    cs_off(w25qxx);

    uint32_t id = w25qxx_read_id(w25qxx);
    if (id) {
        w25qxx->manufacturer_id = (uint8_t) (id >> 16);
        w25qxx->device_id = (uint16_t) (id & 0xFFFF);

        LOG_D("Manufacturer ID: 0x%x, Device ID: 0x%x", w25qxx->manufacturer_id, w25qxx->device_id);
        
        switch (w25qxx->manufacturer_id) {
        case W25QXX_MANUFACTURER_GIGADEVICE:

            w25qxx->block_size = 0x10000;
            w25qxx->sector_size = 0x1000;
            w25qxx->sectors_in_block = 0x10;
            w25qxx->page_size = 0x100;
            w25qxx->pages_in_sector = 0x10;

            switch (w25qxx->device_id) {
            case 0x6017:
                w25qxx->block_count = 0x80;
                break;
            default:
                W25_DBG("Unknown Giga Device device");
                result = W25QXX_Err;
            }

            break;
        case W25QXX_MANUFACTURER_WINBOND:

            w25qxx->block_size = 0x10000;    // 64KB
            w25qxx->sector_size = 0x1000;    // 4KB
            w25qxx->sectors_in_block = 0x10;
            w25qxx->page_size = 0x100;
            w25qxx->pages_in_sector = 0x10;

            switch (w25qxx->device_id) {
            case 0x4014: // W25Q80DV，8Mbit
                w25qxx->block_count = 0x10;
                break;
            case 0x4018:
                w25qxx->block_count = 0x100;
                break;
            default:
                W25_DBG("Unknown Winbond device");
                result = W25QXX_Err;
            }

            break;
        default:
            W25_DBG("Unknown manufacturer");
            result = W25QXX_Err;
        }
    } else {
        result = W25QXX_Err;
    }

    if (result == W25QXX_Err) {
        // Zero the handle so it is clear initialization failed!
        memset(w25qxx, 0, sizeof(W25QXX_HandleTypeDef));
    }

    return result;

}
#endif

W25QXX_result_t w25qxx_read(W25QXX_HandleTypeDef *w25qxx, uint32_t address, uint8_t *buf, uint32_t len) {

    W25_DBG("w25qxx_read - address: 0x%08lx, lengh: 0x%04lx", address, len);

    // Transmit buffer holding command and address
    uint8_t tx[4] = {
    W25QXX_READ_DATA, (uint8_t) (address >> 16), (uint8_t) (address >> 8), (uint8_t) (address), };

    // First wait for device to get ready
    if (w25qxx_wait_for_ready(w25qxx, HAL_MAX_DELAY) != W25QXX_Ok) {
        return W25QXX_Err;
    }

    cs_on(w25qxx);
    if (w25qxx_transmit(w25qxx, tx, 4) == W25QXX_Ok) { // size will always be fixed
        if (w25qxx_receive(w25qxx, buf, len) != W25QXX_Ok) {
            cs_off(w25qxx);
            return W25QXX_Err;
        }
    }
    cs_off(w25qxx);

    return W25QXX_Ok;
}

W25QXX_result_t w25qxx_write(W25QXX_HandleTypeDef *w25qxx, uint32_t address, uint8_t *buf, uint32_t len) {

    W25_DBG("w25qxx_write - address 0x%08lx len 0x%04lx", address, len);

    // Let's determine the pages
    uint32_t first_page = address / w25qxx->page_size;
    uint32_t last_page = (address + len - 1) / w25qxx->page_size;

    W25_DBG("w25qxx_write %lu pages from %lu to %lu", 1 + last_page - first_page, first_page, last_page);

    uint32_t buffer_offset = 0;
    uint32_t start_address = address;

    for (uint32_t page = first_page; page <= last_page; ++page) {

        uint32_t write_len = w25qxx->page_size - (start_address & (w25qxx->page_size - 1));
        write_len = len > write_len ? write_len : len;

        W25_DBG("w25qxx_write: handling page %lu start_address = 0x%08lx buffer_offset = 0x%08lx len = %04lx", page, start_address, buffer_offset, write_len);

        // First wait for device to get ready
        if (w25qxx_wait_for_ready(w25qxx, HAL_MAX_DELAY) != W25QXX_Ok) {
            return W25QXX_Err;
        }

        if (w25qxx_write_enable(w25qxx) == W25QXX_Ok) {

            uint8_t tx[4] = {
            W25QXX_PAGE_PROGRAM, (uint8_t) (start_address >> 16), (uint8_t) (start_address >> 8), (uint8_t) (start_address), };

            cs_on(w25qxx);
            if (w25qxx_transmit(w25qxx, tx, 4) == W25QXX_Ok) { // size will always be fixed
                // Now write the buffer
                if (w25qxx_transmit(w25qxx, buf + buffer_offset, write_len) != W25QXX_Ok) {
                    cs_off(w25qxx);
                    return W25QXX_Err;
                }
            }
            cs_off(w25qxx);
        }
        start_address += write_len;
        buffer_offset += write_len;
        len -= write_len;
    }

    return W25QXX_Ok;
}

W25QXX_result_t w25qxx_erase(W25QXX_HandleTypeDef *w25qxx, uint32_t address, uint32_t len) {

    W25_DBG("w25qxx_erase, address = 0x%08lx len = 0x%04lx", address, len);

    W25QXX_result_t ret = W25QXX_Ok;

    // Let's determine the sector start
    uint32_t first_sector = address / w25qxx->sector_size;
    uint32_t last_sector = (address + len - 1) / w25qxx->sector_size;

    W25_DBG("w25qxx_erase: first sector: 0x%04lx", first_sector);W25_DBG("w25qxx_erase: last sector : 0x%04lx", last_sector);

    for (uint32_t sector = first_sector; sector <= last_sector; ++sector) {

        W25_DBG("Erasing sector %lu, starting at: 0x%08lx", sector, sector * w25qxx->sector_size);

        // First we have to ensure the device is not busy
        if (w25qxx_wait_for_ready(w25qxx, HAL_MAX_DELAY) == W25QXX_Ok) {
            if (w25qxx_write_enable(w25qxx) == W25QXX_Ok) {

                uint32_t sector_start_address = sector * w25qxx->sector_size;

                uint8_t tx[4] = {
                W25QXX_SECTOR_ERASE, (uint8_t) (sector_start_address >> 16), (uint8_t) (sector_start_address >> 8), (uint8_t) (sector_start_address), };

                cs_on(w25qxx);
                if (w25qxx_transmit(w25qxx, tx, 4) != W25QXX_Ok) {
                    ret = W25QXX_Err;
                }
                cs_off(w25qxx);
            }
        } else {
            ret = W25QXX_Timeout;
        }

    }

    return ret;
}

W25QXX_result_t w25qxx_chip_erase(W25QXX_HandleTypeDef *w25qxx) {
    if (w25qxx_write_enable(w25qxx) == W25QXX_Ok) {
        uint8_t tx[1] = {
        W25QXX_CHIP_ERASE };
        cs_on(w25qxx);
        if (w25qxx_transmit(w25qxx, tx, 1) != W25QXX_Ok) {
            return W25QXX_Err;
        }
        cs_off(w25qxx);
        if (w25qxx_wait_for_ready(w25qxx, HAL_MAX_DELAY) != W25QXX_Ok) {
            return W25QXX_Err;
        }
    }
    return W25QXX_Ok;
}

/*
 * vim: ts=4 et nowrap
 */
