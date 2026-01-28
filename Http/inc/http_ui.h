/*
 * http_ui.h
 *
 *  Created on: Dec 9, 2025
 *      Author: Daruin Solano
 */
/*
 * Below is a clean, complete, real-world use case showing how your updated http_ui (widgets + JSON + POST + file upload) is used inside your STM32 HTTP server.

I will show:

A UI page with textboxes, numbers, and a submit button

A handler that receives GET / POST / JSON automatically using http_ui_get_param()

A JSON-based API endpoint (e.g. AJAX/JS fetch) using the cJSON-powered backend

A file upload page and handler using http_ui_get_file()

A final router / main loop showing how it all runs.

✅ 1. UI page with textboxes, dropdowns, number input, submit button
int ui_wifi_page(http_srv_t *hs, const http_request_t *req)
{
    http_ui_begin_page(hs, req, "WiFi Configuration");

    http_ui_heading("WiFi Settings");
    http_ui_paragraph("Enter your SSID and password below:");

    http_ui_form_begin("/wifi/apply", "post", NULL);
        http_ui_textbox("ssid", "SSID:", "");
        http_ui_textbox("pass", "Password:", "");
        http_ui_number("ch", "Channel:", 6);

        const char *bands[] = { "2.4GHz", "5GHz" };
        http_ui_select("band", bands, 2, 0);
        http_ui_submit(NULL, "Apply");
    http_ui_form_end();

    http_ui_end_page();
    return HTTP_OK;
}

✔ Produces a full HTML page
✔ Submits values via POST (urlencoded)
✔ Can also be called via JSON from a browser app
✅ 2. Handler that receives GET, POST, or JSON without changing code
int ui_wifi_apply(http_srv_t *hs, const http_request_t *req)
{
    const char *ssid = http_ui_get_param(req, "ssid");
    const char *pass = http_ui_get_param(req, "pass");
    const char *ch   = http_ui_get_param(req, "ch");
    const char *band = http_ui_get_param(req, "band");

    msg_info("WiFi Apply -> SSID=%s, PASS=%s, CH=%s, BAND=%s\n",
             ssid, pass, ch, band);

    // Example: Apply the settings to your WiFi driver
    // wifi_configure(ssid, pass, atoi(ch), atoi(band));

    http_ui_begin_page(hs, req, "WiFi Updated");
    http_ui_paragraph("Your WiFi configuration has been applied.");
    http_ui_button("/ui", "Back to Menu");
    http_ui_end_page();

    return HTTP_OK;
}

✔ If submitted using the HTML form → values come from POST
✔ If called via /wifi/apply?ssid=X&pass=Y → values come from GET
✔ If called using JSON → values come from JSON

No change in your C code.

✅ 3. JSON API endpoint — uses cJSON internally

Browser example:

fetch("/api/wifi", {
  method: "POST",
  headers: { "Content-Type": "application/json" },
  body: JSON.stringify({
    ssid: "MyHome",
    pass: "12345678",
    ch:   6,
    band: "5GHz"
  })
});


Your C handler:

int api_wifi_handler(http_srv_t *hs, const http_request_t *req)
{
    const char *ssid = http_ui_get_param(req, "ssid");   // JSON
    const char *pass = http_ui_get_param(req, "pass");   // JSON
    const char *ch   = http_ui_get_param(req, "ch");     // JSON
    const char *band = http_ui_get_param(req, "band");   // JSON

    msg_info("API WiFi -> SSID=%s, PASS=%s, CH=%s, BAND=%s\n",
             ssid, pass, ch, band);

    const char *resp = "{\"status\":\"ok\"}";
    http_srv_send_response(hs, 200, "OK",
                           "application/json",
                           (const uint8_t*)resp,
                           strlen(resp),
                           "");

    return HTTP_OK;
}

✔ Uses cJSON (because http_ui_get_param() detects JSON type)
✔ Same exact code used for POST form handling
✔ Works for mobile apps, JS frontends, external API clients
✅ 4. File Upload example — firmware upload
UI Page
int ui_upload_page(http_srv_t *hs, const http_request_t *req)
{
    http_ui_begin_page(hs, req, "Firmware Upload");

    http_ui_heading("Upload New Firmware");
    http_ui_form_begin("/fw/upload", "post", "multipart/form-data");
        http_ui_file_input("fw", "Firmware:");
        http_ui_submit(NULL, "Upload");
    http_ui_form_end();

    http_ui_end_page();
    return HTTP_OK;
}

File Upload Handler
int fw_upload_handler(http_srv_t *hs, const http_request_t *req)
{
    http_ui_file_part_t file;

    if (http_ui_get_file(req, "fw", &file) == 0) {
        msg_info("Uploaded file: %s (%lu bytes)\n",
                 file.filename, file.length);

        // Example: store firmware into flash
        // flash_write(file.data, file.length);
    } else {
        msg_error("No firmware uploaded\n");
    }

    http_ui_begin_page(hs, req, "Upload Status");
    http_ui_paragraph("Firmware uploaded successfully.");
    http_ui_button("/ui", "Return");
    http_ui_end_page();

    return HTTP_OK;
}

✔ Fully supports multipart/form-data
✔ File name, size, content, MIME available
✅ 5. Router table and main loop
static const http_route_t route_table[] = {
    { "/ui",          HTTP_GET,  ui_wifi_page       },
    { "/wifi/apply",  HTTP_POST, ui_wifi_apply      },
    { "/api/wifi",    HTTP_POST, api_wifi_handler   },
    { "/upload",      HTTP_GET,  ui_upload_page     },
    { "/fw/upload",   HTTP_POST, fw_upload_handler  },
};

Server startup:
void start_webserver(void)
{
    http_srv_init(&http_server, hnet, 80);   // bind port 80
    http_srv_set_routes(route_table,
                        sizeof(route_table)/sizeof(route_table[0]));
    http_srv_run(&http_server); // blocking loop
}

🎉 Summary of what this allows

✔ Build high-level UI interfaces using HTML widgets from C
✔ Receive GET, POST, and JSON with one API: http_ui_get_param()
✔ JSON parsing is now backed by cJSON
✔ Serve interactive device configuration pages
✔ Build API endpoints for apps or AJAX
✔ Upload firmware or files
✔ Everything integrated with your net_srv architecture
 */
#ifndef DRIVERS_WIFI_NET_NET_INC_HTTP_UI_H_
#define DRIVERS_WIFI_NET_NET_INC_HTTP_UI_H_

#ifndef HTTP_UI_H
#define HTTP_UI_H

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#include "http_server.h"   // for http_request_t and http_srv_t

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- HTML page & widget helpers ---------- */

void http_ui_begin_page(http_srv_t *hs,
                        const http_srv_request_t *req,
                        const char *title);

void http_ui_end_page(void);

void http_ui_heading(const char *text);
void http_ui_paragraph(const char *text);

/* Simple button using GET navigation */
void http_ui_button(const char *action_path, const char *label);

/* Simple GET submit form */
void http_ui_submit(const char *action_path, const char *label);

/* Generic form container */
void http_ui_form_begin(const char *action_path,
                        const char *method,          /* "get" or "post" */
                        const char *enctype);        /* NULL or e.g. "multipart/form-data" */

void http_ui_form_end(void);

void http_ui_textbox(const char *name,
                     const char *label,
                     const char *default_value);

void http_ui_checkbox(const char *name,
                      const char *label,
                      bool checked);

void http_ui_number(const char *name,
                    const char *label,
                    int value);

void http_ui_select(const char *name,
                    const char **options,
                    int option_count,
                    int selected);

void http_ui_wifi_table(const char *action_path,
                        const char **ssids,
                        const int *rssis,
                        size_t count);

/* File input (for uploads) */
void http_ui_file_input(const char *name,
                        const char *label);

/*----------------------------Dynamic table widget--------------------*/


typedef enum {
    HTTP_UI_COL_STATIC,    /* display-only text; can still be posted as hidden if post_value=true */
    HTTP_UI_COL_TEXT,      /* <input type="text"> */
    HTTP_UI_COL_PASSWORD,  /* <input type="password"> */
    HTTP_UI_COL_NUMBER,    /* <input type="number"> */
    HTTP_UI_COL_CHECKBOX,  /* <input type="checkbox"> */
    HTTP_UI_COL_SELECT     /* <select> with options[] */
} http_ui_col_type_t;

typedef struct {
    const char           *header;       /* Column title */
    const char           *field_name;   /* Form key; can be NULL for pure display cols */
    http_ui_col_type_t    type;

    /* For SELECT columns: options + option_count
       rows[row][col] should be a string that matches one of the options to be "selected". */
    const char * const   *options;
    size_t                option_count;

    /* For STATIC columns: if true, also send it as a hidden field with field_name. */
    bool                  post_value;
} http_ui_table_col_t;

/**
 * @brief Draw a dynamic table with one form per row and a button at the end.
 *
 * @param action_path       URL where each row form is POSTed.
 * @param cols              Array of column descriptors.
 * @param col_count         Number of columns.
 * @param rows              2D string array: rows[row][col] = value (may be NULL).
 * @param row_count         Number of rows.
 * @param button_label      Label on the submit button (e.g., "Join", "Apply", "Select").
 * @param enable_col_index  Index of the column that controls button enable/disable:
 *                          - TEXT/PASSWORD/NUMBER/SELECT: enabled when value non-empty
 *                          - CHECKBOX: enabled when checked
 *                          - If >= col_count, buttons are always enabled.
 * @param row_id_field_name If non-NULL, a hidden field with this name will be added
 *                          per row with the numeric row index as value.
 */
void http_ui_dynamic_table(const char *action_path,
                           const http_ui_table_col_t *cols,
                           size_t col_count,
                           const char * const *rows,   // flat pointer
                           size_t row_count,
                           const char *button_label,
                           size_t enable_col_index,
                           const char *row_id_field_name);


/* ---------- Parameter / Body parsing ---------- */

typedef enum {
    HTTP_UI_PARAM_NONE  = 0,
    HTTP_UI_PARAM_QUERY = 1,   /* URL ?key=value */
    HTTP_UI_PARAM_FORM  = 2,   /* application/x-www-form-urlencoded */
    HTTP_UI_PARAM_JSON  = 3    /* application/json */
} http_ui_param_source_t;

/* Unified parameter getter:
 *  - checks query string (GET)
 *  - checks urlencoded body (POST form)
 *  - checks JSON body (simple key lookup via cJSON)
 *
 * Returns pointer to an internal buffer (null-terminated), or NULL if not found.
 */
const char *http_ui_get_param(const http_srv_request_t *req, const char *key);

/* Same as above, but also tells you where it was found. */
const char *http_ui_get_param_ex(const http_srv_request_t *req,
                                 const char *key,
                                 http_ui_param_source_t *src_out);

/* Explicit JSON field getter (string/number/bool → string) using cJSON.
 *  - looks in req->body (must be JSON)
 *  - returns pointer to internal buffer or NULL
 */
const char *http_ui_get_json_field(const http_srv_request_t *req,
                                   const char *key);

/* ---------- File upload parsing (multipart/form-data) ---------- */

typedef struct {
    const uint8_t *data;       /* pointer inside req->body */
    uint32_t       length;     /* file data length */
    char           filename[64];
    char           content_type[64];
} http_ui_file_part_t;

/* Parse multipart/form-data and find a file field by name.
 * Returns 0 on success, <0 on error/not found.
 */
int http_ui_get_file(const http_srv_request_t   *req,
                     const char             *field_name,
                     http_ui_file_part_t    *out_part);


/*-----------------Declare pages here as extern from http_pages.c-----------------------*/
extern int ui_wifi_page(http_srv_t *hs, const http_srv_request_t *req);
extern int ui_wifi_scan_page(http_srv_t *hs, const http_srv_request_t *req);
extern int ui_wifi_apply(http_srv_t *hs, const http_srv_request_t *req);
extern int ui_wifi_scan_page(http_srv_t *hs, const http_srv_request_t *req);
extern int ui_wifi_join(http_srv_t *hs, const http_srv_request_t *req);
/* Default handler: returns a simple HTML page showing method/path/query. */
extern int http_srv_default_cb(http_srv_t           *hs,
                        const http_srv_request_t *req);

/* Extern route table defined elsewhere */
extern const http_route_t routes[];
extern const size_t routes_count;


#ifdef __cplusplus
}
#endif

#endif /* HTTP_UI_H */


#endif /* DRIVERS_WIFI_NET_NET_INC_HTTP_UI_H_ */
