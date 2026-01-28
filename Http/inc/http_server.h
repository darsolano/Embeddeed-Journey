/*
 * https_server.h
 *
 *  Created on: Dec 9, 2025
 *      Author: Daruin Solano
 */

#ifndef DRIVERS_WIFI_NET_NET_INC_HTTP_SERVER_H_
#define DRIVERS_WIFI_NET_NET_INC_HTTP_SERVER_H_

#ifndef NET_INC_HTTP_SRV_H_
#define NET_INC_HTTP_SRV_H_

/*
Usage example (same style as your new_tcp_server)
extern net_hnd_t     hnet;
extern net_sockhnd_t sock;  // can be NULL; http_srv_bind will create if NULL

void new_http_server(void)
{
    http_srv_t hs;

    while (1) {
        msg_info("HTTP: initializing HTTP server...");

        if (http_srv_bind(hnet, sock, &hs, 80, "HTTP_SRV") != HTTP_OK) {
            msg_error("HTTP: bind failed, retrying...\n");
            // optional delay here
            continue;
        }

        while (hs.running) {
            // Handle exactly one HTTP client: wait, parse, respond, close
            http_srv_handle_once(&hs, http_srv_default_cb);
        }

        http_srv_close(&hs);
    }
}


Now you have a modular HTTP server layer that:

Maps cleanly to your net_srv_* primitives.

Handles realistic HTTP parsing (request line, path/query, headers, Content-Length, body).

Provides reusable helpers to build responses and simple per-request callbacks.

From here you can easily add your own routes in a custom callback:

int my_cb(http_srv_t *hs, const http_request_t *req)
{
    if (strcmp(req->path, "/") == 0) {
        // send index HTML
    } else if (strcmp(req->path, "/time") == 0) {
        // use your NTP/RTC to reply with JSON
    } else {
        // 404
        const char *msg = "Not found";
        http_srv_send_response(hs, 404, "Not Found",
                               "text/plain",
                               (const uint8_t*)msg,
                               strlen(msg),
                               "");
    }
    return HTTP_OK;
}


You can plug that into http_srv_handle_once(&hs, my_cb
 */

#include <stdint.h>
#include <stdbool.h>

#include "net_internal.h"
#include "http_lib.h"  // for HTTP_OK / HTTP_ERR

#ifdef __cplusplus
extern "C" {
#endif

#define HTTP_SRV_RX_BUFFER_SIZE 1400

/* HTTP Method */
typedef enum {
    HTTP_GET,
    HTTP_POST,
    HTTP_PUT,
    HTTP_DELETE,
    HTTP_UNKNOWN
} http_method_t;

typedef enum {
    HTTP_SRV_STATE_RUNNING = 0,
    HTTP_SRV_STATE_DRAINING,
    HTTP_SRV_STATE_STOPPED
} http_srv_state_t;

typedef struct {
	char		 	method[8];          /* "GET", "POST", ... */
    char  			path[128];          /* "/index.html" (no query) */
    char  			query[128];         /* everything after '?' (if any) */
    int   			http_major;         /* usually 1 */
    int   			http_minor;         /* usually 1 */
    char *			headers;            /* pointer into internal buffer */
    uint32_t 		headers_len;
    char *			body;               /* pointer into internal buffer */
    uint32_t 		body_len;        /* bytes of body actually received */
    uint32_t 		content_length;  /* from Content-Length (0 if absent) */
    net_srv_conn_t *srv;			/*Pointer to server handler entity*/
} http_srv_request_t;

/* HTTP server context on top of net_srv_conn_t */
typedef struct {
    net_hnd_t         nethnd;
    net_srv_conn_t    srv;
    uint8_t           rxbuf[HTTP_SRV_RX_BUFFER_SIZE];
    uint32_t          rxlen;
    bool              running;
    http_srv_state_t  state;
    uint16_t          port;
} http_srv_t;

/* User callback: handle one HTTP request and send response */
typedef int (*http_srv_cb_t)(http_srv_t            *hs,
                             const http_srv_request_t  *req);


/* Handler function type */
typedef int (*route_handler_t)(http_srv_t *hs, const http_srv_request_t *req);
/* Route table entry */
typedef struct {
    const char       *path;
    const char       *method;
    route_handler_t   handler;
} http_route_t;




/* Send an HTTP response with headers + body.
 *  - status_code: e.g. 200, 404
 *  - reason: "OK", "Not Found"
 *  - content_type: e.g. "text/html", "application/json"
 *  - body/body_len: payload
 *  - extra_headers: optional header lines ending with "\r\n" or "" (no extra)
 */
int http_srv_send_response(http_srv_t     *hs,
                           int             status_code,
                           const char     *reason,
                           const char     *content_type,
                           const uint8_t  *body,
                           uint32_t        body_len,
                           const char     *extra_headers);


int http_srv_init(http_srv_t *hs, net_hnd_t hnet, uint16_t port);

void http_srv_run(http_srv_t *hs);


/* Convenience: handle exactly one client:
 *  1) http_srv_listen
 *  2) http_srv_recv_request
 *  3) cb(hs, &req)
 *  4) http_srv_next_conn
 */
int http_srv_handle_once(http_srv_t  *hs);

int http_srv_close(http_srv_t *hs);
int http_srv_next_conn(http_srv_t *hs);
int http_srv_restart(http_srv_t *hs);
void http_srv_apply_timeouts(http_srv_t *hs);


#include "http_ui.h"	// widgets, json, parse


#ifdef __cplusplus
}
#endif

#endif /* NET_INC_HTTP_SRV_H_ */



#endif /* DRIVERS_WIFI_NET_NET_INC_HTTP_SERVER_H_ */
