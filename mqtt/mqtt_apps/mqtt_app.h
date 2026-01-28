/*
 * mqtt_app.h
 *
 *  Created on: Dec 23, 2025
 *      Author: Daruin Solano
 */

#ifndef WIFI_NET_MQTT_MQTT_APPS_MQTT_APP_H_
#define WIFI_NET_MQTT_MQTT_APPS_MQTT_APP_H_

#define YIELD_MS          200
#define PUB_INTERVAL_MS  60000	/* in milliseconds*/
#define RECONN_MIN_MS    1000
#define RECONN_MAX_MS   	30000

void mqtt_start(void);
void mqtt_main(void);


#endif /* WIFI_NET_MQTT_MQTT_APPS_MQTT_APP_H_ */
