/*
 * http_pages.c
 *
 *  Created on: Dec 11, 2025
 *      Author: Daruin Solano
 */

#include "http_server.h"
#include "http_ui.h"
#include "msg.h"


/*
 * This should be placed in you app code
 * Build the handlers yourself
 *
 */
const http_route_t routes[] = {
    { "/wifi/connect", "GET",  ui_wifi_page         },
    { "/wifi/apply",   "POST", ui_wifi_apply        },
    { "/wifi/join",    "POST", ui_wifi_join         },
    { "/wifi/scan",    "GET",  ui_wifi_scan_page    },
    { "/",             "GET",  http_srv_default_cb  }
};

const size_t routes_count = sizeof(routes)/sizeof(routes[0]);


int ui_wifi_page(http_srv_t *hs, const http_srv_request_t *req)
{
    http_ui_begin_page(hs, req, "WiFi Configuration");

    http_ui_heading("Network Settings");
    http_ui_paragraph("Enter SSID, password, and wireless preferences:");

    http_ui_form_begin("/wifi/apply", "post", NULL);
        http_ui_textbox("ssid", "SSID:", "");
        http_ui_textbox("pass", "Password:", "");
        http_ui_number("ch",  "Channel:", 6);

        const char *bands[] = { "2.4 GHz", "5 GHz" };
        http_ui_select("band", bands, 2, 0);

        http_ui_submit(NULL, "Apply Settings");
    http_ui_form_end();

    http_ui_end_page();
    return HTTP_OK;
}

int ui_wifi_apply(http_srv_t *hs, const http_srv_request_t *req)
{
    const char *ssid = http_ui_get_param(req, "ssid");
    const char *pass = http_ui_get_param(req, "pass");
    const char *ch   = http_ui_get_param(req, "ch");
    const char *band = http_ui_get_param(req, "band");

    msg_info("WiFi Apply: SSID=%s PASS=%s CH=%s BAND=%s\n",
             ssid, pass, ch, band);

    // TODO: apply to your WiFi driver here
    // wifi_set_config(ssid, pass, atoi(ch), atoi(band));

    http_ui_begin_page(hs, req, "Settings Applied");
    http_ui_paragraph("WiFi settings updated successfully.");
    http_ui_button("/wifi/connect", "Return");
    http_ui_end_page();

    return HTTP_OK;
}

/*--------Wifi page dynamic table parameters---------*/
#define WIFI_COL_SSID     0
#define WIFI_COL_RSSI     1
#define WIFI_COL_SEC      2
#define WIFI_COL_HIDDEN   3
#define WIFI_COL_PASS     4
#define WIFI_COL_COUNT    5

#define WIFI_ROW_COUNT  4

static const char *wifi_security_opts[] = {
    "OPEN",
    "WPA2",
    "WPA3"
};

static const http_ui_table_col_t wifi_cols[WIFI_COL_COUNT] = {
    /* header,      field_name,      type,                 options,              opt_cnt,                        post_value */
    { "SSID",       "ssid",          HTTP_UI_COL_STATIC,   NULL,                 0,                              true  },
    { "RSSI (dBm)", NULL,            HTTP_UI_COL_STATIC,   NULL,                 0,                              false },
    { "Security",   "sec",           HTTP_UI_COL_SELECT,   wifi_security_opts,   sizeof(wifi_security_opts)/sizeof(wifi_security_opts[0]), false },
    { "Hidden",     "hidden",        HTTP_UI_COL_CHECKBOX, NULL,                 0,                              false },
    { "Password",   "pass",          HTTP_UI_COL_PASSWORD, NULL,                 0,                              false },
};
static const char *wifi_rows[WIFI_ROW_COUNT][WIFI_COL_COUNT] = {
    /* SSID                 RSSI   SEC   HIDDEN PASS */
    { "HomeNetwork",       "-45", "WPA2", "0",   "" },
    { "Office",            "-60", "WPA2", "0",   "" },
    { "MyHotspot",         "-70", "OPEN", "0",   "" },
	{ "Neightbor Spot",    "-70", "OPEN", "0",   "" }
};

int ui_wifi_scan_page(http_srv_t *hs, const http_srv_request_t *req)
{
    http_ui_begin_page(hs, req, "Available WiFi Networks");
    http_ui_heading("WiFi Scan Results");

    http_ui_dynamic_table(
           "/wifi/join",
           wifi_cols,
           WIFI_COL_COUNT,
           &wifi_rows[0][0],       // <--- fixed argument
           WIFI_ROW_COUNT,
           "Join",
           WIFI_COL_PASS,
           "row_id"
       );

    http_ui_end_page();
    return HTTP_OK;
}

int ui_wifi_join(http_srv_t *hs, const http_srv_request_t *req)
{
    const char *row_id = http_ui_get_param(req, "row_id");
    const char *ssid   = http_ui_get_param(req, "ssid");
    const char *pass   = http_ui_get_param(req, "pass");
    const char *sec    = http_ui_get_param(req, "sec");
    const char *hidden = http_ui_get_param(req, "hidden");  /* "1" if checked, NULL or "0" otherwise */

    msg_info("WiFi join requested: row=%s SSID=%s SEC=%s HIDDEN=%s PASS=%s\n",
             row_id ? row_id : "(null)",
             ssid   ? ssid   : "(null)",
             sec    ? sec    : "(null)",
             hidden ? hidden : "0",
             pass   ? pass   : "(null)");

    // TODO: wifi_connect(ssid, pass, sec, hidden_flag, row_index);

    http_ui_begin_page(hs, req, "Joining WiFi");
    if (ssid && pass && pass[0] != '\0') {
        http_ui_paragraph("Connecting to:");
        http_ui_heading(ssid);
    } else {
        http_ui_paragraph("Invalid WiFi credentials.");
    }
    http_ui_button("/wifi/scan", "Back to main menu");
    http_ui_end_page();

    return HTTP_OK;
}

int http_srv_default_cb(http_srv_t           *hs,
                        const http_srv_request_t *req)
{
    (void)hs;

    char body[512];
    int body_len = snprintf(body, sizeof(body),
                            "<html>"
                            "<head><title>STM32 HTTP Server</title></head>"
                            "<body>"
                            "<h1>Hello from STM32!</h1>"
                            "<p>Method: %s</p>"
                            "<p>Path: %s</p>"
                            "<p>Query: %s</p>"
                            "</body>"
                            "</html>",
                            req->method,
                            req->path,
                            req->query[0] ? req->query : "(none)");

    if (body_len < 0) body_len = 0;
    if (body_len > (int)sizeof(body)) body_len = (int)sizeof(body);

    return http_srv_send_response(hs,
                                  200, "OK",
                                  "text/html",
                                  (const uint8_t *)body,
                                  (uint32_t)body_len,
                                  "");
}

