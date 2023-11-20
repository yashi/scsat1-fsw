/*
 * Copyright (c) 2023 Space Cubics, LLC.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include "pwrctrl.h"
#include "temp_test.h"
#include "cv_test.h"
#include "csp_test.h"
#include "sunsens_test.h"
#include "mgnm_test.h"
#include "mtq.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(loop_test);

static void update_mtq_idx(uint8_t *axes_idx, uint8_t *pol_idx)
{
	if (*pol_idx < 2) {
		(*pol_idx)++;
	} else {
		*pol_idx = 0;
		(*axes_idx)++;
	}

	if (*axes_idx >= 3) {
		*axes_idx = 0;
	}
}

static int one_loop(uint32_t *err_cnt)
{
	int ret;
	int all_ret = 0;

	LOG_INF("===[Temp Test Start]===");
	ret = temp_test(err_cnt);
	if (ret < 0) {
		all_ret = -1;
	}

	k_sleep(K_MSEC(100));

	LOG_INF("===[CV Test Start]===");
	ret = cv_test(err_cnt);
	if (ret < 0) {
		all_ret = -1;
	}

	k_sleep(K_MSEC(100));

	LOG_INF("===[CSP Test Start]===");
	ret = csp_test(err_cnt);
	if (ret < 0) {
		all_ret = -1;
	}

	k_sleep(K_MSEC(100));

	LOG_INF("===[Sun Sensor Test Start]===");
	ret = sunsens_test(err_cnt);
	if (ret < 0) {
		all_ret = -1;
	}

	k_sleep(K_MSEC(100));

	LOG_INF("===[Magnetometer Test Start]===");
	ret = mgnm_test(err_cnt);
	if (ret < 0) {
		all_ret = -1;
	}

	k_sleep(K_MSEC(100));

	return all_ret;
}

int loop_test(int32_t loop_count, uint32_t *err_cnt)
{
	int ret;
	int all_ret = 0;
	float duty = 1.0;
	enum mtq_axes axes_list[] = {
		MTQ_AXES_MAIN_X,
		MTQ_AXES_MAIN_Y,
		MTQ_AXES_MAIN_Z,
	};
	enum mtq_polarity pol_list[] = {
		MTQ_POL_PLUS,
		MTQ_POL_MINUS,
		MTQ_POL_NON,
	};
	uint8_t axes_idx = 0;
	uint8_t pol_idx = 0;

	if (loop_count < 0) {
		loop_count = INT32_MAX;
	}

	for (int i=1; i<=loop_count; i++) {
		LOG_INF("===[Loop Test %d Start]===", i);

		LOG_INF("===[MTQ Start]===");
		ret = mtq_start(axes_list[axes_idx], pol_list[pol_idx], duty);
		if (ret < 0) {
			(*err_cnt)++;
			all_ret = -1;
		}

		for (int j=0; j<5; j++) {
			ret = one_loop(err_cnt);
			if (ret < 0) {
				all_ret = -1;
			}
		}

		LOG_INF("===[MTQ Sttop]===");
		ret = mtq_stop(axes_list[axes_idx]);
		if (ret < 0) {
			(*err_cnt)++;
			all_ret = -1;
		}

		update_mtq_idx(&axes_idx, &pol_idx);

		LOG_INF("===[Loop Test %d Finish (err_cnt: %d)]===",
				 i, *err_cnt);
	}

	return all_ret;
}