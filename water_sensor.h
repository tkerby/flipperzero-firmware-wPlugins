#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_adc.h>
#include <furi_hal_resources.h>

/*
 * Укажите здесь конкретный внешний аналоговый пин и соответствующий ADC-канал.
 *
 * Примеры:
 *  PC0 (A0):
 *    #define ADC_GPIO (&gpio_ext_pc0)
 *    #define ADC_CHANNEL FuriHalAdcChannel1
 *
 *  PC1 (A1):
 *    #define ADC_GPIO (&gpio_ext_pc1)
 *    #define ADC_CHANNEL FuriHalAdcChannel2
 */

#ifndef ADC_GPIO
#define ADC_GPIO (&gpio_ext_pc0) /* <- замените при необходимости */
#endif

#ifndef ADC_CHANNEL
#define ADC_CHANNEL FuriHalAdcChannel1 /* <- замените при необходимости */
#endif

/* Интервал обновления в миллисекундах */
#define UPDATE_MS 500

/* Точка входа приложения */
int32_t water_sensor_main(void *p);
