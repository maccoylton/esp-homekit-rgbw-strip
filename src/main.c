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
#include <espressif/esp_system.h>
#include <esp/uart.h>
#include <etstimer.h>
#include <esplibs/libmain.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <math.h>
#include <sysparam.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>

#include <multipwm/multipwm.h>
#include <stdio.h>
#include <ir/ir.h>
#include <ir/raw.h>
#include <ir/generic.h>
#include <shared_functions.h>
#include <custom_characteristics.h>
#include <rgbw_lights.h>


#include "main.h"

#define DEVICE_MANUFACTURER "David B Brown"
#define DEVICE_NAME "Led_Strip"
#define DEVICE_MODEL "RGBW"
#define DEVICE_SERIAL "12345678"
#define FW_VERSION "1.0"

#define LPF_SHIFT 8 // divide by 256
#define LPF_INTERVAL 10  // in milliseconds
#define IR_RX_GPIO 4
#define RED_PWM_PIN 12
#define GREEN_PWM_PIN 5
#define BLUE_PWM_PIN 13
#define WHITE_PWM_PIN 15


// add this section to make your device OTA capable
// create the extra characteristic &ota_trigger, at the end of the primary service (before the NULL)
// it can be used in Eve, which will show it, where Home does not
// and apply the four other parameters in the accessories_information section
#include "ota-api.h"

hsi_color_t hsi_colours[77];
const int status_led_gpio = 2; /*set the gloabl variable for the led to be sued for showing status */
int led_off_value=1; /* global varibale to support LEDs set to 0 where the LED is connected to GND, 1 where +3.3v */
// Global variables
float led_hue = 360;              // hue is scaled 0 to 360
float led_saturation = 100;      // saturation is scaled 0 to 100
float led_brightness = 100;     // brightness is scaled 0 to 100
bool led_on = false;            // on is boolean on or off

int white_default_gpio = WHITE_PWM_PIN;
int red_default_gpio = RED_PWM_PIN;
int green_default_gpio = GREEN_PWM_PIN;
int blue_default_gpio = BLUE_PWM_PIN;

rgb_color_t current_color = { { 0, 0, 0, 0 } };
rgb_color_t target_color = { { 0, 0, 0, 0 } };

homekit_characteristic_t wifi_reset   = HOMEKIT_CHARACTERISTIC_(CUSTOM_WIFI_RESET, false, .setter=wifi_reset_set);
homekit_characteristic_t wifi_check_interval   = HOMEKIT_CHARACTERISTIC_(CUSTOM_WIFI_CHECK_INTERVAL, 200, .setter=wifi_check_interval_set);
/* checks the wifi is connected and flashes status led to indicated connected */
homekit_characteristic_t task_stats   = HOMEKIT_CHARACTERISTIC_(CUSTOM_TASK_STATS, false , .setter=task_stats_set);

homekit_characteristic_t ota_trigger  = API_OTA_TRIGGER;
homekit_characteristic_t name         = HOMEKIT_CHARACTERISTIC_(NAME, DEVICE_NAME);
homekit_characteristic_t manufacturer = HOMEKIT_CHARACTERISTIC_(MANUFACTURER,  DEVICE_MANUFACTURER);
homekit_characteristic_t serial       = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, DEVICE_SERIAL);
homekit_characteristic_t model        = HOMEKIT_CHARACTERISTIC_(MODEL,         DEVICE_MODEL);
homekit_characteristic_t revision     = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION,  FW_VERSION);


homekit_characteristic_t on = HOMEKIT_CHARACTERISTIC_(ON, true,
                                                      .getter = led_on_get,
                                                      .setter = led_on_set);

homekit_characteristic_t brightness = HOMEKIT_CHARACTERISTIC_(BRIGHTNESS, 100,
                                                              .getter = led_brightness_get,
                                                              .setter = led_brightness_set);

homekit_characteristic_t hue = HOMEKIT_CHARACTERISTIC_(HUE, 0,
                                                       .getter = led_hue_get,
                                                       .setter = led_hue_set);

homekit_characteristic_t saturation = HOMEKIT_CHARACTERISTIC_(SATURATION, 0,
                                                              .getter = led_saturation_get,
                                                              .setter = led_saturation_set);

homekit_characteristic_t red_gpio     = HOMEKIT_CHARACTERISTIC_( CUSTOM_RED_GPIO, RED_PWM_PIN, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_update) );
homekit_characteristic_t green_gpio   = HOMEKIT_CHARACTERISTIC_( CUSTOM_GREEN_GPIO, GREEN_PWM_PIN, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_update) );
homekit_characteristic_t blue_gpio    = HOMEKIT_CHARACTERISTIC_( CUSTOM_BLUE_GPIO, BLUE_PWM_PIN, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_update) );
homekit_characteristic_t white_gpio   = HOMEKIT_CHARACTERISTIC_( CUSTOM_WHITE_GPIO, WHITE_PWM_PIN, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_update) );

homekit_characteristic_t colours_gpio_test   = HOMEKIT_CHARACTERISTIC_(CUSTOM_COLOURS_GPIO_TEST, false , .setter=colours_gpio_test_set, .getter=colours_gpio_test_get);
homekit_characteristic_t colours_strobe = HOMEKIT_CHARACTERISTIC_(CUSTOM_COLOURS_STROBE, false , .setter=colours_strobe_set, .getter=colours_strobe_get);
homekit_characteristic_t colours_flash = HOMEKIT_CHARACTERISTIC_(CUSTOM_COLOURS_FLASH, false , .setter=colours_flash_set, .getter=colours_flash_get);
homekit_characteristic_t colours_fade = HOMEKIT_CHARACTERISTIC_(CUSTOM_COLOURS_FADE, false , .setter=colours_fade_set, .getter=colours_smooth_get);
homekit_characteristic_t colours_smooth = HOMEKIT_CHARACTERISTIC_(CUSTOM_COLOURS_SMOOTH, false ,.setter=colours_smooth_set, .getter=colours_smooth_get);


double __ieee754_remainder(double x, double y) {
    return x - y * floor(x/y);
}

void ir_dump_task(void *arg) {
    
    ir_rx_init(IR_RX_GPIO, 1024);
    
    ir_decoder_t *nec_decoder = ir_generic_make_decoder(&nec_protocol_config);
    
    int16_t buffer_size = sizeof(uint8_t) * 1024;
    int8_t *buffer = malloc(buffer_size);
    int size=0;
    
    
    while (1) {
        size = ir_recv(nec_decoder, 0, buffer, buffer_size);
        if (size <= 0)
            continue;
        
        printf("%s: Decoded packet (size = %d):\n", __func__, size);
        for (int i=0; i < size; i++) {
            printf("%5d ", buffer[i]);
        }
        printf("\n");
        
        /*
        printf ("\n%s: buffer[2]=%d, mh_on[2]=%d, mh_off[2]=%d, buffer[3]=%d, mh_on[3]=%d, mh_off[3]=%d\n", __func__, buffer[2], mh_on[2], mh_off[2], buffer[3], mh_on[3], mh_off[3]);
        */
        
        int cmd = buffer[2];
        int effect = off_effect;
                
        switch (cmd)
        {
            case on_button:
                printf ("%s: LED command On\n",__func__);
                led_on=true;
                on.value = HOMEKIT_BOOL (true);
                homekit_characteristic_notify(&on,on.value );
                sdk_os_timer_arm (&save_timer, SAVE_DELAY, 0 );
                break;
            case off_button:
                printf ("%s: LED command Off\n",__func__);
                led_on=false;
                on.value = HOMEKIT_BOOL (false);
                homekit_characteristic_notify(&on,on.value );
                sdk_os_timer_arm (&save_timer, SAVE_DELAY, 0 );
                break;
            case up_button:
                printf ("%s: LED command Up\n",__func__);
                if (brightness.value.int_value <= 90){
                    brightness.value.int_value +=10;
                }
                break;
            case down_button:
                printf ("%s: LED command Down\n",__func__);
                if (brightness.value.int_value >=10){
                    brightness.value.int_value -=10;
                }
                break;
            case strobe_button:
                printf ("%s: LED command Strobe\n",__func__);
                effect = strobe_effect;
                break;
            case smooth_button:
                printf ("%s: LED command Smooth\n",__func__);
                effect = smooth_effect;
                break;
            case fade_button:
                printf ("%s: LED command Fade\n",__func__);
                effect = fade_effect;
                break;
            case flash_buton:
                printf ("%s: LED command Flash\n",__func__);
                effect = flash_effect;
                break;
            case aubergene_button:
            case cream_button:
            case purple_button:
            case pink_button:
            case blue_button:
            case light_green_button:
            case green5_button:
            case white_button:
            case light_blue_button:
            case dark_orange_button:
            case red_button:
            case green_button:
            case yellow_button:
            case green4_button:
            case orange_button:
            case sky_blue_button:
                printf ("%s: LED command %d\n",__func__, cmd);
                hue.value = HOMEKIT_FLOAT (hsi_colours[cmd].hue);
                saturation.value = HOMEKIT_FLOAT (hsi_colours[cmd].saturation);
                brightness.value = HOMEKIT_INT (hsi_colours[cmd].brightness);
                break;
            default:
                printf ("%s: LED command unknown %d\n",__func__, buffer[cmd]);
                break;
        }

        homekit_characteristic_notify(&hue,hue.value );
        homekit_characteristic_notify(&saturation,saturation.value );
        homekit_characteristic_notify(&brightness,brightness.value );
        sdk_os_timer_arm (&save_timer, SAVE_DELAY, 0 );
        
        led_hue = hue.value.float_value;
        led_saturation = saturation.value.float_value;
        led_brightness = brightness.value.int_value;

        colour_effect_start_stop (effect);
        if (effect == off_effect ) /* if colour effect is off normal logic applies, otherise start the effect */
        {
            if (led_on==true){
                sdk_os_timer_arm (&rgbw_set_timer, RGBW_SET_DELAY, 0 );
            } else {
                printf ("%s: Led on false so stopping Multi PWM\n", __func__);
                set_colours (0, 0, 0, 0);
                multipwm_stop(&pwm_info);
                sdk_os_timer_disarm (&rgbw_set_timer);
            }
        }
    }
}


void led_strip_init (){
    

    /* set th default values for the GPIOs incase we need to rest them later */

    rgbw_lights_init();
    
    hsi_colours[white_button]  = (hsi_color_t) {{0.0, 0.0, 100}};
    
    hsi_colours[red_button] = (hsi_color_t) {{ 0.0, 100.0, 100}};
    hsi_colours[dark_orange_button] = (hsi_color_t) { { 15, 100, 78 }};
    hsi_colours[orange_button] = (hsi_color_t) { { 30, 100, 100 }};
    hsi_colours[cream_button] = (hsi_color_t) { { 45, 100, 100 }};
    hsi_colours[yellow_button] = (hsi_color_t) { { 60, 100, 100 }};
    
    hsi_colours[green_button] = (hsi_color_t) { { 120, 100, 100 }};
    hsi_colours[light_green_button] = (hsi_color_t) { { 140, 100, 100 }};
    hsi_colours[sky_blue_button] = (hsi_color_t) { { 180, 100, 100 }};
    hsi_colours[green4_button] = (hsi_color_t) { { 180, 100, 80 }};
    hsi_colours[green5_button] = (hsi_color_t) { { 180, 100, 60 }};
    
    hsi_colours[blue_button] = (hsi_color_t) { { 240, 100, 100 }};
    hsi_colours[light_blue_button] = (hsi_color_t) { { 255, 100, 100 }};
    hsi_colours[aubergene_button] = (hsi_color_t) { { 280, 100, 50 }};
    hsi_colours[purple_button] = (hsi_color_t) { { 280, 100, 75 }};
    hsi_colours[pink_button] = (hsi_color_t) { { 300, 100, 100 }};

    xTaskCreate(ir_dump_task, "read_ir_task", 256, NULL, 2, NULL);

}


homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id = 1, .category = homekit_accessory_category_lightbulb, .services = (homekit_service_t*[]) {
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
            &name,
            &manufacturer,
            &serial,
            &model,
            &revision,
            HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
            NULL
        }),
        HOMEKIT_SERVICE(LIGHTBULB, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "LED Strip"),
            &on,
            &saturation,
            &hue,
            &brightness,
            &red_gpio,
            &green_gpio,
            &blue_gpio,
            &white_gpio,
            &ota_trigger,
            &wifi_reset,
            &wifi_check_interval,
            &task_stats,
            &colours_gpio_test,
            &colours_strobe,
            &colours_flash,
            &colours_fade,
            &colours_smooth,
            NULL
        }),
        NULL
    }),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111" ,
    .setupId = "1342",
    .on_event = on_homekit_event
};

void recover_from_reset (int reason){
    /* called if we restarted abnormally */
    printf ("%s: reason %d\n", __func__, reason);
    load_characteristic_from_flash(&on);
}

void accessory_init_not_paired (void) {
    /* initalise anything you don't want started until wifi and homekit imitialisation is confirmed, but not paired */
}

void accessory_init (void ){
    /* initalise anything you don't want started until wifi and pairing is confirmed */
    get_sysparam_info();
    printf ("%s: GPIOS are set as follows : W=%d, R=%d, G=%d, B=%d\n",__func__, white_gpio.value.int_value,red_gpio.value.int_value, green_gpio.value.int_value, blue_gpio.value.int_value );
    led_strip_init ();

    /* sent out values loded from flash, if nothing was loaded from flash then this will be default values */
    homekit_characteristic_notify(&hue,hue.value);
    homekit_characteristic_notify(&saturation,saturation.value );
    homekit_characteristic_notify(&brightness,brightness.value );
}

void user_init(void) {
    
    standard_init (&name, &manufacturer, &model, &serial, &revision);
    
    
    wifi_config_init(DEVICE_NAME, NULL, on_wifi_ready);
    
    
}
