/*
 * Copyright (c) 2023 Space Cubics, LLC.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell_uart.h>
#include "common.h"
#include "csp.h"
#include "wdog.h"
#include "temp_test.h"
#include "cv_test.h"
#include "imu_test.h"
#include "gnss_test.h"
#include "rw_test.h"
#include "adcs_init.h"
#include "loop_test.h"
#include "syshk_test.h"
#include "data_nor.h"
#include "sc_fpgaconf.h"
#include "sc_fpgasys.h"
#include "sc_fpgamon.h"
#include "version.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(adcs_main, CONFIG_SCSAT1_ADCS_LOG_LEVEL);

#define CMD_HANDLER_PRIO (0U)
#define CMD_EXEC_EVENT   (1U)
#define BOOT_COUNT_FILE  CONFIG_SC_LIB_FLASH_DATA_STORE_MNT_POINT "/boot.count"

char last_cmd[32];

K_THREAD_STACK_DEFINE(cmd_thread_stack, 4096);
static struct k_thread cmd_thread;
static struct k_event exec_event;
extern struct k_event loop_event;

static void sc_adcs_print_fpga_ids(void)
{
	LOG_INF("* FSW Version       : %s", ADCS_HWTEST_VERSION);
	LOG_INF("* Boot CFG Memory   : %d", sc_get_boot_cfgmem());
	LOG_INF("* FPGA Boot Status  : 0x%x", sc_fpgaconf_get_bootsts());
	LOG_INF("* IP Version        : %08x", sc_get_fpga_ip_ver());
	LOG_INF("* Build Information : %08x", sc_get_fpga_build_hash());
	LOG_INF("* Device DNA 1      : %08x", sc_get_fpga_dna_1());
	LOG_INF("* Device DNA 2      : %08x",sc_get_fpga_dna_2());
}

static void cmd_handler(void *p1, void *p2, void *p3)
{
	int ret = 0;
	uint32_t err_cnt = 0;
	char *cmd = (char *)p1;
	char *arg = (char *)p2;
	struct adcs_temp_test_result temp_ret;
	struct adcs_cv_test_result cv_ret;
	struct imu_test_result imu_ret;
	struct gnss_hwmon_result hwmon_ret;
	struct gnss_bestpos_result bestpos_ret;

	k_event_set(&exec_event, CMD_EXEC_EVENT);

	ARG_UNUSED(p3);

	if (strcmp(cmd, "info") == 0) {
		sc_adcs_print_fpga_ids();
	} else if (strcmp(cmd, "init") == 0) {
		ret = adcs_init(&err_cnt);
	} else if (strcmp(cmd, "off") == 0) {
		ret = adcs_off(&err_cnt);
	} else if (strcmp(cmd, "temp") == 0) {
		ret = temp_test(&temp_ret, &err_cnt, LOG_ENABLE);
	} else if (strcmp(cmd, "cv") == 0) {
		ret = cv_test(&cv_ret, &err_cnt, LOG_ENABLE);
	} else if (strcmp(cmd, "imu") == 0) {
		ret = imu_test(&imu_ret, &err_cnt, LOG_ENABLE);
	} else if (strcmp(cmd, "gnss") == 0) {
		ret = gnss_test(&hwmon_ret, &bestpos_ret, &err_cnt, LOG_ENABLE);
	} else if (strcmp(cmd, "rw") == 0) {
		ret = rw_test(&err_cnt);
	} else if (strcmp(cmd, "loop") == 0) {
		k_event_set(&loop_event, LOOP_START_EVENT);
		ret = loop_test(atoi(arg), &err_cnt);
		k_event_clear(&loop_event, LOOP_STOP_EVENT);
	} else if (strcmp(cmd, "syshk") == 0) {
		k_event_set(&loop_event, LOOP_START_EVENT);
		ret = syshk_test(atoi(arg), &err_cnt);
		k_event_clear(&loop_event, LOOP_STOP_EVENT);
	} else {
		goto end;
	}

	if (ret < 0) {
		LOG_ERR("%s test Failed. eror count: %d", cmd, err_cnt);
	} else {
		LOG_INF("%s test successed.", cmd);
	}
end:
	k_event_clear(&exec_event, CMD_EXEC_EVENT);

	return;
}

static int start_cmd_thread(const struct shell *sh, size_t argc, char **argv)
{
	int ret = 0;
	k_tid_t cmd_tid;

	strncpy(last_cmd, sh->ctx->temp_buff, MIN(sh->ctx->cmd_tmp_buff_len, sizeof(last_cmd) - 1));

	if (argc < 1) {
		shell_error(sh, "Invalid argument");
		ret = -EINVAL;
		goto end;
	}

	if (k_event_wait(&exec_event, CMD_EXEC_EVENT, false, K_NO_WAIT) != 0) {
		shell_error(sh, "Now another command is executing");
		ret = -EBUSY;
		goto end;
	}

	cmd_tid = k_thread_create(&cmd_thread, cmd_thread_stack,
				  K_THREAD_STACK_SIZEOF(cmd_thread_stack),
				  (k_thread_entry_t)cmd_handler, argv[0], argv[1], NULL,
				  CMD_HANDLER_PRIO, 0, K_NO_WAIT);
	if (!cmd_tid) {
		shell_error(sh, "Failed to create command thread");
		ret = ENOSPC;
	}

end:
	return ret;
}

static int stop_cmd(const struct shell *sh, size_t argc, char **argv)
{
	strncpy(last_cmd, sh->ctx->temp_buff, MIN(sh->ctx->cmd_tmp_buff_len, sizeof(last_cmd) - 1));
	k_event_set(&loop_event, LOOP_STOP_EVENT);
	return 0;
}

int main(void)
{
	int ret;

	printk("This is for HW test program for %s\n", CONFIG_BOARD);

	datafs_init();

	start_kick_wdt_thread();

	sc_adcs_print_fpga_ids();

	k_event_init(&exec_event);

	update_boot_count(BOOT_COUNT_FILE);

	ret = csp_enable();
	if (ret < 0) {
		LOG_ERR("Failed to enable the CSP. (%d)", ret);
	} else {
		LOG_INF("Enable the CSP");
	}

	shell_execute_cmd(shell_backend_uart_get_ptr(), "hwtest init");

	if (IS_ENABLED(CONFIG_SCSAT1_ADCS_AUTO_RUN_HWTEST)) {
		k_sleep(K_SECONDS(30));

		shell_execute_cmd(shell_backend_uart_get_ptr(), "hwtest syshk -1");
	}

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_hwtest, SHELL_CMD(info, NULL, "ADCS Board Information", start_cmd_thread),
	SHELL_CMD(init, NULL, "ADCS Board Initialization", start_cmd_thread),
	SHELL_CMD(off, NULL, "Turn off the each power and CAN", start_cmd_thread),
	SHELL_CMD(temp, NULL, "Temperature test command", start_cmd_thread),
	SHELL_CMD(cv, NULL, "Current/Voltage test command", start_cmd_thread),
	SHELL_CMD(imu, NULL, "IMU test command", start_cmd_thread),
	SHELL_CMD(gnss, NULL, "GNSS test command", start_cmd_thread),
	SHELL_CMD(rw, NULL, "Reaction Wheel test command", start_cmd_thread),
	SHELL_CMD(loop, NULL, "Loop test command", start_cmd_thread),
	SHELL_CMD(syshk, NULL, "System HK test command", start_cmd_thread), SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(hwtest, &sub_hwtest, "SC-Sat1 HW test commands", NULL);
SHELL_CMD_REGISTER(stop, NULL, "SC-Sat1 HW test stop", stop_cmd);
