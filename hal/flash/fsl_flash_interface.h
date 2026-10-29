



#include "fsl_types.h"
#include "fsl.h"
#include "user_def.h"



/*
    fsl_flash_voltage_u08. 
    0x00: Full-speed mode 
    Other than above: Wide voltage mode 

    check hardware manual:
    RL78/F13 and RL78/F14 do not have wide voltage mode, select full-speed mode
*/
#define FULL_SPEED_MODE                         (0x00)          /* Initial setting value ( full-speed mode ) */

/*
    fsl_u08 fsl_frequency_u08
    Configured frequency ( frequency >= 4MHz) 
*/
#define FREQUENCY_8M                            (0x08)
#define FREQUENCY_20M                           (0x14)          /* Initial setting value ( frequency ) */
#define FREQUENCY_24M                           (0x18)          /* Initial setting value ( frequency ) */
#define FREQUENCY_32M                           (0x20)          /* Initial setting value ( frequency ) */

/*
    fsl_auto_status_check_u08
    0x00: Status check user mode 
    Other than above: Status check internal mode 
*/
#define INTERNAL_MODE                           (0x01)          /* Initial setting value ( internal mode ) */


#define BLOCK_BOOT0_BEGIN			0
#define BLOCK_BOOT0_END				3
#define BLOCK_BOOT1_BEGIN			4
#define BLOCK_BOOT1_END				7
#define BYTES_PER_WRITE				64			//max of 256

uint8_t init_fcl(void);

uint8_t flash_erase_app_area(unsigned long i_u32_start_addr);
// uint8_t flash_erase_app_area(void);

// uint8_t flash_boot_swap_reset(void);
uint8_t flash_write_packet_data(uint32_t addr, uint8_t data[], uint16_t size);
void flash_read_packet_data(uint32_t addr, uint8_t data[], uint16_t size);


























