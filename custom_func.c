/*_____ I N C L U D E S ____________________________________________________*/
#include <stdio.h>
#include <string.h>

#include "inc_main.h"

#include "misc_config.h"
#include "custom_func.h"
#include "can_boot_config.h"
#include "../RL78_F13_Boot_loader_CAN/f13_board_support.h"
#include "RSCANconf/r_rl78_can_drv.h"

/* Compile the same validated polling CAN driver used by the boot loader. */
#include "RSCANconf/RSCANlite.c"
#include "RSCANconf/r_rl78_can_drv.c"

/*_____ D E C L A R A T I O N S ____________________________________________*/
static unsigned char g_can_app_ready;

volatile struct flag_32bit flag_PROJ_CTL;
#define FLAG_PROJ_TIMER_PERIOD_1000MS                 	(flag_PROJ_CTL.bit0)
#define FLAG_PROJ_TRIG_BTN1                       	    (flag_PROJ_CTL.bit1)
#define FLAG_PROJ_TRIG_BTN2                 	        (flag_PROJ_CTL.bit2)
#define FLAG_PROJ_REVERSE3                    		    (flag_PROJ_CTL.bit3)
#define FLAG_PROJ_REVERSE4                              (flag_PROJ_CTL.bit4)
#define FLAG_PROJ_REVERSE5                              (flag_PROJ_CTL.bit5)
#define FLAG_PROJ_REVERSE6                              (flag_PROJ_CTL.bit6)
#define FLAG_PROJ_REVERSE7                              (flag_PROJ_CTL.bit7)


#define FLAG_PROJ_TRIG_1                                (flag_PROJ_CTL.bit8)
#define FLAG_PROJ_TRIG_2                                (flag_PROJ_CTL.bit9)
#define FLAG_PROJ_TRIG_3                                (flag_PROJ_CTL.bit10)
#define FLAG_PROJ_TRIG_4                                (flag_PROJ_CTL.bit11)
#define FLAG_PROJ_TRIG_5                                (flag_PROJ_CTL.bit12)
#define FLAG_PROJ_REVERSE13                             (flag_PROJ_CTL.bit13)
#define FLAG_PROJ_REVERSE14                             (flag_PROJ_CTL.bit14)
#define FLAG_PROJ_REVERSE15                             (flag_PROJ_CTL.bit15)

/*_____ D E F I N I T I O N S ______________________________________________*/

volatile unsigned long counter_tick = 0;
volatile unsigned long btn_counter_tick = 0;

#define BTN_PRESSED_LONG                                (2500)

#define CAN_BOOT_CMD_ENTER                              (0x06U)
#define CAN_BOOT_STATUS_OK                              (0x00U)
#define CAN_BOOT_STATUS_BAD_SIZE                        (0x04U)

#pragma address (reset_to_bootloader = 0x000FF700)
volatile unsigned long reset_to_bootloader;
#pragma address (boot_shared_ram = 0x000FF710)
volatile unsigned char boot_shared_ram[CAN_BOOT_SHARED_RAM_SIZE];

static unsigned char app_launch_via_boot;
static unsigned char app_after_can_update;

/*_____ M A C R O S ________________________________________________________*/

/*_____ F U N C T I O N S __________________________________________________*/

static unsigned long AppSharedRead32(unsigned char offset)
{
    unsigned long value;
    unsigned char index;

    if (offset > (CAN_BOOT_SHARED_RAM_SIZE - 4U))
    {
        return 0UL;
    }
    value = 0UL;
    for (index = 0U; index < 4U; index++)
    {
        value |= (unsigned long)boot_shared_ram[offset + index] <<
                 ((unsigned long)index * 8UL);
    }
    return value;
}

static void AppSharedWrite32(unsigned char offset, unsigned long value)
{
    unsigned char index;

    if (offset > (CAN_BOOT_SHARED_RAM_SIZE - 4U))
    {
        return;
    }
    for (index = 0U; index < 4U; index++)
    {
        boot_shared_ram[offset + index] = (unsigned char)(value >>
            ((unsigned long)index * 8UL));
    }
}

void set_TIMER_PERIOD_1000MS(void)
{
    FLAG_PROJ_TIMER_PERIOD_1000MS = 1;
}

void reset_TIMER_PERIOD_1000MS(void)
{
    FLAG_PROJ_TIMER_PERIOD_1000MS = 0;
}

bool Is_TIMER_PERIOD_1000MS_Trig(void)
{
    return FLAG_PROJ_TIMER_PERIOD_1000MS;
}

unsigned long btn_get_tick(void)
{
	return (btn_counter_tick);
}

void btn_set_tick(unsigned long t)
{
	btn_counter_tick = t;
}

void btn_tick_counter(void)
{
	btn_counter_tick++;
    if (btn_get_tick() >= 60000)
    {
        btn_set_tick(0);
    }
}

unsigned long get_tick(void)
{
	return (counter_tick);
}

void set_tick(unsigned long t)
{
	counter_tick = t;
}

void tick_counter(void)
{
	counter_tick++;
    if (get_tick() >= 60000)
    {
        set_tick(0);
    }
}

void delay_ms(unsigned long ms)
{
	#if 1
    unsigned long tickstart = get_tick();
    unsigned long wait = ms;
	unsigned long tmp = 0;
	
    while (1)
    {
		if (get_tick() > tickstart)	// tickstart = 59000 , tick_counter = 60000
		{
			tmp = get_tick() - tickstart;
		}
		else // tickstart = 59000 , tick_counter = 2048
		{
			tmp = 60000 -  tickstart + get_tick();
		}		
		
		if (tmp > wait)
			break;
    }
	
	#else
	TIMER_Delay(TIMER0, 1000*ms);
	#endif
}

void Timer_1ms_IRQ(void)
{
    tick_counter();

    if ((get_tick() % 1000) == 0)
    {
        set_TIMER_PERIOD_1000MS();
    }

    if ((get_tick() % 50) == 0)
    {

    }	

    Button_Process_long_counter();
}


/*
    F13 target board
    LED1 connected to P66, LED2 connected to P67
*/
void LED_Toggle(void)
{
    // PIN_WRITE(6,6) = ~PIN_READ(6,6);
    // PIN_WRITE(6,7) = ~PIN_READ(6,7);
    P6_bit.no6 = ~P6_bit.no6;
    P6_bit.no7 = ~P6_bit.no7;
}

void loop(void)
{
	static unsigned long LOG1 = 0;

    if (Is_TIMER_PERIOD_1000MS_Trig())
    {
        reset_TIMER_PERIOD_1000MS();

        printf("log(timer)-" APP_RUNTIME_LOG_TAG ":%4ld\r\n", LOG1++);
        LED_Toggle();             
    }

    Button_Process_in_polling();
    CAN_BootRequestProcess();
}

unsigned char CAN_AppInit(void)
{
    Can_RtnType result;
    unsigned long poll_count;

    g_can_app_ready = 0U;
    if (F13_BoardCanPrepare() == 0U)
    {
        return 0U;
    }
    result = CAN_RTN_RAM_INIT;
    poll_count = 0UL;
    while ((result != CAN_RTN_OK) &&
           (poll_count < CAN_BOOT_CAN_INIT_POLL_LIMIT))
    {
        result = R_CAN_Init();
        poll_count++;
    }
    if (result != CAN_RTN_OK)
    {
        P1_bit.no2 = 1U;
        return 0U;
    }
    result = CAN_RTN_MODE_WAIT;
    poll_count = 0UL;
    while ((result == CAN_RTN_MODE_WAIT) &&
           (poll_count < CAN_BOOT_CAN_INIT_POLL_LIMIT))
    {
        result = R_CAN_GlobalStart();
        poll_count++;
    }
    if (result != CAN_RTN_OK)
    {
        P1_bit.no2 = 1U;
        return 0U;
    }
    result = CAN_RTN_MODE_WAIT;
    poll_count = 0UL;
    while ((result == CAN_RTN_MODE_WAIT) &&
           (poll_count < CAN_BOOT_CAN_INIT_POLL_LIMIT))
    {
        result = R_CAN_ChStart(CAN_CH0);
        poll_count++;
    }
    if (result != CAN_RTN_OK)
    {
        P1_bit.no2 = 1U;
        return 0U;
    }

    P1_bit.no2 = 0U; /* TJA1050 S LOW: enable TX only after initialization. */
    CAN0TRMMK = 1U;
    CAN0CFRMK = 1U;
    CAN0ERRMK = 1U;
    CANGRFRMK = 1U;
    CANGERRMK = 1U;
    g_can_app_ready = 1U;
    return 1U;
}

static unsigned char CAN_AppTransmit(can_frame_t * frame)
{
    Can_RtnType result;
    unsigned long poll_count;

    poll_count = 0UL;
    do
    {
        result = R_CAN_TrmByTxBuf(CAN_CH0, CAN_TXBUF0, frame);
        poll_count++;
    } while ((result == CAN_RTN_STS_ERROR) &&
             (poll_count < CAN_BOOT_CAN_TX_POLL_LIMIT));
    if (result != CAN_RTN_OK)
    {
        return 0U;
    }

    poll_count = 0UL;
    do
    {
        result = R_CAN_CheckTxBufResult(CAN_CH0, CAN_TXBUF0);
        poll_count++;
    } while ((result == CAN_RTN_TRANSMITTING) &&
             (poll_count < CAN_BOOT_CAN_TX_POLL_LIMIT));

    return (result == CAN_RTN_TX_END) ? 1U : 0U;
}

static void CAN_AppSendInfo(unsigned char page)
{
    can_frame_t frame;
    unsigned long value;
    unsigned char status;

    value = 0UL;
    status = CAN_BOOT_STATUS_OK;
    if (page == CAN_BOOT_INFO_PAGE_CORE)
    {
        value = (unsigned long)CAN_BOOT_TARGET_ID |
                ((unsigned long)CAN_BOOT_PROTOCOL_VERSION << 16) |
                ((unsigned long)CAN_BOOT_IMAGE_FORMAT_VERSION << 24);
    }
    else if (page == CAN_BOOT_INFO_PAGE_BOOT_VERSION)
    {
        value = CAN_BOOT_FIRMWARE_VERSION;
    }
    else if (page == CAN_BOOT_INFO_PAGE_BOOT_BUILD)
    {
        value = CAN_BOOT_FIRMWARE_BUILD;
    }
    else if (page == CAN_BOOT_INFO_PAGE_APP_VERSION)
    {
        value = CAN_BOOT_APP_FIRMWARE_VERSION;
    }
    else if (page == CAN_BOOT_INFO_PAGE_APP_BUILD)
    {
        value = CAN_BOOT_APP_FIRMWARE_BUILD;
    }
    else if (page == CAN_BOOT_INFO_PAGE_APP_START)
    {
        value = CAN_BOOT_APPLICATION_START;
    }
    else if (page == CAN_BOOT_INFO_PAGE_APP_CAPACITY)
    {
        value = CAN_BOOT_APPLICATION_CAPACITY;
    }
    else if (page == CAN_BOOT_INFO_PAGE_CODE_FLASH)
    {
        value = CAN_BOOT_CODE_FLASH_SIZE;
    }
    else if (page == CAN_BOOT_INFO_PAGE_BOOT_RESERVED)
    {
        value = CAN_BOOT_RESERVED_SIZE;
    }
    else if (page == CAN_BOOT_INFO_PAGE_ERASE_BLOCK)
    {
        value = CAN_BOOT_ERASE_BLOCK_SIZE;
    }
    else if (page == CAN_BOOT_INFO_PAGE_CAN_CAPS)
    {
        value = (unsigned long)CAN_BOOT_CAN_MODE_CLASSICAL |
                ((unsigned long)CAN_BOOT_CAN_ID_STANDARD << 8) |
                ((unsigned long)CAN_BOOT_CHECKSUM_CRC32 << 16) |
                ((unsigned long)CAN_BOOT_PROTOCOL_ID << 24);
    }
    else if (page == CAN_BOOT_INFO_PAGE_NOMINAL_MASK)
    {
        value = CAN_BOOT_NOMINAL_500K_MASK;
    }
    else if (page == CAN_BOOT_INFO_PAGE_DATA_MASK)
    {
        value = CAN_BOOT_DATA_BITRATE_MASK;
    }
    else if (page == CAN_BOOT_INFO_PAGE_REQUEST_ID)
    {
        value = CAN_BOOT_REQUEST_ID;
    }
    else if (page == CAN_BOOT_INFO_PAGE_RESPONSE_ID)
    {
        value = CAN_BOOT_RESPONSE_ID;
    }
    else if (page == CAN_BOOT_INFO_PAGE_PART_0)
    {
        value = CAN_BOOT_MCU_PART_0;
    }
    else if (page == CAN_BOOT_INFO_PAGE_PART_1)
    {
        value = CAN_BOOT_MCU_PART_1;
    }
    else if (page == CAN_BOOT_INFO_PAGE_PART_2)
    {
        value = CAN_BOOT_MCU_PART_2;
    }
    else if (page == CAN_BOOT_INFO_PAGE_PART_3)
    {
        value = CAN_BOOT_MCU_PART_3;
    }
    else if (page == CAN_BOOT_INFO_PAGE_FAMILY_0)
    {
        value = CAN_BOOT_MCU_FAMILY_0;
    }
    else if (page == CAN_BOOT_INFO_PAGE_FAMILY_1)
    {
        value = CAN_BOOT_MCU_FAMILY_1;
    }
    else if (page == CAN_BOOT_INFO_PAGE_FAMILY_2)
    {
        value = CAN_BOOT_MCU_FAMILY_2;
    }
    else if (page == CAN_BOOT_INFO_PAGE_APP_IMAGE_SIZE)
    {
        value = *(__far unsigned long *)(CAN_BOOT_APPLICATION_START +
                                         CAN_BOOT_MANIFEST_OFFSET +
                                         CAN_BOOT_MANIFEST_SIZE_OFFSET);
    }
    else if (page == CAN_BOOT_INFO_PAGE_UPDATE_POLICY)
    {
        value = CAN_BOOT_POLICY_WORD;
    }
    else if (page == CAN_BOOT_INFO_PAGE_IRQ_ABI)
    {
        value = CAN_BOOT_IRQ_ABI;
    }
    else if (page == CAN_BOOT_INFO_PAGE_BLOCK)
    {
        value = CAN_BOOT_BLOCK_INFO;
    }
    else if (page == CAN_BOOT_INFO_PAGE_MIN_IMAGE_SIZE)
    {
        value = CAN_BOOT_MINIMUM_IMAGE_SIZE;
    }
    else
    {
        status = CAN_BOOT_STATUS_BAD_SIZE;
    }

    frame.IDL = CAN_BOOT_RESPONSE_ID;
    frame.IDH = 0U;
    frame.THDSE = 0U;
    frame.RTR = 0U;
    frame.IDE = 0U;
    frame.TS = 0U;
    frame.LBL = 0U;
    frame.DLC = 8U;
    frame.DB[0] = CAN_BOOT_CMD_INFO;
    frame.DB[1] = status;
    frame.DB[2] = page;
    frame.DB[3] = CAN_BOOT_STATE_APPLICATION;
    frame.DB[4] = (unsigned char)value;
    frame.DB[5] = (unsigned char)(value >> 8);
    frame.DB[6] = (unsigned char)(value >> 16);
    frame.DB[7] = (unsigned char)(value >> 24);
    (void)CAN_AppTransmit(&frame);
}

void CAN_BootRequestProcess(void)
{
    can_frame_t frame;
    Can_RtnType result;

    if ((g_can_app_ready == 0U) || (F13_BoardClockReady() == 0U))
    {
        return;
    }
    result = R_CAN_ReadRxFIFO(CAN_RXFIFO0, &frame);
    if ((result != CAN_RTN_OK) && (result != CAN_RTN_OK_WITH_LOST))
    {
        return;
    }
    if ((frame.IDE != 0U) || (frame.RTR != 0U) ||
        (frame.IDL != CAN_BOOT_REQUEST_ID) || (frame.DLC == 0U))
    {
        return;
    }

    if (frame.DB[0] == CAN_BOOT_CMD_INFO)
    {
        if (frame.DLC == 2U)
        {
            CAN_AppSendInfo(frame.DB[1]);
        }
        else
        {
            CAN_AppSendInfo(0xFFU);
        }
        return;
    }
    if ((frame.DB[0] != CAN_BOOT_CMD_ENTER) || (frame.DLC != 1U))
    {
        return;
    }

    frame.IDL = CAN_BOOT_RESPONSE_ID;
    frame.IDH = 0U;
    frame.THDSE = 0U;
    frame.RTR = 0U;
    frame.IDE = 0U;
    frame.TS = 0U;
    frame.LBL = 0U;
    frame.DLC = 2U;
    frame.DB[0] = CAN_BOOT_CMD_ENTER;
    frame.DB[1] = 0U;
    (void)CAN_AppTransmit(&frame);
    request_bootloader_update();
}


// F13 EVB , P137/INTP0 , set both edge 
void Button_Process_long_counter(void)
{
    if (FLAG_PROJ_TRIG_BTN2)
    {
        btn_tick_counter();
    }
    else
    {
        btn_set_tick(0);
    }
}

void Button_Process_in_polling(void)
{
    static unsigned char cnt = 0;

    if (FLAG_PROJ_TRIG_BTN1)
    {
        FLAG_PROJ_TRIG_BTN1 = 0;
        printf("BTN pressed(%d)\r\n",cnt);

        if (cnt == 0)   //set both edge  , BTN pressed
        {
            FLAG_PROJ_TRIG_BTN2 = 1;
        }
        else if (cnt == 1)  //set both edge  , BTN released
        {
            FLAG_PROJ_TRIG_BTN2 = 0;
        }

        cnt = (cnt >= 1) ? (0) : (cnt+1) ;
    }

    if ((FLAG_PROJ_TRIG_BTN2 == 1) && 
        (btn_get_tick() > BTN_PRESSED_LONG))
    {         
        printf("BTN pressed LONG\r\n");
        btn_set_tick(0);
        FLAG_PROJ_TRIG_BTN2 = 0;
    }
}

// F13 EVB , P137/INTP0
void Button_Process_in_IRQ(void)    
{
    FLAG_PROJ_TRIG_BTN1 = 1;
}

void UARTx_Process(unsigned char rxbuf)
{    
    if (rxbuf > 0x7F)
    {
        printf("invalid command\r\n");
    }
    else
    {
        printf("press:%c(0x%02X)\r\n" , rxbuf,rxbuf);   // %c :  C99 libraries.
        switch(rxbuf)
        {
            case '1':
                FLAG_PROJ_TRIG_1 = 1;
                break;
            case '2':
                FLAG_PROJ_TRIG_2 = 1;
                break;
            case '3':
                FLAG_PROJ_TRIG_3 = 1;
                break;
            case '4':
                FLAG_PROJ_TRIG_4 = 1;
                break;
            case '5':
                FLAG_PROJ_TRIG_5 = 1;
                break;

            case 'X':
            case 'x':
                RL78_soft_reset(7);
                break;
            case 'Z':
            case 'z':
                request_bootloader_update();
                break;
        }
    }
}

/*
    Reset Control Flag Register (RESF) 
    BIT7 : TRAP
    BIT6 : 0
    BIT5 : 0
    BIT4 : WDCLRF
    BIT3 : 0
    BIT2 : 0
    BIT1 : IAWRF
    BIT0 : LVIRF
*/
void check_reset_source(void)
{
    /*
        Internal reset request by execution of illegal instruction
        0  Internal reset request is not generated, or the RESF register is cleared. 
        1  Internal reset request is generated. 
    */
    uint8_t src = RESF;
    printf("Reset Source <0x%08X>\r\n", src);

    #if 1   //DEBUG , list reset source
    if (src & BIT0)
    {
        printf("0)voltage detector (LVD)\r\n");       
    }
    if (src & BIT1)
    {
        printf("1)illegal-memory access\r\n");       
    }
    if (src & BIT2)
    {
        printf("2)EMPTY\r\n");       
    }
    if (src & BIT3)
    {
        printf("3)EMPTY\r\n");       
    }
    if (src & BIT4)
    {
        printf("4)watchdog timer (WDT) or clock monitor\r\n");       
    }
    if (src & BIT5)
    {
        printf("5)EMPTY\r\n");       
    }
    if (src & BIT6)
    {
        printf("6)EMPTY\r\n");       
    }
    if (src & BIT7)
    {
        printf("7)execution of illegal instruction\r\n");       
    }
    #endif

}

/*
    7:Internal reset by execution of illegal instruction
    1:Internal reset by illegal-memory access
*/
//perform sofware reset
void _reset_by_illegal_instruction(void)
{
    static const unsigned char illegal_Instruction = 0xFF;
    void (*dummy) (void) = (void (*)(void))&illegal_Instruction;
    dummy();
}
void _reset_by_illegal_memory_access(void)
{
    #if 1
    const unsigned char ILLEGAL_ACCESS_ON = 0x80;
    IAWCTL |= ILLEGAL_ACCESS_ON;            // switch IAWEN on (default off)
    *(__far volatile char *)0x00000 = 0x00; //write illegal address 0x00000(RESET VECTOR)
    #else
    signed char __far* a;                   // Create a far-Pointer
    IAWCTL |= _80_CGC_ILLEGAL_ACCESS_ON;    // switch IAWEN on (default off)
    a = (signed char __far*) 0x0000;        // Point to 0x000000 (FLASH-ROM area)
    *a = 0;
    #endif
}

void RL78_soft_reset(unsigned char flag)
{
    switch(flag)
    {
        case 7: // do not use under debug mode
            _reset_by_illegal_instruction();        
            break;
        case 1:
            _reset_by_illegal_memory_access();
            break;
    }
}

void request_bootloader_update(void)
{
    reset_to_bootloader = CAN_BOOT_RESET_TO_BOOT_SIGNATURE;
    RL78_soft_reset(1);
}

void boot_application_started(void)
{
    app_launch_via_boot =
        (AppSharedRead32(CAN_BOOT_RAM_HANDOFF_OFFSET) ==
         CAN_BOOT_HANDOFF_SIGNATURE) ? 1U : 0U;
    app_after_can_update =
        (AppSharedRead32(CAN_BOOT_RAM_UPDATE_OFFSET) ==
         CAN_BOOT_UPDATE_COMPLETE_SIGNATURE) ? 1U : 0U;
    AppSharedWrite32(CAN_BOOT_RAM_HANDOFF_OFFSET, 0UL);
    AppSharedWrite32(CAN_BOOT_RAM_UPDATE_OFFSET, 0UL);
    boot_shared_ram[CAN_BOOT_RAM_OWNER_OFFSET] =
        CAN_BOOT_RAM_OWNER_APPLICATION;
}

// retarget printf
int __far putchar(int c)
{
    // F13 , UART0
    STMK0 = 1U;    /* disable INTST0 interrupt */
    SDR00L = (unsigned char)c;
    while(STIF0 == 0)
    {

    }
    STIF0 = 0U;    /* clear INTST0 interrupt flag */
    return c;
}

void hardware_init(void)
{
    unsigned char can_initialized;
    unsigned char clock_status;

    clock_status = F13_BoardClockStart();
    // const unsigned char indicator[] = "hardware_init";
    EI();   //BSP_EI();
    R_UART0_Start();         		// UART , P15 , P16
    R_TMR_RJ0_Start();        		// TIMER  
    R_INTC0_Start();    		// BUTTON , P137
    can_initialized = CAN_AppInit();     // CAN , P10(CTXD0) , P11(CRXD0)
    
    // check_reset_source();
    printf("app image tag: %s\r\n", APP_RUNTIME_LOG_TAG);
    printf("clock status: 0x%02X; X1=4M PLL/CPU=32M\r\n", clock_status);
    printf("CAN init: %s; Classical 500K request=0x%03X response=0x%03X S=P12_%s EVDD=5V required\r\n",
           can_initialized ? "OK" : "FAIL",
           (unsigned int)CAN_BOOT_REQUEST_ID, (unsigned int)CAN_BOOT_RESPONSE_ID,
           ((P1 & 0x04U) == 0U) ? "LOW" : "HIGH");
    printf("app launch: %s; CAN update: %s\r\n",
           app_launch_via_boot ? "boot-validated" : "direct/unknown",
           app_after_can_update ? "completed" : "not reported");
    printf("%s finish\r\n\r\n",__func__);
    if (can_initialized == 0U)
    {
        printf("CAN initialization failed\r\n");
    }
    //printf("%s finish\r\n\r\n",indicator);
}
