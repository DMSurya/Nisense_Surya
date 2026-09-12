/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Wi-Fi UI Screen Implementation
 *
 * LVGL-based UI for Wi-Fi status and management using Zephyr Wi-Fi management API.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>
#include <lvgl.h>
#include "resource_fonts.h"
#include <stdio.h>
#include <string.h>

#include "wifi_ui.h"
#include "ui_common.h"
#include "ui_theme.h"
#include "ui_shell.h"
#if defined(CONFIG_APP_FEATURE_BLE)
#include "ble_gatt.h"
#endif

LOG_MODULE_REGISTER(wifi_ui, CONFIG_WIFI_UI_LOG_LEVEL);

/* Device tree node for Wi-Fi */
#define WIFI_NODE DT_ALIAS(wifi0)

/* UI Style colors (mapped to NiSense brand status tokens) */
#define COLOR_CONNECTED    NS_COLOR_NORMAL    /* Connected / OK  */
#define COLOR_DISCONNECTED NS_COLOR_RISK      /* Disconnected    */
#define COLOR_SCANNING     NS_COLOR_SECONDARY /* Scanning / info */
#define COLOR_ERROR        NS_COLOR_CRITICAL  /* Error           */
#define COLOR_SIGNAL_GOOD  NS_COLOR_NORMAL    /* Strong signal   */
#define COLOR_SIGNAL_MED   NS_COLOR_WARNING   /* Medium signal   */
#define COLOR_SIGNAL_WEAK  NS_COLOR_RISK      /* Weak signal     */
#define COLOR_SIGNAL_BAD   NS_COLOR_CRITICAL  /* Bad signal      */

/* Max scan results to display */
#define MAX_DISPLAYED_SCAN_RESULTS 15

/* Screen state */
static struct {
	lv_obj_t *screen;
	
	/* Header section */
	lv_obj_t *title_label;
	lv_obj_t *status_label;
	
	/* Connected view widgets */
	lv_obj_t *connected_container;
	lv_obj_t *ssid_label;
	lv_obj_t *ip_label;
	lv_obj_t *mac_label;
	lv_obj_t *signal_bar;
	lv_obj_t *signal_label;
	lv_obj_t *channel_label;
	
	/* Disconnected view widgets */
	lv_obj_t *scan_container;
	lv_obj_t *scan_list;
	lv_obj_t *scan_btn;
	lv_obj_t *scan_status_label;
	
	/* Buttons */
	lv_obj_t *disconnect_btn;
	
	/* State */
	bool connected;
	bool scanning;
	
	/* Network interface */
	struct net_if *iface;
	
	/* Scan results storage */
	struct wifi_scan_result scan_results[MAX_DISPLAYED_SCAN_RESULTS];
	size_t scan_count;
	
	/* Event handler */
	struct net_mgmt_event_callback wifi_cb;
} wifi_ui_state;

/* Forward declarations */
static void update_connected_view(void);
static void update_disconnected_view(void);
static void scan_btn_event_cb(lv_event_t *e);
static void disconnect_btn_event_cb(lv_event_t *e);
static void ap_list_event_cb(lv_event_t *e);
static void wifi_event_handler(struct net_mgmt_event_callback *cb,
			       uint32_t mgmt_event, struct net_if *iface);

static void password_kb_event_cb(lv_event_t *e);
static void connect_to_ap(const char *ssid, const char *password, enum wifi_security_type sec);

static lv_obj_t *pwd_ta;
static lv_obj_t *pwd_kb;
static struct wifi_scan_result *selected_ap;

/* Get signal strength color */
static lv_color_t get_signal_color(int8_t rssi)
{
	if (rssi >= -50) return COLOR_SIGNAL_GOOD;
	if (rssi >= -65) return COLOR_SIGNAL_MED;
	if (rssi >= -75) return COLOR_SIGNAL_WEAK;
	return COLOR_SIGNAL_BAD;
}

/* Get signal strength percentage */
static uint8_t rssi_to_percent(int8_t rssi)
{
	if (rssi >= -50) return 100;
	if (rssi <= -100) return 0;
	return (uint8_t)(2 * (rssi + 100));
}

/* Get security type string */
static const char *security_to_str(enum wifi_security_type security)
{
	switch (security) {
	case WIFI_SECURITY_TYPE_NONE: return "Open";
	case WIFI_SECURITY_TYPE_WEP: return "WEP";
	case WIFI_SECURITY_TYPE_WPA_PSK: return "WPA";
	case WIFI_SECURITY_TYPE_PSK: return "WPA2";
	case WIFI_SECURITY_TYPE_PSK_SHA256: return "WPA2-SHA256";
	case WIFI_SECURITY_TYPE_SAE: return "WPA3";
	default: return "?";
	}
}

/* Wi-Fi event handler */
static void wifi_event_handler(struct net_mgmt_event_callback *cb,
			       uint32_t mgmt_event, struct net_if *iface)
{
	ARG_UNUSED(cb);
	
	if (wifi_ui_state.iface && iface != wifi_ui_state.iface) {
		return;
	}
	
	switch (mgmt_event) {
	case NET_EVENT_WIFI_SCAN_RESULT: {
		const struct wifi_scan_result *result = 
			(const struct wifi_scan_result *)cb->info;
		
		if (result && wifi_ui_state.scan_count < MAX_DISPLAYED_SCAN_RESULTS) {
			memcpy(&wifi_ui_state.scan_results[wifi_ui_state.scan_count],
			       result, sizeof(struct wifi_scan_result));
			wifi_ui_state.scan_count++;
			/* Omit per-result logging to reduce RTT overhead during burst */
		}
		break;
	}
	
	case NET_EVENT_WIFI_SCAN_DONE:
		wifi_ui_state.scanning = false;
		LOG_INF("Scan complete, found %zu APs", wifi_ui_state.scan_count);
		break;
	
	case NET_EVENT_WIFI_CONNECT_RESULT:
		wifi_ui_state.connected = true;
		LOG_INF("Wi-Fi connected");
#if defined(CONFIG_APP_FEATURE_BLE)
		{
			struct ble_wifi_status ws = {0};

			ws.connected = 2; /* 2 = connected */
			if (selected_ap) {
				ws.rssi_dbm = selected_ap->rssi;
				size_t slen = MIN(selected_ap->ssid_length,
						  sizeof(ws.ssid) - 1);
				memcpy(ws.ssid, selected_ap->ssid, slen);
			}
			(void)ble_notify_wifi_status(&ws);
		}
#endif
		break;

	case NET_EVENT_WIFI_DISCONNECT_RESULT:
		wifi_ui_state.connected = false;
		LOG_INF("Wi-Fi disconnected");
#if defined(CONFIG_APP_FEATURE_BLE)
		{
			struct ble_wifi_status ws = {0};

			ws.connected = 0; /* 0 = disconnected */
			(void)ble_notify_wifi_status(&ws);
		}
#endif
		break;

	case NET_EVENT_IPV4_ADDR_ADD:
		/* DHCP has assigned an IP — re-push wifi status with the IP filled in. */
		if (!wifi_ui_state.connected) {
			break;
		}
		LOG_INF("IPv4 address assigned");
#if defined(CONFIG_APP_FEATURE_BLE)
		{
			struct ble_wifi_status ws = {0};

			ws.connected = 2;
			if (selected_ap) {
				ws.rssi_dbm = selected_ap->rssi;
				size_t slen = MIN(selected_ap->ssid_length,
						  sizeof(ws.ssid) - 1);
				memcpy(ws.ssid, selected_ap->ssid, slen);
			}
			/* Iterate the interface's unicast list to find the assigned IPv4. */
			const struct net_if_config *ifcfg = net_if_get_config(iface);

			if (ifcfg && ifcfg->ip.ipv4) {
				for (int i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
					const struct net_if_addr *a =
						&ifcfg->ip.ipv4->unicast[i].ipv4;

					if (a->is_used &&
					    a->address.family == AF_INET) {
						memcpy(ws.ip_addr,
						       a->address.in_addr.s4_addr,
						       sizeof(ws.ip_addr));
						break;
					}
				}
			}
			(void)ble_notify_wifi_status(&ws);
		}
#endif
		break;

	default:
		break;
	}
}

/* Create connected view */
static void create_connected_view(lv_obj_t *parent)
{
	wifi_ui_state.connected_container = lv_obj_create(parent);
	lv_obj_set_size(wifi_ui_state.connected_container, lv_pct(100), LV_SIZE_CONTENT);
	lv_obj_set_style_bg_opa(wifi_ui_state.connected_container, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(wifi_ui_state.connected_container, 0, 0);
	lv_obj_set_style_pad_all(wifi_ui_state.connected_container, 5, 0);
	lv_obj_set_flex_flow(wifi_ui_state.connected_container, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_flex_align(wifi_ui_state.connected_container, 
			      LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
	
	/* SSID */
	lv_obj_t *ssid_row = lv_obj_create(wifi_ui_state.connected_container);
	lv_obj_set_size(ssid_row, lv_pct(100), LV_SIZE_CONTENT);
	lv_obj_set_style_bg_opa(ssid_row, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(ssid_row, 0, 0);
	lv_obj_set_style_pad_all(ssid_row, 2, 0);
	lv_obj_set_flex_flow(ssid_row, LV_FLEX_FLOW_ROW);
	
	lv_obj_t *ssid_title = lv_label_create(ssid_row);
	lv_label_set_text(ssid_title, "SSID: ");
	lv_obj_set_style_text_color(ssid_title, NS_COLOR_TEXT_DIM, 0);
	
	wifi_ui_state.ssid_label = lv_label_create(ssid_row);
	lv_label_set_text(wifi_ui_state.ssid_label, "---");
	lv_obj_set_style_text_color(wifi_ui_state.ssid_label, NS_COLOR_TEXT, 0);
	
	/* IP Address */
	lv_obj_t *ip_row = lv_obj_create(wifi_ui_state.connected_container);
	lv_obj_set_size(ip_row, lv_pct(100), LV_SIZE_CONTENT);
	lv_obj_set_style_bg_opa(ip_row, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(ip_row, 0, 0);
	lv_obj_set_style_pad_all(ip_row, 2, 0);
	lv_obj_set_flex_flow(ip_row, LV_FLEX_FLOW_ROW);
	
	lv_obj_t *ip_title = lv_label_create(ip_row);
	lv_label_set_text(ip_title, "IP: ");
	lv_obj_set_style_text_color(ip_title, NS_COLOR_TEXT_DIM, 0);
	
	wifi_ui_state.ip_label = lv_label_create(ip_row);
	lv_label_set_text(wifi_ui_state.ip_label, "---");
	lv_obj_set_style_text_color(wifi_ui_state.ip_label, NS_COLOR_TEXT, 0);
	
	/* Channel */
	lv_obj_t *ch_row = lv_obj_create(wifi_ui_state.connected_container);
	lv_obj_set_size(ch_row, lv_pct(100), LV_SIZE_CONTENT);
	lv_obj_set_style_bg_opa(ch_row, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(ch_row, 0, 0);
	lv_obj_set_style_pad_all(ch_row, 2, 0);
	lv_obj_set_flex_flow(ch_row, LV_FLEX_FLOW_ROW);
	
	lv_obj_t *ch_title = lv_label_create(ch_row);
	lv_label_set_text(ch_title, "Channel: ");
	lv_obj_set_style_text_color(ch_title, NS_COLOR_TEXT_DIM, 0);
	
	wifi_ui_state.channel_label = lv_label_create(ch_row);
	lv_label_set_text(wifi_ui_state.channel_label, "---");
	lv_obj_set_style_text_color(wifi_ui_state.channel_label, NS_COLOR_TEXT, 0);
	
	/* MAC Address */
	lv_obj_t *mac_row = lv_obj_create(wifi_ui_state.connected_container);
	lv_obj_set_size(mac_row, lv_pct(100), LV_SIZE_CONTENT);
	lv_obj_set_style_bg_opa(mac_row, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(mac_row, 0, 0);
	lv_obj_set_style_pad_all(mac_row, 2, 0);
	lv_obj_set_flex_flow(mac_row, LV_FLEX_FLOW_ROW);
	
	lv_obj_t *mac_title = lv_label_create(mac_row);
	lv_label_set_text(mac_title, "BSSID: ");
	lv_obj_set_style_text_color(mac_title, NS_COLOR_TEXT_DIM, 0);
	
	wifi_ui_state.mac_label = lv_label_create(mac_row);
	lv_label_set_text(wifi_ui_state.mac_label, "---");
	lv_obj_set_style_text_color(wifi_ui_state.mac_label, NS_COLOR_TEXT, 0);
	
	/* Signal strength */
	lv_obj_t *signal_row = lv_obj_create(wifi_ui_state.connected_container);
	lv_obj_set_size(signal_row, lv_pct(100), LV_SIZE_CONTENT);
	lv_obj_set_style_bg_opa(signal_row, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(signal_row, 0, 0);
	lv_obj_set_style_pad_all(signal_row, 2, 0);
	lv_obj_set_flex_flow(signal_row, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(signal_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	
	lv_obj_t *signal_title = lv_label_create(signal_row);
	lv_label_set_text(signal_title, "Signal: ");
	lv_obj_set_style_text_color(signal_title, NS_COLOR_TEXT_DIM, 0);
	
	wifi_ui_state.signal_bar = lv_bar_create(signal_row);
	lv_obj_set_size(wifi_ui_state.signal_bar, 80, 15);
	lv_bar_set_range(wifi_ui_state.signal_bar, 0, 100);
	lv_bar_set_value(wifi_ui_state.signal_bar, 0, LV_ANIM_OFF);
	
	wifi_ui_state.signal_label = lv_label_create(signal_row);
	lv_label_set_text(wifi_ui_state.signal_label, " 0 dBm");
	lv_obj_set_style_text_color(wifi_ui_state.signal_label, NS_COLOR_TEXT, 0);
	
	/* Disconnect button */
	wifi_ui_state.disconnect_btn = lv_btn_create(wifi_ui_state.connected_container);
	lv_obj_set_size(wifi_ui_state.disconnect_btn, 120, 35);
	lv_obj_set_style_bg_color(wifi_ui_state.disconnect_btn, COLOR_ERROR, 0);
	lv_obj_add_event_cb(wifi_ui_state.disconnect_btn, disconnect_btn_event_cb, LV_EVENT_CLICKED, NULL);
	
	lv_obj_t *btn_label = lv_label_create(wifi_ui_state.disconnect_btn);
	lv_label_set_text(btn_label, "Disconnect");
	lv_obj_center(btn_label);
	
	/* Initially hidden */
	lv_obj_add_flag(wifi_ui_state.connected_container, LV_OBJ_FLAG_HIDDEN);
}

/* Create disconnected/scan view */
static void create_scan_view(lv_obj_t *parent)
{
	wifi_ui_state.scan_container = lv_obj_create(parent);
	lv_obj_set_size(wifi_ui_state.scan_container, lv_pct(100), lv_pct(80));
	lv_obj_set_style_bg_opa(wifi_ui_state.scan_container, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(wifi_ui_state.scan_container, 0, 0);
	lv_obj_set_style_pad_all(wifi_ui_state.scan_container, 5, 0);
	lv_obj_set_flex_flow(wifi_ui_state.scan_container, LV_FLEX_FLOW_COLUMN);
	
	/* Scan status */
	wifi_ui_state.scan_status_label = lv_label_create(wifi_ui_state.scan_container);
	lv_label_set_text(wifi_ui_state.scan_status_label, "Not connected. Tap Scan to find networks.");
	lv_obj_set_style_text_color(wifi_ui_state.scan_status_label, NS_COLOR_TEXT_DIM, 0);
	
	/* AP List */
	wifi_ui_state.scan_list = lv_list_create(wifi_ui_state.scan_container);
	lv_obj_set_size(wifi_ui_state.scan_list, lv_pct(100), 120);
	lv_obj_set_style_bg_color(wifi_ui_state.scan_list, NS_COLOR_CARD, 0);
	lv_obj_set_style_border_color(wifi_ui_state.scan_list, NS_COLOR_BORDER, 0);
	lv_obj_set_style_border_width(wifi_ui_state.scan_list, 1, 0);
	
	/* Scan button */
	wifi_ui_state.scan_btn = lv_btn_create(wifi_ui_state.scan_container);
	lv_obj_set_size(wifi_ui_state.scan_btn, 100, 35);
	lv_obj_set_style_bg_color(wifi_ui_state.scan_btn, NS_COLOR_PRIMARY, 0);
	lv_obj_add_event_cb(wifi_ui_state.scan_btn, scan_btn_event_cb, LV_EVENT_CLICKED, NULL);
	
	lv_obj_t *btn_label = lv_label_create(wifi_ui_state.scan_btn);
	lv_label_set_text(btn_label, "Scan");
	lv_obj_center(btn_label);
}

/* Scan button callback */
static void scan_btn_event_cb(lv_event_t *e)
{
	ARG_UNUSED(e);
	wifi_ui_start_scan();
}

/* Disconnect button callback */
static void disconnect_btn_event_cb(lv_event_t *e)
{
	ARG_UNUSED(e);
	
	if (wifi_ui_state.iface) {
		LOG_INF("Disconnecting from Wi-Fi");
		net_mgmt(NET_REQUEST_WIFI_DISCONNECT, wifi_ui_state.iface, NULL, 0);
	}
}

/* Connect to an AP */
static void connect_to_ap(const char *ssid, const char *password, enum wifi_security_type sec)
{
	if (!wifi_ui_state.iface) return;

	struct wifi_connect_req_params params = {0};
	params.ssid = (uint8_t *)ssid;
	params.ssid_length = strlen(ssid);
	params.security = sec;
	
	if (password && strlen(password) > 0) {
		params.psk = (uint8_t *)password;
		params.psk_length = strlen(password);
	}
	
	lv_label_set_text(wifi_ui_state.scan_status_label, "Connecting...");
	
	int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, wifi_ui_state.iface, &params, sizeof(params));
	if (ret) {
		LOG_ERR("Connection request failed: %d", ret);
		lv_label_set_text(wifi_ui_state.scan_status_label, "Connection request failed");
	}
}

/* Keyboard event for password input */
static void password_kb_event_cb(lv_event_t *e)
{
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t *pwd_cont = lv_event_get_user_data(e);
	
	if (code == LV_EVENT_READY) {
		const char *pwd = lv_textarea_get_text(pwd_ta);
		connect_to_ap((const char *)selected_ap->ssid, pwd, selected_ap->security);
	}
	
	if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
		lv_obj_del(pwd_cont);
		lv_obj_clear_flag(wifi_ui_state.scan_container, LV_OBJ_FLAG_HIDDEN);
	}
}

/* AP list item callback */
static void ap_list_event_cb(lv_event_t *e)
{
	lv_event_code_t code = lv_event_get_code(e);
	int ap_idx = (int)(intptr_t)lv_event_get_user_data(e);
	
	if (code == LV_EVENT_CLICKED && ap_idx >= 0 && (size_t)ap_idx < wifi_ui_state.scan_count) {
		selected_ap = &wifi_ui_state.scan_results[ap_idx];
		LOG_INF("Selected AP: %s", selected_ap->ssid);
		
		if (selected_ap->security == WIFI_SECURITY_TYPE_NONE) {
			connect_to_ap((const char *)selected_ap->ssid, NULL, selected_ap->security);
		} else {
			/* Show password dialog */
			lv_obj_t *parent = lv_obj_get_parent(wifi_ui_state.scan_container);
			lv_obj_add_flag(wifi_ui_state.scan_container, LV_OBJ_FLAG_HIDDEN);
			
			lv_obj_t *pwd_cont = lv_obj_create(parent);
			lv_obj_set_size(pwd_cont, lv_pct(100), lv_pct(100));
			
			lv_obj_t *label = lv_label_create(pwd_cont);
			lv_label_set_text_fmt(label, "Password for %s:", selected_ap->ssid);
			lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 5);
			
			pwd_ta = lv_textarea_create(pwd_cont);
			lv_obj_set_size(pwd_ta, lv_pct(90), 40);
			lv_obj_align(pwd_ta, LV_ALIGN_TOP_MID, 0, 30);
			lv_textarea_set_password_mode(pwd_ta, true);
			lv_textarea_set_one_line(pwd_ta, true);
			
			pwd_kb = lv_keyboard_create(pwd_cont);
			lv_obj_set_size(pwd_kb, lv_pct(100), lv_pct(60));
			lv_keyboard_set_textarea(pwd_kb, pwd_ta);
			lv_obj_align(pwd_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
			
			lv_obj_add_event_cb(pwd_kb, password_kb_event_cb, LV_EVENT_READY, pwd_cont);
			lv_obj_add_event_cb(pwd_kb, password_kb_event_cb, LV_EVENT_CANCEL, pwd_cont);
		}
	}
}

/* Update connected view with current info */
static void update_connected_view(void)
{
	if (!wifi_ui_state.iface) {
		return;
	}
	
	/* Get interface status via Zephyr Wi-Fi management API */
	struct wifi_iface_status status = {0};
	int ret = net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, wifi_ui_state.iface,
			   &status, sizeof(status));
	
	if (ret != 0) {
		LOG_WRN("Failed to get iface status: %d", ret);
		return;
	}
	
	/* Update SSID */
	if (status.ssid_len > 0) {
		lv_label_set_text(wifi_ui_state.ssid_label, status.ssid);
	} else {
		lv_label_set_text(wifi_ui_state.ssid_label, "---");
	}
	
	/* Update Channel */
	if (status.channel > 0) {
		char ch_str[8];
		snprintf(ch_str, sizeof(ch_str), "%d", status.channel);
		lv_label_set_text(wifi_ui_state.channel_label, ch_str);
	} else {
		lv_label_set_text(wifi_ui_state.channel_label, "---");
	}
	
	/* Update BSSID */
	if (status.bssid[0] || status.bssid[1] || status.bssid[2]) {
		char bssid_str[18];
		snprintf(bssid_str, sizeof(bssid_str), "%02X:%02X:%02X:%02X:%02X:%02X",
			 status.bssid[0], status.bssid[1], status.bssid[2],
			 status.bssid[3], status.bssid[4], status.bssid[5]);
		lv_label_set_text(wifi_ui_state.mac_label, bssid_str);
	} else {
		lv_label_set_text(wifi_ui_state.mac_label, "---");
	}
	
	/* Signal strength */
	uint8_t pct = rssi_to_percent(status.rssi);
	lv_bar_set_value(wifi_ui_state.signal_bar, pct, LV_ANIM_ON);
	lv_obj_set_style_bg_color(wifi_ui_state.signal_bar, 
				  get_signal_color(status.rssi), LV_PART_INDICATOR);
	
	char rssi_str[16];
	snprintf(rssi_str, sizeof(rssi_str), " %d dBm", status.rssi);
	lv_label_set_text(wifi_ui_state.signal_label, rssi_str);
	
	/* Show connected status (IP address retrieval needs offloaded networking) */
	lv_label_set_text(wifi_ui_state.ip_label, "Connected");
}

/* Update disconnected view with scan results */
static void update_disconnected_view(void)
{
	if (wifi_ui_state.scanning) {
		lv_label_set_text(wifi_ui_state.scan_status_label, "Scanning...");
		lv_obj_set_style_text_color(wifi_ui_state.scan_status_label, COLOR_SCANNING, 0);
		return;
	}
	
	/* Clear list */
	lv_obj_clean(wifi_ui_state.scan_list);
	
	if (wifi_ui_state.scan_count == 0) {
		lv_label_set_text(wifi_ui_state.scan_status_label, "No networks found. Tap Scan.");
		lv_obj_set_style_text_color(wifi_ui_state.scan_status_label, 
					    NS_COLOR_TEXT_DIM, 0);
		return;
	}
	
	char status_text[32];
	snprintf(status_text, sizeof(status_text), "Found %zu networks:", wifi_ui_state.scan_count);
	lv_label_set_text(wifi_ui_state.scan_status_label, status_text);
	lv_obj_set_style_text_color(wifi_ui_state.scan_status_label, NS_COLOR_TEXT, 0);
	
	/* Populate list with scan results */
	for (size_t i = 0; i < wifi_ui_state.scan_count; i++) {
		struct wifi_scan_result *res = &wifi_ui_state.scan_results[i];
		
		char item_text[64];
		snprintf(item_text, sizeof(item_text), "%s (%s, %d dBm)",
			 res->ssid,
			 security_to_str(res->security),
			 res->rssi);
		
		lv_obj_t *btn = lv_list_add_btn(wifi_ui_state.scan_list, LV_SYMBOL_WIFI, item_text);
		lv_obj_set_style_bg_color(btn, NS_COLOR_CARD, 0);
		lv_obj_add_event_cb(btn, ap_list_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
		
		/* Color based on signal */
		lv_obj_t *label = lv_obj_get_child(btn, 1);
		if (label) {
			lv_obj_set_style_text_color(label, get_signal_color(res->rssi), 0);
		}
	}
}

/* ===== Public API Implementation ===== */

lv_obj_t *wifi_ui_create(void)
{
	LOG_INF("Creating Wi-Fi UI screen (legacy)");
	
	memset(&wifi_ui_state, 0, sizeof(wifi_ui_state));
	
	/* Get Wi-Fi network interface */
#if DT_NODE_HAS_STATUS(WIFI_NODE, okay)
	const struct device *wifi_dev = DEVICE_DT_GET(WIFI_NODE);
	if (device_is_ready(wifi_dev)) {
		wifi_ui_state.iface = net_if_lookup_by_dev(wifi_dev);
		if (!wifi_ui_state.iface) {
			LOG_WRN("No network interface found for Wi-Fi device");
		} else {
			/* Register for Wi-Fi and IP events. NET_EVENT_IPV4_ADDR_ADD fires
			 * after DHCP assigns an address so we can push the IP over BLE. */
			net_mgmt_init_event_callback(&wifi_ui_state.wifi_cb, wifi_event_handler,
						     NET_EVENT_WIFI_SCAN_RESULT |
						     NET_EVENT_WIFI_SCAN_DONE |
						     NET_EVENT_WIFI_CONNECT_RESULT |
						     NET_EVENT_WIFI_DISCONNECT_RESULT |
						     NET_EVENT_IPV4_ADDR_ADD);
			net_mgmt_add_event_callback(&wifi_ui_state.wifi_cb);
			LOG_INF("Wi-Fi event callbacks registered");
		}
	} else {
		LOG_WRN("Wi-Fi device not ready");
	}
#else
	LOG_WRN("Wi-Fi device not defined in device tree");
#endif
	
	/* Create screen */
	wifi_ui_state.screen = lv_obj_create(NULL);
	ns_apply_screen_bg(wifi_ui_state.screen);
	
	/* Main container (top padding clears the global status strip) */
	lv_obj_t *container = lv_obj_create(wifi_ui_state.screen);
	lv_obj_set_size(container, lv_pct(100), lv_pct(100));
	lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(container, 0, 0);
	lv_obj_set_style_pad_all(container, 10, 0);
	lv_obj_set_style_pad_top(container, UI_SHELL_TOP_INSET, 0);
	lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	
	/* Title */
	wifi_ui_state.title_label = lv_label_create(container);
	lv_label_set_text(wifi_ui_state.title_label, LV_SYMBOL_WIFI " Wi-Fi");
	lv_obj_set_style_text_font(wifi_ui_state.title_label, UI_FONT_20, 0);
	lv_obj_set_style_text_color(wifi_ui_state.title_label, NS_COLOR_SECONDARY, 0);
	
	/* Status */
	wifi_ui_state.status_label = lv_label_create(container);
	lv_label_set_text(wifi_ui_state.status_label, "Status: Disconnected");
	lv_obj_set_style_text_color(wifi_ui_state.status_label, COLOR_DISCONNECTED, 0);
	
	/* Create both views */
	create_connected_view(container);
	create_scan_view(container);
	
	LOG_INF("Wi-Fi UI screen created");
	return wifi_ui_state.screen;
}

void wifi_ui_update(void)
{
	if (!wifi_ui_state.screen) {
		return;
	}
	
	/* Check connection state via Zephyr Wi-Fi API */
	if (wifi_ui_state.iface && !wifi_ui_state.scanning) {
		static int64_t last_status_ms;
		int64_t now = k_uptime_get();
		/* Poll at most twice per second to avoid blocking UI/input */
		if (now - last_status_ms > 500) {
			struct wifi_iface_status status = {0};
			int ret = net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, wifi_ui_state.iface,
				   &status, sizeof(status));
			if (ret == 0) {
				wifi_ui_state.connected = (status.state == WIFI_STATE_COMPLETED);
			}
			last_status_ms = now;
		}
	}
	
	/* Update status label */
	if (wifi_ui_state.connected) {
		lv_label_set_text(wifi_ui_state.status_label, "Status: Connected");
		lv_obj_set_style_text_color(wifi_ui_state.status_label, COLOR_CONNECTED, 0);
		
		/* Show connected view, hide scan view */
		lv_obj_clear_flag(wifi_ui_state.connected_container, LV_OBJ_FLAG_HIDDEN);
		lv_obj_add_flag(wifi_ui_state.scan_container, LV_OBJ_FLAG_HIDDEN);
		
		update_connected_view();
	} else if (wifi_ui_state.scanning) {
		lv_label_set_text(wifi_ui_state.status_label, "Status: Scanning...");
		lv_obj_set_style_text_color(wifi_ui_state.status_label, COLOR_SCANNING, 0);
		
		update_disconnected_view();
	} else {
		lv_label_set_text(wifi_ui_state.status_label, "Status: Disconnected");
		lv_obj_set_style_text_color(wifi_ui_state.status_label, COLOR_DISCONNECTED, 0);
		
		/* Show scan view, hide connected view */
		lv_obj_add_flag(wifi_ui_state.connected_container, LV_OBJ_FLAG_HIDDEN);
		lv_obj_clear_flag(wifi_ui_state.scan_container, LV_OBJ_FLAG_HIDDEN);
		
		update_disconnected_view();
	}
}

void wifi_ui_cleanup(void)
{
	LOG_INF("Cleaning up Wi-Fi UI");
	
	/* Remove event callback only if interface was initialized */
	if (wifi_ui_state.iface) {
		net_mgmt_del_event_callback(&wifi_ui_state.wifi_cb);
	}
	
	memset(&wifi_ui_state, 0, sizeof(wifi_ui_state));
}

bool wifi_ui_is_connected(void)
{
	return wifi_ui_state.connected;
}

void wifi_ui_start_scan(void)
{
	LOG_INF("Starting Wi-Fi scan");

	/* Clear previous results */
	wifi_ui_state.scan_count = 0;
	memset(wifi_ui_state.scan_results, 0, sizeof(wifi_ui_state.scan_results));

	if (!wifi_ui_state.iface) {
		LOG_WRN("No Wi-Fi interface available for scan");
		return;
	}

	/* Guard against concurrent scans */
	if (wifi_ui_state.scanning) {
		LOG_WRN("Scan already in progress");
		return;
	}

	wifi_ui_state.scanning = true;
	lv_label_set_text(wifi_ui_state.scan_status_label, "Scanning...");
	lv_obj_set_style_text_color(wifi_ui_state.scan_status_label, COLOR_SCANNING, 0);
	lv_obj_clean(wifi_ui_state.scan_list);

	/* Initiate scan - results arrive via NET_EVENT_WIFI_SCAN_RESULT callbacks */
	int ret = net_mgmt(NET_REQUEST_WIFI_SCAN, wifi_ui_state.iface, NULL, 0);
	if (ret != 0) {
		LOG_ERR("Scan request failed: %d", ret);
		wifi_ui_state.scanning = false;
	}
}

lv_obj_t *wifi_ui_get_screen(void)
{
	return wifi_ui_state.screen;
}

void wifi_ui_create_screen(lv_obj_t *parent)
{
	LOG_INF("Creating Wi-Fi UI screen (using parent)");
	
	if (!parent) {
		LOG_ERR("Parent screen is NULL");
		return;
	}
	
	memset(&wifi_ui_state, 0, sizeof(wifi_ui_state));
	
	/* Use provided parent as screen */
	wifi_ui_state.screen = parent;
	
	/* Get Wi-Fi network interface */
#if DT_NODE_HAS_STATUS(WIFI_NODE, okay)
	const struct device *wifi_dev = DEVICE_DT_GET(WIFI_NODE);
	if (device_is_ready(wifi_dev)) {
		wifi_ui_state.iface = net_if_lookup_by_dev(wifi_dev);
		if (!wifi_ui_state.iface) {
			LOG_WRN("No network interface found for Wi-Fi device");
		} else {
			/* Register for Wi-Fi and IP events. NET_EVENT_IPV4_ADDR_ADD fires
			 * after DHCP assigns an address so we can push the IP over BLE. */
			net_mgmt_init_event_callback(&wifi_ui_state.wifi_cb, wifi_event_handler,
						     NET_EVENT_WIFI_SCAN_RESULT |
						     NET_EVENT_WIFI_SCAN_DONE |
						     NET_EVENT_WIFI_CONNECT_RESULT |
						     NET_EVENT_WIFI_DISCONNECT_RESULT |
						     NET_EVENT_IPV4_ADDR_ADD);
			net_mgmt_add_event_callback(&wifi_ui_state.wifi_cb);
			LOG_INF("Wi-Fi event callbacks registered");
		}
	} else {
		LOG_WRN("Wi-Fi device not ready");
	}
#else
	LOG_WRN("Wi-Fi device not defined in device tree");
#endif
	
	/* Set parent screen background */
	lv_obj_set_style_bg_color(parent, lv_color_hex(NS_RGB_BG), 0);
	
	/* Main container (top padding clears the global status strip) */
	lv_obj_t *container = lv_obj_create(parent);
	lv_obj_set_size(container, lv_pct(100), lv_pct(100));
	lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(container, 0, 0);
	lv_obj_set_style_pad_all(container, 10, 0);
	lv_obj_set_style_pad_top(container, UI_SHELL_TOP_INSET, 0);
	lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	
	/* Title */
	wifi_ui_state.title_label = lv_label_create(container);
	lv_label_set_text(wifi_ui_state.title_label, LV_SYMBOL_WIFI " Wi-Fi");
	lv_obj_set_style_text_font(wifi_ui_state.title_label, UI_FONT_20, 0);
	lv_obj_set_style_text_color(wifi_ui_state.title_label, NS_COLOR_SECONDARY, 0);
	
	/* Status */
	wifi_ui_state.status_label = lv_label_create(container);
	lv_label_set_text(wifi_ui_state.status_label, "Status: Disconnected");
	lv_obj_set_style_text_color(wifi_ui_state.status_label, COLOR_DISCONNECTED, 0);
	
	/* Create both views */
	create_connected_view(container);
	create_scan_view(container);
	
	LOG_INF("Wi-Fi UI screen created (using parent)");
}

void wifi_ui_update_screen(void)
{
	/* Update the screen with current data */
	wifi_ui_update();
}
