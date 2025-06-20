#include "mqtt_freertos.h"

#include "board.h"
#include "fsl_silicon_id.h"

#include "lwip/opt.h"
#include "lwip/api.h"
#include "lwip/apps/mqtt.h"
#include "lwip/tcpip.h"

//#define TOPIC_PARAM_TEMP  "lwip_topic/Sensors/Parameters/Temperature"
//#define TOPIC_PARAM_LIGHT "lwip_topic/Sensors/Parameters/Light"
//
//#define TOPIC_PUB_TEMP    "lwip_topic/Sensors/Temperature"
//#define TOPIC_PUB_LIGHT   "lwip_topic/Sensors/Light"

// FIXME cleanup

/***************************
 * Definitions
 **************************/

/*! @brief MQTT server host name or IP address. */
#ifndef EXAMPLE_MQTT_SERVER_HOST
#define EXAMPLE_MQTT_SERVER_HOST "broker.hivemq.com"
#endif

/*! @brief MQTT server port number. */
#ifndef EXAMPLE_MQTT_SERVER_PORT
#define EXAMPLE_MQTT_SERVER_PORT 1883
#endif

/*! @brief Stack size of the temporary lwIP initialization thread. */
#define INIT_THREAD_STACKSIZE 1024

/*! @brief Priority of the temporary lwIP initialization thread. */
#define INIT_THREAD_PRIO DEFAULT_THREAD_PRIO

/*! @brief Stack size of the temporary initialization thread. */
#define APP_THREAD_STACKSIZE 1024

/*! @brief Priority of the temporary initialization thread. */
#define APP_THREAD_PRIO DEFAULT_THREAD_PRIO


/* Tópicos */
#define TOPIC_PARAM_TEMP  "lwip_topic/Sensors/Parameters/Temperature/Auto"
#define TOPIC_PARAM_LIGHT "lwip_topic/Sensors/Parameters/Light/Auto"

#define TOPIC_PUB_TEMP    "lwip_topic/Sensors/Temperature"
#define TOPIC_PUB_LIGHT   "lwip_topic/Sensors/Light"

/***************************
 * Prototypes
 **************************/

static void connect_to_mqtt(void *ctx);

/***************************
 * Variables
 **************************/

/* Mine */

uint8_t Lights_Samp;
uint8_t Temper_Samp;

uint8_t Tempe_Data;
uint8_t Light_Data;

/* Mine_End */

/*! @brief MQTT client data. */
static mqtt_client_t *mqtt_client;

/*! @brief MQTT client ID string. */
static char client_id[(SILICONID_MAX_LENGTH * 2) + 5];

/*! @brief MQTT client information. */
static const struct mqtt_connect_client_info_t mqtt_client_info = {
    .client_id   = (const char *)&client_id[0],
    .client_user = NULL,
    .client_pass = NULL,
    .keep_alive  = 100,
    .will_topic  = NULL,
    .will_msg    = NULL,
    .will_qos    = 0,
    .will_retain = 0,
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    .tls_config = NULL,
#endif
};

/*! @brief MQTT broker IP address. */
static ip_addr_t mqtt_addr;

/*! @brief Indicates connection to MQTT broker. */
static volatile bool connected = false;

/***************************
 * Code
 **************************/
static void Temp_Init(void)
{
    if (((SENSOR_CTRL->MISC_CTRL_REG & SENSOR_CTRL_MISC_CTRL_REG_TIMER_1_ENABLE_MASK) >>
         SENSOR_CTRL_MISC_CTRL_REG_TIMER_1_ENABLE_SHIFT) == 0U)
        SENSOR_CTRL->MISC_CTRL_REG |= SENSOR_CTRL_MISC_CTRL_REG_TIMER_1_ENABLE_MASK;
}
static uint8_t Temp_GetCelsius(void)
{
    uint16_t raw = (uint16_t)((SENSOR_CTRL->TSEN_CTRL_1_REG_2 &
                               SENSOR_CTRL_TSEN_CTRL_1_REG_2_TSEN_TEMP_VALUE_MASK) >>
                              SENSOR_CTRL_TSEN_CTRL_1_REG_2_TSEN_TEMP_VALUE_SHIFT);
    float tempC = raw * 0.480561F - 220.7074F;
    return (uint8_t)tempC;
}

/*!
 * @brief Called when subscription request finishes.
 */
static void mqtt_topic_subscribed_cb(void *arg, err_t err)
{
    const char *topic = (const char *)arg;

    if (err == ERR_OK) { PRINTF("Subscribed to the topic \"%s\".\r\n", topic); }
    else { PRINTF("Failed to subscribe to the topic \"%s\": %d.\r\n", topic, err); }
}

/*!
 * @brief Called when there is a message on a subscribed topic.
 */
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{
    LWIP_UNUSED_ARG(arg);
    Lights_Samp = 0;
    if (strcmp(topic, "lwip_topic/Sensors/Parameters/Temperature/Auto") == 0) {
        Temper_Samp = 1;
    }
    else{ Temper_Samp = 0;
    }
    if (strcmp(topic, "lwip_topic/Sensors/Parameters/Light/Auto") == 0) {
        Lights_Samp = 1;
    }



    PRINTF("Received %u bytes from the topic \"%s\"\n", tot_len, topic);
}
/*!
 * @brief Called when recieved incoming published message fragment.
 */
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
    int i;

    LWIP_UNUSED_ARG(arg);

    if( Temper_Samp == 1) { Tempe_Data = 1; }
    if( Temper_Samp == 0) { Tempe_Data = 0; }

    if( Lights_Samp == 1) { Light_Data = 1; }
    if( Lights_Samp == 0) { Light_Data = 0; }

    for (i = 0; i < len; i++) {

    	if (isprint(data[i])) { PRINTF("%c", (char)data[i]); }

        else { PRINTF("\\x%02x", data[i]); }
    }

    if (flags & MQTT_DATA_FLAG_LAST) { PRINTF("\"\r\n"); }
}

/*!
 * @brief Subscribe to MQTT topics.
 */
static void mqtt_subscribe_topics(mqtt_client_t *client)
{
    static const char *topics[] = {TOPIC_PARAM_LIGHT, TOPIC_PARAM_TEMP};
    int qos[]                   = {0, 0};
    err_t err;
    int i;

    mqtt_set_inpub_callback(client, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, LWIP_CONST_CAST(void *, &mqtt_client_info));

    for (i = 0; i < ARRAY_SIZE(topics); i++) {
        err = mqtt_subscribe(client, topics[i], qos[i], mqtt_topic_subscribed_cb, LWIP_CONST_CAST(void *, topics[i]));

        if (err == ERR_OK)
        {
            PRINTF("Subscribing to the topic \"%s\" with QoS %d...\r\n", topics[i], qos[i]);
        }
        else
        {
            PRINTF("Failed to subscribe to the topic \"%s\" with QoS %d: %d.\r\n", topics[i], qos[i], err);
        }
    }
}

/*!
 * @brief Called when connection state changes.
 */
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
    const struct mqtt_connect_client_info_t *client_info = (const struct mqtt_connect_client_info_t *)arg;

    connected = (status == MQTT_CONNECT_ACCEPTED);

    switch (status)
    {
        case MQTT_CONNECT_ACCEPTED:
            PRINTF("MQTT client \"%s\" connected.\r\n", client_info->client_id);
            mqtt_subscribe_topics(client);
            break;

        case MQTT_CONNECT_DISCONNECTED:
            PRINTF("MQTT client \"%s\" not connected.\r\n", client_info->client_id);
            /* Try to reconnect 1 second later */
            sys_timeout(1000, connect_to_mqtt, NULL);
            break;

        case MQTT_CONNECT_TIMEOUT:
            PRINTF("MQTT client \"%s\" connection timeout.\r\n", client_info->client_id);
            /* Try again 1 second later */
            sys_timeout(1000, connect_to_mqtt, NULL);
            break;

        case MQTT_CONNECT_REFUSED_PROTOCOL_VERSION:
        case MQTT_CONNECT_REFUSED_IDENTIFIER:
        case MQTT_CONNECT_REFUSED_SERVER:
        case MQTT_CONNECT_REFUSED_USERNAME_PASS:
        case MQTT_CONNECT_REFUSED_NOT_AUTHORIZED_:
            PRINTF("MQTT client \"%s\" connection refused: %d.\r\n", client_info->client_id, (int)status);
            /* Try again 10 seconds later */
            sys_timeout(10000, connect_to_mqtt, NULL);
            break;

        default:
            PRINTF("MQTT client \"%s\" connection status: %d.\r\n", client_info->client_id, (int)status);
            /* Try again 10 seconds later */
            sys_timeout(10000, connect_to_mqtt, NULL);
            break;
    }
}

/*!
 * @brief Starts connecting to MQTT broker. To be called on tcpip_thread.
 */
static void connect_to_mqtt(void *ctx)
{
    LWIP_UNUSED_ARG(ctx);

    PRINTF("Connecting to MQTT broker at %s...\r\n", ipaddr_ntoa(&mqtt_addr));

    mqtt_client_connect(mqtt_client, &mqtt_addr, EXAMPLE_MQTT_SERVER_PORT, mqtt_connection_cb, LWIP_CONST_CAST(void *, &mqtt_client_info), &mqtt_client_info);
}

/*!
 * @brief Called when publish request finishes.
 */
static void mqtt_message_published_cb(void *arg, err_t err)
{
    const char *topic = (const char *)arg;

    if (err == ERR_OK)
    {
        PRINTF("Published to the topic \"%s\".\r\n", topic);
    }
    else
    {
        PRINTF("Failed to publish to the topic \"%s\": %d.\r\n", topic, err);
    }
}

static void mqtt_pub_cb(void *arg, err_t err)
{
    PRINTF((err==ERR_OK)?"Published \"%s\"\r\n":"Publish fail \"%s\": %d\r\n", (char*)arg, err);
}
/*!
 * @brief Publishes a message. To be called on tcpip_thread.
 */
static void publish_message(void *ctx)
{
    static const char *topic   = "P1_EV/Sensors/Temperature";

    LWIP_UNUSED_ARG(ctx);

    if( Tempe_Data ==1 ){
    	char payload[4];
    	int len = snprintf(payload, sizeof(payload), "%u", Temp_GetCelsius() );
    	mqtt_publish(mqtt_client, topic, payload, (u16_t)len, 1, 0, mqtt_pub_cb, (void *)topic);

    }

	static const char *topic2   = "P1_EV/Sensors/Light";

	if(	Light_Data==0 ){
		char *message2 = "0";
		PRINTF("Going to publish to the topic \"%s\"...\r\n", topic2);
		mqtt_publish(mqtt_client, topic2, message2, strlen(message2), 1, 0, mqtt_message_published_cb, (void *)topic2);

	}

	if(	Light_Data==1 ){
		 char *message2 = "1";
		LWIP_UNUSED_ARG(ctx);
		Lights_Samp=0;
		PRINTF("Going to publish to the topic \"%s\"...\r\n", topic2);
		mqtt_publish(mqtt_client, topic2, message2, strlen(message2), 1, 0, mqtt_message_published_cb, (void *)topic2);
	}
}



/*!
 * @brief Application thread.
 */
static void app_thread(void *arg)
{
    struct netif *netif = (struct netif *)arg;
    err_t err;
    int i;

    PRINTF("\r\nIPv4 Address     : %s\r\n", ipaddr_ntoa(&netif->ip_addr));
    PRINTF("IPv4 Subnet mask : %s\r\n", ipaddr_ntoa(&netif->netmask));
    PRINTF("IPv4 Gateway     : %s\r\n\r\n", ipaddr_ntoa(&netif->gw));

    /*
     * Check if we have an IP address or host name string configured.
     * Could just call netconn_gethostbyname() on both IP address or host name,
     * but we want to print some info if goint to resolve it.
     */
    if (ipaddr_aton(EXAMPLE_MQTT_SERVER_HOST, &mqtt_addr) && IP_IS_V4(&mqtt_addr))
    {
        /* Already an IP address */
        err = ERR_OK;
    }
    else
    {
        /* Resolve MQTT broker's host name to an IP address */
        PRINTF("Resolving \"%s\"...\r\n", EXAMPLE_MQTT_SERVER_HOST);
        err = netconn_gethostbyname(EXAMPLE_MQTT_SERVER_HOST, &mqtt_addr);
    }

    for (;;) {

    if (err == ERR_OK)
    {
        /* Start connecting to MQTT broker from tcpip_thread */
        err = tcpip_callback(connect_to_mqtt, NULL);
        if (err != ERR_OK)
        {
            PRINTF("Failed to invoke broker connection on the tcpip_thread: %d.\r\n", err);
        }
    }
    else
    {
        PRINTF("Failed to obtain IP address: %d.\r\n", err);
    }

    /* Publish some messages */
    for (i = 0; i < 1;)
    {
        if (connected)
        {
            err = tcpip_callback(publish_message, NULL);
            if (err != ERR_OK)
            {
                PRINTF("Failed to invoke publishing of a message on the tcpip_thread: %d.\r\n", err);
            }
            i++;
        }
    }
        sys_msleep(1000U);
    }

    vTaskDelete(NULL);
}

static void generate_client_id(void)
{
    uint8_t silicon_id[SILICONID_MAX_LENGTH];
    const char *hex = "0123456789abcdef";
    status_t status;
    uint32_t id_len = sizeof(silicon_id);
    int idx         = 0;
    int i;
    bool id_is_zero = true;

    /* Get unique ID of SoC */
    status = SILICONID_GetID(&silicon_id[0], &id_len);
    assert(status == kStatus_Success);
    assert(id_len > 0U);
    (void)status;

    /* Covert unique ID to client ID string in form: nxp_hex-unique-id */

    /* Check if client_id can accomodate prefix, id and terminator */
    assert(sizeof(client_id) >= (5U + (2U * id_len)));

    /* Fill in prefix */
    client_id[idx++] = 'n';
    client_id[idx++] = 'x';
    client_id[idx++] = 'p';
    client_id[idx++] = '_';

    /* Append unique ID */
    for (i = (int)id_len - 1; i >= 0; i--)
    {
        uint8_t value    = silicon_id[i];
        client_id[idx++] = hex[value >> 4];
        client_id[idx++] = hex[value & 0xFU];

        if (value != 0)
        {
            id_is_zero = false;
        }
    }

    /* Terminate string */
    client_id[idx] = '\0';

    if (id_is_zero)
    {
        PRINTF(
            "WARNING: MQTT client id is zero. (%s)"
#ifdef OCOTP
            " This might be caused by blank OTP memory."
#endif
            "\r\n",
            client_id);
    }
}

/*!
 * @brief Create and run example thread
 *
 * @param netif  netif which example should use
 */
void mqtt_freertos_run_thread(struct netif *netif)
{
    LOCK_TCPIP_CORE();
    mqtt_client = mqtt_client_new();
    UNLOCK_TCPIP_CORE();
    if (mqtt_client == NULL)
    {
        PRINTF("mqtt_client_new() failed.\r\n");
        while (1)
        {
        }
    }
    Temp_Init();

    generate_client_id();

    if (sys_thread_new("app_task", app_thread, netif, APP_THREAD_STACKSIZE, APP_THREAD_PRIO) == NULL)
    {
        LWIP_ASSERT("mqtt_freertos_start_thread(): Task creation failed.", 0);
    }
}












/***************************
 * Includes
 **************************/
//#include "mqtt_freertos.h"
//#include "board.h"
//#include "fsl_silicon_id.h"
//#include "lwip/opt.h"
//#include "lwip/api.h"
//#include "lwip/apps/mqtt.h"
//#include "lwip/tcpip.h"
//
///***************************
// * Definitions
// **************************/
//#ifndef EXAMPLE_MQTT_SERVER_HOST
//#define EXAMPLE_MQTT_SERVER_HOST "broker.hivemq.com"
//#endif
//#ifndef EXAMPLE_MQTT_SERVER_PORT
//#define EXAMPLE_MQTT_SERVER_PORT 1883
//#endif
//#define INIT_THREAD_STACKSIZE 1024
//#define INIT_THREAD_PRIO       DEFAULT_THREAD_PRIO
//#define APP_THREAD_STACKSIZE   1024
//#define APP_THREAD_PRIO        DEFAULT_THREAD_PRIO
//
//#define TOPIC_PARAM_TEMP   "lwip_topic/Sensors/Parameters/Temperature/Auto"
//#define TOPIC_PARAM_LIGHT  "lwip_topic/Sensors/Parameters/Light/Auto"
//#define TOPIC_PUB_TEMP     "lwip_topic/Sensors/Temperature"
//#define TOPIC_PUB_LIGHT    "lwip_topic/Sensors/Light"
//
///***************************
// * Prototypes
// **************************/
//static void connect_to_mqtt(void *ctx);
//static void mqtt_topic_subscribed_cb(void *arg, err_t err);
//static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len);
//static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags);
//static void mqtt_subscribe_topics(mqtt_client_t *client);
//static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status);
//static void mqtt_message_published_cb(void *arg, err_t err);
//static void publish_message(void *ctx);
//static void Temp_Init(void);
//static uint8_t Temp_GetCelsius(void);
//static void app_thread(void *arg);
//static void generate_client_id(void);
//
///***************************
// * Variables
// **************************/
///* Flags de petición recibida */
//static volatile bool temper_req = false;
//static volatile bool light_req  = false;
//
///* Estado auto‐muestreo */
//static volatile uint8_t Temper_Samp = 0;
//static volatile uint8_t Lights_Samp = 0;
//
///* MQTT client */
//static mqtt_client_t *mqtt_client;
//static char client_id[(SILICONID_MAX_LENGTH * 2) + 5];
//static const struct mqtt_connect_client_info_t mqtt_client_info = {
//    .client_id   = (const char *)&client_id[0],
//    .client_user = NULL,
//    .client_pass = NULL,
//    .keep_alive  = 100,
//    .will_topic  = NULL,
//    .will_msg    = NULL,
//    .will_qos    = 0,
//    .will_retain = 0,
//#if LWIP_ALTCP && LWIP_ALTCP_TLS
//    .tls_config = NULL,
//#endif
//};
//static ip_addr_t mqtt_addr;
//static volatile bool connected = false;
//
///***************************
// * Code
// **************************/
//static void Temp_Init(void)
//{
//    if (((SENSOR_CTRL->MISC_CTRL_REG & SENSOR_CTRL_MISC_CTRL_REG_TIMER_1_ENABLE_MASK) >>
//         SENSOR_CTRL_MISC_CTRL_REG_TIMER_1_ENABLE_SHIFT) == 0U)
//    {
//        SENSOR_CTRL->MISC_CTRL_REG |= SENSOR_CTRL_MISC_CTRL_REG_TIMER_1_ENABLE_MASK;
//    }
//}
//static uint8_t Temp_GetCelsius(void)
//{
//    uint16_t raw = (uint16_t)((SENSOR_CTRL->TSEN_CTRL_1_REG_2 &
//                               SENSOR_CTRL_TSEN_CTRL_1_REG_2_TSEN_TEMP_VALUE_MASK) >>
//                              SENSOR_CTRL_TSEN_CTRL_1_REG_2_TSEN_TEMP_VALUE_SHIFT);
//    float tempC = raw * 0.480561F - 220.7074F;
//    return (uint8_t)tempC;
//}
//
//static void mqtt_topic_subscribed_cb(void *arg, err_t err)
//{
//    const char *topic = (const char *)arg;
//    if (err == ERR_OK)
//        PRINTF("Subscribed to \"%s\".\r\n", topic);
//    else
//        PRINTF("Failed to subscribe \"%s\": %d.\r\n", topic, err);
//}
//
//static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
//{
//    LWIP_UNUSED_ARG(arg);
//    if (strcmp(topic, TOPIC_PARAM_TEMP) == 0) {
//        temper_req = true;
//    }
//    else if (strcmp(topic, TOPIC_PARAM_LIGHT) == 0) {
//        light_req = true;
//    }
//    PRINTF("Inicio de PUBLISH en \"%s\" (bytes: %u)\r\n", topic, tot_len);
//}
//
//static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
//{
//    LWIP_UNUSED_ARG(arg);
//    if (!(flags & MQTT_DATA_FLAG_LAST)) return;
//
//    if (temper_req) {
//        if (len >= 1 && (data[0] == '0' || data[0] == '1')) {
//            Temper_Samp = data[0] - '0';
//            PRINTF("Temper_Auto = %u\r\n", Temper_Samp);
//        }
//        temper_req = false;
//    }
//
//    if (light_req) {
//        if (len >= 1 && (data[0] == '0' || data[0] == '1')) {
//            Lights_Samp = data[0] - '0';
//            PRINTF("Light_Auto = %u\r\n", Lights_Samp);
//        }
//        light_req = false;
//    }
//}
//
//static void mqtt_subscribe_topics(mqtt_client_t *client)
//{
//    static const char *topics[] = { TOPIC_PARAM_LIGHT, TOPIC_PARAM_TEMP };
//    int qos[]                   = { 0, 0 };
//    err_t err;
//    int i;
//
//    mqtt_set_inpub_callback(client,
//                            mqtt_incoming_publish_cb,
//                            mqtt_incoming_data_cb,
//                            LWIP_CONST_CAST(void *, &mqtt_client_info));
//
//    for (i = 0; i < ARRAY_SIZE(topics); i++) {
//        err = mqtt_subscribe(client,
//                             topics[i],
//                             qos[i],
//                             mqtt_topic_subscribed_cb,
//                             LWIP_CONST_CAST(void *, topics[i]));
//        if (err == ERR_OK)
//            PRINTF("Subscribing to \"%s\" qos %d...\r\n", topics[i], qos[i]);
//        else
//            PRINTF("Failed subscribe \"%s\" qos %d: %d\r\n", topics[i], qos[i], err);
//    }
//}
//
//static void mqtt_connection_cb(mqtt_client_t *client, void *arg,
//                               mqtt_connection_status_t status)
//{
//    const struct mqtt_connect_client_info_t *info = (const void *)arg;
//    connected = (status == MQTT_CONNECT_ACCEPTED);
//
//    if (status == MQTT_CONNECT_ACCEPTED) {
//        PRINTF("MQTT \"%s\" connected.\r\n", info->client_id);
//        mqtt_subscribe_topics(client);
//    }
//    else {
//        PRINTF("MQTT connect status: %d. Reintentando...\r\n", status);
//        sys_timeout(1000, connect_to_mqtt, NULL);
//    }
//}
//
//static void mqtt_message_published_cb(void *arg, err_t err)
//{
//    const char *topic = (const char *)arg;
//    if (err == ERR_OK)
//        PRINTF("Published \"%s\" OK.\r\n", topic);
//    else
//        PRINTF("Publish \"%s\" fail: %d.\r\n", topic, err);
//}
//
//static void publish_message(void *ctx)
//{
//    LWIP_UNUSED_ARG(ctx);
//
//    if (Temper_Samp) {
//        char payload[4];
//        int len = snprintf(payload, sizeof(payload), "%u", Temp_GetCelsius());
//        mqtt_publish(mqtt_client,
//                     TOPIC_PUB_TEMP,
//                     payload,
//                     (u16_t)len,
//                     1, 0,
//                     mqtt_message_published_cb,
//                     (void *)TOPIC_PUB_TEMP);
//    }
//
//    /* Luz: siempre publico 0 o 1 */
//    const char *light_msg = Lights_Samp ? "1" : "0";
//    mqtt_publish(mqtt_client,
//                 TOPIC_PUB_LIGHT,
//                 light_msg,
//                 (u16_t)strlen(light_msg),
//                 1, 0,
//                 mqtt_message_published_cb,
//                 (void *)TOPIC_PUB_LIGHT);
//}
//
//static void connect_to_mqtt(void *ctx)
//{
//    LWIP_UNUSED_ARG(ctx);
//    PRINTF("Conectando a %s...\r\n", ipaddr_ntoa(&mqtt_addr));
//    mqtt_client_connect(mqtt_client,
//                        &mqtt_addr,
//                        EXAMPLE_MQTT_SERVER_PORT,
//                        mqtt_connection_cb,
//                        LWIP_CONST_CAST(void *, &mqtt_client_info),
//                        &mqtt_client_info);
//}
//
//static void app_thread(void *arg)
//{
//    struct netif *netif = (struct netif *)arg;
//    err_t err;
//
//    PRINTF("IP: %s\r\n", ipaddr_ntoa(&netif->ip_addr));
//
//    if (ipaddr_aton(EXAMPLE_MQTT_SERVER_HOST, &mqtt_addr)) {
//        err = ERR_OK;
//    } else {
//        PRINTF("Resolviendo %s...\r\n", EXAMPLE_MQTT_SERVER_HOST);
//        err = netconn_gethostbyname(EXAMPLE_MQTT_SERVER_HOST, &mqtt_addr);
//    }
//
//    if (err == ERR_OK) {
//        tcpip_callback(connect_to_mqtt, NULL);
//    } else {
//        PRINTF("Error DNS: %d\r\n", err);
//        return;
//    }
//
//    while (1) {
//        if (connected) {
//            tcpip_callback(publish_message, NULL);
//        }
//        sys_msleep(1000U);
//    }
//}
//
//static void generate_client_id(void)
//{
//    uint8_t silicon_id[SILICONID_MAX_LENGTH];
//    uint32_t id_len = sizeof(silicon_id);
//    char *hex = "0123456789abcdef";
//    int idx = 0, i;
//    bool all_zero = true;
//
//    SILICONID_GetID(silicon_id, &id_len);
//
//    client_id[idx++] = 'n'; client_id[idx++] = 'x';
//    client_id[idx++] = 'p'; client_id[idx++] = '_';
//
//    for (i = id_len - 1; i >= 0; i--) {
//        uint8_t v = silicon_id[i];
//        client_id[idx++] = hex[v >> 4];
//        client_id[idx++] = hex[v & 0xF];
//        if (v) all_zero = false;
//    }
//    client_id[idx] = '\0';
//
//    if (all_zero) {
//        PRINTF("WARNING: client_id is zero.\r\n");
//    }
//}
//
//void mqtt_freertos_run_thread(struct netif *netif)
//{
//    LOCK_TCPIP_CORE();
//    mqtt_client = mqtt_client_new();
//    UNLOCK_TCPIP_CORE();
//
//    if (!mqtt_client) {
//        PRINTF("mqtt_client_new() failed.\r\n");
//        for (;;) {}
//    }
//
//    Temp_Init();
//    generate_client_id();
//
//    if (!sys_thread_new("app_task",
//                        app_thread,
//                        netif,
//                        APP_THREAD_STACKSIZE,
//                        APP_THREAD_PRIO)) {
//        LWIP_ASSERT("Task creation failed", 0);
//    }
//}
