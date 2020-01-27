![GitHub All Releases](https://img.shields.io/github/downloads/maccoylton/esp-homekit-rgbw-strip/total) 

# esp-homekit-rgbw-strip
A homekit firmware  for a magic_home RGBW controller



Supports IR remote control, including Fade, Strobe, Smooth and Flash functions. 

Use Test Colour GPIOs to check if you have the right GPIOs set for each colour. Each colour will be displayed for approx 5 seconds, in the order RGBW.  

Use the GPIO buttons to change the GPIOs for each colour, note each value must not be equal to any other value, changes wil be applied 5 seconds after you change a GPIO. So if you need to change more than one, just make all the changes and then wait 5 seconds

NOTE GPIO6 to GPIO11 are usually connected to the flash chip in ESP8266 boards. So these are not recommended for general use. 

