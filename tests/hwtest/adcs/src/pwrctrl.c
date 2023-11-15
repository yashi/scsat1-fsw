/*
 * Copyright (c) 2023 Space Cubics,LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>

/* Registers */
#define SCADCS_SYSREG_BASE_ADDR (0x50000000)
#define SCADCS_PCR_OFFSET  (0x0000)
#define SCADCS_IRCR_OFFSET (0x0004)

/* Power Control Register */
#define SCADCS_PCR_KEYCODE (0x5A5A << 16)

/* IMU Reset Control Register */
#define SCADCS_IMU_RESET         (0U)
#define SCADCS_IMU_RESET_RELEASE BIT(0)

void sc_adcs_power_enable(uint8_t target_bit)
{
	sys_set_bits(SCADCS_SYSREG_BASE_ADDR + SCADCS_PCR_OFFSET,
				 SCADCS_PCR_KEYCODE | target_bit);
}

void sc_adcs_power_disable(uint8_t target_bit)
{
	sys_clear_bits(SCADCS_SYSREG_BASE_ADDR + SCADCS_PCR_OFFSET,
				 SCADCS_PCR_KEYCODE | target_bit);
}

void sc_adcs_imu_reset(void)
{
	sys_set_bits(SCADCS_SYSREG_BASE_ADDR + SCADCS_IRCR_OFFSET,
				 SCADCS_IMU_RESET);
	k_sleep(K_USEC(1));
}

void sc_adcs_imu_reset_release(void)
{
	sys_set_bits(SCADCS_SYSREG_BASE_ADDR + SCADCS_IRCR_OFFSET,
				 SCADCS_IMU_RESET_RELEASE);
	k_sleep(K_MSEC(300));
}
