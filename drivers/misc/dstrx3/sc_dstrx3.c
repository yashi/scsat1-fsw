/*
 * Copyright (c) 2024 Space Cubics,LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT sc_dstrx3

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sc_dstrx3, LOG_LEVEL_INF);

#include <zephyr/irq.h>
#include <zephyr/sys/byteorder.h>
#include "sc_dstrx3.h"

/* Registers */
#define SC_DSTRX3_CS_OFFSET    (0x0000) /* DSTRX3 Control Status Register */
#define SC_DSTRX3_INTST_OFFSET (0x0004) /* Interrupt Status Register */
#define SC_DSTRX3_INTEN_OFFSET (0x0008) /* Interrupt Enable Register */
#define SC_DSTRX3_CMTX_OFFSET  (0x0010) /* Command Transmit Register */
#define SC_DSTRX3_HK1_OFFSET                                                                       \
	(0x0020) /* House-Keeping Receive 1 (WDT Counter/RSSI/Receive Frequency) Register */
#define SC_DSTRX3_HK2_OFFSET                                                                       \
	(0x0024) /* House-Keeping Receive 2 (Temperature/Voltage/Transmit Power/Status) Register   \
		  */
#define SC_DSTRX3_HK3_OFFSET                                                                       \
	(0x0028) /* House-Keeping Receive 3 (Prog No/Reserve/Check Sum) Register */
#define SC_DSTRX3_HKCIE_OFFSET (0x002C) /* House-Keeping Change Interrupt Enable Register */
#define SC_DSTRX3_PRESC_OFFSET (0x0030) /* Command/House-Keeping UART Prescaler Register */
#define SC_DSTRX3_HKTO_OFFSET  (0x0034) /* House-Keeping Timeout Setting Register */
#define SC_DSTRX3_ULDC_OFFSET  (0x0040) /* Uplink Data Control Register */
#define SC_DSTRX3_ULBCS_OFFSET (0x0044) /* Uplink Buffer Control/Status Register */
#define SC_DSTRX3_ULBTH_OFFSET (0x0048) /* Uplink Buffer Threshold Register */
#define SC_DSTRX3_DLDC_OFFSET  (0x0050) /* Downlink Data Control Register */
#define SC_DSTRX3_DLBCS_OFFSET (0x0054) /* Downlink Buffer Control/Status Register */
#define SC_DSTRX3_DLBTH_OFFSET (0x0058) /* Donwlink Buffer Threshold Register */
#define SC_DSTRX3_ULB_OFFSET   (0x0100) /* Uplink Data Buffer */
#define SC_DSTRX3_DLB_OFFSET   (0x0200) /* Downlink Data Buffer */

/* DSTRX3 Control Status Register */
#define SC_DSTRX3_ULIF_EN_BIT (24)
#define SC_DSTRX3_DLIF_EN_BIT (16)
#define SC_DSTRX3_HKIF_EN_BIT (8)
#define SC_DSTRX3_CMIF_EN_BIT (0)

/* Interrupt Enable Register */
#define SC_DSTRX3_ULIF_CLE_EN   BIT(28)
#define SC_DSTRX3_ULD_RSERR_EN  BIT(26)
#define SC_DSTRX3_ULD_CRCERR_EN BIT(25)
#define SC_DSTRX3_ULD_LENERR_EN BIT(24)
#define SC_DSTRX3_ULB_URINT_EN  BIT(23)
#define SC_DSTRX3_ULB_ORINT_EN  BIT(22)
#define SC_DSTRX3_ULB_THINT_EN  BIT(21)
#define SC_DSTRX3_ULD_RXCOMP_EN BIT(20)
#define SC_DSTRX3_DLB_OFINT_EN  BIT(18)
#define SC_DSTRX3_DLB_THINT_EN  BIT(17)
#define SC_DSTRX3_DLD_TXCOMP_EN BIT(16)
#define SC_DSTRX3_HK_FMERR_EN   BIT(11)
#define SC_DSTRX3_HK_CSERR_EN   BIT(10)
#define SC_DSTRX3_HK_UERR_EN    BIT(9)
#define SC_DSTRX3_HK_TOUT_EN    BIT(8)
#define SC_DSTRX3_HK_CHANGE_EN  BIT(5)
#define SC_DSTRX3_HK_RCV_EN     BIT(4)
#define SC_DSTRX3_CM_COMP_EN    BIT(0)
#define SC_DSTRX3_INTEN_ALL                                                                        \
	(SC_DSTRX3_ULIF_CLE_EN | SC_DSTRX3_ULD_RSERR_EN | SC_DSTRX3_ULD_CRCERR_EN |                \
	 SC_DSTRX3_ULD_LENERR_EN | SC_DSTRX3_ULB_URINT_EN | SC_DSTRX3_ULB_ORINT_EN |               \
	 SC_DSTRX3_ULB_THINT_EN | SC_DSTRX3_ULD_RXCOMP_EN | SC_DSTRX3_DLB_OFINT_EN |               \
	 SC_DSTRX3_DLB_THINT_EN | SC_DSTRX3_DLD_TXCOMP_EN | SC_DSTRX3_HK_FMERR_EN |                \
	 SC_DSTRX3_HK_CSERR_EN | SC_DSTRX3_HK_UERR_EN | SC_DSTRX3_HK_TOUT_EN |                     \
	 SC_DSTRX3_HK_CHANGE_EN | SC_DSTRX3_HK_RCV_EN | SC_DSTRX3_CM_COMP_EN)

/* House-Keeping Receive */
#define SC_DSTRX3_HK1_RCV_FREQ(x)     (((x) & 0xffff0000) >> 16)
#define SC_DSTRX3_HK1_RSSI(x)         (((x) & 0x0000ff00) >> 8)
#define SC_DSTRX3_HK1_WDT_COUNTER(x)  (((x) & 0x000000ff) >> 0)
#define SC_DSTRX3_HK2_CARR_LOCK(x)    (((x) & 0x80000000) >> 31)
#define SC_DSTRX3_HK2_SUBCARR_LOCK(x) (((x) & 0x40000000) >> 30)
#define SC_DSTRX3_HK2_TX_PWR_SET(x)   (((x) & 0x3C000000) >> 26)
#define SC_DSTRX3_HK2_BIT_RATE_SET(x) (((x) & 0x03000000) >> 24)
#define SC_DSTRX3_HK2_TX_PWR(x)       (((x) & 0x00ff0000) >> 16)
#define SC_DSTRX3_HK2_VOLTAGE(x)      (((x) & 0x0000ff00) >> 8)
#define SC_DSTRX3_HK2_TEMPERATURE(x)  (((x) & 0x000000ff) >> 0)
#define SC_DSTRX3_HK3_CHK_SUM(x)      (((x) & 0x00ff0000) >> 16)
#define SC_DSTRX3_HK3_FREE_COUNT(x)   (((x) & 0x0000ff00) >> 8)
#define SC_DSTRX3_HK3_PROG_NO(x)      (((x) & 0x000000ff) >> 0)

/* Command Transmit Register */
#define SC_DSTRX3_CM_PWR_MODE(x) (((x) & 0x0000000F) << 4)
#define SC_DSTRX3_CM_BIT_RATE(x) (((x) & 0x00000003) << 2)

/* Uplink Buffer Control/Status Register */
#define SC_DSTRX_ULB_DLEN(x)   (((x) & 0x001FF000) >> 16)
#define SC_DSTRX_ULB_COUNT(x)  (((x) & 0x00000F00) >> 8)
#define SC_DSTRX_ULB_STATUS(x) (((x) & 0x00000070))
#define SC_DSTRX_ULB_CRC_ERR   BIT(6)
#define SC_DSTRX_ULB_RS_ERR    BIT(5)
#define SC_DSTRX_ULB_DLEN_ERR  BIT(4)
#define SC_DSTRX_ULB_CLEAR     BIT(1)
#define SC_DSTRX_ULB_CHANGE    BIT(0)

/* Downlink Buffer Control/Status Register */
#define SC_DSTRX_DLBCS_TYPE(x)     (((x) & 0x0000000F) << 28)
#define SC_DSTRX_DLBCS_DLEN(x)     (((x) & 0x000001FF) << 16)
#define SC_DSTRX_DLBCS_INT         BIT(8)
#define SC_DSTRX_DLBCS_COUNT_SHIFT (((x) & 0x0000000F) << 4)
#define SC_DSTRX_DLBCS_CLEAR       BIT(1)
#define SC_DSTRX_DLBCS_WCOMP       BIT(0)

typedef void (*irq_init_func_t)(const struct device *dev);

struct sc_dstrx3_cfg {
	mm_reg_t base;
	irq_init_func_t irq_init;
	enum sc_dstrx3_tx_power tx_power;
	enum sc_dstrx3_bit_rate bit_rate;
};

int sc_dstrx3_get_hk_telemetry(const struct device *dev, struct sc_dstrx3_hk *hk)
{
	uint32_t hk1;
	uint32_t hk2;
	uint32_t hk3;
	const struct sc_dstrx3_cfg *cfg = dev->config;

	hk1 = sys_read32(cfg->base + SC_DSTRX3_HK1_OFFSET);
	hk2 = sys_read32(cfg->base + SC_DSTRX3_HK2_OFFSET);
	hk3 = sys_read32(cfg->base + SC_DSTRX3_HK3_OFFSET);

	LOG_DBG("HK1 raw: 0x%08x", hk1);
	LOG_DBG("HK2 raw: 0x%08x", hk2);
	LOG_DBG("HK3 raw: 0x%08x", hk3);

	hk->free_count = SC_DSTRX3_HK3_FREE_COUNT(hk3);
	hk->wdt_count = SC_DSTRX3_HK1_WDT_COUNTER(hk1);
	hk->rssi = SC_DSTRX3_HK1_RSSI(hk1);
	hk->rcv_freq = SC_DSTRX3_HK1_RCV_FREQ(hk1);
	hk->temperature = SC_DSTRX3_HK2_TEMPERATURE(hk2);
	hk->voltage = SC_DSTRX3_HK2_VOLTAGE(hk2);
	hk->tx_power = SC_DSTRX3_HK2_TX_PWR(hk2);
	hk->carrier_lock = SC_DSTRX3_HK2_CARR_LOCK(hk2);
	hk->sub_carrier_lock = SC_DSTRX3_HK2_SUBCARR_LOCK(hk2);
	hk->tx_power_set = SC_DSTRX3_HK2_TX_PWR_SET(hk2);
	hk->bit_rate_set = SC_DSTRX3_HK2_BIT_RATE_SET(hk2);
	hk->program_no = SC_DSTRX3_HK3_PROG_NO(hk3);
	hk->checksum = SC_DSTRX3_HK3_CHK_SUM(hk3);

	LOG_DBG("FREE_COUNTER     : %d", hk->free_count);
	LOG_DBG("WDT_COUNTER      : %d", hk->wdt_count);
	LOG_DBG("RSSI             : %d", hk->rssi);
	LOG_DBG("RCV_FREQ         : %d", hk->rcv_freq);
	LOG_DBG("TEMPERATURE      : %d", hk->temperature);
	LOG_DBG("VOLTAGE          : %f [v] (raw: %d)", (double)(hk->voltage / 256.0f * 2.5f),
		hk->voltage);
	LOG_DBG("TX_PWR           : %d", hk->tx_power);
	LOG_DBG("CARRIER_LOCK     : %d", hk->carrier_lock);
	LOG_DBG("SUB_CARRIER_LOCK : %d", hk->sub_carrier_lock);
	LOG_DBG("TX_POWER_SET     : %d", hk->tx_power_set);
	LOG_DBG("BIT_RATE_SET     : %d", hk->bit_rate_set);
	LOG_DBG("PROG_NO          : %d", hk->program_no);
	LOG_DBG("CHK_SUM          : %d", hk->checksum);

	return 0;
}

void sc_dstrx3_set_tx_param(const struct device *dev, enum sc_dstrx3_tx_power tx_power,
			    enum sc_dstrx3_bit_rate bit_rate)
{
	const struct sc_dstrx3_cfg *cfg = dev->config;
	uint32_t val = SC_DSTRX3_CM_PWR_MODE(tx_power) | SC_DSTRX3_CM_BIT_RATE(bit_rate);

	sys_write32(val, cfg->base + SC_DSTRX3_CMTX_OFFSET);
	LOG_DBG("CMTX: 0x%08x", sys_read32(cfg->base + SC_DSTRX3_CMTX_OFFSET));
}

void sc_dstrx3_set_default_tx_param(const struct device *dev)
{
	const struct sc_dstrx3_cfg *cfg = dev->config;
	sc_dstrx3_set_tx_param(dev, cfg->tx_power, cfg->bit_rate);
}

static void sc_dstrx3_isr(const struct device *dev)
{
	uint32_t isr;
	const struct sc_dstrx3_cfg *cfg = dev->config;

	isr = sys_read32(cfg->base + SC_DSTRX3_INTST_OFFSET);
	LOG_DBG("IRQ Status 0x%08x", isr);
	sys_write32(isr, cfg->base + SC_DSTRX3_INTST_OFFSET);
}

static void sc_dstrx3_enable_irq(const struct device *dev)
{
	const struct sc_dstrx3_cfg *cfg = dev->config;

	sys_set_bits(cfg->base + SC_DSTRX3_INTEN_OFFSET, SC_DSTRX3_INTEN_ALL);
	cfg->irq_init(dev);
}

void sc_dstrx3_enable_hk(const struct device *dev)
{
	const struct sc_dstrx3_cfg *cfg = dev->config;
	sys_set_bit(cfg->base + SC_DSTRX3_CS_OFFSET, SC_DSTRX3_HKIF_EN_BIT);
}

void sc_dstrx3_disable_hk(const struct device *dev)
{
	const struct sc_dstrx3_cfg *cfg = dev->config;
	sys_clear_bit(cfg->base + SC_DSTRX3_CS_OFFSET, SC_DSTRX3_HKIF_EN_BIT);
}

void sc_dstrx3_enable_cmdif(const struct device *dev)
{
	const struct sc_dstrx3_cfg *cfg = dev->config;
	sys_set_bit(cfg->base + SC_DSTRX3_CS_OFFSET, SC_DSTRX3_CMIF_EN_BIT);
}

void sc_dstrx3_disable_cmdif(const struct device *dev)
{
	const struct sc_dstrx3_cfg *cfg = dev->config;
	sys_clear_bit(cfg->base + SC_DSTRX3_CS_OFFSET, SC_DSTRX3_CMIF_EN_BIT);
}

static int sc_dstrx3_init(const struct device *dev)
{
	sc_dstrx3_enable_irq(dev);
	sc_dstrx3_enable_uplink(dev);
	/* Enable de-randomize */
	sc_dstrx3_set_uplink_control(dev, ULD_PR_EN);

	return 0;
}

void sc_dstrx3_enable_downlink(const struct device *dev)
{
	const struct sc_dstrx3_cfg *cfg = dev->config;
	sys_set_bit(cfg->base + SC_DSTRX3_CS_OFFSET, SC_DSTRX3_DLIF_EN_BIT);
}

void sc_dstrx3_disable_downlink(const struct device *dev)
{
	const struct sc_dstrx3_cfg *cfg = dev->config;
	sys_clear_bit(cfg->base + SC_DSTRX3_CS_OFFSET, SC_DSTRX3_DLIF_EN_BIT);
}

void sc_dstrx3_enable_uplink(const struct device *dev)
{
	const struct sc_dstrx3_cfg *cfg = dev->config;
	sys_set_bit(cfg->base + SC_DSTRX3_CS_OFFSET, SC_DSTRX3_ULIF_EN_BIT);
}

void sc_dstrx3_disable_uplink(const struct device *dev)
{
	const struct sc_dstrx3_cfg *cfg = dev->config;
	sys_clear_bit(cfg->base + SC_DSTRX3_CS_OFFSET, SC_DSTRX3_ULIF_EN_BIT);
}

void sc_dstrx3_set_downlink_control(const struct device *dev, uint32_t control)
{
	const struct sc_dstrx3_cfg *cfg = dev->config;
	sys_write32(control, cfg->base + SC_DSTRX3_DLDC_OFFSET);
}

int sc_dstrx3_downlink_data(const struct device *dev, const uint8_t *data, uint16_t size)
{
	int ret;
	uint32_t ctrl;
	const struct sc_dstrx3_cfg *cfg = dev->config;

	if (size > SC_DSTRX3_MAX_DOWNLINK_BUFFER_SIZE) {
		LOG_ERR("Invalid data size. Downlink buffer of DSTRX-3 must be 256 bytes or less.");
		ret = -EMSGSIZE;
		goto end;
	}

	for (mem_addr_t offset = 0; offset < size; offset++) {
		LOG_DBG("Write 0x%02x to DLB", data[offset]);
		sys_write8(data[offset], cfg->base + SC_DSTRX3_DLB_OFFSET + offset);
	}

	ctrl = sys_read32(cfg->base + SC_DSTRX3_DLBCS_OFFSET);
	LOG_DBG("DLBCS Reg: 0x%08x", ctrl);
	ctrl |= SC_DSTRX_DLBCS_DLEN(size);
	ctrl |= SC_DSTRX_DLBCS_INT;
	LOG_DBG("DLBCS Reg: 0x%08x", ctrl);
	ctrl |= SC_DSTRX_DLBCS_WCOMP;
	LOG_DBG("DLBCS Reg: 0x%08x", ctrl);

	sys_write32(ctrl, cfg->base + SC_DSTRX3_DLBCS_OFFSET);

end:
	return ret;
}

void sc_dstrx3_set_uplink_control(const struct device *dev, uint32_t control)
{
	const struct sc_dstrx3_cfg *cfg = dev->config;
	sys_write32(control, cfg->base + SC_DSTRX3_ULDC_OFFSET);
}

void sc_dstrx3_get_uplink_status(const struct device *dev, uint8_t *count, uint8_t *status)
{
	const struct sc_dstrx3_cfg *cfg = dev->config;
	uint32_t val;

	val = sys_read32(cfg->base + SC_DSTRX3_ULBCS_OFFSET);
	*count = SC_DSTRX_ULB_COUNT(val);
	*status = SC_DSTRX_ULB_STATUS(val);
}

bool sc_dstrx3_is_uplink_crc_error(const struct device *dev, uint8_t status)
{
	return status & SC_DSTRX_ULB_CRC_ERR ? true : false;
}

bool sc_dstrx3_is_uplink_rs_error(const struct device *dev, uint8_t status)
{
	return status & SC_DSTRX_ULB_RS_ERR ? true : false;
}

bool sc_dstrx3_is_uplink_dlen_error(const struct device *dev, uint8_t status)
{
	return status & SC_DSTRX_ULB_DLEN_ERR ? true : false;
}

int sc_dstrx3_get_uplink_data(const struct device *dev, uint8_t *data, uint16_t *size)
{
	const struct sc_dstrx3_cfg *cfg = dev->config;
	int ret = 0;

	*size = SC_DSTRX_ULB_DLEN(sys_read32(cfg->base + SC_DSTRX3_ULBCS_OFFSET));
	if (*size > SC_DSTRX3_MAX_UPLINK_BUFFER_SIZE) {
		LOG_ERR("Invalid data size. Uplink buffer of DSTRX-3 must be 256 bytes or less.");
		ret = -EMSGSIZE;
		goto end;
	}

	for (mem_addr_t offset = 0; offset < *size; offset++) {
		LOG_DBG("Read 0x%02x from DLB", data[offset]);
		data[offset] = sys_read8(cfg->base + SC_DSTRX3_ULB_OFFSET + offset);
	}

end:
	return ret;
}

void sc_dstrx3_clear_uplink_buffer(const struct device *dev)
{
	const struct sc_dstrx3_cfg *cfg = dev->config;
	sys_set_bits(cfg->base + SC_DSTRX3_ULBCS_OFFSET, SC_DSTRX_ULB_CLEAR);
}

void sc_dstrx3_discard_uplink_data(const struct device *dev)
{
	const struct sc_dstrx3_cfg *cfg = dev->config;
	sys_set_bits(cfg->base + SC_DSTRX3_ULBCS_OFFSET, SC_DSTRX_ULB_CHANGE);
}

#define SC_DSTRX3_INIT(n)                                                                          \
	static void sc_dstrx3_##n##_irq_init(const struct device *dev);                            \
	static const struct sc_dstrx3_cfg sc_dstrx3_cfg_##n = {                                    \
		.base = DT_INST_REG_ADDR(n),                                                       \
		.irq_init = sc_dstrx3_##n##_irq_init,                                              \
		.tx_power = DT_INST_PROP(n, tx_power),                                             \
		.bit_rate = DT_INST_PROP(n, bit_rate),                                             \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, sc_dstrx3_init, NULL, NULL, &sc_dstrx3_cfg_##n, POST_KERNEL,      \
			      CONFIG_SC_DSTRX3_INIT_PRIORITY, NULL);                               \
	static void sc_dstrx3_##n##_irq_init(const struct device *dev)                             \
	{                                                                                          \
		IRQ_CONNECT(DT_INST_IRQN(n), 0, sc_dstrx3_isr, DEVICE_DT_INST_GET(n), 0);          \
		irq_enable(DT_INST_IRQN(n));                                                       \
	}

DT_INST_FOREACH_STATUS_OKAY(SC_DSTRX3_INIT)
