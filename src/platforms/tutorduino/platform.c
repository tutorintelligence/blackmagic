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
 * This file implements the platform specific functions for the tutorduino
 * BMP clone — a fixed-hardware platform with no runtime version detection.
 */

#include "general.h"
#include "platform.h"
#include "usb.h"
#include "aux_serial.h"
#include "morse.h"

#include <libopencm3/cm3/vector.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/scs.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/systick.h>

static void adc_init(void);
static void setup_vbus_irq(void);

/* Soft-start step count for target power PWM — 64 steps at 562.5 kHz gives ~114 µs ramp */
#define TPWR_SOFT_START_STEPS 64U

void platform_init(void)
{
	SCS_DEMCR |= SCS_DEMCR_VC_MON_EN;

	rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_72MHZ]);

	/* Enable peripherals */
	rcc_periph_clock_enable(RCC_USB);
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_TIM1);
	/* Power up the timer used for trace (TIM3) */
	rcc_periph_clock_enable(RCC_TIM3);
	rcc_periph_clock_enable(RCC_AFIO);
	rcc_periph_clock_enable(RCC_CRC);

	/* Setup GPIO ports */
	gpio_clear(USB_PU_PORT, USB_PU_PIN);
	gpio_set_mode(USB_PU_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, USB_PU_PIN);

	gpio_set_mode(JTAG_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, TMS_DIR_PIN | TCK_PIN | TDI_PIN);
	gpio_set_mode(JTAG_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_INPUT_FLOAT, TMS_PIN);
	gpio_set_mode(JTAG_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, TDO_PIN);

	/* Toggle required to sort out line drivers... */
	gpio_port_write(GPIOA, 0x8102);
	gpio_port_write(GPIOB, 0x2000);

	gpio_port_write(GPIOA, 0x8182);
	gpio_port_write(GPIOB, 0x2002);

	gpio_set_mode(LED_PORT, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, LED_UART | LED_IDLE_RUN | LED_ERROR);

	/*
	 * Enable nRST output. The clone uses an NPN to pull down,
	 * so setting the output HIGH asserts reset.
	 */
	platform_nrst_set_val(false);
	gpio_set_mode(NRST_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, NRST_PIN);

	/* nRST sense — pull-up input */
	gpio_set(NRST_SENSE_PORT, NRST_SENSE_PIN);
	gpio_set_mode(NRST_SENSE_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, NRST_SENSE_PIN);

	/*
	 * Enable internal pull-up on PWR_BR and configure target power control.
	 * The clone has the same power control circuitry as BMP mini v2.1+.
	 */
	gpio_set(PWR_BR_PORT, PWR_BR_PIN);
	gpio_set_mode(PWR_BR_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_OPENDRAIN, PWR_BR_PIN);

	/* Configure Timer 1 Channel 3N for tpwr soft start */
	gpio_primary_remap(AFIO_MAPR_SWJ_CFG_FULL_SWJ, AFIO_MAPR_TIM1_REMAP_PARTIAL_REMAP);
	timer_set_mode(TIM1, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
	timer_set_oc3_mode(TIM1, TIM_OCM_PWM1);
	timer_set_oc_polarity_low(TIM1, TIM_OC3N);
	timer_enable_oc_output(TIM1, TIM_OC3N);
	timer_set_oc_value(TIM1, TIM_OC3, 0);
	timer_set_deadtime(TIM1, 0);
	/* 64 steps → 562.5 kHz PWM from 36 MHz APB1 */
	timer_set_period(TIM1, TPWR_SOFT_START_STEPS - 1U);
	timer_enable_break_main_output(TIM1);
	timer_continuous_mode(TIM1);
	timer_update_on_overflow(TIM1);
	timer_enable_counter(TIM1);

	adc_init();

	/* Set up the NVIC vector table for the firmware */
	SCB_VTOR = (uintptr_t)&vector_table; // NOLINT(clang-diagnostic-pointer-to-int-cast, performance-no-int-to-ptr)

	platform_timing_init();
	blackmagic_usb_init();
	aux_serial_init();
	setup_vbus_irq();
}

void platform_nrst_set_val(bool assert)
{
	gpio_set(TMS_PORT, TMS_PIN);
	/* NPN pull-down: setting HIGH asserts reset */
	gpio_set_val(NRST_PORT, NRST_PIN, assert);

	if (assert) {
		for (volatile size_t i = 0; i < 10000U; ++i)
			continue;
	}
}

bool platform_nrst_get_val(void)
{
	return gpio_get(NRST_SENSE_PORT, NRST_SENSE_PIN) != 0;
}

bool platform_target_get_power(void)
{
	return !gpio_get(PWR_BR_PORT, PWR_BR_PIN);
}

static inline void platform_wait_pwm_cycle(void)
{
	while (!timer_get_flag(TIM1, TIM_SR_UIF))
		continue;
	timer_clear_flag(TIM1, TIM_SR_UIF);
}

bool platform_target_set_power(const bool power)
{
	if (power) {
		/* Configure the pin to be driven by the timer */
		gpio_set_mode(PWR_BR_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, PWR_BR_PIN);
		timer_clear_flag(TIM1, TIM_SR_UIF);
		platform_wait_pwm_cycle();
		/* Soft start power on the target */
		for (size_t step = 1U; step < TPWR_SOFT_START_STEPS; ++step) {
			timer_set_oc_value(TIM1, TIM_OC3, step);
			platform_wait_pwm_cycle();
		}
	}
	gpio_set_val(PWR_BR_PORT, PWR_BR_PIN, !power);
	if (power) {
		gpio_set_mode(PWR_BR_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_OPENDRAIN, PWR_BR_PIN);
		timer_set_oc_value(TIM1, TIM_OC3, 0U);
	}
	return true;
}

static void adc_init(void)
{
	rcc_periph_clock_enable(RCC_ADC1);

	gpio_set_mode(TPWR_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_ANALOG, TPWR_PIN);

	adc_power_off(ADC1);
	adc_disable_scan_mode(ADC1);
	adc_set_single_conversion_mode(ADC1);
	adc_disable_external_trigger_regular(ADC1);
	adc_set_right_aligned(ADC1);
	adc_set_sample_time_on_all_channels(ADC1, ADC_SMPR_SMP_239DOT5CYC);
	adc_enable_temperature_sensor();
	adc_power_on(ADC1);

	/* Wait for the ADC to finish starting up */
	for (volatile size_t i = 0; i < 800000U; ++i)
		continue;

	adc_reset_calibration(ADC1);
	adc_calibrate(ADC1);
}

uint32_t platform_target_voltage_sense(void)
{
	uint8_t channel = 8;
	adc_set_regular_sequence(ADC1, 1, &channel);

	adc_start_conversion_direct(ADC1);

	/* Wait for end of conversion. */
	while (!adc_eoc(ADC1))
		continue;

	uint32_t val = adc_read_regular(ADC1); /* 0-4095 */
	/* Clear EOC bit. The GD32F103 does not automatically reset it on ADC read. */
	ADC_SR(ADC1) &= ~ADC_SR_EOC;
	return (val * 99U) / 8191U;
}

const char *platform_target_voltage(void)
{
	static char ret[] = "0.0V";
	uint32_t val = platform_target_voltage_sense();
	ret[0] = '0' + val / 10U;
	ret[2] = '0' + val % 10U;
	return ret;
}

void platform_request_boot(void)
{
	/* Disconnect USB cable */
	gpio_set_mode(USB_PU_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, USB_PU_PIN);
	gpio_set_mode(USB_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, USB_DP_PIN | USB_DM_PIN);
	gpio_clear(USB_PORT, USB_DP_PIN | USB_DM_PIN);
	/* Make sure we drive the USB reset condition for at least 10ms */
	while (!(STK_CSR & STK_CSR_COUNTFLAG))
		continue;
	for (size_t count = 0U; count < 10U * SYSTICKMS; ++count) {
		while (!(STK_CSR & STK_CSR_COUNTFLAG))
			continue;
	}

	/* Drive boot request pin */
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO12);
	gpio_clear(GPIOB, GPIO12);
}

void exti15_10_isr(void)
{
	if (gpio_get(USB_VBUS_PORT, USB_VBUS_PIN))
		/* Drive pull-up high if VBUS connected */
		gpio_set_mode(USB_PU_PORT, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, USB_PU_PIN);
	else
		/* Allow pull-up to float if VBUS disconnected */
		gpio_set_mode(USB_PU_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, USB_PU_PIN);

	exti_reset_request(USB_VBUS_PIN);
}

static void setup_vbus_irq(void)
{
	nvic_set_priority(USB_VBUS_IRQ, IRQ_PRI_USB_VBUS);
	nvic_enable_irq(USB_VBUS_IRQ);

	gpio_set(USB_VBUS_PORT, USB_VBUS_PIN);
	gpio_set(USB_PU_PORT, USB_PU_PIN);

	gpio_set_mode(USB_VBUS_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, USB_VBUS_PIN);

	/* Configure EXTI for USB VBUS monitor */
	exti_select_source(USB_VBUS_PIN, USB_VBUS_PORT);
	exti_set_trigger(USB_VBUS_PIN, EXTI_TRIGGER_BOTH);
	exti_enable_request(USB_VBUS_PIN);

	exti15_10_isr();
}

int platform_hwversion(void)
{
	/* Fixed hardware — no version detection needed */
	return 3;
}

void platform_target_clk_output_enable(bool enable)
{
	(void)enable;
	/* No TCK direction control on this hardware */
}

bool platform_spi_init(const spi_bus_e bus)
{
	(void)bus;
	/* No SPI support on this hardware */
	return false;
}

bool platform_spi_deinit(const spi_bus_e bus)
{
	(void)bus;
	return true;
}

bool platform_spi_chip_select(const uint8_t device_select)
{
	(void)device_select;
	return false;
}

uint8_t platform_spi_xfer(const spi_bus_e bus, const uint8_t value)
{
	(void)bus;
	(void)value;
	return 0;
}

/*
 * DMA1 channel 5 is shared between USART1 RX and SWO UART.
 * On this hardware, UART SWO is disabled (SWO_UART = 0U), so
 * DMA channel 5 is always used for USART1 RX.
 */
void dma1_channel5_isr(void)
{
	usart1_rx_dma_isr();
}
