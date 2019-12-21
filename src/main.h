//
//  main.h
//  esp-homekit-rgbw-strip
//
//  Created by David B Brown on 16/06/2018.
//  Copyright © 2018 maccoylton. All rights reserved.
//

#ifndef main_h
#define main_h

#include <stdio.h>
#include <ir/ir.h>
#include <ir/raw.h>
#include <ir/generic.h>

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


enum {strobe=0, aubergene=4, up=9, cream=10,smooth=12, on_button=13, purple=14, pink=15, blue=17, light_green=18,green5=20, white=21, light_blue=22, dark_orange=23, red=25, fade=26, green=27, yellow=28, down=29, green4=30, off=31, orange=64, sky_blue=76, flash=77} rest_buttons;


int8_t mh_strobe[]={0 , -1, 0, -1 };
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

void led_strip_set();

#endif /* main_h */
