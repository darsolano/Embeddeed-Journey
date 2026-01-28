/*
 * http_ui.c
 *
 *  Created on: Dec 9, 2025
 *      Author: Daruin Solano
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "http_ui.h"
#include "msg.h"
#include "cJSON.h"   // NEW: JSON parsing

/* -------------------- Internal HTML buffer -------------------- */

static char      html_buffer[4096];
static uint32_t  html_len = 0;
static http_srv_t *current_hs = NULL;

static void html_add(const char *fmt, ...)
{
    if (html_len >= sizeof(html_buffer))
        return;

    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(html_buffer + html_len,
                            sizeof(html_buffer) - html_len,
                            fmt, args);
    va_end(args);

    if (written > 0)
        html_len += (uint32_t)written;

    if (html_len >= sizeof(html_buffer))
        html_len = sizeof(html_buffer) - 1;
}

/* -------------------- HTML PAGE + WIDGETS -------------------- */

void http_ui_begin_page(http_srv_t *hs,
                        const http_srv_request_t *req,
                        const char *title)
{
    (void)req; /* Not used now, but kept for future customization */
    current_hs = hs;
    html_len   = 0;
    memset(html_buffer, 0, sizeof(html_buffer));

    html_add("<html><head><title>%s</title>", title);
    html_add("<style>"
             "body{font-family:Arial,Helvetica,sans-serif;margin:20px;}"
             "button{padding:10px;margin:5px;}"
             "input,select{padding:5px;margin:5px;}"
             "label{display:inline-block;width:100px;}"
             "</style>");
    html_add("</head><body>");
    html_add("<h2>%s</h2>", title);
}

void http_ui_end_page(void)
{
    html_add("</body></html>");

    http_srv_send_response(current_hs,
                           200, "OK",
                           "text/html",
                           (const uint8_t *)html_buffer,
                           html_len,
                           "");
}

void http_ui_heading(const char *text)
{
    html_add("<h3>%s</h3>", text);
}

void http_ui_paragraph(const char *text)
{
    html_add("<p>%s</p>", text);
}

void http_ui_button(const char *action_path, const char *label)
{
    html_add("<p><button onclick=\"location.href='%s'\">%s</button></p>",
             action_path, label);
}

void http_ui_submit(const char *action_path, const char *label)
{
    if (action_path && action_path[0] != '\0') {
        // Standalone form with its own action
        html_add("<form action=\"%s\" method=\"get\">"
                 "<input type=\"submit\" value=\"%s\">"
                 "</form>",
                 action_path, label);
    } else {
        // Use existing <form> around it
        html_add("<input type=\"submit\" value=\"%s\">", label);
    }
}

void http_ui_form_begin(const char *action_path,
                        const char *method,
                        const char *enctype)
{
    if (!method)   method   = "post";
    if (!enctype)  enctype  = "application/x-www-form-urlencoded";

    html_add("<form action=\"%s\" method=\"%s\" enctype=\"%s\">",
             action_path, method, enctype);
}

void http_ui_form_end(void)
{
    html_add("</form>");
}

void http_ui_textbox(const char *name,
                     const char *label,
                     const char *default_value)
{
    if (!default_value) default_value = "";
    html_add("<label>%s</label>"
             "<input type=\"text\" name=\"%s\" value=\"%s\"><br>",
             label, name, default_value);
}

void http_ui_checkbox(const char *name,
                      const char *label,
                      bool checked)
{
    html_add("<label>%s</label>"
             "<input type=\"checkbox\" name=\"%s\" %s><br>",
             label, name, checked ? "checked" : "");
}

void http_ui_number(const char *name,
                    const char *label,
                    int value)
{
    html_add("<label>%s</label>"
             "<input type=\"number\" name=\"%s\" value=\"%d\"><br>",
             label, name, value);
}

void http_ui_select(const char *name,
                    const char **options,
                    int option_count,
                    int selected)
{
    html_add("<label>%s</label><select name=\"%s\">", name, name);
    for (int i = 0; i < option_count; i++) {
        html_add("<option value=\"%d\" %s>%s</option>",
                 i,
                 (i == selected) ? "selected" : "",
                 options[i]);
    }
    html_add("</select><br>");
}

void http_ui_dynamic_table(const char *action_path,
                           const http_ui_table_col_t *cols,
                           size_t col_count,
                           const char * const *rows,   // flat
                           size_t row_count,
                           const char *button_label,
                           size_t enable_col_index,
                           const char *row_id_field_name)
{
    if (!action_path || !cols || col_count == 0 || !rows || row_count == 0) {
        html_add("<p>No data available.</p>");
        return;
    }

    if (!button_label) {
        button_label = "Submit";
    }

    /* JavaScript: controls per-row button using a specific column input */
    html_add(
        "<script>"
        "function uiDynTblChanged(row){"
        "  var el = document.getElementById('cell_'+row+'_EC');"
        "  var btn = document.getElementById('btn_'+row);"
        "  if (!el || !btn) return;"
        "  var enable = false;"
        "  if (el.type === 'checkbox'){"
        "    enable = el.checked;"
        "  } else {"
        "    enable = (el.value && el.value.length > 0);"
        "  }"
        "  btn.disabled = !enable;"
        "}"
        "</script>"
    );

    html_add("<table border=\"1\" cellpadding=\"4\" cellspacing=\"0\">");

    /* Header row */
    html_add("<tr>");
    for (size_t c = 0; c < col_count; ++c) {
        const char *hdr = cols[c].header ? cols[c].header : "";
        html_add("<th>%s</th>", hdr);
    }
    html_add("<th>Action</th></tr>");

    /* Data rows */
    for (size_t r = 0; r < row_count; ++r) {
        html_add("<tr>");
        html_add("<form action=\"%s\" method=\"post\">", action_path);

        /* Optional hidden row ID */
        if (row_id_field_name && row_id_field_name[0] != '\0') {
            html_add("<input type=\"hidden\" name=\"%s\" value=\"%lu\">",
                     row_id_field_name, (unsigned long)r);
        }

        for (size_t c = 0; c < col_count; ++c) {
            const http_ui_table_col_t *col = &cols[c];
            const char *val = rows[r * col_count + c];  // flat indexing
            if (!val) val = "";

            html_add("<td>");

            bool is_enable_col = (c == enable_col_index);

            const char *id_attr = "";
            const char *event_attr = "";
            char id_buf[32];

            if (is_enable_col) {
                snprintf(id_buf, sizeof(id_buf), "cell_%lu_EC", (unsigned long)r);
                id_attr = id_buf;
                event_attr = "oninput=\"uiDynTblChanged(%lu)\" onchange=\"uiDynTblChanged(%lu)\"";
            }

            switch (col->type) {
            case HTTP_UI_COL_TEXT:
            case HTTP_UI_COL_PASSWORD:
            case HTTP_UI_COL_NUMBER:
            {
                const char *type_str =
                    (col->type == HTTP_UI_COL_TEXT)     ? "text"     :
                    (col->type == HTTP_UI_COL_PASSWORD) ? "password" :
                                                           "number";

                if (col->field_name) {
                    html_add("<input type=\"%s\" name=\"%s\" value=\"%s\"",
                             type_str, col->field_name, val);
                    if (is_enable_col) {
                        html_add(" id=\"%s\" ", id_attr);
                        html_add(event_attr, (unsigned long)r, (unsigned long)r);
                    }
                    html_add(">");
                } else {
                    html_add("%s", val);
                }
                break;
            }

            case HTTP_UI_COL_CHECKBOX:
                if (col->field_name) {
                    html_add("<input type=\"checkbox\" name=\"%s\" value=\"1\"",
                             col->field_name);
                    if (val[0] == '1' || strcasecmp(val, "true") == 0 ||
                        strcasecmp(val, "on") == 0) {
                        html_add(" checked");
                    }
                    if (is_enable_col) {
                        html_add(" id=\"%s\" ", id_attr);
                        html_add(event_attr, (unsigned long)r, (unsigned long)r);
                    }
                    html_add(">");
                } else {
                    html_add("%s", val);
                }
                break;

            case HTTP_UI_COL_SELECT:
                if (col->field_name && col->options && col->option_count > 0) {
                    html_add("<select name=\"%s\"", col->field_name);
                    if (is_enable_col) {
                        html_add(" id=\"%s\" ", id_attr);
                        html_add(" onchange=\"uiDynTblChanged(%lu)\"",
                                 (unsigned long)r);
                    }
                    html_add(">");

                    for (size_t oi = 0; oi < col->option_count; ++oi) {
                        const char *opt = col->options[oi] ? col->options[oi] : "";
                        html_add("<option value=\"%s\"%s>%s</option>",
                                 opt,
                                 (strcmp(opt, val) == 0 ? " selected" : ""),
                                 opt);
                    }
                    html_add("</select>");
                } else {
                    html_add("%s", val);
                }
                break;

            case HTTP_UI_COL_STATIC:
            default:
                html_add("%s", val);
                if (col->field_name && col->post_value) {
                    html_add("<input type=\"hidden\" name=\"%s\" value=\"%s\">",
                             col->field_name, val);
                }
                break;
            }

            html_add("</td>");
        }

        /* Action button column */
        html_add("<td>"
                 "<input type=\"submit\" id=\"btn_%lu\" value=\"%s\"",
                 (unsigned long)r, button_label);

        if (enable_col_index < col_count) {
            html_add(" disabled");
        }

        html_add("></td>");

        html_add("</form>");
        html_add("</tr>");   // <- missing before
    }

    html_add("</table>");
}

void http_ui_file_input(const char *name,
                        const char *label)
{
    html_add("<label>%s</label>"
             "<input type=\"file\" name=\"%s\"><br>",
             label, name);
}

/* -------------------- Header helpers -------------------- */

/* Get Content-Type header into buf (null-terminated).
 * Returns true if found, false otherwise.
 */
static bool http_ui_get_content_type(const http_srv_request_t *req,
                                     char *buf, size_t buf_len)
{
    if (!req || !req->headers || buf_len == 0) return false;

    const char *p   = req->headers;
    const char *end = req->headers + req->headers_len;

    while (p < end) {
        const char *line_end = strstr(p, "\r\n");
        if (!line_end) line_end = end;

        if ((size_t)(line_end - p) >= strlen("Content-Type:")) {
            if (strncasecmp(p, "Content-Type:", strlen("Content-Type:")) == 0) {
                const char *val = p + strlen("Content-Type:");
                while (val < line_end && (*val == ' ' || *val == '\t')) val++;

                size_t len = (size_t)(line_end - val);
                if (len >= buf_len) len = buf_len - 1;
                memcpy(buf, val, len);
                buf[len] = 0;
                return true;
            }
        }

        if (line_end >= end) break;
        p = line_end + 2;
    }

    return false;
}

/* -------------------- URL decoding + key=value parsing -------------------- */

/* Simple URL decode: modifies dest, returns dest */
static char *http_ui_url_decode(const char *src, char *dest, size_t dest_size)
{
    size_t di = 0;
    for (size_t si = 0; src[si] != 0 && di + 1 < dest_size; si++) {
        if (src[si] == '+') {
            dest[di++] = ' ';
        } else if (src[si] == '%' && isxdigit((unsigned char)src[si+1]) && isxdigit((unsigned char)src[si+2])) {
            char hex[3];
            hex[0] = src[si+1];
            hex[1] = src[si+2];
            hex[2] = 0;
            dest[di++] = (char)strtol(hex, NULL, 16);
            si += 2;
        } else {
            dest[di++] = src[si];
        }
    }
    dest[di] = 0;
    return dest;
}

/* Find param in &-separated key=value string. Returns pointer to internal static buffer. */
static char param_value[128];

static const char *http_ui_find_param_in_kv(const char *data,
                                            const char *key)
{
    if (!data || !key || !*key) return NULL;

    size_t key_len = strlen(key);
    const char *p = data;

    while (*p) {
        /* find start of key */
        const char *eq = strchr(p, '=');
        const char *amp = strchr(p, '&');

        if (!eq || (amp && eq > amp)) {
            /* malformed or no '=' before '&' -> skip segment */
            if (!amp) break;
            p = amp + 1;
            continue;
        }

        /* p..eq-1 is key */
        if ((size_t)(eq - p) == key_len && strncmp(p, key, key_len) == 0) {
            /* value is eq+1..amp-1 or end */
            const char *val_start = eq + 1;
            const char *val_end   = amp ? amp : (p + strlen(p));
            size_t len = (size_t)(val_end - val_start);
            if (len >= sizeof(param_value)) len = sizeof(param_value)-1;

            memcpy(param_value, val_start, len);
            param_value[len] = 0;

            /* URL decode in-place */
            static char decoded[128];
            http_ui_url_decode(param_value, decoded, sizeof(decoded));
            strncpy(param_value, decoded, sizeof(param_value));
            param_value[sizeof(param_value)-1] = 0;

            return param_value;
        }

        if (!amp) break;
        p = amp + 1;
    }

    return NULL;
}

/* -------------------- JSON parsing (now via cJSON) -------------------- */

/* NEW IMPLEMENTATION:
 * Use cJSON to parse the body and extract the field as a string.
 * - If JSON value is a string → return it.
 * - If JSON value is a number → stringify into param_value.
 * - If JSON value is bool → "true"/"false".
 */
const char *http_ui_get_json_field(const http_srv_request_t *req,
                                   const char *key)
{
    if (!req || !req->body || !key) return NULL;

    /* Check Content-Type is JSON-ish */
    char ctype[64];
    if (!http_ui_get_content_type(req, ctype, sizeof(ctype)))
        return NULL;
    if (strncasecmp(ctype, "application/json", strlen("application/json")) != 0)
        return NULL;

    /* cJSON parsing */
    /* We use ParseWithLength so body doesn't need to be NUL-terminated. */
    cJSON *root = cJSON_ParseWithLength(req->body, req->body_len);
    if (!root) {
        return NULL;
    }

    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!item) {
        cJSON_Delete(root);
        return NULL;
    }

    /* String */
    if (cJSON_IsString(item) && item->valuestring) {
        size_t len = strlen(item->valuestring);
        if (len >= sizeof(param_value)) len = sizeof(param_value) - 1;
        memcpy(param_value, item->valuestring, len);
        param_value[len] = 0;
        cJSON_Delete(root);
        return param_value;
    }

    /* Number */
    if (cJSON_IsNumber(item)) {
        snprintf(param_value, sizeof(param_value), "%g", item->valuedouble);
        cJSON_Delete(root);
        return param_value;
    }

    /* Bool */
    if (cJSON_IsBool(item)) {
        strncpy(param_value,
                cJSON_IsTrue(item) ? "true" : "false",
                sizeof(param_value));
        param_value[sizeof(param_value)-1] = 0;
        cJSON_Delete(root);
        return param_value;
    }

    /* Fallback: unsupported type */
    cJSON_Delete(root);
    return NULL;
}

/* -------------------- Unified param getter -------------------- */

const char *http_ui_get_param_ex(const http_srv_request_t *req,
                                 const char *key,
                                 http_ui_param_source_t *src_out)
{
    if (src_out) *src_out = HTTP_UI_PARAM_NONE;

    if (!req || !key || !*key) return NULL;

    /* 1) Check query string (GET) */
    if (req->query[0]) {
        const char *v = http_ui_find_param_in_kv(req->query, key);
        if (v) {
            if (src_out) *src_out = HTTP_UI_PARAM_QUERY;
            return v;
        }
    }

    /* Get Content-Type */
    char ctype[64] = {0};
    bool has_ctype = http_ui_get_content_type(req, ctype, sizeof(ctype));

    /* 2) Check URL-encoded form body */
    if (has_ctype &&
        strncasecmp(ctype, "application/x-www-form-urlencoded",
                    strlen("application/x-www-form-urlencoded")) == 0) {
        if (req->body && req->body_len > 0) {
            /* Body is key=value&key2=value2 */
            char tmp[HTTP_SRV_RX_BUFFER_SIZE];
            size_t copy_len = req->body_len;
            if (copy_len >= sizeof(tmp)) copy_len = sizeof(tmp) - 1;
            memcpy(tmp, req->body, copy_len);
            tmp[copy_len] = 0;

            const char *v = http_ui_find_param_in_kv(tmp, key);
            if (v) {
                if (src_out) *src_out = HTTP_UI_PARAM_FORM;
                return v;
            }
        }
    }

    /* 3) Check JSON (if Content-Type says so) */
    if (has_ctype &&
        strncasecmp(ctype, "application/json",
                    strlen("application/json")) == 0) {
        const char *v = http_ui_get_json_field(req, key);
        if (v) {
            if (src_out) *src_out = HTTP_UI_PARAM_JSON;
            return v;
        }
    }

    return NULL;
}

const char *http_ui_get_param(const http_srv_request_t *req, const char *key)
{
    return http_ui_get_param_ex(req, key, NULL);
}

/* -------------------- File upload parsing (multipart/form-data) --------- */

int http_ui_get_file(const http_srv_request_t   *req,
                     const char             *field_name,
                     http_ui_file_part_t    *out_part)
{
    if (!req || !req->body || !out_part || !field_name)
        return -1;

    char ctype[64];
    if (!http_ui_get_content_type(req, ctype, sizeof(ctype)))
        return -1;

    if (strncasecmp(ctype, "multipart/form-data", strlen("multipart/form-data")) != 0)
        return -1;

    /* Extract boundary=... */
    const char *bptr = strstr(ctype, "boundary=");
    if (!bptr) return -1;
    bptr += strlen("boundary=");

    char boundary[64];
    size_t b_len = 0;
    while (*bptr && *bptr != ';' && !isspace((unsigned char)*bptr) && b_len + 1 < sizeof(boundary)) {
        boundary[b_len++] = *bptr++;
    }
    boundary[b_len] = 0;
    if (!boundary[0]) return -1;

    /* Construct boundary markers */
    char boundary_start[80];
    snprintf(boundary_start, sizeof(boundary_start), "--%s", boundary);

    const char *body     = req->body;
    const char *body_end = req->body + req->body_len;

    const char *p = body;

    while (p < body_end) {
        /* Find next boundary */
        const char *b = strstr(p, boundary_start);
        if (!b || b >= body_end)
            break;

        b += strlen(boundary_start);

        if (b + 2 > body_end)
            break;

        /* Skip CRLF after boundary */
        if (b[0] == '\r' && b[1] == '\n') {
            b += 2;
        }

        const char *part_hdr = b;
        const char *part_hdr_end = strstr(part_hdr, "\r\n\r\n");
        if (!part_hdr_end || part_hdr_end >= body_end)
            break;

        /* Parse Content-Disposition header line */
        const char *cd_line_end = strstr(part_hdr, "\r\n");
        if (!cd_line_end || cd_line_end > part_hdr_end)
            break;

        char cd_line[256];
        size_t cd_len = (size_t)(cd_line_end - part_hdr);
        if (cd_len >= sizeof(cd_line)) cd_len = sizeof(cd_line) - 1;
        memcpy(cd_line, part_hdr, cd_len);
        cd_line[cd_len] = 0;

        if (strncasecmp(cd_line, "Content-Disposition:", strlen("Content-Disposition:")) != 0) {
            p = part_hdr_end + 4;
            continue;
        }

        /* Look for name="field_name" and filename="..." */
        char *name_pos = strstr(cd_line, "name=\"");
        if (!name_pos) {
            p = part_hdr_end + 4;
            continue;
        }

        name_pos += strlen("name=\"");
        char *name_end = strchr(name_pos, '\"');
        if (!name_end) {
            p = part_hdr_end + 4;
            continue;
        }

        char found_name[64];
        size_t name_len = (size_t)(name_end - name_pos);
        if (name_len >= sizeof(found_name)) name_len = sizeof(found_name) - 1;
        memcpy(found_name, name_pos, name_len);
        found_name[name_len] = 0;

        if (strcmp(found_name, field_name) != 0) {
            p = part_hdr_end + 4;
            continue;
        }

        /* Get filename if present */
        char *fn_pos = strstr(cd_line, "filename=\"");
        if (fn_pos) {
            fn_pos += strlen("filename=\"");
            char *fn_end = strchr(fn_pos, '\"');
            size_t fn_len = fn_end ? (size_t)(fn_end - fn_pos) : 0;
            if (fn_len >= sizeof(out_part->filename)) fn_len = sizeof(out_part->filename) - 1;
            memcpy(out_part->filename, fn_pos, fn_len);
            out_part->filename[fn_len] = 0;
        } else {
            out_part->filename[0] = 0;
        }

        /* No Content-Type parsing for simplicity (can be added like before) */
        out_part->content_type[0] = 0;

        /* Data starts after header CRLFCRLF */
        const uint8_t *data_start = (const uint8_t *)(part_hdr_end + 4);

        /* Find next boundary to know data length */
        const char *next_boundary = strstr((const char *)data_start, boundary_start);
        if (!next_boundary) {
            next_boundary = body_end;
        }

        const uint8_t *data_end = (const uint8_t *)next_boundary;
        if (data_end > (const uint8_t *)body_end)
            data_end = (const uint8_t *)body_end;

        /* Trim trailing CRLF */
        while (data_end > data_start &&
               (data_end[-1] == '\r' || data_end[-1] == '\n')) {
            data_end--;
        }

        out_part->data   = data_start;
        out_part->length = (uint32_t)(data_end - data_start);

        return 0;   /* success */
    }

    return -1;
}

