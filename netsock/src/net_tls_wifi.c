/*
 * net_tls_wifi.c
 *
 *  Created on: Jan 7, 2026
 *      Author: Daruin Solano
 */

/**
  ******************************************************************************
  * @file    net_tcp_wifi.c
  * @author  MCD Application Team
  * @brief   Network abstraction at transport layer level. TCP implementation
  *          on ST WiFi connectivity API.
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
#ifndef USE_MBED_TLS

/* Private defines -----------------------------------------------------------*/
/* The socket timeout of the non-blocking sockets is supposed to be 0.
 * But the underlying component does not necessarily supports a non-blocking
 * socket interface.
 * The NOBLOCKING timeouts are intended to be set to the lowest possible value
 * supported by the underlying component. */
#define NET_DEFAULT_NOBLOCKING_WRITE_TIMEOUT  1
#define NET_DEFAULT_NOBLOCKING_READ_TIMEOUT   1

/* Workaround for the incomplete WIFI_LL API */
#ifdef ES_WIFI_MAX_SSID_NAME_SIZE
#define WIFI_PAYLOAD_SIZE                           ES_WIFI_PAYLOAD_SIZE
#endif

int net_sock_create_tls_wifi(net_hnd_t nethnd, net_sockhnd_t * sockhnd, net_proto_t proto);
int net_sock_open_tls_wifi(net_sockhnd_t sockhnd, const char * hostname, int dstport, int localport);
extern int net_sock_recv_tcp_wifi(net_sockhnd_t sockhnd, uint8_t * buf, size_t len);
extern int net_sock_send_tcp_wifi(net_sockhnd_t sockhnd, const uint8_t * buf, size_t len);
extern int net_sock_close_tcp_wifi(net_sockhnd_t sockhnd);
extern int net_sock_destroy_tcp_wifi(net_sockhnd_t sockhnd);


int net_sock_create_tls_wifi(net_hnd_t nethnd, net_sockhnd_t * sockhnd, net_proto_t proto)
{
  int rc = NET_ERR;
  net_ctxt_t *ctxt = (net_ctxt_t *) nethnd;
  net_sock_ctxt_t *sock = NULL;
  WiFi_Tls_t *wifitls = NULL;
  WiFi_MQTT_Config_t *mqttData = NULL;

  sock = net_malloc(sizeof(net_sock_ctxt_t));
  if (sock == NULL)
  {
    msg_error("net_sock_create allocation failed.\n");
    rc = NET_ERR;
  }
  else
  {
    memset(sock, 0, sizeof(net_sock_ctxt_t));
    sock->net = ctxt;
    sock->next = ctxt->sock_list;

    if (proto == NET_PROTO_TLS || proto == NET_PROTO_MQTT){
		wifitls = net_malloc(sizeof(WiFi_Tls_t));
		if (wifitls == NULL)
		{
		  msg_error("net_sock_create allocation wifi tls data context failed.\n");
		  net_free(sock);
		  rc = NET_ERR;
		}else{
		    memset(wifitls, 0, sizeof(WiFi_Tls_t));
			sock->wifi_tls = wifitls;
		}
    }

    if (proto == NET_PROTO_MQTT){
		mqttData = net_malloc(sizeof(WiFi_MQTT_Config_t));
		if (mqttData == NULL)
		{
		  msg_error("net_sock_create allocation mqtt context failed.\n");
		  net_free(sock);
		  rc = NET_ERR;
		}else{
		    memset(mqttData, 0, sizeof(WiFi_MQTT_Config_t));
			sock->mqtt_ctx = mqttData;
		}
    }

    sock->methods.open      	= (net_sock_open_tls_wifi);
    switch(proto)
    {
      case NET_PROTO_TLS:
      case NET_PROTO_MQTT:
        sock->methods.recv      = (net_sock_recv_tcp_wifi);
        sock->methods.send      = (net_sock_send_tcp_wifi);
       break;
      default:
        free(sock);
        return NET_PARAM;
    }
    sock->methods.close     = (net_sock_close_tcp_wifi);
    sock->methods.destroy   = (net_sock_destroy_tcp_wifi);
    sock->proto             = proto;
    sock->blocking          = NET_DEFAULT_BLOCKING;
    sock->read_timeout      = NET_DEFAULT_BLOCKING_READ_TIMEOUT;
    sock->write_timeout     = NET_DEFAULT_BLOCKING_WRITE_TIMEOUT;
    ctxt->sock_list         = sock; /* Insert at the head of the list */
    *sockhnd = (net_sockhnd_t) sock;

    rc = NET_OK;
  }

  return rc;
}


/**
  * @brief  High-level wrapper to open an AWS MQTT connection using the Net Library context.
  * @param  sockhnd: The socket handle created by net_sock_create_wifi.
  * @param  hostname: AWS Endpoint (e.g., "xxx-ats.iot.us-east-1.amazonaws.com").
  * @param  remoteport: Usually 8883.
  * @param  config: AWS specific topics and Client ID.
  * @retval NET_OK if success, NET_ERR if failure.
  */
int net_sock_open_tls_wifi(net_sockhnd_t sockhnd, const char * hostname, int dstport, int localport)
{
  net_sock_ctxt_t *sock = (net_sock_ctxt_t *)sockhnd;
  uint8_t ip_addr[4]  = {0};

  if (sock->wifi_tls->tls_ca_certs || sock->wifi_tls->tls_dev_cert || sock->wifi_tls->tls_dev_key){
	  /* Must setopt all tls security data before calling this function*/
	  if (WIFI_SetCertificatesCredentials(sock->wifi_tls, WIFI_CRED_MODE_MQTT) != WIFI_STATUS_OK){
		  msg_error("Could not set the credential TLS secure connection: %s\n", hostname);
		  return NET_ERR;
	  }
  }

  if (sock->proto == NET_PROTO_MQTT){
	  /* 1. Resolve Hostname (Reusing logic from your file) */
	  if (WIFI_GetHostAddress((char *)hostname, ip_addr, sizeof(ip_addr)) != WIFI_STATUS_OK) {
		  msg_error("Could not resolve mqtt server endpoint: %s\n", hostname);
		  return NET_ERR;
	  }


	  /* 4. Call the low-level firmware interface */
	  // We use the global EsWifiObj used by the B-L475E-IOT01A1
	  if (WIFI_MQTTIoTConnect((uint32_t)sock->underlying_sock_ctxt, ip_addr, sock->mqtt_ctx) != WIFI_STATUS_OK) {
		  msg_error("mqtt Handshake Failed on Socket %d\n", (int)sock->underlying_sock_ctxt);
		  return NET_ERR;
	  }
	  return NET_OK;
  }
  if (sock->proto == NET_PROTO_TLS){
	  /* 1. Resolve Hostname (Reusing logic from your file) */
	  if (WIFI_GetHostAddress((char *)hostname, ip_addr, sizeof(ip_addr)) != WIFI_STATUS_OK) {
		  msg_error("Could not resolve tls server endpoint: %s\n", hostname);
		  return NET_ERR;
	  }

	  ES_WIFI_TLS_SEC_MODE_t mode;

	  if (!sock->wifi_tls->tls_srv_verification && (sock->wifi_tls->tls_ca_certs == NULL || !sock->wifi_tls->tls_ca_certs)){
		  mode = TLS_NONE;
	  }
	  if (sock->wifi_tls->tls_srv_verification && sock->wifi_tls->tls_ca_certs){/* at least root ca must be present*/
		  mode = TLS_ROOTCA;
	  }
	  if (sock->wifi_tls->tls_srv_verification
			  && sock->wifi_tls->tls_ca_certs
			  && sock->wifi_tls->tls_dev_cert
			  && sock->wifi_tls->tls_dev_key){
		  mode = TLS_MUTUAL;
	  }


	  if (WIFI_OpenClientConnection((uint32_t)sock->underlying_sock_ctxt,
			  WIFI_TLS_PROTOCOL, mode, ip_addr, dstport, 0) != WIFI_STATUS_OK)
	  {
		  msg_error("Could not open tls client connection to endpoint: %s\n", hostname);
		  return NET_ERR;
	  }

  }

  return NET_OK;
}

#endif

