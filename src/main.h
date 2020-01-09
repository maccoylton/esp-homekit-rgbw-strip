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
