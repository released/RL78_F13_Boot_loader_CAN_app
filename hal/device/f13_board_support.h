#ifndef F13_BOARD_SUPPORT_H
#define F13_BOARD_SUPPORT_H

/*
 * QB-R5F10BMG-TB baseline verified with RL78_F13_CAN_Transfer:
 * X1 4 MHz / 2 * 16 = PLL/fCLK 32 MHz, MDIV=0; CAN uses fCLK/2.
 * BRP register 1 and 1+11+4 TQ give Classical 500 kbit/s, sample point 75%.
 * UART and TMRJ0 retain their existing 32-MHz divisors; FSL frequency is 32.
 *
 * IC2 TJA1050T needs EVDD=4.75..5.25 V. MCU operation at 3.3 V does NOT
 * validate CAN power. P12 controls S (silent mode), not standby: LOW enables
 * TX. P10/P11 connect to TXD/RXD; neither pin is an analog input here.
 *
 * ClockPrepare is called before C RAM initialization, so it uses no globals.
 * It writes CMC in X1 mode on reset but defers oscillator/PLL waits to main.
 * The F8 option byte keeps HOCO at 64 MHz; MDIV=1 gives a 32-MHz startup/
 * fallback CPU clock. ClockStart preserves an already valid boot-to-app PLL.
 * Never enable CAN/FSL on a failed clock start. Do not import F24 clock or
 * flash-library settings into F13; these MCUs have different requirements.
 */
#define F13_BOARD_CPU_MHZ (32U)
#define F13_BOARD_CLOCK_OK (0U)
#define F13_BOARD_CLOCK_CONFIG_ERROR (1U)
#define F13_BOARD_CLOCK_X1_TIMEOUT (2U)
#define F13_BOARD_CLOCK_MAIN_TIMEOUT (3U)
#define F13_BOARD_CLOCK_PLL_TIMEOUT (4U)
#define F13_BOARD_CLOCK_SELECT_TIMEOUT (5U)
#define F13_BOARD_CLOCK_FALLBACK_ERROR (6U)

#ifndef F13_BOARD_CLOCK_POLL_LIMIT
#define F13_BOARD_CLOCK_POLL_LIMIT (2000000UL)
#endif
#ifndef F13_BOARD_CAN_POLL_LIMIT
#define F13_BOARD_CAN_POLL_LIMIT (60000UL)
#endif

void F13_BoardClockPrepare(void);
unsigned char F13_BoardClockStart(void);
unsigned char F13_BoardClockReady(void);
/* Mask CAN IRQs, configure pins, and return inherited CAN state to reset. */
unsigned char F13_BoardCanPrepare(void);

#endif
