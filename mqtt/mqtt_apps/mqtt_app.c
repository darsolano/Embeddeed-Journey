/*
 * mqtt_app.c
 *
 *  Created on: Dec 23, 2025
 *      Author: Daruin Solano
 */

#include "mqtt_app.h"
#include "MQTTClient.h"
#include "cJSON.h"
#include "aws_cert.h"
#include "timedate.h"

extern timestamp_t ts;


static uint32_t backoff_ms = RECONN_MIN_MS;
static void allpurposeMessageHandler(MessageData *data);
static int mqtt_client_publish(MQTTClient *client, device_config_t *dev) ;

/* Detailed variables for the mqtt apps*/
/*For use in MQTT client task*/
pub_data_t pub_data = {  };
status_data_t status_data = { MODEL_DEFAULT_MAC,
							   MODEL_DEFAULT_LEDON,
							   MODEL_DEFAULT_TELEMETRYINTERVAL };

static unsigned char mqtt_send_buffer[MQTT_SEND_BUFFER_SIZE];
static unsigned char mqtt_read_buffer[MQTT_READ_BUFFER_SIZE];

/* Warning: The subscribed topics names strings must be allocated separately,
 * because Paho does not copy them and uses references to dispatch the incoming message. */
static char mqtt_subtopic[MQTT_TOPIC_BUFFER_SIZE];
static char mqtt_pubtopic[MQTT_TOPIC_BUFFER_SIZE];
static char mqtt_msg[MQTT_MSG_BUFFER_SIZE];

/* Detailed variables for the mqtt apps*/
extern net_hnd_t hnet;
extern RTC_t rtc;

int rc = NET_ERR;
Network net;
MQTTClient mc;
device_config_t dev;
/* MQTT connect */
MQTTPacket_connectData options = MQTTPacket_connectData_initializer;


static void mqtt_hard_reset(Network *net, MQTTClient *mc)
{
    /* Best effort clean disconnect */
    if (MQTTIsConnected(mc)) {
        MQTTDisconnect(mc);
    }

    /* Always close underlying socket/TLS */
    if (net && net->sockHandle) {
        net_sock_close(net->sockHandle);
        net_sock_destroy(net->sockHandle);
        net->sockHandle = NULL;
    }
}

static int mqtt_do_connect_and_subscribe(MQTTClient *mc,
                                        MQTTPacket_connectData *opt,
                                        const char *subtopic)
{
    int rc = MQTTConnect(mc, opt);
    if (rc != SUCCESS) return rc;

    rc = MQTTSubscribe(mc, subtopic, QOS0, allpurposeMessageHandler);
    return rc;
}

void mqtt_main(void) {
	uint32_t last_pub = 0;

	while (1) {

		/* 1) Ensure connected */
		if (!MQTTIsConnected(&mc)) {

			msg_debug("MQTT: connecting...\n");
			int rc = mqtt_do_connect_and_subscribe(&mc, &options,
					mqtt_subtopic);

			if (rc != SUCCESS) {
				msg_error("MQTT: connect/sub failed rc=%d\n", rc);

				mqtt_hard_reset(&net, &mc);

				HAL_Delay(backoff_ms);
				backoff_ms =
						(backoff_ms < RECONN_MAX_MS) ?
								(backoff_ms * 2) : RECONN_MAX_MS;
				continue;
			}

			/* Success => reset backoff */
			backoff_ms = RECONN_MIN_MS;
		}

		/* 2) Service keepalive + incoming packets */
		int rc = MQTTYield(&mc, YIELD_MS);
		if (rc != SUCCESS) {
			msg_error("MQTT: yield failed rc=%d -> reset\n", rc);
			mqtt_hard_reset(&net, &mc);
			HAL_Delay(500);
			continue;
		}

		/* 3) Publish periodically */
		uint32_t now = HAL_GetTick();
		if ((now - last_pub) >= PUB_INTERVAL_MS) {
			rc = mqtt_client_publish(&mc, &dev); /* make this return SUCCESS/FAIL */
			if (rc != SUCCESS) {
				msg_error("MQTT: publish failed rc=%d -> reset\n", rc);
				mqtt_hard_reset(&net, &mc);
				HAL_Delay(500);
				continue;
			}
			last_pub = now;
		}
	}
}


static int mqtt_client_publish(MQTTClient *client, device_config_t* dev) {
	MQTTMessage mqmsg;

	snprintf(mqtt_pubtopic, MQTT_TOPIC_BUFFER_SIZE, "/sensors/%s",
			dev->MQClientId);
#ifdef SENSORS
	/*Read data from sensors*/
	pub_data.temperature = BSP_TSENSOR_ReadTemp();
	pub_data.humidity = BSP_HSENSOR_ReadHumidity();
#endif
	getTimestamp(ts.ts, sizeof(ts.ts));
	pub_data.tstamp = &(ts.ts[0]);

	/*create Json object to publish data*/
	cJSON *publish_data = cJSON_CreateObject();
	cJSON_AddStringToObject(publish_data, "ID", dev->MQClientId);
	cJSON_AddBoolToObject(publish_data, "LedOn", status_data.LedOn);
	cJSON_AddNumberToObject(publish_data, "Temperature",
			(int)(pub_data.temperature * 9.0 / 5.0) + 32.0);	// from celcius to farenheit
	cJSON_AddNumberToObject(publish_data, "Humidity", (int)pub_data.humidity);
	cJSON_AddStringToObject(publish_data, "timestamp", pub_data.tstamp);
	cJSON_AddStringToObject(publish_data, "MacAddress", pub_data.mac);
	// Convert JSON to String
	char *msg = cJSON_Print(publish_data);
	if (msg == NULL) {
	    cJSON_Delete(publish_data);
	    return -1; // Handle allocation failure
	}
	strcpy(mqtt_msg, msg);
	cJSON_Delete(publish_data);
	rc = strlen(mqtt_msg);
	free(msg);

	if ((rc < 0) || (rc >= MQTT_MSG_BUFFER_SIZE)) {
		msg_error("MQTT Telemetry message formatting error...");
	} else {
		mqmsg.qos = QOS0;
		mqmsg.payload = (char*) mqtt_msg;
		mqmsg.payloadlen = strlen(mqtt_msg);

		rc = MQTTPublish(client, mqtt_pubtopic, &mqmsg);

		if (rc != MQSUCCESS) {
			msg_error("Failed mqtt publishing %s on %s",
					(char* )(mqmsg.payload), mqtt_pubtopic);
		}

		if (rc == MQSUCCESS) {
			/* Visual notification of the telemetry publication: LED blink. */
			msg_info("#\n");
			msg_info("MQTT publication topic: %s \tpayload: %s", mqtt_pubtopic,
					mqtt_msg);
		} else {
			msg_error("Telemetry publication failed...");
		}
	}
	return rc;

}


/** Message callback
 *
 *  Note: No context handle is passed by the callback. Must rely on static variables.
 *        TODO: Maybe store couples of hander/contextHanders so that the context could
 *              be retrieved from the handler address. */
static void allpurposeMessageHandler(MessageData *data) {

	snprintf(mqtt_msg, MIN(MQTT_MSG_BUFFER_SIZE, data->message->payloadlen + 1),
			"%s", (char*) data->message->payload);

	msg_info("Received message: length: %d topic: %s content: %s\n",
			data->topicName->lenstring.len, data->topicName->lenstring.data,
			mqtt_msg);

	cJSON *json = cJSON_Parse(mqtt_msg);


	json = cJSON_GetObjectItemCaseSensitive(json, "LedOn");
	if (json != NULL) {
		if (cJSON_IsBool(json) == true) {
			status_data.LedOn = (cJSON_IsTrue(json) == true)?true:false;
			Led_SetState(status_data.LedOn);
		} else {
			msg_error("JSON parsing error of LedOn value.\n");
		}
	}
	cJSON_Delete(json);
}


char root[2048];
char devca[2048];
char key[2048];

void mqtt_start(void)
 {
#ifdef SENSORS
	if (BSP_TSENSOR_Init() != TSENSOR_OK){
		msg_error("Temperature sensor failed init HTS221...");
	}
	if (BSP_HSENSOR_Init() != HSENSOR_OK){
		msg_error("Humidity sensor failed init HTS221...");
	}
#endif
	/* https://testclient-cloud.mqtt.cool/ */
//	dev.HostName = "broker.mqtt.cool";
//	dev.HostPort = 1883;
//	dev.ConnSecurity = CONN_SEC_NONE;
//	dev.MQClientId = "IOT_STM32";

	/* AWS IOT_CORE*/
	dev.ConnSecurity = CONN_SEC_MUTUALAUTH;
	dev.MQClientId = "IOT_STM32";
	dev.HostName = "a1rowpbf3j3tx6-ats.iot.us-east-2.amazonaws.com";
	dev.HostPort = 8883;
	dev.tls_ca_certs = AWS_ROOT_CA1;
	dev.tls_ca_certs_len = strlen(AWS_ROOT_CA1);
	dev.tls_dev_cert = AWS_CERTIFICATE;
	dev.tls_dev_cert_len = strlen(AWS_CERTIFICATE);
	dev.tls_dev_key = AWS_PRIVATE_KEY;
	dev.tls_dev_key_len = strlen(AWS_PRIVATE_KEY);

	/*HiveMQ*/
//	dev.HostName = "17a7d54f7ea545de987ea3e8b031234a.s1.eu.hivemq.cloud";
//	dev.MQClientId = "IOT_STM32";
//	dev.HostPort = 8883;
//	dev.MQUserName = "iotstm32_client";
//	dev.MQUserPwd = "av!27E25rApuJP!";
//	dev.ConnSecurity = CONN_SEC_SERVERAUTH;
//	dev.tls_ca_certs = HIVEMQ_CERTIFICATE;
//	dev.tls_ca_certs_len = strlen(HIVEMQ_CERTIFICATE);
//	dev.tls_dev_cert = NULL;
//	dev.tls_dev_key = NULL;

	net_macaddr_t  macAddr;

	/* Network init, just in case there network is not connected yet*/
	rc = mqtt_network_init(&net, &dev);

	if (rc != NET_OK || net.netHandle == NULL || net.sockHandle == NULL) {
		if (rc == NET_NOT_FOUND) {
			msg_error("ALERT, netif module is not present or failing big time...\n");
		}
		msg_error("network connection error or socket creation failure...!!! ---> reseting the system");
		HAL_Delay(20000);
		NVIC_SystemReset();
	}

	msg_debug("MQTT network connection created with success");

	net_get_mac_address(net.netHandle, &macAddr);
	sprintf(pub_data.mac, "%02X:%02X:%02X:%02X:%02X:%02X", macAddr.mac[0],macAddr.mac[1],
															macAddr.mac[2],macAddr.mac[3],
															macAddr.mac[4],macAddr.mac[5]);

	/*MQTT init just set some variables for the context, no return messages*/
	MQTTClientInit(&mc, &net, MQTT_CMD_TIMEOUT, mqtt_send_buffer,
	MQTT_SEND_BUFFER_SIZE, mqtt_read_buffer, MQTT_READ_BUFFER_SIZE);

	/* MQTT connect */
	options.clientID.cstring = dev.MQClientId;
	options.username.cstring = dev.MQUserName;
	options.password.cstring = dev.MQUserPwd;
	options.keepAliveInterval = 60;
	options.will.message.cstring = "will message";
	options.will.qos = 1;
	options.will.retained = 0;
	options.will.topicName.cstring = "will topic";

	rc = MQTTConnect(&mc, &options);
	if (rc != 0) {
		msg_error("MQTTConnect() failed: %d", rc);
	} else {
		snprintf(mqtt_subtopic, MQTT_TOPIC_BUFFER_SIZE, "/devices/%s/control",	/*/devices/IOT_STM32/control*/
				dev.MQClientId);
		rc = MQTTSubscribe(&mc, mqtt_subtopic, QOS0, allpurposeMessageHandler);
		msg_debug("MQTTSubscribe: topic %s", mqtt_subtopic);
	}

	if (rc != MQSUCCESS) {
		msg_error("Failed subscribing to the %s topic rc = %d.", mqtt_subtopic, rc);
	} else {
		msg_info("Subscribed to %s.", mqtt_subtopic);
	}

	Led_SetState(true);
}
