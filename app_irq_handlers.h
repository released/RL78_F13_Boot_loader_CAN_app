#ifndef APP_IRQ_HANDLERS_H
#define APP_IRQ_HANDLERS_H

/* X(physical vector byte address, global C interrupt function).
 * Change this list only when adding/removing a real App ISR.
 * Address source: this MCU's iodefine.h INTxxx definitions and hardware manual
 * interrupt-vector table; the number is not a CAN port or an IRQ ordinal.
 * App slot address = IMAGE_APPLICATION_START + vector * 2.
 * Each handler needs one global #pragma interrupt without vect=, returns RETI,
 * and its C source must be registered in this App's MTPJ.
 * Initialize/clear the peripheral cause before enabling an IRQ. Keep unused
 * sources masked; the generated default RETI does not clear their causes.
 * Reset/reserved/BRK entries are generated, not user mappings.
 * https://released.github.io/slide_boot_loader_for_RL78/#device-app-irq
 *
 * 0x08 = INTP0: external input, r_intc0_interrupt in r_cg_intc_user.c.
 * 0x16 = INTST0: UART0 transmit, r_uart0_interrupt_send.
 * 0x18 = INTSR0: UART0 receive, r_uart0_interrupt_receive.
 * 0x1E = INTTRJ0: timer RJ0, r_tmr_rj0_interrupt.
 * Keep a backslash on every entry except the last when extending the list.
 */
#define APP_IRQ_HANDLERS(X) \
    X(0x08, r_intc0_interrupt) \
    X(0x16, r_uart0_interrupt_send) \
    X(0x18, r_uart0_interrupt_receive) \
    X(0x1E, r_tmr_rj0_interrupt)

#endif
