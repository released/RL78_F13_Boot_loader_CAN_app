/* Project/API reading notes: F13 clock and peripheral binding.
 * Boot establishes the board clock contract; App startup checks/inherits the paired configuration.
 * Keep diagnostic UART, timebase and CAN routing aligned with the actual board.
 */
#if defined(F13_BOARD_HOST_TEST)
#include "f13_board_mock.h"
#else
#include "r_cg_macrodriver.h"
#endif
#include "f13_board_support.h"

static unsigned char BoardWait8(volatile unsigned char __near *reg, unsigned char mask,
                                unsigned char expected)
{
    unsigned long poll;

    for (poll = 0UL; poll < F13_BOARD_CLOCK_POLL_LIMIT; poll++)
    {
        if ((*reg & mask) == expected)
        {
            return 1U;
        }
        NOP();
    }
    return 0U;
}

unsigned char F13_BoardClockReady(void)
{
    return (((CMC & 0xC1U) == 0x40U) && (MSTOP == 0U) && ((CKC & 0xF0U) == 0x30U) && (MDIV == 0U) &&
            (PLLCTL == 0x47U) && ((PLLSTS & 0x88U) == 0x88U))
               ? 1U
               : 0U;
}

void F13_BoardClockPrepare(void)
{
    /* App startup may inherit the boot PLL; never stop or rewrite it here. */
    if (PLLON != 0U)
    {
        return;
    }
    HIOSTOP = 0U;
    SELLOSC = 1U;
    CMC = 0x40U; /* Write-once X1 crystal mode, under 10 MHz. */
    MSTOP = 1U;
    MCM0 = 0U;
    SELPLL = 0U;
    MDIV = 1U; /* F8 option byte: HOCO64 / 2 = CPU32 until main. */
    XTSTOP = 1U;
    CSS = 0U;
    RTCCL = 0xC2U;
    TRD_CKSEL = 0U;
}

static unsigned char BoardClockFallback(unsigned char reason)
{
    SELPLL = 0U;
    if (BoardWait8(&PLLSTS, 0x08U, 0U) == 0U)
    {
        return F13_BOARD_CLOCK_FALLBACK_ERROR;
    }
    PLLON = 0U; /* Stop PLL before changing its main-clock source. */
    MCM0 = 0U;
    if (BoardWait8(&CKC, 0x20U, 0U) == 0U)
    {
        return F13_BOARD_CLOCK_FALLBACK_ERROR;
    }
    MDIV = 1U;
    MSTOP = 1U;
    return reason;
}

unsigned char F13_BoardClockStart(void)
{
    if (F13_BoardClockReady() != 0U)
    {
        return F13_BOARD_CLOCK_OK;
    }
    /* CMC cannot be repaired by a second write after a wrong generated setup. */
    if (((CMC & 0xC1U) != 0x40U) || (PLLON != 0U))
    {
        return F13_BOARD_CLOCK_CONFIG_ERROR;
    }
    OSTS = 0x07U; /* Same 2^18-cycle X1 stabilization threshold as benchmark. */
    MSTOP = 0U;
    if (BoardWait8(&OSTC, 0xFFU, 0xFFU) == 0U)
    {
        return BoardClockFallback(F13_BOARD_CLOCK_X1_TIMEOUT);
    }
    MCM0 = 1U;
    if (BoardWait8(&CKC, 0x20U, 0x20U) == 0U)
    {
        return BoardClockFallback(F13_BOARD_CLOCK_MAIN_TIMEOUT);
    }
    PLLCTL = 0x42U; /* X1 4 MHz, divide 2, multiply 16, lock wait 64 us. */
    NOP();
    PLLON = 1U;
    if (BoardWait8(&PLLSTS, 0x80U, 0x80U) == 0U)
    {
        return BoardClockFallback(F13_BOARD_CLOCK_PLL_TIMEOUT);
    }
    MDIV = 0U;
    SELPLL = 1U;
    if (BoardWait8(&PLLSTS, 0x88U, 0x88U) == 0U)
    {
        return BoardClockFallback(F13_BOARD_CLOCK_SELECT_TIMEOUT);
    }
    RTCCL = 0x42U;
    return F13_BoardClockReady() ? F13_BOARD_CLOCK_OK : F13_BOARD_CLOCK_CONFIG_ERROR;
}

unsigned char F13_BoardCanPrepare(void)
{
    unsigned long poll;

    /* TJA1050 S HIGH silences TX while controller registers are configured. */
    P1 |= 0x05U;
    PM1 = (PM1 | 0x02U) & 0xFAU;
    PSRSEL &= 0xFEU;
    CAN0TRMMK = 1U;
    CAN0CFRMK = 1U;
    CAN0ERRMK = 1U;
    CANGRFRMK = 1U;
    CANGERRMK = 1U;
    if (F13_BoardClockReady() == 0U)
    {
        return 0U;
    }
    CAN0EN = 1U;
    for (poll = 0UL; poll < F13_BOARD_CAN_POLL_LIMIT; poll++)
    {
        if ((GSTS & 0x08U) == 0U)
        {
            break;
        }
        NOP();
    }
    if (poll == F13_BOARD_CAN_POLL_LIMIT)
    {
        return 0U;
    }
    /* Wake global stop first, as in the Renesas initialization flow. A
     * boot-to-app handoff is not a reset: both modes must be reset before
     * rewriting timing, filters or message RAM. */
    GCTRL = (GCTRL & 0xFFF8U) | 0x0001U;
    for (poll = 0UL; poll < F13_BOARD_CAN_POLL_LIMIT; poll++)
    {
        if ((GSTS & 0x07U) == 0x01U)
        {
            break;
        }
        NOP();
    }
    if (poll == F13_BOARD_CAN_POLL_LIMIT)
    {
        return 0U;
    }
    C0CTRL = (C0CTRL & 0xFFF8U) | 0x0001U;
    for (poll = 0UL; poll < F13_BOARD_CAN_POLL_LIMIT; poll++)
    {
        if ((C0STSL & 0x07U) == 0x01U)
        {
            return 1U;
        }
        NOP();
    }
    return 0U;
}
