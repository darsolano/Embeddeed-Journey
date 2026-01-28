/*
 * net_mqtt.c
 *
 *  Created on: Feb 1, 2025
 *      Author: Daruin Solano
 */
/**
 ******************************************************************************
 * @file    net.c
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

#include <net_mqtt.h>
#include <rtc.h>
#include "timingSystem.h"
#include "timedate.h"
#include <stddef.h>
#include "MQTTClient.h"

#ifdef MQTT_TASK
#include "cmsis_os.h"
#endif

/* Private define ------------------------------------------------------------*/



/** Function to read data from the socket opened into provided buffer
 * @param - Address of Network Structure
 *        - Buffer to store the data read from socket
 *        - Expected number of bytes to read from socket
 *        - Timeout in milliseconds
 * @return - Number of Bytes read on SUCCESS
 *         - -1 on FAILURE
 **/
int network_read(Network *n, unsigned char *buffer, int len, int timeout_ms) {
	int bytes;

	if (n->sockHandle == NULL) return NET_NOT_FOUND;

	bytes = net_sock_recv((net_sockhnd_t) n->sockHandle, buffer, len);
	if (bytes < 0 && bytes != NET_TIMEOUT) {
		msg_error("net_sock_recv failed - %d\n", bytes);
	}

	return (bytes==NET_TIMEOUT)?NET_OK:bytes;	// timeout is normal response, we catch it anyway but returns ok
}

/** Function to write data to the socket opened present in provided buffer
 * @param - Address of Network Structure
 *        - Buffer storing the data to write to socket
 *        - Number of bytes of data to write to socket
 *        - Timeout in milliseconds
 * @return - Number of Bytes written on SUCCESS
 *         - -1 on FAILURE
 **/
int network_write(Network *n, unsigned char *buffer, int len, int timeout_ms) {
	int rc;

	if (n->sockHandle == NULL) return NET_NOT_FOUND;

	rc = net_sock_send((net_sockhnd_t) n->sockHandle, buffer, len);
	if (rc < 0) {
		msg_error("net_sock_send failed - %d\n", rc);
	}

	return rc;
}
int network_disconnect(Network *n) {
	net_sock_close(n->sockHandle);
	net_sock_destroy(n->sockHandle);
	return 0;
}

int mqtt_network_init(Network *n, device_config_t* dev) {
	int rc = NET_ERR;

	n->mqttdisconnect = network_disconnect;
	n->mqttread = network_read;
	n->mqttwrite = network_write;

	if (hnet == NULL){ /* if network is not yet initialized*/
		rc = net_init(&hnet, NET_IF, net_if_init);
		if (rc != NET_OK) {
			return NET_NOT_FOUND;	/* Must return as there is no network link*/
		}else{
			n->netHandle = hnet;
			rc = NET_OK;
		}
	}

	/* start the rtc timer*/
	uint8_t date[12];
	uint8_t time[12];
	if (setRTCTimeDateFromNetwork(0) == TD_ERR_RTC){
		msg_error("ntp get time from network failed...");
		msg_error("program halted...!!!");
		while(1);
	}
	RTC_CalendarShow(time, date);
	msg_info("[RTC]** UTC-date: %s UTC-time: %s ** -5 to actual time **", date, time);
	//rtc_initialize(&rtc);

	rc = net_sock_create(n->netHandle, &n->sockHandle, (dev->HostPort == 1883)?NET_PROTO_TCP:NET_PROTO_TLS);
	if (rc != NET_OK) {
		rc = NET_ERR;
		msg_error("error creating mqtt socket...\n");
	}else{
		rc = NET_OK;
	}

#ifdef USE_MBED_TLS

	if (dev->ConnSecurity == CONN_SEC_MUTUALAUTH && rc == NET_OK){
		/************************************************************/
		if (dev->tls_ca_certs && dev->ConnSecurity == CONN_SEC_MUTUALAUTH){
			(void)net_sock_setopt(n->sockHandle, "tls_ca_certs",
					(const uint8_t*)dev->tls_ca_certs, dev->tls_ca_certs_len);
		}

		if (dev->tls_dev_cert && dev->ConnSecurity == CONN_SEC_MUTUALAUTH){
			(void)net_sock_setopt(n->sockHandle, "tls_dev_cert",
							  (const uint8_t*)dev->tls_dev_cert, dev->tls_dev_cert_len);
		}

		if (dev->tls_dev_key && dev->ConnSecurity == CONN_SEC_MUTUALAUTH){
			(void)net_sock_setopt(n->sockHandle, "tls_dev_key",
							  (const uint8_t*)dev->tls_dev_key, dev->tls_dev_key_len);
		}

		(void)net_sock_setopt(n->sockHandle, "tls_server_verification", NULL, 0);


		/************************************************************/
	}

	if (dev->ConnSecurity == CONN_SEC_SERVERAUTH && rc == NET_OK){
		(void)net_sock_setopt(n->sockHandle, "tls_ca_certs",
				(const uint8_t*)dev->tls_ca_certs, strlen(dev->tls_ca_certs));
		(void)net_sock_setopt(n->sockHandle, "tls_server_verification", NULL, 0);
	}

	if (dev->ConnSecurity == CONN_SEC_NONE && rc == NET_OK){
		(void)net_sock_setopt(n->sockHandle, "tls_server_noverification", NULL, 0);
	}

	if (rc == NET_OK){
		/* ASCII timeout strings (include the null terminator or pass strlen) */
		net_sock_setopt(n->sockHandle, "sock_read_timeout",  (const uint8_t*)"5000", strlen("5000"));
		net_sock_setopt(n->sockHandle, "sock_write_timeout", (const uint8_t*)"5000", strlen("5000"));
		(void)net_sock_setopt(n->sockHandle, "tls_server_name",
							  (const uint8_t*)dev->HostName, strlen(dev->HostName));
		rc = net_sock_open(n->sockHandle, dev->HostName, NULL, dev->HostPort, 0);
	}
#endif

	if (rc != NET_OK) {
		rc = NET_ERR;
		msg_error("error creating/opening socket for mqtt client connection...\n");
	}else{
		n->port = dev->HostPort;
		net_get_hostaddress(n->netHandle, &n->hostip, dev->HostName);
	}
	return rc;
}

#ifdef MQTT_TASK
osMutexDef(mqtt);

const osThreadAttr_t mqttrun_attributes = {
  .name = "defaultTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

int ThreadStart(Thread* thread, mqttrun_t mqrun, MQTTClient* c){
	*(osThreadId_t*)thread = osThreadNew(mqrun, (MQTTClient*)c, &mqttrun_attributes);
	if (thread == NULL){
		msg_error("Fail creating thread MQTTRun...\n");
		return FAILURE;
	}
	return MQSUCCESS;
}

void MutexLock(Mutex* mtx, int timeout){
	osStatus_t status;
	status = osMutexAcquire((osMutexId_t)mtx, timeout);
	if (status != osOK){
		msg_error("mutex acquire not succeeded..\n");
	}
}

void MutexUnlock(Mutex* mtx){
	osStatus_t status;
	status = osMutexRelease((osMutexId_t)mtx);
	if (status != osOK){
		msg_error("mutex release not succeeded..\n");
	}
}

void MutexInit(Mutex* mtx){
	*(osMutexId_t*)mtx = osMutexNew(&os_mutex_def_mqtt);
	if (mtx == NULL){
		msg_error("Failed creating MQTT mutex...\n");
	}
	MutexUnlock(mtx);
}


#endif

