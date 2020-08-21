//
//  main.h
//  esp-homekit-rgbw-strip
//
//  Created by David B Brown on 16/06/2018.
//  Copyright © 2018 maccoylton. All rights reserved.
//

#ifndef main_h
#define main_h

static ir_generic_config_t nec_protocol_config = {
    .header_mark = 9000,
    .header_space = -4500,
    
    .bit1_mark = 560,
    .bit1_space = -1689,
    
    .bit0_mark = 560,
    .bit0_space = -560,
    
    .footer_mark = 560,
    .footer_space = -560,
    
    .tolerance = 25,
};


enum {strobe_button=0, aubergene_button=4, up_button=9, cream_button=10,smooth_button=12, on_button=13, purple_button=14, pink_button=15, blue_button=17, light_green_button=18,green5_button=20, white_button=21, light_blue_button=22, dark_orange_button=23, red_button=25, fade_button=26, green_button=27, yellow_button=28, down_button=29, green4_button=30, off_button=31, orange_button=64, sky_blue_button=76, flash_buton=77} rest_buttons;



/*enum {aubergene_index, cream_index, purple_index, pink_index, blue_index, light_green_index,green5_index, white_index, light_blue_index, dark_orange_index, red_index, green_index, yellow_index, green4_index, orange_index, sky_blue_index} colour_index;
*/


const static  hsi_color_t white_colour = {{0.0, 0.0, 100}};

const static hsi_color_t red_colour = {{ 0.0, 100.0, 100}};
const static hsi_color_t dark_orange_colour = { { 15, 100, 78 }};
const static hsi_color_t orange_colour = { { 30, 100, 100 }};
const static hsi_color_t cream_colour = { { 45, 100, 100 }};
const static hsi_color_t yellow_colour = { { 60, 100, 100 }};

const static hsi_color_t green_colour = { { 120, 100, 100 }};
const static hsi_color_t light_green_colour = { { 140, 100, 100 }};
const static hsi_color_t sky_blue_colour = { { 180, 100, 100 }};
const static hsi_color_t green4_colour = { { 180, 100, 80 }};
const static hsi_color_t green5_colour = { { 180, 100, 60 }};

const static hsi_color_t blue_colour = { { 240, 100, 100 }};
const static hsi_color_t light_blue_colour = { { 255, 100, 100 }};
const static hsi_color_t aubergene_colour = { { 280, 100, 50 }};
const static hsi_color_t purple_colour = { { 280, 100, 75 }};
const static hsi_color_t pink_colour = { { 300, 100, 100 }};



/*int8_t mh_strobe[]={0 , -1, 0, -1 };
int8_t mh_aubergene[]={0 , -1, 4, -5 };
int8_t mh_up[]={0 , -1, 9, -10};
int8_t mh_cream[]={0 , -1, 10, -11};
int8_t mh_smooth[]={0 , -1, 12, -13};
int8_t mh_on[]={0 , -1, 13, -14};
int8_t mh_purple[]={0 , -1, 14, -15};
int8_t mh_pink[]={0 , -1, 15, -16 };
int8_t mh_blue[]={0 , -1, 17, -18};
int8_t mh_light_green[]={0 , -1, 18, -19};
int8_t mh_green5[]={ 0 , -1, 20, -21 };
int8_t mh_white[]={0 , -1, 21, -22};
int8_t mh_light_blue[]={0 , -1, 22, -23};
int8_t mh_dark_orange[]={0 , -1, 23, -24};
int8_t mh_red[]={0 , -1, 25, -26};
int8_t mh_fade[]={0 , -1, 26, -27 };
int8_t mh_green[]={0 , -1, 27, -28};
int8_t mh_yellow[]={0 , -1, 28, -29 };
int8_t mh_down[]={0 , -1, 29, -30};
int8_t mh_green4[]={0 , -1, 30, -31 };
int8_t mh_off[]={0 , -1, 31, -32};
int8_t mh_orange[]={0, -1, 64, -65};
int8_t mh_sky_blue[]={0 , -1, 76, -77 };
int8_t mh_flash[]={0 , -1, 77, -78 };
*/
#endif /* main_h */
