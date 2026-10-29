/*_____ I N C L U D E S ____________________________________________________*/

/*_____ D E C L A R A T I O N S ____________________________________________*/
#define bool  _Bool
/* Project/API reading notes: User entry declarations shared with the selected main/startup
 * adapter.
 * Implement application initialization in hardware_init() and recurring foreground work in
 * loop(); peripheral ISR declarations remain with their owning driver.
 */

#ifndef TRUE
#define TRUE  (1)
#endif

#ifndef true
#define true  (1)
#endif

#ifndef FALSE
#define FALSE  (0)
#endif

#ifndef false
#define false  (0)
#endif

#ifndef BIT0
#define BIT0     (0x00000001UL)       ///< Bit 0 mask of an 32 bit integer
#endif

#ifndef BIT1
#define BIT1     (0x00000002UL)       ///< Bit 1 mask of an 32 bit integer
#endif

#ifndef BIT2
#define BIT2     (0x00000004UL)       ///< Bit 2 mask of an 32 bit integer
#endif

#ifndef BIT3
#define BIT3     (0x00000008UL)       ///< Bit 3 mask of an 32 bit integer
#endif

#ifndef BIT4
#define BIT4     (0x00000010UL)       ///< Bit 4 mask of an 32 bit integer
#endif

#ifndef BIT5
#define BIT5     (0x00000020UL)       ///< Bit 5 mask of an 32 bit integer
#endif

#ifndef BIT6
#define BIT6     (0x00000040UL)       ///< Bit 6 mask of an 32 bit integer
#endif

#ifndef BIT7
#define BIT7     (0x00000080UL)       ///< Bit 7 mask of an 32 bit integer
#endif

/*_____ D E F I N I T I O N S ______________________________________________*/

/* Change this short tag when preparing an app image for update verification. */
#define APP_RUNTIME_LOG_TAG  "V1"
// #define APP_RUNTIME_LOG_TAG  "V2"

/*_____ M A C R O S ________________________________________________________*/

/*_____ F U N C T I O N S __________________________________________________*/

void set_TIMER_PERIOD_1000MS(void);
void reset_TIMER_PERIOD_1000MS(void);
bool Is_TIMER_PERIOD_1000MS_Trig(void);
void Timer_1ms_IRQ(void);

void Button_Process_long_counter(void);
void Button_Process_in_polling(void);
void Button_Process_in_IRQ(void);
void UARTx_Process(unsigned char rxbuf);
unsigned char CAN_AppInit(void);
void CAN_BootRequestProcess(void);

void LED_Toggle(void);
void loop(void);

void check_reset_source(void);
void RL78_soft_reset(unsigned char flag);
void request_bootloader_update(void);
void boot_application_started(void);
void hardware_init(void);
