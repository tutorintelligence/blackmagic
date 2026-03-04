/*
 * This file is part of the Black Magic Debug project.
 *
 * Copyright (C) 2011  Black Sphere Technologies Ltd.
 * Written by Gareth McMullin <gareth@blacksphere.co.nz>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * This file provides the platform specific declarations for the tutorduino
 * implementation — a BMP clone with fixed pin mappings (pre-hw6 equivalent).
 */

#ifndef PLATFORMS_TUTORDUINO_PLATFORM_H
#define PLATFORMS_TUTORDUINO_PLATFORM_H

#include "gpio.h"
#include "timing.h"
#include "timing_stm32.h"

#define PLATFORM_HAS_TRACESWO
#define PLATFORM_HAS_POWER_SWITCH

#if ENABLE_DEBUG == 1
#define PLATFORM_HAS_DEBUG
extern bool debug_bmp;
#endif

#define PLATFORM_IDENT   "tutorduino"
#define UPD_IFACE_STRING "@Internal Flash   /0x08000000/8*001Kg"

/*
 * Pin mappings for the tutorduino BMP clone.
 * All pins are hardcoded — no runtime hardware version detection.
 *
 * LED0     = PB2   (Yellow LED : Running)
 * LED1     = PB10  (Orange LED : Idle)
 * LED2     = PB11  (Red LED    : Error)
 *
 * TPWR     = PB0  (input)  -- analogue ADC1, CH8
 * nTRST    = PB1  (output)
 * nRST     = PA2  (output)
 * TDI      = PA3  (output)
 * TMS      = PA4  (input/output for SWDIO)
 * TCK      = PA5  (output SWCLK)
 * TDO      = PA6  (input)
 * nRST_SNS = PA7  (input)
 * TRACESWO = PB7  (input)  -- TDO hardwired to TRACESWO
 *
 * USB_PU   = PA8  (output)
 * USB_VBUS = PB13 (input)
 * BTN1     = PB12 (input)  -- Force DFU bootloader
 *
 * UART_TX  = PA9  (output) -- USART1
 * UART_RX  = PA10 (input)  -- USART1
 */

/* Hardware definitions... */
#define JTAG_PORT    GPIOA
#define TDI_PORT     JTAG_PORT
#define TMS_DIR_PORT JTAG_PORT
#define TMS_PORT     JTAG_PORT
#define TCK_PORT     JTAG_PORT
#define TDO_PORT     JTAG_PORT
#define TDI_PIN      GPIO3
#define TMS_DIR_PIN  GPIO1
#define TMS_PIN      GPIO4
#define TCK_PIN      GPIO5
#define TDO_PIN      GPIO6

#define SWDIO_DIR_PORT JTAG_PORT
#define SWDIO_PORT     JTAG_PORT
#define SWCLK_PORT     JTAG_PORT
#define SWDIO_DIR_PIN  TMS_DIR_PIN
#define SWDIO_PIN      TMS_PIN
#define SWCLK_PIN      TCK_PIN

#define TRST_PORT       GPIOB
#define TRST_PIN        GPIO1
#define NRST_PORT       GPIOA
#define NRST_PIN        GPIO2
#define NRST_SENSE_PORT GPIOA
#define NRST_SENSE_PIN  GPIO7

/* SWO comes in on TDO (PA6, TIM3 CH1) */
#define SWO_PORT GPIOA
#define SWO_PIN  GPIO6

/*
 * These are the control output pin definitions for TPWR.
 * TPWR is sensed via PB0 by sampling ADC1's channel 8.
 */
#define PWR_BR_PORT GPIOB
#define PWR_BR_PIN  GPIO1
#define TPWR_PORT   GPIOB
#define TPWR_PIN    GPIO0

/* USB pin definitions */
#define USB_PU_PORT GPIOA
#define USB_PORT    GPIOA
#define USB_PU_PIN  GPIO8
#define USB_DP_PIN  GPIO12
#define USB_DM_PIN  GPIO11

#define USB_VBUS_PORT GPIOB
#define USB_VBUS_PIN  GPIO13
#define USB_VBUS_IRQ  NVIC_EXTI15_10_IRQ

#define LED_PORT      GPIOB
#define LED_PORT_UART GPIOB
#define LED_0         GPIO2
#define LED_1         GPIO10
#define LED_2         GPIO11
#define LED_UART      LED_0
#define LED_IDLE_RUN  LED_1
#define LED_ERROR     LED_2

#define SWD_CR       GPIO_CRL(SWDIO_PORT)
#define SWD_CR_SHIFT (4U << 2U)

#define TMS_SET_MODE()                                                                       \
	do {                                                                                     \
		gpio_set(TMS_DIR_PORT, TMS_DIR_PIN);                                                 \
		gpio_set_mode(TMS_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, TMS_PIN); \
	} while (0)

#define SWDIO_MODE_FLOAT()                        \
	do {                                          \
		uint32_t cr = SWD_CR;                     \
		cr &= ~(0xfU << SWD_CR_SHIFT);            \
		cr |= (0x4U << SWD_CR_SHIFT);             \
		GPIO_BRR(SWDIO_DIR_PORT) = SWDIO_DIR_PIN; \
		SWD_CR = cr;                              \
	} while (0)

#define SWDIO_MODE_DRIVE()                         \
	do {                                           \
		uint32_t cr = SWD_CR;                      \
		cr &= ~(0xfU << SWD_CR_SHIFT);             \
		cr |= (0x1U << SWD_CR_SHIFT);              \
		GPIO_BSRR(SWDIO_DIR_PORT) = SWDIO_DIR_PIN; \
		SWD_CR = cr;                               \
	} while (0)

#define UART_PIN_SETUP()                                                                                        \
	do {                                                                                                        \
		gpio_set_mode(USBUSART_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, USBUSART_TX_PIN); \
		gpio_set_mode(USBUSART_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, USBUSART_RX_PIN);             \
		gpio_set(USBUSART_PORT, USBUSART_RX_PIN);                                                               \
	} while (0)

#define USB_DRIVER st_usbfs_v1_usb_driver
#define USB_IRQ    NVIC_USB_LP_CAN_RX0_IRQ
#define USB_ISR(x) usb_lp_can_rx0_isr(x)

/*
 * Interrupt priorities. Low numbers are high priority.
 * TIM3 is used for traceswo capture and must be highest priority.
 */
#define IRQ_PRI_USB          (1U << 4U)
#define IRQ_PRI_USBUSART     (2U << 4U)
#define IRQ_PRI_USBUSART_DMA (2U << 4U)
#define IRQ_PRI_USB_VBUS     (14U << 4U)
#define IRQ_PRI_SWO_TIM      (0U << 4U)
#define IRQ_PRI_SWO_DMA      (0U << 4U)

/* Hardcoded to USART1 — no hw version switching */
#define USBUSART               USART1
#define USBUSART_IRQ           NVIC_USART1_IRQ
#define USBUSART_CLK           RCC_USART1
#define USBUSART_PORT          GPIOA
#define USBUSART_TX_PIN        GPIO9
#define USBUSART_RX_PIN        GPIO10
#define USBUSART_ISR(x)        usart1_isr(x)
#define USBUSART_DMA_BUS       DMA1
#define USBUSART_DMA_CLK       RCC_DMA1
#define USBUSART_DMA_TX_CHAN   DMA_CHANNEL4
#define USBUSART_DMA_TX_IRQ    NVIC_DMA1_CHANNEL4_IRQ
#define USBUSART_DMA_TX_ISR(x) dma1_channel4_isr(x)
#define USBUSART_DMA_RX_CHAN   DMA_CHANNEL5
#define USBUSART_DMA_RX_IRQ    NVIC_DMA1_CHANNEL5_IRQ
#define USBUSART_DMA_RX_ISR(x) usart1_rx_dma_isr(x)

/* Use TIM3 Input 1 (from PA6/TDO) for Manchester data recovery */
#define SWO_TIM TIM3
#define SWO_TIM_CLK_EN()
#define SWO_TIM_IRQ         NVIC_TIM3_IRQ
#define SWO_TIM_ISR(x)      tim3_isr(x)
#define SWO_IC_IN           TIM_IC_IN_TI1
#define SWO_IC_RISING       TIM_IC1
#define SWO_CC_RISING       TIM3_CCR1
#define SWO_ITR_RISING      TIM_DIER_CC1IE
#define SWO_STATUS_RISING   TIM_SR_CC1IF
#define SWO_IC_FALLING      TIM_IC2
#define SWO_CC_FALLING      TIM3_CCR2
#define SWO_STATUS_FALLING  TIM_SR_CC2IF
#define SWO_STATUS_OVERFLOW (TIM_SR_CC1OF | TIM_SR_CC2OF)
#define SWO_TRIG_IN         TIM_SMCR_TS_TI1FP1

/*
 * No UART SWO on this hardware — DMA channel 5 is used for USART1 RX.
 * SWO_UART is 0U so the UART SWO code path is never taken at runtime,
 * but the macros must still be defined for compilation.
 */
#define SWO_UART        0U
#define SWO_UART_CLK    RCC_USART1
#define SWO_UART_DR     USART1_DR
#define SWO_UART_PORT   GPIOA
#define SWO_UART_RX_PIN GPIO10

#define SWO_DMA_BUS    DMA1
#define SWO_DMA_CLK    RCC_DMA1
#define SWO_DMA_CHAN   DMA_CHANNEL5
#define SWO_DMA_IRQ    NVIC_DMA1_CHANNEL5_IRQ
#define SWO_DMA_ISR(x) swo_dma_isr(x)

#define SET_RUN_STATE(state)   running_status = (state)
#define SET_IDLE_STATE(state)  gpio_set_val(LED_PORT, LED_IDLE_RUN, state)
#define SET_ERROR_STATE(state) gpio_set_val(LED_PORT, LED_ERROR, state)

/*
 * These are bounce declarations for the ISR handlers competing for dma1_channel5_isr().
 * The actual handler is defined in platform.c, the USART1 RX handler in aux_serial.c,
 * and the SWO DMA handler in swo_uart.c.
 */
void usart1_rx_dma_isr(void);
void swo_dma_isr(void);

/* Frequency constants (in Hz) for the bitbanging routines */
#define BITBANG_CALIBRATED_FREQS
/* Same calibration as the native BMP — identical STM32F103 @ 72 MHz */
#define BITBANG_NO_DELAY_FREQ  1951961U
#define BITBANG_0_DELAY_FREQ   1384484U
#define BITBANG_DIVIDER_OFFSET 52U
#define BITBANG_DIVIDER_FACTOR 30U

#endif /* PLATFORMS_TUTORDUINO_PLATFORM_H */
