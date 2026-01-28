/*
 * net_mqtt.h
 *
 *  Created on: Feb 1, 2025
 *      Author: Daruin Solano
 */

#ifndef NET_ES_WIFI_NET_INC_NET_MQTT_H_
#define NET_ES_WIFI_NET_INC_NET_MQTT_H_

#include "net_internal.h"


#define MODEL_MAC_SIZE                    20
#define MODEL_DEFAULT_MAC                 "0102030405"
#define MODEL_DEFAULT_LEDON               true
#define MODEL_DEFAULT_TELEMETRYINTERVAL   15
/* Uncomment one of the below defines to use an alternative MQTT service */
/* #define LITMUS_LOOP */
/* #define UBIDOTS_MQTT */

#ifdef LITMUS_LOOP
#define MQTT_SEND_BUFFER_SIZE             1500  /* Has to be large enough for the message size plus the topic name size */
#else
#define MQTT_SEND_BUFFER_SIZE             600
#endif /* LITMUS_LOOP */
#define MQTT_READ_BUFFER_SIZE             600
#define MQTT_CMD_TIMEOUT                  5000
#define MAX_SOCKET_ERRORS_BEFORE_NETIF_RESET  3

#define MQTT_TOPIC_BUFFER_SIZE            100  /**< Maximum length of the application-defined topic names. */
#define MQTT_MSG_BUFFER_SIZE              MQTT_SEND_BUFFER_SIZE /**< Maximum length of the application-defined MQTT messages. */

/** TLS connection security level */
typedef enum {
  CONN_SEC_UNDEFINED = -1,
  CONN_SEC_NONE = 0,          /**< Clear connection */
  CONN_SEC_SERVERNOAUTH = 1,  /**< Encrypted TLS connection, with no authentication of the remote host: Shall NOT be used in a production environment. */
  CONN_SEC_SERVERAUTH = 2,    /**< Encrypted TLS connection, with authentication of the remote host. */
  CONN_SEC_MUTUALAUTH = 3     /**< Encrypted TLS connection, with mutual authentication. */
} conn_sec_t;

typedef struct Network_s Network;

typedef int net_read_t(Network* n, unsigned char* buffer, int len, int timeout_ms);
typedef int net_write_t(Network* n, unsigned char* buffer, int len, int timeout_ms);
typedef int net_disconnect_t(Network* n);


typedef uint32_t Mutex;
typedef uint32_t Thread;
typedef void (*mqttrun_t)(void * client);

typedef struct {
  char      mac[MODEL_MAC_SIZE];      /*< To be read from the netif */
  char*		tstamp;           /* pointer to formatted timestamp data: 2025-12-27T05:08:11Z */
  uint32_t 	unixtime;	/*< Tick count since MCU boot. */
#ifdef SENSORS
  int16_t   ACC_Value[3]; /*< Accelerometer */
  float    GYR_Value[3]; /*< Gyroscope */
  int16_t   MAG_Value[3]; /*< Magnetometer */
  float    temperature;
  float    humidity;
  float    pressure;
  int32_t   roximity;
#endif /* SENSOR */
} pub_data_t;

typedef struct {
  char      mac[MODEL_MAC_SIZE];      /*< To be read from the netif */
  bool      LedOn;
  uint32_t  TelemetryInterval;
} status_data_t;

typedef struct {
  char*			HostName;
  uint16_t		HostPort;
  conn_sec_t	ConnSecurity;
  char*			MQClientId;
  char*			MQUserName;
  char*			MQUserPwd;
  const char* 	tls_ca_certs;
  uint32_t		tls_ca_certs_len;
  const char* 	tls_dev_cert;
  uint32_t		tls_dev_cert_len;
  const char* 	tls_dev_key;
  uint32_t		tls_dev_key_len;

#ifdef LITMUS_LOOP
  char *LoopTopicId;
#endif
} device_config_t;

struct Network_s
{
	net_read_t       *	mqttread;
	net_write_t      *	mqttwrite;
	net_disconnect_t *	mqttdisconnect;
	net_hnd_t 			netHandle;
	net_sockhnd_t	 	sockHandle;
	uint16_t			port;
	net_ipaddr_t		hostip;
};

int  mqtt_network_init(Network *n, device_config_t* dev);
void MutexLock(Mutex* mtx, int timeout);
void MutexUnlock(Mutex* mtx);
void MutexInit(Mutex* mtx);




#endif /* NET_ES_WIFI_NET_INC_NET_MQTT_H_ */
