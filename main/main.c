#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include "nvs.h"
#include "nvs_flash.h"

#include "esp_bt.h"// configures the BT controoller and VHCI from the host side
#include "esp_gap_ble_api.h" //implements the GAP confiugartion, such as advertising and connection parameters
#include "esp_gattc_api.h" //implemets the CATT Client configuration, such as connectin to peripherals and searching for services
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt.h"
#include "esp_sleep.h"
#include "driver/gpio.h"

#define GATTC_TAG "BLE DEMO"
#define REMOTE_SERVICE_UUID        0x180F //Batter Service
#define REMOTE_NOTIFY_CHAR_UUID    0x2A19 	//Battery Level
#define PROFILE_NUM      1
#define PROFILE_A_APP_ID 0
#define INVALID_HANDLE   0
#define RMT_TX_CHANNEL RMT_CHANNEL_0
#define RMT_TX_GPIO_NUM (4)

#define GPIO_INPUT_IO_0     2
#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_IO_0))

static bool connect = false;
static bool get_server = false;
static esp_gattc_char_elem_t *char_elem_result = NULL;
static esp_gattc_descr_elem_t *descr_elem_result = NULL;
static const char *TAG = "example";
static const rmt_item32_t morse_esp[];




//MetaWearC MAC Adress EB:76:BC:F0:C2:6F
uint8_t remote_adress[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
uint8_t device_adress[] = { 0xEB, 0x76, 0xBC, 0xF0, 0xC2, 0x6F };

uint8_t eq_adr = 1;
int8_t rssi_received = 0;
uint8_t distanceMeter = 0;
int8_t txPower = -78;	//dBm set in the Metawear App
uint8_t envFactor = 2; //Range 2-4, set to 2 for freespace
esp_bd_addr_t bd;
uint8_t tens = 0;
uint8_t single = 0;


/* Declare static functions */
static void esp_gap_cb(esp_gap_ble_cb_event_t event,
		esp_ble_gap_cb_param_t *param);
static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
		esp_ble_gattc_cb_param_t *param);
static void gattc_profile_event_handler(esp_gattc_cb_event_t event,
		esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);

static void end();

static esp_bt_uuid_t remote_filter_service_uuid = { .len = ESP_UUID_LEN_16,
		.uuid = { .uuid16 = REMOTE_SERVICE_UUID, }, };

static esp_bt_uuid_t remote_filter_char_uuid = { .len = ESP_UUID_LEN_16, .uuid =
		{ .uuid16 = REMOTE_NOTIFY_CHAR_UUID, }, };

static esp_bt_uuid_t notify_descr_uuid = { .len = ESP_UUID_LEN_16, .uuid = {
		.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG, }, };

static double calcRssiToMeter(uint8_t distanceMeter, int8_t rssi_received, int8_t txPower,  uint8_t envFactor);

static void end(){
	//esp_bluedroid_disable();
	//esp_bt_controller_disable();
	esp_light_sleep_start();
}



struct gattc_profile_inst {
	esp_gattc_cb_t gattc_cb;
	uint16_t gattc_if;
	uint16_t app_id;
	uint16_t conn_id;
	uint16_t service_start_handle;
	uint16_t service_end_handle;
	uint16_t char_handle;
	esp_bd_addr_t remote_bda;
};

//static const rmt_item32_t tone_tens[] = {
//
//
//    // Single
//    {{{ 32767, 1, 32767, 0 }}}, // dot
//    {{{ 32767, 1, 32767, 0 }}}, // dot
//    {{{ 32767, 1, 32767, 0 }}}, // dot
//    {{{ 32767, 0, 32767, 0 }}}, // SPACE
//    // P : dot, dash, dash, dot
//    {{{ 32767, 1, 32767, 0 }}}, // dot
//    {{{ 32767, 1, 32767, 1 }}},
//    {{{ 32767, 1, 32767, 0 }}}, // dash
//    {{{ 32767, 1, 32767, 1 }}},
//    {{{ 32767, 1, 32767, 0 }}}, // dash
//    {{{ 32767, 1, 32767, 0 }}}, // dot
//    // RMT end marker
//    {{{ 0, 1, 0, 0 }}}
//};

static const rmt_item32_t tone_single[] = {
    // Single
    {{{ 32767, 1, 32767, 0 }}}, // dot

};

static const rmt_item32_t tone_zero[] = {
	//Zeros one long tone
    {{{ 32767, 1, 32767, 1 }}},
    {{{ 32767, 1, 32767, 0 }}}, // dash
	 {{{ 32767, 0, 32767, 0 }}}, // SPACE
    // RMT end marker
    {{{ 0, 1, 0, 0 }}}
};




static double calcRssiToMeter(uint8_t distanceMeter, int8_t rssi_received, int8_t txPower,  uint8_t envFactor ){
	distanceMeter= pow(10, ((double) txPower - rssi_received) / (10 * envFactor));
	return distanceMeter;
}

static void rmt_tx_init(void)
{
   rmt_config_t config = RMT_DEFAULT_CONFIG_TX(RMT_TX_GPIO_NUM, RMT_TX_CHANNEL);
   // enable the carrier to be able to hear the Morse sound
   // if the RMT_TX_GPIO is connected to a speaker
   config.tx_config.carrier_en = true;
   config.tx_config.carrier_duty_percent = 50;
   // set audible career frequency of 611 Hz
   // actually 611 Hz is the minimum, that can be set
   // with current implementation of the RMT API
   config.tx_config.carrier_freq_hz = 611;
   // set the maximum clock divider to be able to output
   // RMT pulses in range of about one hundred milliseconds
   config.clk_div = 255;

   ESP_ERROR_CHECK(rmt_config(&config));
   ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));
}

static esp_ble_scan_params_t ble_scan_params = { .scan_type =
		BLE_SCAN_TYPE_ACTIVE, .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
		.scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL, .scan_interval = 0x50,
		.scan_window = 0x30, .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE };

/* One gatt-based profile one app_id and one gattc_if, this array will store the gattc_if returned by ESP_GATTS_REG_EVT */
static struct gattc_profile_inst gl_profile_tab[PROFILE_NUM] = {
		[PROFILE_A_APP_ID] = { .gattc_cb = gattc_profile_event_handler,
				.gattc_if = ESP_GATT_IF_NONE, /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
		}, };

static void gattc_profile_event_handler(esp_gattc_cb_event_t event,
		esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
	esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t*) param;

	switch (event) {
	case ESP_GATTC_REG_EVT:
		ESP_LOGI(GATTC_TAG, "REG_EVT");
		esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
		if (scan_ret) {
			ESP_LOGE(GATTC_TAG, "set scan params error, error code = %x",
					scan_ret);
		}
		break;
	case ESP_GATTC_CONNECT_EVT: {
		ESP_LOGI(GATTC_TAG, "ESP_GATTC_CONNECT_EVT conn_id %d, if %d",
				p_data->connect.conn_id, gattc_if);
		gl_profile_tab[PROFILE_A_APP_ID].conn_id = p_data->connect.conn_id;
		memcpy(gl_profile_tab[PROFILE_A_APP_ID].remote_bda,
				p_data->connect.remote_bda, sizeof(esp_bd_addr_t));
		ESP_LOGI(GATTC_TAG, "REMOTE BDA:");
		esp_log_buffer_hex(GATTC_TAG, gl_profile_tab[PROFILE_A_APP_ID].remote_bda, sizeof(esp_bd_addr_t));
		esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req(gattc_if,
				p_data->connect.conn_id);
		if (mtu_ret) {
			ESP_LOGE(GATTC_TAG, "config MTU error, error code = %x", mtu_ret);
		}
		break;
	}
	case ESP_GATTC_OPEN_EVT:
		if (param->open.status != ESP_GATT_OK) {
			ESP_LOGE(GATTC_TAG, "open failed, status %d", p_data->open.status);
			break;
		}
		ESP_LOGI(GATTC_TAG, "open success");
		break;
	case ESP_GATTC_DIS_SRVC_CMPL_EVT:
		if (param->dis_srvc_cmpl.status != ESP_GATT_OK) {
			ESP_LOGE(GATTC_TAG, "discover service failed, status %d",
					param->dis_srvc_cmpl.status);
			break;
		}
		ESP_LOGI(GATTC_TAG, "discover service complete conn_id %d",
				param->dis_srvc_cmpl.conn_id);
		esp_ble_gattc_search_service(gattc_if, param->cfg_mtu.conn_id,
				&remote_filter_service_uuid);
		break;
	case ESP_GATTC_CFG_MTU_EVT:
		if (param->cfg_mtu.status != ESP_GATT_OK) {
			ESP_LOGE(GATTC_TAG, "config mtu failed, error status = %x",
					param->cfg_mtu.status);
		}
		ESP_LOGI(GATTC_TAG,
				"ESP_GATTC_CFG_MTU_EVT, Status %d, MTU %d, conn_id %d",
				param->cfg_mtu.status, param->cfg_mtu.mtu,
				param->cfg_mtu.conn_id);
		break;
	case ESP_GATTC_SEARCH_RES_EVT: {
		ESP_LOGI(GATTC_TAG, "SEARCH RES: conn_id = %x is primary service %d",
				p_data->search_res.conn_id, p_data->search_res.is_primary);
		ESP_LOGI(GATTC_TAG,
				"start handle %d end handle %d current handle value %d",
				p_data->search_res.start_handle, p_data->search_res.end_handle,
				p_data->search_res.srvc_id.inst_id);
		if (p_data->search_res.srvc_id.uuid.len == ESP_UUID_LEN_16
				&& p_data->search_res.srvc_id.uuid.uuid.uuid16
						== REMOTE_SERVICE_UUID) {
			ESP_LOGI(GATTC_TAG, "service found");
			get_server = true;
			gl_profile_tab[PROFILE_A_APP_ID].service_start_handle =
					p_data->search_res.start_handle;
			gl_profile_tab[PROFILE_A_APP_ID].service_end_handle =
					p_data->search_res.end_handle;
			ESP_LOGI(GATTC_TAG, "UUID16: %x",
					p_data->search_res.srvc_id.uuid.uuid.uuid16);
		}
		break;
	}
	case ESP_GATTC_SEARCH_CMPL_EVT:
		if (p_data->search_cmpl.status != ESP_GATT_OK) {
			ESP_LOGE(GATTC_TAG, "search service failed, error status = %x",
					p_data->search_cmpl.status);
			break;
		}
		if (p_data->search_cmpl.searched_service_source
				== ESP_GATT_SERVICE_FROM_REMOTE_DEVICE) {
			ESP_LOGI(GATTC_TAG, "Get service information from remote device");
		} else if (p_data->search_cmpl.searched_service_source
				== ESP_GATT_SERVICE_FROM_NVS_FLASH) {
			ESP_LOGI(GATTC_TAG, "Get service information from flash");
		} else {
			ESP_LOGI(GATTC_TAG, "unknown service source");
		}
		ESP_LOGI(GATTC_TAG, "ESP_GATTC_SEARCH_CMPL_EVT");
		if (get_server) {
			uint16_t count = 0;
			esp_gatt_status_t status = esp_ble_gattc_get_attr_count(gattc_if,
					p_data->search_cmpl.conn_id, ESP_GATT_DB_CHARACTERISTIC,
					gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
					gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
					INVALID_HANDLE, &count);
			if (status != ESP_GATT_OK) {
				ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_attr_count error");
			}

			if (count > 0) {
				char_elem_result = (esp_gattc_char_elem_t*) malloc(
						sizeof(esp_gattc_char_elem_t) * count);
				if (!char_elem_result) {
					ESP_LOGE(GATTC_TAG, "gattc no mem");
				} else {
					status =
							esp_ble_gattc_get_char_by_uuid(gattc_if,
									p_data->search_cmpl.conn_id,
									gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
									gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
									remote_filter_char_uuid, char_elem_result,
									&count);
					if (status != ESP_GATT_OK) {
						ESP_LOGE(GATTC_TAG,
								"esp_ble_gattc_get_char_by_uuid error");
					}

					/*  Every service have only one char in our 'ESP_GATTS_DEMO' demo, so we used first 'char_elem_result' */
					if (count > 0
							&& (char_elem_result[0].properties
									& ESP_GATT_CHAR_PROP_BIT_NOTIFY)) {
						gl_profile_tab[PROFILE_A_APP_ID].char_handle =
								char_elem_result[0].char_handle;
						esp_ble_gattc_register_for_notify(gattc_if,
								gl_profile_tab[PROFILE_A_APP_ID].remote_bda,
								char_elem_result[0].char_handle);
					}
				}
				/* free char_elem_result */
				free(char_elem_result);
			} else {
				ESP_LOGE(GATTC_TAG, "no char found");
			}
		}
		break;
	case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
		ESP_LOGI(GATTC_TAG, "ESP_GATTC_REG_FOR_NOTIFY_EVT");
		if (p_data->reg_for_notify.status != ESP_GATT_OK) {
			ESP_LOGE(GATTC_TAG, "REG FOR NOTIFY failed: error status = %d",
					p_data->reg_for_notify.status);
		} else {
			uint16_t count = 0;
			uint16_t notify_en = 1;
			esp_gatt_status_t ret_status = esp_ble_gattc_get_attr_count(
					gattc_if, gl_profile_tab[PROFILE_A_APP_ID].conn_id,
					ESP_GATT_DB_DESCRIPTOR,
					gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
					gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
					gl_profile_tab[PROFILE_A_APP_ID].char_handle, &count);
			if (ret_status != ESP_GATT_OK) {
				ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_attr_count error");
			}
			if (count > 0) {
				descr_elem_result = malloc(
						sizeof(esp_gattc_descr_elem_t) * count);
				if (!descr_elem_result) {
					ESP_LOGE(GATTC_TAG, "malloc error, gattc no mem");
				} else {
					ret_status = esp_ble_gattc_get_descr_by_char_handle(
							gattc_if, gl_profile_tab[PROFILE_A_APP_ID].conn_id,
							p_data->reg_for_notify.handle, notify_descr_uuid,
							descr_elem_result, &count);
					if (ret_status != ESP_GATT_OK) {
						ESP_LOGE(GATTC_TAG,
								"esp_ble_gattc_get_descr_by_char_handle error");
					}
					/* Every char has only one descriptor in our 'ESP_GATTS_DEMO' demo, so we used first 'descr_elem_result' */
					if (count
							> 0&& descr_elem_result[0].uuid.len == ESP_UUID_LEN_16 && descr_elem_result[0].uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG) {
						ret_status = esp_ble_gattc_write_char_descr(gattc_if,
								gl_profile_tab[PROFILE_A_APP_ID].conn_id,
								descr_elem_result[0].handle, sizeof(notify_en),
								(uint8_t*) &notify_en, ESP_GATT_WRITE_TYPE_RSP,
								ESP_GATT_AUTH_REQ_NONE);
					}

					if (ret_status != ESP_GATT_OK) {
						ESP_LOGE(GATTC_TAG,
								"esp_ble_gattc_write_char_descr error");
					}

					/* free descr_elem_result */
					free(descr_elem_result);
				}
			} else {
				ESP_LOGE(GATTC_TAG, "decsr not found");
			}

		}
		break;
	}
	case ESP_GATTC_NOTIFY_EVT:
		if (p_data->notify.is_notify) {
			ESP_LOGI(GATTC_TAG, "ESP_GATTC_NOTIFY_EVT, receive notify value:");
			end();
		} else {
			ESP_LOGI(GATTC_TAG,
					"ESP_GATTC_NOTIFY_EVT, receive indicate value:");
		}
		esp_log_buffer_hex(GATTC_TAG, p_data->notify.value, p_data->notify.value_len);
		break;
	case ESP_GATTC_WRITE_DESCR_EVT:
		if (p_data->write.status != ESP_GATT_OK) {
			ESP_LOGE(GATTC_TAG, "write descr failed, error status = %x",
					p_data->write.status);
			break;
		}
		ESP_LOGI(GATTC_TAG, "write descr success ");
		uint8_t write_char_data[35];
		for (int i = 0; i < sizeof(write_char_data); ++i) {
			write_char_data[i] = i % 256;
		}
		esp_ble_gattc_write_char(gattc_if,
				gl_profile_tab[PROFILE_A_APP_ID].conn_id,
				gl_profile_tab[PROFILE_A_APP_ID].char_handle,
				sizeof(write_char_data), write_char_data,
				ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
		break;
	case ESP_GATTC_SRVC_CHG_EVT: {
		esp_bd_addr_t bda;
		memcpy(bda, p_data->srvc_chg.remote_bda, sizeof(esp_bd_addr_t));
		ESP_LOGI(GATTC_TAG, "ESP_GATTC_SRVC_CHG_EVT, bd_addr:");
		esp_log_buffer_hex(GATTC_TAG, bda, sizeof(esp_bd_addr_t));
		break;
	}
	case ESP_GATTC_WRITE_CHAR_EVT:
		if (p_data->write.status != ESP_GATT_OK) {
			ESP_LOGE(GATTC_TAG, "write char failed, error status = %x",
					p_data->write.status);
			break;
		}
		ESP_LOGI(GATTC_TAG, "write char success ");
		break;
	case ESP_GATTC_DISCONNECT_EVT:

		memcpy(bd, p_data->srvc_chg.remote_bda, sizeof(esp_bd_addr_t));
		connect = false;
		get_server = false;
		ESP_LOGI(GATTC_TAG, "ESP_GATTC_DISCONNECT_EVT, reason = %d",
				p_data->disconnect.reason);
		esp_ble_gap_disconnect(bd);
		break;
	default:
		break;
	}
}

static void esp_gap_cb(esp_gap_ble_cb_event_t event,
		esp_ble_gap_cb_param_t *param) {

	switch (event) {
	case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
		//the unit of the duration is second
		uint32_t duration = 30;
		esp_ble_gap_start_scanning(duration);
		break;
	}
	case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
		//scan start complete event to indicate scan start successfully or failed
		if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
			ESP_LOGE(GATTC_TAG, "scan start failed, error status = %x",
					param->scan_start_cmpl.status);
			break;
		}
		ESP_LOGI(GATTC_TAG, "scan start success");

		break;
	case ESP_GAP_BLE_SCAN_RESULT_EVT: {
		esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t*) param;
		switch (scan_result->scan_rst.search_evt) {
		case ESP_GAP_SEARCH_INQ_RES_EVT:
			esp_log_buffer_hex(GATTC_TAG, scan_result->scan_rst.bda, 6);

			for (uint8_t i = 0; i < 6; i++) {
				remote_adress[i] = scan_result->scan_rst.bda[i];
			}

			ESP_LOGI(GATTC_TAG,
					"searched Adv Data Len %d, Scan Response Len %d",
					scan_result->scan_rst.adv_data_len,
					scan_result->scan_rst.scan_rsp_len);


#if CONFIG_EXAMPLE_DUMP_ADV_DATA_AND_SCAN_RESP
             if (scan_result->scan_rst.adv_data_len > 0) {
                 ESP_LOGI(GATTC_TAG, "adv data:");
                 esp_log_buffer_hex(GATTC_TAG, &scan_result->scan_rst.ble_adv[0], scan_result->scan_rst.adv_data_len);
             }
             if (scan_result->scan_rst.scan_rsp_len > 0) {
                 ESP_LOGI(GATTC_TAG, "scan resp:");
                 esp_log_buffer_hex(GATTC_TAG, &scan_result->scan_rst.ble_adv[scan_result->scan_rst.adv_data_len], scan_result->scan_rst.scan_rsp_len);
             }
 #endif
			ESP_LOGI(GATTC_TAG, "\n");

			if (remote_adress != NULL) {
				for (uint8_t i = 0; i < 6; i++) {
					if (remote_adress[i] == device_adress[i]) {
						eq_adr = 1;
						continue;
					} else {
						eq_adr = 0;
						break;
					}
				}
				if (eq_adr == 1) {
					ESP_LOGI(GATTC_TAG, "Device found! Success!");
					if (connect == false) {
						connect = true;
						ESP_LOGI(GATTC_TAG, "connect to the remote device.");
						rssi_received = scan_result->scan_rst.rssi;
						if (rssi_received != 0) {
							distanceMeter = calcRssiToMeter(distanceMeter,
									rssi_received, txPower, envFactor);

							single = distanceMeter % 10;
							tens = distanceMeter - single;

							ESP_LOGI(GATTC_TAG, "Meter: %d", distanceMeter);

							if (tens == 0) {
								ESP_ERROR_CHECK(
										rmt_write_items(RMT_TX_CHANNEL, tone_zero, sizeof(tone_zero) / sizeof(tone_zero[0]), true));
							} else {
								for (uint8_t i = 0; i < tens; i++) {
									ESP_ERROR_CHECK(
											rmt_write_items(RMT_TX_CHANNEL, tone_single, sizeof(tone_single) / sizeof(tone_single[0]), true));
								}
							}

							if (single == 0) {
								ESP_ERROR_CHECK(
										rmt_write_items(RMT_TX_CHANNEL, tone_zero, sizeof(tone_zero) / sizeof(tone_zero[0]), true));
							} else {
								for (uint8_t i = 0; i < single; i++) {
									ESP_ERROR_CHECK(
											rmt_write_items(RMT_TX_CHANNEL, tone_single, sizeof(tone_single) / sizeof(tone_single[0]), true));
								}
							}

							//	 ESP_ERROR_CHECK(rmt_write_items(RMT_TX_CHANNEL, morse_esp, sizeof(morse_esp) / sizeof(morse_esp[0]), true));
							// ESP_ERROR_CHECK(rmt_write_items(RMT_TX_CHANNEL, morse_esp, sizeof(morse_esp) / sizeof(morse_esp[0]), true));
							ESP_LOGI(TAG, "Transmission complete");
							vTaskDelay(1000 / portTICK_PERIOD_MS);
						}
						ESP_LOGI(GATTC_TAG, "RSSI: %d", rssi_received);
						esp_ble_gap_stop_scanning();
						esp_ble_gattc_open(
								gl_profile_tab[PROFILE_A_APP_ID].gattc_if,
								scan_result->scan_rst.bda,
								scan_result->scan_rst.ble_addr_type, true);
					}
				}
			}

			break;
		case ESP_GAP_SEARCH_INQ_CMPL_EVT:
			break;
		default:
			break;
		}
		break;
	}

	case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
		if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
			ESP_LOGE(GATTC_TAG, "scan stop failed, error status = %x",
					param->scan_stop_cmpl.status);
			break;
		}
		ESP_LOGI(GATTC_TAG, "stop scan successfully");
		break;

	case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
		if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
			ESP_LOGE(GATTC_TAG, "adv stop failed, error status = %x",
					param->adv_stop_cmpl.status);
			break;
		}
		ESP_LOGI(GATTC_TAG, "stop adv successfully");
		break;
	case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
		ESP_LOGI(GATTC_TAG,
				"update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
				param->update_conn_params.status,
				param->update_conn_params.min_int,
				param->update_conn_params.max_int,
				param->update_conn_params.conn_int,
				param->update_conn_params.latency,
				param->update_conn_params.timeout);
		break;
	default:
		break;
	}
}

static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
		esp_ble_gattc_cb_param_t *param) {
	/* If event is register event, store the gattc_if for each profile */
	if (event == ESP_GATTC_REG_EVT) {
		if (param->reg.status == ESP_GATT_OK) {
			gl_profile_tab[param->reg.app_id].gattc_if = gattc_if;
		} else {
			ESP_LOGI(GATTC_TAG, "reg app failed, app_id %04x, status %d",
					param->reg.app_id, param->reg.status);
			return;
		}
	}

	/* If the gattc_if equal to profile A, call profile A cb handler,
	 * so here call each profile's callback */
	do {
		int idx;
		for (idx = 0; idx < PROFILE_NUM; idx++) {
			if (gattc_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
			gattc_if == gl_profile_tab[idx].gattc_if) {
				if (gl_profile_tab[idx].gattc_cb) {
					gl_profile_tab[idx].gattc_cb(event, gattc_if, param);
				}
			}
		}
	} while (0);
}

//Programs entry point

void app_main() {

	  gpio_config_t io_conf = {};
	  io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
	  io_conf.pull_up_en = 1;
	  gpio_config(&io_conf);



	// Initialize Non-Volatile-Storage
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	//Initialize Bluetooth controller
	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT()
	;
	ret = esp_bt_controller_init(&bt_cfg);
	if (ret) {
		ESP_LOGE(GATTC_TAG,
				"%s initialize controller failed, error code = %x\n", __func__,
				ret);
		return;
	}

	ret = esp_bt_controller_enable(ESP_BT_MODE_BLE); //Enables BLE Mode
	if (ret) {
		ESP_LOGE(GATTC_TAG, "%s enable controller failed, error code = %x\n",
				__func__, ret);
		return;
	}

	ret = esp_bluedroid_init();
	if (ret) {
		ESP_LOGE(GATTC_TAG, "%s init bluetooth failed, error code = %x\n",
				__func__, ret);
		return;
	}

	ret = esp_bluedroid_enable();
	if (ret) {
		ESP_LOGE(GATTC_TAG, "%s enable bluetooth failed, error code = %x\n",
				__func__, ret);
		return;
	}


	//Inizialize RMT
	 ESP_LOGI(TAG, "Configuring transmitter");
	 rmt_tx_init();


	//register the  callback function to the gap module, takes care of scanning and connection to servers
	ret = esp_ble_gap_register_callback(esp_gap_cb);
	if (ret) {
		ESP_LOGE(GATTC_TAG, "%s gap register failed, error code = %x\n",
				__func__, ret);
		return;
	}

	//register the callback function to the gattc module, for services, writing and reading data
	ret = esp_ble_gattc_register_callback(esp_gattc_cb);
	if (ret) {
		ESP_LOGE(GATTC_TAG, "%s gattc register failed, error code = %x\n",
				__func__, ret);
		return;
	}

	ret = esp_ble_gattc_app_register(PROFILE_A_APP_ID);
	if (ret) {
		ESP_LOGE(GATTC_TAG, "%s gattc app register failed, error code = %x\n",
				__func__, ret);
	}

	esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
	if (local_mtu_ret) {
		ESP_LOGE(GATTC_TAG, "set local  MTU failed, error code = %x",
				local_mtu_ret);
	}


}


