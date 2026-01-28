/*
 * mqtt_tasks.c
 *
 *  Created on: Feb 24, 2025
 *      Author: Daruin Solano
 */

#if defined(MQTT_TASK)
#include "cmsis_os.h"
#endif
#ifdef SENSOR
#include "stm32l475e_iot01.h"
#include "stm32l475e_iot01_tsensor.h"
#include "stm32l475e_iot01_hsensor.h"
#endif
#include "cJSON.h"
#include "rtc.h"
#include "utils_datetime.h"
#include "net.h"
#include "mqtt_tasks.h"

int 	mqtt_publish_helper(void *mqtt_ctxt, const char *topic, const char *msg);
void 	check_MqttConnection_Task(void* argument);
void 	allpurposeMessageHandler(MessageData *data);

extern net_hnd_t 		hnet;	/*Network handle*/
extern net_sockhnd_t  	sock;	/*Socket handle*/

#if defined(MQTT_TASK)
/* RTOS configuration variables*/
const osThreadAttr_t mqttyieldTask_attributes = {
  .name = "mqttyieldTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityRealtime
};
osThreadId_t mqttyieldTaskHandle;

const osThreadAttr_t mqttPublishTask_attributes = {
  .name = "mqttPubTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityHigh
};
osThreadId_t mqttPublishTaskHandle;
#endif
/*For use in MQTT client task*/
pub_data_t pub_data = { MODEL_DEFAULT_MAC, 0 };
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
int rc = NET_ERR;
Network net;
MQTTClient mc;
device_config_t dev;
/* MQTT connect */
MQTTPacket_connectData options = MQTTPacket_connectData_initializer;



void mqtt_setup_tsk_env(void)
 {
#ifdef SENSORS
	if (BSP_TSENSOR_Init() != TSENSOR_OK){
		msg_error("Temperature sensor failed init HTS221...\n");
	}
	if (BSP_HSENSOR_Init() != HSENSOR_OK){
		msg_error("Humidity sensor failed init HTS221...\n");
	}
#endif

	dev.ConnSecurity = CONN_SEC_;
	dev.MQClientId = "IOT_STM32";
	dev.HostName = "test.mosquitto.org";
	dev.HostPort = "1883";

	net_macaddr_t  macAddr;

	/* Network init, just in case there network is not connected yet*/
	rc = mqtt_network_init(&net, dev.HostName, atoi(dev.HostPort));
	if (rc != NET_OK || net.netHandle == NULL || net.sockHandle == NULL) {
		if (rc == NET_NOT_FOUND) {
			msg_error(
					"ALERT, netif module is not present or failing big time...\n");
		}
		msg_error("network connection error or socket creation failure...!!!\n");
	}
	/* start the rtc timer*/
	rtc_initialize(&rtc);




	net_get_mac_address(hnet, &macAddr);
	sprintf(pub_data.mac, "%02X%02X%02X%02X%02X%02X", macAddr.mac[0],macAddr.mac[1],
															macAddr.mac[2],macAddr.mac[3],
															macAddr.mac[4],macAddr.mac[5]);

	/*MQTT init just set some variables for the context, no return messages*/
	MQTTClientInit(&mc, &net, MQTT_CMD_TIMEOUT, mqtt_send_buffer,
	MQTT_SEND_BUFFER_SIZE, mqtt_read_buffer, MQTT_READ_BUFFER_SIZE);

	/************************************************************/

	/* MQTT connect */
	options.clientID.cstring = dev.MQClientId;

	rc = MQTTConnect(&mc, &options);
	if (rc != 0) {
		msg_error("MQTTConnect() failed: %d\n", rc);
	} else {
		snprintf(mqtt_subtopic, MQTT_TOPIC_BUFFER_SIZE, "/devices/%s/control",
				dev.MQClientId);
		rc = MQTTSubscribe(&mc, mqtt_subtopic, QOS0, allpurposeMessageHandler);
	}

	if (rc != MQSUCCESS) {
		msg_error("Failed subscribing to the %s topic.\n", mqtt_subtopic);
	} else {
		msg_info("Subscribed to %s.\n", mqtt_subtopic);
		//rc = MQTTYield(&mc, 500);
	}
//	if (rc != MQSUCCESS) {
//		msg_error("Yield failed.\n");
//	}


	Led_SetState(true);


	/*****************OS TASK CREATION MQTTYIELD & MQTTPUBLISH******************/
//	mqttyieldTaskHandle = osThreadNew(check_MqttConnection_Task, ((MQTTClient*) &mc), &mqttyieldTask_attributes);
//	mqttPublishTaskHandle = osThreadNew(mqtt_client_publish_task, ((MQTTClient*) &mc), &mqttPublishTask_attributes);
}


/*
 * This task guard the MQTT connection to the server and reads all responses back
 */
void check_MqttConnection_Task(void* argument) {
	MQTTClient* client = (MQTTClient*) argument;
	int r;
	for(;;)
	{
		if (MQTTYield(client, 1000) != MQSUCCESS) {
			msg_debug("MQTT Disconnected, attempting to reconnect...\n");
			MQTTDisconnect(client);
				msg_debug("re-initiating socket connection...\n");
				r = mqtt_network_init(&net, dev.HostName,atoi(dev.HostPort));
				if (r != NET_OK) {
					r = NET_ERR;
					msg_error("error opening socket for mqtt client connection...\n");
				}
				if (MQTTConnect(client, &options) == SUCCESS) {
					msg_debug("MQTT Reconnected Successfully!\n");
				} else {
					msg_error("MQTT Reconnection Failed!\n");
				}
		}
		HAL_Delay(30000);	// runs each 15 secs
	}
}



void mqtt_client_publish_task(MQTTClient* client){
//	MQTTClient* client = (MQTTClient*) argument;
	MQTTMessage mqmsg;

	for(;;){
		snprintf(mqtt_pubtopic, MQTT_TOPIC_BUFFER_SIZE, "/sensors/%s",	dev.MQClientId);
#ifdef SENSORS
		/*Read data from sensors*/
		pub_data.temperature = BSP_TSENSOR_ReadTemp();
		pub_data.humidity = BSP_HSENSOR_ReadHumidity();
#endif
		rtc_gettime(&rtc);
		pub_data.ts = rtc.unixtime;

		/*create Json object to publish data*/
		cJSON* publish_data =cJSON_CreateObject();
		cJSON_AddBoolToObject(publish_data, "LedOn", status_data.LedOn);
		cJSON_AddNumberToObject(publish_data, "Temperature", (pub_data.temperature * 9.0 / 5.0) + 32.0);
		cJSON_AddNumberToObject(publish_data, "Humidity", pub_data.humidity);
		cJSON_AddNumberToObject(publish_data, "TelemetryInterval", pub_data.ts);
		cJSON_AddStringToObject(publish_data, "MacAddress", pub_data.mac);
		// Convert JSON to String
		char *msg = cJSON_Print(publish_data);
		strcpy(mqtt_msg, msg);
		cJSON_Delete(publish_data);
		rc = strlen(mqtt_msg);


		if ((rc < 0) || (rc >= MQTT_MSG_BUFFER_SIZE)) {
			msg_error("Telemetry message formatting error.\n");
		} else {
			mqmsg.qos = QOS0;
			mqmsg.payload = (char*) mqtt_msg;
			mqmsg.payloadlen = strlen(mqtt_msg);

			rc = MQTTPublish(client, mqtt_pubtopic, &mqmsg);
			if (rc != MQSUCCESS) {
				msg_error("Failed publishing %s on %s\n", (char* )(mqmsg.payload),
						mqtt_pubtopic);
			}

			if (rc == MQSUCCESS) {
				/* Visual notification of the telemetry publication: LED blink. */
				msg_info("#\n");
				msg_info("publication topic: %s \tpayload: %s\n", mqtt_pubtopic, mqtt_msg);
			} else {
				msg_error("Telemetry publication failed.\n");
			}

			rc = MQTTYield(client, 500);
			if (rc) {
				msg_error("Yield failed. Reconnection needed?.\n");
			}
		}
		HAL_Delay(60000);
	}
}


/** Message callback
 *
 *  Note: No context handle is passed by the callback. Must rely on static variables.
 *        TODO: Maybe store couples of hander/contextHanders so that the context could
 *              be retrieved from the handler address. */
void allpurposeMessageHandler(MessageData *data) {

	snprintf(mqtt_msg, MIN(MQTT_MSG_BUFFER_SIZE, data->message->payloadlen + 1),
			"%s", (char*) data->message->payload);

	msg_info("Received message: length: %d topic: %s content: %s\n",
			data->topicName->lenstring.len, data->topicName->lenstring.data,
			mqtt_msg);

	cJSON *json = cJSON_Parse(mqtt_msg);


	json = cJSON_GetObjectItemCaseSensitive(json, "LedOn");
	if (json != NULL) {
		if (cJSON_IsBool(json) == true) {
			status_data.LedOn = (cJSON_IsTrue(json) == true);
			Led_SetState(status_data.LedOn);
		} else {
			msg_error("JSON parsing error of LedOn value.\n");
		}
	}
	cJSON_Delete(json);
}

