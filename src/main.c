//
//  main.c
//  esp-homekit-rgbw-strip
//
//  Created by David B Brown on 16/06/2018.
//  Copyright © 2018 maccoylton. All rights reserved.
//
/*
 * This is an example of an rgb led strip using Magic Home wifi controller
 *
 * Debugging printf statements and UART are disabled below because it interfere with mutipwm
 * you can uncomment them for debug purposes
 *
 * more info about the controller and flashing can be found here:
 * https://github.com/arendst/Sonoff-Tasmota/wiki/MagicHome-LED-strip-controller
 *
 * Based on code Contributed April 2018 by https://github.com/PCSaito
 */

#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <math.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>

#include <multipwm/multipwm.h>
#include "main.h"

#define LPF_SHIFT 4  // divide by 16
#define LPF_INTERVAL 10  // in milliseconds


#define WHITE_PWM_PIN 13
#define BLUE_PWM_PIN 12
#define RED_PWM_PIN 5
#define GREEN_PWM_PIN 14
#define LED_RGB_SCALE 32768       // this is the scaling factor used for color conversion

// add this section to make your device OTA capable
// create the extra characteristic &ota_trigger, at the end of the primary service (before the NULL)
// it can be used in Eve, which will show it, where Home does not
// and apply the four other parameters in the accessories_information section

#include "ota-api.h"
#define DEVICE_MANUFACTURER "David B Brown"
#define DEVICE_NAME "Led Strip"
#define DEVICE_MODEL "RGBW"
#define DEVICE_SERIAL "12345678"
#define FW_VERSION "1.0"

homekit_characteristic_t ota_trigger  = API_OTA_TRIGGER;
homekit_characteristic_t name         = HOMEKIT_CHARACTERISTIC_(NAME, DEVICE_NAME);
homekit_characteristic_t manufacturer = HOMEKIT_CHARACTERISTIC_(MANUFACTURER,  DEVICE_MANUFACTURER);
homekit_characteristic_t serial       = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, DEVICE_SERIAL);
homekit_characteristic_t model        = HOMEKIT_CHARACTERISTIC_(MODEL,         DEVICE_MODEL);
homekit_characteristic_t revision     = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION,  FW_VERSION);


typedef union {
    struct {
        uint16_t white;
        uint16_t blue;
        uint16_t green;
        uint16_t red;
    };
    uint64_t color;
} rgb_color_t;

// Color smoothing variables
rgb_color_t current_color = { { 0, 0, 0, 0 } };
rgb_color_t target_color = { { 0, 0, 0, 0 } };

// Global variables
float led_hue = 0;              // hue is scaled 0 to 360
float led_saturation = 59;      // saturation is scaled 0 to 100
float led_brightness = 100;     // brightness is scaled 0 to 100
bool led_on = false;            // on is boolean on or off
uint8_t pins[] = {WHITE_PWM_PIN, BLUE_PWM_PIN,GREEN_PWM_PIN, RED_PWM_PIN};
pwm_info_t pwm_info;



double __ieee754_remainder(double x, double y) {
    return x - y * floor(x/y);
}


void led_strip_init(){

    multipwm_init(&pwm_info);
    multipwm_set_freq(&pwm_info, 65535);
    for (uint8_t i=0; i<pwm_info.channels; i++) {
        multipwm_set_pin(&pwm_info, i, pins[i]);
        printf ("Set pin %d \n",i);
    }
}
    

#define DEG_TO_RAD(X) (M_PI*(X)/180)
//http://blog.saikoled.com/post/44677718712/how-to-convert-from-hsi-to-rgb-white
void hsi2rgbw(float h, float s, float i, rgb_color_t* rgbw) {
    int r, g, b;
    
    while (h < 0) { h += 360.0F; };     // cycle h around to 0-360 degrees
    while (h >= 360) { h -= 360.0F; };
    h = 3.14159F*h / 180.0F;            // convert to radians.
    s /= 100.0F;                        // from percentage to ratio
    i /= 100.0F;                        // from percentage to ratio
    s = s > 0 ? (s < 1 ? s : 1) : 0;    // clamp s and i to interval [0,1]
    i = i > 0 ? (i < 1 ? i : 1) : 0;    // clamp s and i to interval [0,1]
    //i = i * sqrt(i);                    // shape intensity to have finer granularity near 0
    
    if (h < 2.09439) {
        r = LED_RGB_SCALE * i / 3 * (1 + s * cos(h) / cos(1.047196667 - h));
        g = LED_RGB_SCALE * i / 3 * (1 + s * (1 - cos(h) / cos(1.047196667 - h)));
        b = LED_RGB_SCALE * i / 3 * (1 - s);
    }
    else if (h < 4.188787) {
        h = h - 2.09439;
        g = LED_RGB_SCALE * i / 3 * (1 + s * cos(h) / cos(1.047196667 - h));
        b = LED_RGB_SCALE * i / 3 * (1 + s * (1 - cos(h) / cos(1.047196667 - h)));
        r = LED_RGB_SCALE * i / 3 * (1 - s);
    }
    else {
        h = h - 4.188787;
        b = LED_RGB_SCALE * i / 3 * (1 + s * cos(h) / cos(1.047196667 - h));
        r = LED_RGB_SCALE * i / 3 * (1 + s * (1 - cos(h) / cos(1.047196667 - h)));
        g = LED_RGB_SCALE * i / 3 * (1 - s);
    }
    
    rgbw->red = (uint8_t) r;
    rgbw->green = (uint8_t) g;
    rgbw->blue = (uint8_t) b;
    rgbw->white=0;
}

void hsi2rgbw_hack(float h, float s, float i, rgb_color_t* rgbw) {
    uint16_t r, g, b, w;
    float cos_h, cos_1047_h;
    //h = fmod(h,360); // cycle h around to 0-360 degrees
    h = 3.14159*h/(float)180; // Convert to radians.
    s /=(float)100; i/=(float)100; //from percentage to ratio
    s = s>0?(s<1?s:1):0; // clamp s and i to interval [0,1]
    i = i>0?(i<1?i:1):0;
    i = i*sqrt(i); //shape intensity to have finer granularity near 0
    
    if(h < 2.09439) {
        cos_h = cos(h);
        cos_1047_h = cos(1.047196667-h);
        r = s*LED_RGB_SCALE*i/3*(1+cos_h/cos_1047_h);
        g = s*LED_RGB_SCALE*i/3*(1+(1-cos_h/cos_1047_h));
        b = 0;
        w = LED_RGB_SCALE*(1-s)*i;
    } else if(h < 4.188787) {
        h = h - 2.09439;
        cos_h = cos(h);
        cos_1047_h = cos(1.047196667-h);
        g = s*LED_RGB_SCALE*i/3*(1+cos_h/cos_1047_h);
        b = s*LED_RGB_SCALE*i/3*(1+(1-cos_h/cos_1047_h));
        r = 0;
        w = LED_RGB_SCALE*(1-s)*i;
    } else {
        h = h - 4.188787;
        cos_h = cos(h);
        cos_1047_h = cos(1.047196667-h);
        b = s*LED_RGB_SCALE*i/3*(1+cos_h/cos_1047_h);
        r = s*LED_RGB_SCALE*i/3*(1+(1-cos_h/cos_1047_h));
        g = 0;
        w = LED_RGB_SCALE*(1-s)*i;
    }
    
    rgbw->red=r;
    rgbw->green=g;
    rgbw->blue=b;
    rgbw->white=w;
}

void led_strip_send (rgb_color_t *color){
    
    multipwm_stop(&pwm_info);
    multipwm_set_duty(&pwm_info, 0, color->white);
    multipwm_set_duty(&pwm_info, 1, color->blue);
    multipwm_set_duty(&pwm_info, 2, color->green);
    multipwm_set_duty(&pwm_info, 3, color->red);
    printf ("sending r=%d,g=%d, b=%d, w=%d,\n", color->red,color->green, color->blue, color->white );
    multipwm_start(&pwm_info);
    
}

void led_strip_set() {

    if (led_on) {
        // convert HSI to RGBW
        hsi2rgbw_hack(led_hue, led_saturation, led_brightness, &target_color);
        printf("h=%d,s=%d,b=%d => r=%d,g=%d, b=%d, w=%d,\n",(int)led_hue,(int)led_saturation,(int)led_brightness, target_color.red,target_color.green, target_color.blue, target_color.white );
    } else {
        printf("led srtip off\n");
        target_color.red = 0;
        target_color.green = 0;
        target_color.blue = 0;
        target_color.white = 0;
    }
    
 //   current_color.red += ((target_color.red * 256) - current_color.red) >> LPF_SHIFT ;
 //   current_color.green += ((target_color.green * 256) - current_color.green) >> LPF_SHIFT ;
 //   current_color.blue += ((target_color.blue * 256) - current_color.blue) >> LPF_SHIFT ;

    
    
    led_strip_send(&target_color);
}

void led_identify_task(void *_args) {

    printf("LED identify\n");

    rgb_color_t black_color = { { 0, 0, 0, 0 } };
    rgb_color_t white_color = { { 128, 128, 128, 128 } };
    
    for (int i=0; i<3; i++) {
        for (int j=0; j<2; j++) {
            led_strip_send (&black_color);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            led_strip_send (&white_color);
            vTaskDelay(100 / portTICK_PERIOD_MS);

        }
        
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }
    

    led_strip_send (&target_color);
    vTaskDelete(NULL);
}

void led_identify(homekit_value_t _value) {
    xTaskCreate(led_identify_task, "LED identify", 128, NULL, 2, NULL);
}

homekit_value_t led_on_get() {
    return HOMEKIT_BOOL(led_on);
}

void led_on_set(homekit_value_t value) {
    if (value.format != homekit_format_bool) {
        // printf("Invalid on-value format: %d\n", value.format);
        return;
    }
    
    led_on = value.bool_value;
    led_strip_set();
}

homekit_value_t led_brightness_get() {
    return HOMEKIT_INT(led_brightness);
}

void led_brightness_set(homekit_value_t value) {
    if (value.format != homekit_format_int) {
        // printf("Invalid brightness-value format: %d\n", value.format);
        return;
    }
    led_brightness = value.int_value;
    led_strip_set();
}

homekit_value_t led_hue_get() {
    return HOMEKIT_FLOAT(led_hue);
}

void led_hue_set(homekit_value_t value) {
    if (value.format != homekit_format_float) {
        // printf("Invalid hue-value format: %d\n", value.format);
        return;
    }
    led_hue = value.float_value;
    led_strip_set();
}

homekit_value_t led_saturation_get() {
    return HOMEKIT_FLOAT(led_saturation);
}

void led_saturation_set(homekit_value_t value) {
    if (value.format != homekit_format_float) {
        // printf("Invalid sat-value format: %d\n", value.format);
        return;
    }
    led_saturation = value.float_value;
    led_strip_set();
}

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id = 1, .category = homekit_accessory_category_lightbulb, .services = (homekit_service_t*[]) {
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
            &name,
            &manufacturer,
            &serial,
            &model,
            &revision,
            HOMEKIT_CHARACTERISTIC(IDENTIFY, led_identify),
            NULL
        }),
        HOMEKIT_SERVICE(LIGHTBULB, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "LED Strip"),
            HOMEKIT_CHARACTERISTIC(
                                   ON, true,
                                   .getter = led_on_get,
                                   .setter = led_on_set
                                   ),
            HOMEKIT_CHARACTERISTIC(
                                   BRIGHTNESS, 100,
                                   .getter = led_brightness_get,
                                   .setter = led_brightness_set
                                   ),
            HOMEKIT_CHARACTERISTIC(
                                   HUE, 0,
                                   .getter = led_hue_get,
                                   .setter = led_hue_set
                                   ),
            HOMEKIT_CHARACTERISTIC(
                                   SATURATION, 0,
                                   .getter = led_saturation_get,
                                   .setter = led_saturation_set
                                   ),
            &ota_trigger,
            NULL
        }),
        NULL
    }),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111"    
};


void create_accessory_name() {
    
    int serialLength = snprintf(NULL, 0, "%d", sdk_system_get_chip_id());
    
    char *serialNumberValue = malloc(serialLength + 1);
    
    snprintf(serialNumberValue, serialLength + 1, "%d", sdk_system_get_chip_id());
    
    int name_len = snprintf(NULL, 0, "%s-%s-%s",
                            DEVICE_NAME,
                            DEVICE_MODEL,
                            serialNumberValue);
    
    if (name_len > 63) {
        name_len = 63;
    }
    
    char *name_value = malloc(name_len + 1);
    
    snprintf(name_value, name_len + 1, "%s-%s-%s",
             DEVICE_NAME, DEVICE_MODEL, serialNumberValue);
    
    
    name.value = HOMEKIT_STRING(name_value);
    serial.value = name.value;
}


void user_init(void) {
    //uart_set_baud(0, 115200);
    

    create_accessory_name();
    
    pwm_info.channels = 4;
    led_strip_init();

    int c_hash=ota_read_sysparam(&manufacturer.value.string_value,&serial.value.string_value,
                                 &model.value.string_value,&revision.value.string_value);
    if (c_hash==0) c_hash=1;
    config.accessories[0]->config_number=c_hash;
    
    homekit_server_init(&config);
}
