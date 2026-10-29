/* Project/API reading notes: FSL adapter for RL78 code Flash.
 * Keep address/range validation before the vendor API; preparation, interrupt masking and RAM
 * execution requirements belong to this layer rather than the transport decoder.
 */

#include <r_cg_macrodriver.h>
#include "fsl_flash_interface.h"

#include "user_def.h"
#include "f13_board_support.h"

__near fsl_write_t fsl_write_str;

/********************************************************************************
 * Function Name : init_fcl
 * Description   : This function initializes the FSL driver.
 * Parameters    : None
 * Return value  : FSL status code
 ********************************************************************************/
uint8_t init_fcl(void)
{
    fsl_u08 fsl_err = FSL_ERR_PARAMETER;
    fsl_descriptor_t fsl_descr;
    // fsl_fsw_t           fsl_window;

    /* Benchmark CPU clock is 32 MHz. Never initialize FSL on a failed PLL. */
    if (F13_BoardClockReady() == 0U)
    {
        return FSL_ERR_PARAMETER;
    }

    /* initialize self-programming environment */

    fsl_descr.fsl_flash_voltage_u08 = FULL_SPEED_MODE;
    fsl_descr.fsl_frequency_u08 = FREQUENCY_32M; /* CPU32 from the board PLL. */
    fsl_descr.fsl_auto_status_check_u08 = INTERNAL_MODE;

    fsl_err = FSL_Init((__far fsl_descriptor_t *)&fsl_descr);
    if (fsl_err != FSL_OK)
    {
        return fsl_err;
    }

    FSL_Open();
    FSL_PrepareFunctions();
    FSL_PrepareExtFunctions();

    // fsl_window.fsl_start_block_u16 = 0;
    // fsl_window.fsl_end_block_u16 = BLOCK_APP_END;
    // FSL_SetFlashShieldWindow(&fsl_window);

    return FSL_OK;
}

uint8_t flash_erase_app_area(unsigned long i_u32_start_addr)
{
    uint8_t fsl_status = FSL_ERR_PARAMETER;
    uint16_t l_u16_block_number;

    /* Reject Boot, protected tail and partial erase blocks before FSL access. */
    if ((i_u32_start_addr < APP_FLASH_START) ||
        (i_u32_start_addr > (FLASH_END_ADDRESS - FLASH_BYTE_PER_BLOCK + 1UL)) ||
        ((i_u32_start_addr & (FLASH_BYTE_PER_BLOCK - 1UL)) != 0UL))
    {
        return FSL_ERR_PARAMETER;
    }

    /* This expression (actual block number) never exceeds the range of casting uint16_t */
    l_u16_block_number = (uint16_t)(i_u32_start_addr >> SAMPLE_VALUE_U08_SHIFT_ADDR_TO_BLOCK_CF);

    fsl_status = FSL_BlankCheck(l_u16_block_number);
#if 0 // status check user mode
	while(fsl_status == FSL_BUSY) 
	{ 
		fsl_status = FSL_StatusCheck(); 
	}
#endif

    /* If the target block is not in the blank state */
    if (fsl_status == FSL_ERR_BLANKCHECK)
    {
        fsl_status = FSL_Erase(l_u16_block_number);
#if 1
        if (fsl_status != FSL_OK)
        {
            return fsl_status;
        }
#else // status check user mode
        while (fsl_status == FSL_BUSY)
        {
            fsl_status = FSL_StatusCheck();
        }
#endif
    }

    return fsl_status;
}

// uint8_t flash_erase_app_area(void)
// {
// 	uint16_t    i;
// 	uint8_t     fsl_status;

// 	for (i = BLOCK_BOOT1_BEGIN; i <= BLOCK_APP_END; i++)
// 	{
// 		fsl_status = FSL_Erase(i);
// 		if (fsl_status != FSL_OK)
// 		{
// 			return fsl_status;
// 		}
// 	}

// 	return fsl_status;
// }

uint8_t flash_write_packet_data(uint32_t addr, uint8_t data[], uint16_t size)
{
    uint8_t fsl_status = FSL_ERR_PARAMETER;

    /* This wrapper writes one aligned four-byte word per call. */
    (void)size;
    if ((data == 0) || (addr < APP_FLASH_START) || (addr > (FLASH_END_ADDRESS - 3UL)) ||
        ((addr & 3UL) != 0UL))
    {
        return FSL_ERR_PARAMETER;
    }

    /* Setup data handler for write */
    fsl_write_str.fsl_data_buffer_p_u08 = (__near fsl_u08 *)data;
    // fsl_write_str.fsl_word_count_u08 = (uint8_t)(size/4);
    fsl_write_str.fsl_word_count_u08 = (fsl_u08)(1); // 1 word = 4 bytes
    fsl_write_str.fsl_destination_address_u32 = addr;

    fsl_status = FSL_Write((__near fsl_write_t *)&fsl_write_str);
#if 0 // status check user mode
    while(fsl_status == FSL_BUSY) 
    { 
        fsl_status = FSL_StatusCheck(); 
    }
#endif

    return fsl_status;
}

// void flash_read_packet_data(uint32_t addr, uint8_t data[], uint16_t size)
// {
// 	 uint8_t __far *ptr = (uint8_t __far *)addr;
// 	 uint8_t i;

// 	 for(i = 0; i < size; i++)
// 	 {
// 		 data[i] = ptr[i];
// 	 }
// }

// uint8_t flash_boot_swap_reset(void)
// {
// 	fsl_u08 fsl_status;

// 	/* Invert boot flag to prepare for boot swap */
// 	fsl_status = FSL_InvertBootFlag();

// 	if( fsl_status != 0x00 )
// 	{
// 		return fsl_status;
// 	}

// 	/* Generate a reset */
// 	FSL_ForceReset();

// 	/* Should never occur, this silences compiler */
// 	return fsl_status;
// }
