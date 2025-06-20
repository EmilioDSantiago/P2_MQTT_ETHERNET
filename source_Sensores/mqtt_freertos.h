/*
 * Copyright 2022 NXP
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MQTT_FREERTOS_H
#define MQTT_FREERTOS_H

#include "lwip/netif.h"

/*!
 * @brief Create and run example thread
 *
 * @param netif  netif which example should use
 */
void mqtt_freertos_run_thread(struct netif *netif);
static void app_light(void *arg);
static void app_temp(void *arg);
static void Temp_Init(void);
static uint8_t Temp_GetCelsius(void);


#endif /* MQTT_FREERTOS_H */
