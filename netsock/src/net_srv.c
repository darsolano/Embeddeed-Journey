/*
 * net_srv.c
 *
 *  Created on: Mar 24, 2025
 *      Author: Daruin Solano
 */
/**
  ******************************************************************************
  * @file    net_srv.c
  * @author  MCD Application Team
  * @brief   Network abstraction at transport layer level.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2017 STMicroelectronics International N.V.
  * All rights reserved.</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice,
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other
  *    contributors to this software may be used to endorse or promote products
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under
  *    this license is void and will automatically terminate your rights under
  *    this license.
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "net_internal.h"

/* Private defines -----------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Functions Definition ------------------------------------------------------*/

int net_srv_bind(net_hnd_t nethnd, net_sockhnd_t sockhnd, net_srv_conn_t* srv)
{
	int rc = NET_NOT_FOUND;
	WIFI_Protocol_t proto;
	if (nethnd != NULL){
		rc = NET_OK;
	}
	if (rc == NET_OK)
	{
		if (sockhnd == NULL)
		{
			rc = net_sock_create(nethnd, &sockhnd, srv->protocol);
		}
		if ( rc == NET_OK)
		{
			net_sock_ctxt_t *sock = (net_sock_ctxt_t * ) sockhnd;
			proto = (srv->protocol==NET_PROTO_TCP) ? WIFI_TCP_PROTOCOL:WIFI_UDP_PROTOCOL;
			if (WIFI_StartServer((uint32_t)sock->underlying_sock_ctxt, proto, 2, srv->name, srv->localport) == WIFI_STATUS_OK)
			{
				srv->sock = sockhnd;
				msg_debug("server has started: %s...", srv->name);
				rc = NET_OK;
			}
		}
	}
	if (rc != NET_OK){
		msg_error("error in network connection...");
	}
	return rc;
}

/*this function will wait forever for a remote connection*/
int net_srv_listen(net_srv_conn_t* srv )
{
	int rc = NET_ERR;
	uint8_t ip[4] = {0};
	uint16_t port;
	net_sock_ctxt_t *sock = (net_sock_ctxt_t * ) srv->sock;
	while( (rc = WIFI_WaitServerConnection((uint32_t)sock->underlying_sock_ctxt, 2000, ip, sizeof(ip), &port)) != WIFI_STATUS_OK);

	if (rc == WIFI_STATUS_OK){
		srv->remoteport = port;
		for (int i=0;i<4;i++){
			srv->remoteip.ip[12+i] = ip[i];
		}
	}
	return rc;
}

int net_srv_next_conn(net_srv_conn_t* srv)
{
	int rc = NET_ERR;
	net_sock_ctxt_t *sock = (net_sock_ctxt_t * ) (srv->sock);
	if (WIFI_CloseServerConnection((uint32_t)sock->underlying_sock_ctxt) == WIFI_STATUS_OK)
	{
		rc = NET_OK;
	}
	return rc;
}

int net_srv_close(net_srv_conn_t* srv)
{
    if (!srv || !srv->sock) return NET_OK; /* nothing to close */

    net_sock_ctxt_t *sock = (net_sock_ctxt_t *)srv->sock;

    /* If underlying socket id is invalid, treat as already closed */
    if ((int)sock->underlying_sock_ctxt <= 0 || sock->underlying_sock_ctxt == (net_sockhnd_t)-1) {
        net_sock_destroy(srv->sock);   // your internal destroy
        memset(srv, 0, sizeof(*srv));
        return NET_OK;
    }

    int rc = NET_ERR;

    if (WIFI_StopServer((uint32_t)sock->underlying_sock_ctxt) == WIFI_STATUS_OK) {
        rc = NET_OK;
    } else {
        /* DON’T block restart just because the module refused stop */
        msg_error("net_srv_close: WIFI_StopServer failed, forcing local destroy");
        rc = NET_OK; /* force success so upper layers can restart */
    }

    sock->underlying_sock_ctxt = (net_sockhnd_t)-1;
    net_sock_destroy(srv->sock);
    memset(srv, 0, sizeof(*srv));

    return rc;
}
