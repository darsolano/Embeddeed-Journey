/*
 * http_server.c
 *
 *  Created on: Dec 9, 2025
 *      Author: Daruin Solano
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stm32l4xx_hal.h>

#include "http_server.h"
#include "msg.h"


#define HTTP_ERR_LIMIT        5
#define HTTP_NET_DOWN_LIMIT   3
#define HTTP_RESTART_DELAY_MS 50




/* ------------------------------------------------------------------------- */
/* Internal helpers                                                          */
/* ------------------------------------------------------------------------- */

/* Find position of "\r\n\r\n" in buffer; return index AFTER it, or -1 if not found */
static int http_find_headers_end(const uint8_t *buf, uint32_t len)
{
    if (len < 4) return -1;
    for (uint32_t i = 0; i <= len - 4; i++) {
        if (buf[i]   == '\r' &&
            buf[i+1] == '\n' &&
            buf[i+2] == '\r' &&
            buf[i+3] == '\n') {
            return (int)(i + 4);  /* position of body (just after \r\n\r\n) */
        }
    }
    return -1;
}

/* Very simple parser: "METHOD PATH HTTP/x.y" */
static int http_parse_request_line(char *line, http_srv_request_t *req) {
	char *method = strtok(line, " ");
	char *uri = strtok(NULL, " ");
	char *vers = strtok(NULL, " ");

	if (!method || !uri || !vers) {
		return HTTP_ERR;
	}

	/* METHOD */
	strncpy(req->method, method, sizeof(req->method) - 1);
	req->method[sizeof(req->method) - 1] = 0;

	/* Split URI into path and query */
	char *q = strchr(uri, '?');
	if (q) {
		*q = 0;
		q++;
		strncpy(req->query, q, sizeof(req->query) - 1);
		req->query[sizeof(req->query) - 1] = 0;
	} else {
		req->query[0] = 0;
	}
	strncpy(req->path, uri, sizeof(req->path) - 1);
	req->path[sizeof(req->path) - 1] = 0;

	/* Version "HTTP/x.y" */
	int major = 1, minor = 1;
	if (sscanf(vers, "HTTP/%d.%d", &major, &minor) != 2) {
		return HTTP_ERR;
	}
	req->http_major = major;
	req->http_minor = minor;

	return HTTP_OK;
}


/* Find Content-Length in headers (case-insensitive); return value or 0 if not present */
static uint32_t http_parse_content_length(const char *headers, uint32_t headers_len)
{
    const char *p = headers;
    const char *end = headers + headers_len;
    const char *needle = "Content-Length:";
    size_t needle_len = strlen(needle);

    while (p < end) {
        /* find end of this line */
        const char *line_end = strstr(p, "\r\n");
        if (!line_end || line_end > end) {
            line_end = end;
        }

        /* case-insensitive compare */
        if ((size_t)(line_end - p) >= needle_len) {
            if (strncasecmp(p, needle, needle_len) == 0) {
                const char *val = p + needle_len;
                while (*val == ' ' || *val == '\t') val++;
                return (uint32_t)atoi(val);
            }
        }

        if (line_end >= end) break;
        p = line_end + 2; /* skip \r\n */
    }

    return 0;
}



/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */


static int http_srv_recv_request(http_srv_t *hs, http_srv_request_t *req)
{
    if (!hs || !req) return HTTP_ERR;

    memset(req, 0, sizeof(*req));
    hs->rxlen = 0;
    memset(hs->rxbuf, 0, sizeof(hs->rxbuf));

    int hdr_end = -1;

    /* 1) Read until we see full headers (\r\n\r\n) or buffer full */
    while (hs->rxlen < sizeof(hs->rxbuf) - 1)
    {
        int rc = net_sock_recv(hs->srv.sock,
                               hs->rxbuf + hs->rxlen,
                               sizeof(hs->rxbuf) - 1 - hs->rxlen);

        /* ---- CASE A: no data at all on this connection ---- */
        /* Adjust rc==-3 to whatever your WiFi driver returns for "no data/timeout" */
        if (rc == 0 || rc == NET_TIMEOUT) {
            if (hs->rxlen == 0) {
                msg_debug("http_srv_recv_request: no data (rc=%d) => HTTP_NO_REQUEST", rc);
                return HTTP_NO_REQUEST;
            } else {
                msg_error("http_srv_recv_request: connection closed mid-headers rc=%d", rc);
                return HTTP_ERR;
            }
        }

        /* ---- CASE B: real error ---- */
        if (rc < 0) {
            msg_error("http_srv_recv_request: recv error rc=%d", rc);
            return HTTP_ERR;
        }

        /* ---- CASE C: got some data ---- */
        hs->rxlen += (uint32_t)rc;
        hs->rxbuf[hs->rxlen] = 0;  /* keep null-terminated */

        hdr_end = http_find_headers_end(hs->rxbuf, hs->rxlen);
        if (hdr_end >= 0) {
            break; /* full headers available */
        }
    }

    if (hdr_end < 0) {
        msg_error("http_srv_recv_request: headers too big or no terminator");
        return HTTP_ERR;
    }

    /* 2) Parse first line: METHOD PATH HTTP/x.y */
    char *first_line = (char *)hs->rxbuf;
    char *crlf = strstr(first_line, "\r\n");
    if (!crlf) {
        msg_error("http_srv_recv_request: malformed request line");
        return HTTP_ERR;
    }
    *crlf = 0;   /* terminate first line */

    if (http_parse_request_line(first_line, req) != HTTP_OK) {
        msg_error("http_srv_recv_request: cannot parse request line '%s'", first_line);
        return HTTP_ERR;
    }

    msg_debug("HTTP: %s %s?%s HTTP/%d.%d",
              req->method, req->path,
              req->query[0] ? req->query : "",
              req->http_major, req->http_minor);

    /* 3) Headers and body pointers into rxbuf */
    req->headers = crlf + 2;  /* skip "\r\n" */
    req->headers_len = (uint32_t)(hdr_end - (int)(req->headers - (char*)hs->rxbuf));

    req->body = (char *)hs->rxbuf + hdr_end;
    req->body_len = hs->rxlen - (uint32_t)hdr_end;

    /* 4) Content-Length if present */
    req->content_length = http_parse_content_length(req->headers, req->headers_len);

    /* 5) If body incomplete and Content-Length known, read the rest */
    if (req->content_length > req->body_len) {
        uint32_t needed = req->content_length - req->body_len;

        if ((uint32_t)hdr_end + req->content_length > sizeof(hs->rxbuf)) {
            msg_error("http_srv_recv_request: body too large for buffer");
            return HTTP_ERR;
        }

        while (needed > 0) {
            int rc = net_sock_recv(hs->srv.sock,
                                   (uint8_t *)req->body + req->body_len,
                                   needed);
            if (rc <= 0) {
                msg_error("http_srv_recv_request: recv body rc=%d", rc);
                return HTTP_ERR;
            }
            req->body_len += (uint32_t)rc;
            needed        -= (uint32_t)rc;
        }
    }

    /* Attach srv pointer so handlers can see connection info if needed */
    req->srv = &hs->srv;

    return HTTP_OK;
}

static int send_all(net_sockhnd_t sock, const uint8_t *buf, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        int rc = net_sock_send(sock, (uint8_t *)(buf + sent), len - sent);
        if (rc <= 0) {
            msg_error("send_all: rc=%d, sent=%u of %u\n",
                      rc, (unsigned)sent, (unsigned)len);
            return HTTP_ERR;
        }
        sent += (size_t)rc;
    }
    return HTTP_OK;
}

int http_srv_send_response(http_srv_t     *hs,
                           int             status_code,
                           const char     *reason,
                           const char     *content_type,
                           const uint8_t  *body,
                           uint32_t        body_len,
                           const char     *extra_headers)
{
    if (!hs || !hs->srv.sock) return HTTP_ERR;

    if (!reason)       reason       = "OK";
    if (!content_type) content_type = "text/plain";
    if (!extra_headers) extra_headers = "";

    char header[256];

    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 %d %s\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %lu\r\n"
                              "%s"
                              "Connection: close\r\n"
                              "\r\n",
                              status_code,
                              reason,
                              content_type,
                              (unsigned long)body_len,
                              extra_headers);

    if (header_len < 0 || header_len >= (int)sizeof(header)) {
        msg_error("http_srv_send_response: header too large\n");
        return HTTP_ERR;
    }


    int rc = send_all(hs->srv.sock, (const uint8_t *)header, header_len);
    if (rc < 0) {
        msg_debug("http_srv_send_response: send header rc=%d\n", rc);
        return HTTP_ERR;
    }

    if (body && body_len > 0) {
        rc = send_all(hs->srv.sock, body, body_len);
        if (rc < 0) {
            msg_debug("http_srv_send_response: send body rc=%d\n", rc);
            return HTTP_ERR;
        }
    }

    return HTTP_OK;
}


int http_srv_handle_once(http_srv_t *hs)
{
    int rc;

    /* 1) Wait for a client connection (blocks inside wifi driver) */
    rc = net_srv_listen(&hs->srv);
    if (rc != NET_OK) {
    	if (rc == NET_ERR || rc == NET_TIMEOUT){
			msg_error("http_srv_handle_once: net_srv_listen rc=%d\n", rc);
			net_srv_next_conn(&hs->srv);  /* close client connection */
			return HTTP_ERR;
		}
    }

    /* 2) Parse one HTTP request from this client */
    http_srv_request_t req;
    rc = http_srv_recv_request(hs, &req);

    if (rc == HTTP_NO_REQUEST) {
        msg_debug("http_srv_handle_once: no HTTP request on this connection\n");
        net_srv_next_conn(&hs->srv);  /* close client connection */
        return HTTP_OK;
    }

    if (rc != HTTP_OK) {
        msg_error("http_srv_handle_once: bad request or parse error\n");
        net_srv_next_conn(&hs->srv);
        return HTTP_ERR;
    }

    /* 3) Route dispatch */
    bool matched = false;
    int handler_rc = HTTP_OK;

    for (size_t i = 0; i < routes_count; i++) {
        const char *route_path   = routes[i].path;
        const char *route_method = routes[i].method;  /* may be NULL */

        if (strcmp(req.path, route_path) == 0 &&
            (!route_method || strcmp(req.method, route_method) == 0))
        {
            handler_rc = routes[i].handler(hs, &req);
            matched = true;
            break;
        }
    }

    if (!matched) {
        /* 404 response */
        const char *msg = "404 Not Found\r\n";
        http_srv_send_response(hs,
                               404, "Not Found",
                               "text/plain",
                               (const uint8_t*)msg,
                               strlen(msg),
                               NULL);
    }

    /* 4) Always close this client connection after one request */
    net_srv_next_conn(&hs->srv);

    return handler_rc;
}



int http_srv_init(http_srv_t *hs, net_hnd_t hnet, uint16_t port)
{
    if (!hs) return HTTP_ERR;
    memset(hs, 0, sizeof(*hs));

    hs->srv.localport = port;
    hs->srv.protocol  = NET_PROTO_TCP;
    hs->srv.name      = "http_server";
    hs->srv.timeout   = 0;    /* or as you like */
    hs->nethnd    	  = hnet;
    hs->port 		  = port;

    int rc = net_srv_bind(hnet, NULL, &hs->srv);
    if (rc != NET_OK) {
        msg_error("http_srv_init: net_srv_bind rc=%d\n", rc);
        return HTTP_ERR;
    }

    hs->running = 1;
    http_srv_run(hs);
    return HTTP_OK;
}


void http_srv_run(http_srv_t *hs)
{
    uint32_t err_count = 0;
    uint32_t netdown_count = 0;

    hs->state = HTTP_SRV_STATE_RUNNING;

    while (hs->running) {

        /* 1) Network supervision */
        if (!net_is_up(hs->nethnd)) {
            netdown_count++;
            if (netdown_count >= HTTP_NET_DOWN_LIMIT) {
                msg_error("HTTP: network down, restarting server...");
                http_srv_restart(hs);
                netdown_count = 0;
                err_count = 0;
            }
            // watchdog_kick();
            HAL_Delay(10);
            continue;
        } else {
            netdown_count = 0;
        }

        /* 2) Serve one client/request (should not block forever) */
        int rc = http_srv_handle_once(hs);

        if (rc != HTTP_OK) err_count++;
        else err_count = 0;

        /* 3) Too many errors -> restart */
        if (err_count >= HTTP_ERR_LIMIT) {
            msg_error("HTTP: error storm, restarting server...");
            http_srv_restart(hs);
            err_count = 0;
        }

        /* 4) Watchdog safe point */
        // watchdog_kick();
        HAL_Delay(1);
    }
}

int http_srv_restart(http_srv_t *hs)
{
    if (!hs) return HTTP_ERR;

    msg_error("HTTP server restarting...");

    /* Close any active client */
    http_srv_next_conn(hs);

    /* Stop server completely */
    http_srv_close(hs);

    /* Give Wi-Fi module time to recover */
    HAL_Delay(50);

    /* Re-bind server */
    if( http_srv_init(hs, hs->nethnd, hs->port) == HTTP_ERR){
    	msg_error("restarting the system...");
    	NVIC_SystemReset();
    }
    msg_debug("http server restarted successfully...");
    return HTTP_OK;
}

int http_srv_next_conn(http_srv_t *hs)
{
    if (!hs) return HTTP_ERR;

    return (net_srv_next_conn(&hs->srv) == NET_OK)
           ? HTTP_OK
           : HTTP_ERR;
}

int http_srv_close(http_srv_t *hs)
{
    if (!hs) return HTTP_ERR;

    hs->running = false;

    if (net_srv_close(&hs->srv) != NET_OK) {
        msg_error("http_srv_close: net_srv_close failed");
        return HTTP_ERR;
    }

    memset(&hs->srv, 0, sizeof(hs->srv));
    return HTTP_OK;
}

void http_srv_apply_timeouts(http_srv_t *hs)
{
    uint32_t to_ms = 2000;
    /* Only if your net_sock_setopt supports this key */
    net_sock_setopt(hs->srv.sock, "sock_read_timeout", (uint8_t*)&to_ms, sizeof(to_ms));
}
