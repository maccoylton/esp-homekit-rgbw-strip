PROGRAM = main

EXTRA_COMPONENTS = \
	extras/dht \
	extras/http-parser \
	extras/rboot-ota \
    extras/multipwm \
	$(abspath ../components/esp-wolfssl) \
	$(abspath ../components/esp-cjson) \
	$(abspath ../components/esp-homekit) \
	$(abspath ../components/esp-ir) \
    /Volumes/case-sensitive/esp8266-rtos-sample-code/02Peripheral/Infrared

FLASH_SIZE ?= 8
FLASH_MODE ?= dout
FLASH_SPEED ?= 40

HOMEKIT_MAX_CLIENTS = 16
HOMEKIT_SMALL = 0
HOMEKIT_SPI_FLASH_BASE_ADDR ?= 0x8c000

EXTRA_CFLAGS += -I../.. -DHOMEKIT_SHORT_APPLE_UUIDS -DHOMEKIT_DEBUG
EXTRA_CFLAGS += -DHOMEKIT_OVERCLOCK_PAIR_VERIFY
EXTRA_CFLAGS += -DHOMEKIT_OVERCLOCK_PAIR_SETUP


LIBS ?= hal m

include $(SDK_PATH)/common.mk

signature: 
	$(shell /usr/local/opt/openssl/bin/openssl   sha384 -binary -out firmware/main.bin.sig firmware/main.bin)
	$(shell printf "%08x" `cat firmware/main.bin | wc -c`| xxd -r -p >>firmware/main.bin.sig)
 
monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
