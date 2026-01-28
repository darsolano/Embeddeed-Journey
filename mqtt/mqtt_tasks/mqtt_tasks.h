/*
 * mqtt_tasks.h
 *
 *  Created on: Feb 24, 2025
 *      Author: Daruin Solano
 */

#ifndef MQTT_MQTT_TASKS_MQTT_TASKS_H_
#define MQTT_MQTT_TASKS_MQTT_TASKS_H_

#if defined(MQTT_TASK)
#include <cmsis_os.h>
#endif
#include "MQTTClient.h"

extern MQTTClient mc;

void mqtt_setup_tsk_env(void);
void mqtt_client_publish_task(MQTTClient* client);


#endif /* MQTT_MQTT_TASKS_MQTT_TASKS_H_ */
