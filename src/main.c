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
#include "main.h"
#include <ir/ir.h>
#include <ir/raw.h>
#include <ir/generic.h>
#include <custom_characteristics.h>
#include <udplogger.h>
#include <shared_functions.h>
#include <colour_conversion.h>



#define LPF_SHIFT 8 // divide by 256
#define LPF_INTERVAL 10  // in milliseconds
#define IR_RX_GPIO 4

#define DUMMY_PWM_PIN 14
#define WHITE_PWM_PIN 15
#define BLUE_PWM_PIN 13
#define RED_PWM_PIN 12
#define GREEN_PWM_PIN 5
#define PWM_SCALE 255
#define LED_STRIP_SET_DELAY 100


// add this section to make your device OTA capable
// create the extra characteristic &ota_trigger, at the end of the primary service (before the NULL)
// it can be used in Eve, which will show it, where Home does not
// and apply the four other parameters in the accessories_information section

#include "ota-api.h"
#define DEVICE_MANUFACTURER "David B Brown"
#define DEVICE_NAME "Led_Strip"
#define DEVICE_MODEL "RGBW"
#define DEVICE_SERIAL "12345678"
#define FW_VERSION "1.0"

hsi_color_t hsi_colours[77];

rgb_color_t current_color = { { 0, 0, 0, 0 } };
rgb_color_t target_color = { { 0, 0, 0, 0 } };


void on_update(homekit_characteristic_t *ch, homekit_value_t value, void *context);
static pwm_info_t pwm_info;
ETSTimer led_strip_timer;

homekit_value_t led_on_get();

void led_on_set(homekit_value_t value);

homekit_value_t led_brightness_get();

void led_brightness_set(homekit_value_t value);

homekit_value_t led_hue_get();

void led_hue_set(homekit_value_t value);

homekit_value_t led_saturation_get();

void led_saturation_set(homekit_value_t value);

homekit_characteristic_t wifi_reset   = HOMEKIT_CHARACTERISTIC_(CUSTOM_WIFI_RESET, false, .setter=wifi_reset_set);
homekit_characteristic_t wifi_check_interval   = HOMEKIT_CHARACTERISTIC_(CUSTOM_WIFI_CHECK_INTERVAL, 10, .setter=wifi_check_interval_set);
/* checks the wifi is connected and flashes status led to indicated connected */
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

homekit_characteristic_t red_gpio     = HOMEKIT_CHARACTERISTIC_( CUSTOM_RED_GPIO, 12, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_update) );
homekit_characteristic_t green_gpio   = HOMEKIT_CHARACTERISTIC_( CUSTOM_GREEN_GPIO, 5, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_update) );
homekit_characteristic_t blue_gpio    = HOMEKIT_CHARACTERISTIC_( CUSTOM_BLUE_GPIO, 13, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_update) );
homekit_characteristic_t white_gpio   = HOMEKIT_CHARACTERISTIC_( CUSTOM_WHITE_GPIO, 15, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_update) );
homekit_characteristic_t led_boost    = HOMEKIT_CHARACTERISTIC_( CUSTOM_LED_BOOST, 0, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_update) );

const int status_led_gpio = 2; /*set the gloabl variable for the led to be sued for showing status */
int led_off_value=1; /* global varibale to support LEDs set to 0 where the LED is connected to GND, 1 where +3.3v */

// Global variables
float led_hue = 0;              // hue is scaled 0 to 360
float led_saturation = 59;      // saturation is scaled 0 to 100
float led_brightness = 100;     // brightness is scaled 0 to 100
bool led_on = false;            // on is boolean on or off



void default_rgbw_pins() {
    red_gpio.value.int_value = RED_PWM_PIN;
    green_gpio.value.int_value = GREEN_PWM_PIN;
    blue_gpio.value.int_value = BLUE_PWM_PIN;
    white_gpio.value.int_value = WHITE_PWM_PIN;

}


void on_update (homekit_characteristic_t *ch, homekit_value_t value, void *context	){
    if (red_gpio.value.int_value == green_gpio.value.int_value ||
        red_gpio.value.int_value == blue_gpio.value.int_value ||
        red_gpio.value.int_value == white_gpio.value.int_value ||
        green_gpio.value.int_value == blue_gpio.value.int_value ||
        green_gpio.value.int_value == white_gpio.value.int_value ||
        blue_gpio.value.int_value == white_gpio.value.int_value)
    {
        default_rgbw_pins();
    }
}

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
        
        printf("Decoded packet (size = %d):\n", size);
        for (int i=0; i < size; i++) {
            printf("%5d ", buffer[i]);
        }
        printf("\n");
        
        printf ("\nbuffer[2]=%d, mh_on[2]=%d, mh_off[2]=%d, buffer[3]=%d, mh_on[3]=%d, mh_off[3]=%d\n", buffer[2], mh_on[2], mh_off[2], buffer[3], mh_on[3], mh_off[3]);
        
        int cmd = buffer[2];
        
        switch (cmd)
        {
            case on_button:
                printf ("%s: LED command On\n",__func__);
                led_on=true;
                on.value = HOMEKIT_BOOL (true);
                break;
            case off:
                printf ("%s: LED command Off\n",__func__);
                led_on=false;
                on.value = HOMEKIT_BOOL (false);
                break;
            case up:
                printf ("%s: LED command Up\n",__func__);
                if (brightness.value.int_value <= 90){
                    brightness.value.int_value +=10;
                }
                break;
            case down:
                printf ("%s: LED command Down\n",__func__);
                if (brightness.value.int_value >=10){
                    brightness.value.int_value -=10;
                }
                break;
            case strobe:
                printf ("%s: LED command Strobe\n",__func__);
                break;
            case smooth:
                printf ("%s: LED command Smooth\n",__func__);
                break;
            case fade:
                printf ("%s: LED command Fade\n",__func__);
                break;
            case flash:
                printf ("%s: LED command Flash\n",__func__);
                break;
            case aubergene:
            case cream:
            case purple:
            case pink:
            case blue:
            case light_green:
            case green5:
            case white:
            case light_blue:
            case dark_orange:
            case red:
            case green:
            case yellow:
            case green4:
            case orange:
            case sky_blue:
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
        
        led_hue = hue.value.float_value;
        led_saturation = saturation.value.float_value;
        led_brightness = brightness.value.int_value;
        
        if (led_on==true){
            sdk_os_timer_arm (&led_strip_timer, LED_STRIP_SET_DELAY, 0 );
        } else {
            printf ("%s: Led on false so stopping Multi PWM\n", __func__);
            multipwm_set_duty(&pwm_info, 0, 0);
            multipwm_set_duty(&pwm_info, 1, 0);
            multipwm_set_duty(&pwm_info, 2, 0);
            multipwm_set_duty(&pwm_info, 3, 0);
/*            sdk_os_timer_disarm(&led_strip_timer);
            multipwm_stop(&pwm_info);*/
        }
    }
}

    




void led_strip_set (){

    printf("\n%s: Current colour before set r=%d,g=%d, b=%d, w=%d,\n",__func__, current_color.red,current_color.green, current_color.blue, current_color.white );
    if (led_on) {
        // convert HSI to RGBW
/*        hsi2rgb(led_hue, led_saturation, led_brightness, &target_color);*/
        HSVtoRGB(led_hue, led_saturation, led_brightness, &target_color);
        printf("%s: h=%d,s=%d,b=%d => r=%d,g=%d, b=%d\n",__func__, (int)led_hue,(int)led_saturation,(int)led_brightness, target_color.red,target_color.green, target_color.blue );
        RBGtoRBGW (&target_color);

        printf("%s: h=%d,s=%d,b=%d => r=%d,g=%d, b=%d, w=%d,\n",__func__, (int)led_hue,(int)led_saturation,(int)led_brightness, target_color.red,target_color.green, target_color.blue, target_color.white );

    } else {
        printf("%s: led srtip off\n", __func__);
        target_color.red = 1;
        target_color.green = 1;
        target_color.blue = 1;
        target_color.white = 1;
    }
    

    current_color.red = target_color.red * PWM_SCALE;
    current_color.green = target_color.green * PWM_SCALE;
    current_color.blue = target_color.blue * PWM_SCALE;
    current_color.white = target_color.white * PWM_SCALE;
    
    printf("%s:Current colour after set r=%d,g=%d, b=%d, w=%d,\n",__func__, current_color.red,current_color.green, current_color.blue, current_color.white );
   
    printf ("%s: Stopping multipwm \n",__func__);
    multipwm_stop(&pwm_info);
    multipwm_set_duty(&pwm_info, 0, current_color.white);
    multipwm_set_duty(&pwm_info, 1, current_color.blue);
    multipwm_set_duty(&pwm_info, 2, current_color.green);
    multipwm_set_duty(&pwm_info, 3, current_color.red);
/*    if (led_on==true) {*/
        multipwm_start(&pwm_info);
        printf ("%s: Starting multipwm \n\n",__func__);
/*    }*/
    
}

void led_strip_init (){
    
    uint8_t pins[] = {WHITE_PWM_PIN, BLUE_PWM_PIN,GREEN_PWM_PIN, RED_PWM_PIN, DUMMY_PWM_PIN};
    pwm_info.channels = 5;
    
    multipwm_init(&pwm_info);
/*    multipwm_set_freq(&pwm_info, 65535);*/
    for (uint8_t i=0; i<pwm_info.channels; i++) {
        multipwm_set_pin(&pwm_info, i, pins[i]);
        printf ("Set pin %d to %d\n",i, pins[i]);
    }
    multipwm_set_duty(&pwm_info, 4, 65025);
    sdk_os_timer_setfn(&led_strip_timer, led_strip_set, NULL);
    printf ("Sdk_os_timer_Setfn called\n");
    
    xTaskCreate(ir_dump_task, "read_ir_task", 256, NULL, 2, NULL);
    
    
    hsi_colours[white]  = (hsi_color_t) {{0.0, 0.0, 100}};
    
    hsi_colours[red] = (hsi_color_t) {{ 0.0, 100.0, 100}};
    hsi_colours[dark_orange] = (hsi_color_t) { { 15, 100, 78 }};
    hsi_colours[orange] = (hsi_color_t) { { 30, 100, 100 }};
    hsi_colours[cream] = (hsi_color_t) { { 45, 100, 100 }};
    hsi_colours[yellow] = (hsi_color_t) { { 60, 100, 100 }};
    
    
    hsi_colours[green] = (hsi_color_t) { { 120, 100, 100 }};
    hsi_colours[light_green] = (hsi_color_t) { { 140, 100, 100 }};
    hsi_colours[sky_blue] = (hsi_color_t) { { 180, 100, 100 }};
    hsi_colours[green4] = (hsi_color_t) { { 180, 100, 80 }};
    hsi_colours[green5] = (hsi_color_t) { { 180, 100, 60 }};
    
    
    hsi_colours[blue] = (hsi_color_t) { { 240, 100, 100 }};
    hsi_colours[light_blue] = (hsi_color_t) { { 255, 100, 100 }};
    hsi_colours[aubergene] = (hsi_color_t) { { 280, 100, 50 }};
    hsi_colours[purple] = (hsi_color_t) { { 280, 100, 75 }};
    hsi_colours[pink] = (hsi_color_t) { { 300, 100, 100 }};


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
    if (led_on == false )
    {
/*        sdk_os_timer_disarm(&led_strip_timer);*/
        printf ("%s: Led on false so stopping Multi PWM\n", __func__);
        multipwm_set_duty(&pwm_info, 0, 0);
        multipwm_set_duty(&pwm_info, 1, 0);
        multipwm_set_duty(&pwm_info, 2, 0);
        multipwm_set_duty(&pwm_info, 3, 0);
/*        sdk_os_timer_disarm(&led_strip_timer);
        multipwm_stop(&pwm_info);
        gpio_write(WHITE_PWM_PIN, 0);
        gpio_write(RED_PWM_PIN, 0);
        gpio_write(GREEN_PWM_PIN, 0);
        gpio_write(BLUE_PWM_PIN, 0);
*/
        sdk_os_timer_arm (&led_strip_timer, LED_STRIP_SET_DELAY, 0 );
    } else
    {
        printf ("%s: Led on TRUE so setting colour\n", __func__);
        sdk_os_timer_arm (&led_strip_timer, LED_STRIP_SET_DELAY, 0 );
    }
    
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
    sdk_os_timer_arm (&led_strip_timer, LED_STRIP_SET_DELAY, 0 );
    printf ("led_brigthness_set timer armed\n");
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
    sdk_os_timer_arm (&led_strip_timer, LED_STRIP_SET_DELAY, 0 );
    printf ("led_hugh_set timer armed\n");

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
    sdk_os_timer_arm (&led_strip_timer, LED_STRIP_SET_DELAY, 0 );
    printf ("led_saturation_set timer armed\n");

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
            &led_boost,
            &ota_trigger,
            &wifi_reset,
            &wifi_check_interval,
            NULL
        }),
        NULL
    }),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111" ,
    .setupId = "1234",
    .on_event = on_homekit_event
};


void accessory_init_not_paired (void) {
    /* initalise anything you don't want started until wifi and homekit imitialisation is confirmed, but not paired */
}

void accessory_init (void ){
    /* initalise anything you don't want started until wifi and pairing is confirmed */
    
}

void user_init(void) {
    
    standard_init (&name, &manufacturer, &model, &serial, &revision);
    
    led_strip_init ();
    
    wifi_config_init(DEVICE_NAME, NULL, on_wifi_ready);
    
    
}
