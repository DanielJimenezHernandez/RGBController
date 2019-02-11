/**
 * @file dht.h
 *
 * ESP-IDF driver for DHT11/DHT22
 *
 * Ported from esp-open-rtos
 *
 * Copyright (C) 2016 Jonathan Hartsuiker (https://github.com/jsuiker)
 * Copyright (C) 2018 Ruslan V. Uss (https://github.com/UncleRus)
 * BSD Licensed as described in the file LICENSE
 */
#ifndef __DHT_H__
#define __DHT_H__

#include <driver/gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Sensor type
 */
typedef enum
{
    DHT_TYPE_DHT11 = 0, //!< DHT11
    DHT_TYPE_DHT22      //!< DHT22
} dht_sensor_type_t;

#define DHT_TYPE DHT_TYPE_DHT22

typedef struct{
        float humidity;
        float temperature;
}sensor_data_t;

sensor_data_t sensor_data;

/**
 * @brief Read data from sensor on specified pin.
 * Humidity and temperature are returned as integers.
 * For example: humidity=625 is 62.5 %
 *              temperature=24.4 is 24.4 degrees Celsius
 *
 * @param[in] sensor_type DHT11 or DHT22
 * @param[in] pin GPIO pin connected to sensor OUT
 * @param[out] humidity Humidity, percents * 10
 * @param[out] temperature Temperature, degrees Celsius * 10
 * @return `ESP_OK` on success
 */
esp_err_t dht_read_data(dht_sensor_type_t sensor_type, gpio_num_t pin,
        int16_t *humidity, int16_t *temperature);

/**
 * @brief Read data from sensor on specified pin.
 * Humidity and temperature are returned as floats.
 *
 * @param[in] sensor_type DHT11 or DHT22
 * @param[in] pin GPIO pin connected to sensor OUT
 * @param[out] humidity Humidity, percents
 * @param[out] temperature Temperature, degrees Celsius
 * @return `ESP_OK` on success
 */
esp_err_t dht_read_float_data(dht_sensor_type_t sensor_type, gpio_num_t pin,
        float *humidity, float *temperature);

void dht_init();

void get_dht_data(sensor_data_t *data);

#ifdef __cplusplus
}
#endif

#endif  // __DHT_H__
