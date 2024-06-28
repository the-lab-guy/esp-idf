/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include "sdkconfig.h"
#include "unity.h"
#include "driver/isp.h"
#include "soc/soc_caps.h"

TEST_CASE("ISP processor exhausted allocation", "[isp]")
{
    esp_isp_processor_cfg_t isp_config = {
        .clk_hz = 80 * 1000 * 1000,
        .input_data_source = ISP_INPUT_DATA_SOURCE_CSI,
        .input_data_color_type = ISP_COLOR_RAW8,
        .output_data_color_type = ISP_COLOR_RGB565,
    };
    isp_proc_handle_t isp_proc[SOC_ISP_NUMS + 1] = {};

    for (int i = 0; i < SOC_ISP_NUMS; i++) {
        TEST_ESP_OK(esp_isp_new_processor(&isp_config, &isp_proc[i]));
    }

    TEST_ASSERT(esp_isp_new_processor(&isp_config, &isp_proc[SOC_ISP_NUMS]) == ESP_ERR_NOT_FOUND);

    for (int i = 0; i < SOC_ISP_NUMS; i++) {
        TEST_ESP_OK(esp_isp_del_processor(isp_proc[i]));
    }
}

TEST_CASE("ISP AF controller exhausted allocation", "[isp]")
{
    esp_isp_processor_cfg_t isp_config = {
        .clk_hz = 80 * 1000 * 1000,
        .input_data_source = ISP_INPUT_DATA_SOURCE_CSI,
        .input_data_color_type = ISP_COLOR_RAW8,
        .output_data_color_type = ISP_COLOR_RGB565,
    };
    isp_proc_handle_t isp_proc = NULL;
    TEST_ESP_OK(esp_isp_new_processor(&isp_config, &isp_proc));

    esp_isp_af_config_t af_config = {
        .edge_thresh = 128,
    };
    isp_af_ctlr_t af_ctrlr[SOC_ISP_AF_CTLR_NUMS + 1] = {};
    for (int i = 0; i < SOC_ISP_AF_CTLR_NUMS; i++) {
        TEST_ESP_OK(esp_isp_new_af_controller(isp_proc, &af_config, &af_ctrlr[i]));
    }

    TEST_ASSERT(esp_isp_new_af_controller(isp_proc, &af_config, &af_ctrlr[SOC_ISP_AF_CTLR_NUMS]) == ESP_ERR_NOT_FOUND);

    for (int i = 0; i < SOC_ISP_AF_CTLR_NUMS; i++) {
        TEST_ESP_OK(esp_isp_del_af_controller(af_ctrlr[i]));
    }
    TEST_ESP_OK(esp_isp_del_processor(isp_proc));
}

static bool test_isp_awb_default_on_statistics_done_cb(isp_awb_ctlr_t awb_ctlr, const esp_isp_awb_evt_data_t *edata, void *user_data)
{
    (void) awb_ctlr;
    (void) edata;
    (void) user_data;
    // Do nothing
    return false;
}

TEST_CASE("ISP AWB driver basic function", "[isp]")
{
    esp_isp_processor_cfg_t isp_config = {
        .clk_hz = 80 * 1000 * 1000,
        .input_data_source = ISP_INPUT_DATA_SOURCE_CSI,
        .input_data_color_type = ISP_COLOR_RAW8,
        .output_data_color_type = ISP_COLOR_RGB565,
    };
    isp_proc_handle_t isp_proc = NULL;
    TEST_ESP_OK(esp_isp_new_processor(&isp_config, &isp_proc));
    TEST_ESP_OK(esp_isp_enable(isp_proc));

    isp_awb_ctlr_t awb_ctlr = NULL;
    uint32_t image_width = 800;
    uint32_t image_height = 600;
    /* Default parameters from helper macro */
    esp_isp_awb_config_t awb_config = {
        .sample_point = ISP_AWB_SAMPLE_POINT_AFTER_CCM,
        .window = {
            .top_left = {.x = image_width * 0.2, .y = image_height * 0.2},
            .btm_right = {.x = image_width * 0.8, .y = image_height * 0.8},
        },
        .white_patch = {
            .luminance = {.min = 0, .max = 220 * 3},
            .red_green_ratio = {.min = 0.0f, .max = 3.999f},
            .blue_green_ratio = {.min = 0.0f, .max = 3.999f},
        },
    };
    isp_awb_stat_result_t stat_res = {};
    /* Create the awb controller */
    TEST_ESP_OK(esp_isp_new_awb_controller(isp_proc, &awb_config, &awb_ctlr));
    /* Register AWB callback */
    esp_isp_awb_cbs_t awb_cb = {
        .on_statistics_done = test_isp_awb_default_on_statistics_done_cb,
    };
    TEST_ESP_OK(esp_isp_awb_register_event_callbacks(awb_ctlr, &awb_cb, NULL));
    /* Enabled the awb controller */
    TEST_ESP_OK(esp_isp_awb_controller_enable(awb_ctlr));
    /* Start continuous AWB statistics */
    TEST_ESP_OK(esp_isp_awb_controller_start_continuous_statistics(awb_ctlr));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, esp_isp_awb_controller_get_oneshot_statistics(awb_ctlr, 0, &stat_res));
    /* Stop continuous AWB statistics */
    TEST_ESP_OK(esp_isp_awb_controller_stop_continuous_statistics(awb_ctlr));
    TEST_ESP_ERR(ESP_ERR_TIMEOUT, esp_isp_awb_controller_get_oneshot_statistics(awb_ctlr, 1, &stat_res));
    /* Disable the awb controller */
    TEST_ESP_OK(esp_isp_awb_controller_disable(awb_ctlr));
    /* Delete the awb controller and free the resources */
    TEST_ESP_OK(esp_isp_del_awb_controller(awb_ctlr));

    TEST_ESP_OK(esp_isp_disable(isp_proc));
    TEST_ESP_OK(esp_isp_del_processor(isp_proc));
}

TEST_CASE("ISP CCM basic function", "[isp]")
{
    esp_isp_processor_cfg_t isp_config = {
        .clk_hz = 80 * 1000 * 1000,
        .input_data_source = ISP_INPUT_DATA_SOURCE_CSI,
        .input_data_color_type = ISP_COLOR_RAW8,
        .output_data_color_type = ISP_COLOR_RGB565,
    };
    isp_proc_handle_t isp_proc = NULL;
    TEST_ESP_OK(esp_isp_new_processor(&isp_config, &isp_proc));
    TEST_ESP_OK(esp_isp_enable(isp_proc));

    esp_isp_ccm_config_t ccm_cfg = {
        .matrix = {
            {5.0, 0.0, 0.0},
            {0.0, 1.0, 0.0},
            {0.0, 0.0, 1.0}
        },
        .saturation = false,
    };
    // Out of range case
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, esp_isp_ccm_configure(isp_proc, &ccm_cfg));
    // saturation case
    ccm_cfg.saturation = true;
    TEST_ESP_OK(esp_isp_ccm_configure(isp_proc, &ccm_cfg));
    TEST_ESP_OK(esp_isp_ccm_enable(isp_proc));
    // Allow to be called after enabled
    ccm_cfg.matrix[0][0] = -1.1;
    TEST_ESP_OK(esp_isp_ccm_configure(isp_proc, &ccm_cfg));
    TEST_ESP_OK(esp_isp_ccm_disable(isp_proc));

    TEST_ESP_OK(esp_isp_disable(isp_proc));
    TEST_ESP_OK(esp_isp_del_processor(isp_proc));
}
