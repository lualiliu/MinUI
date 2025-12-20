TARGET = keymon
PRODUCT = $(TARGET).elf

CROSS_COMPILE ?=

CC = $(CROSS_COMPILE)gcc -I../../all/common/ -I../platform/ -I../libmsettings/
FLAGS	= -Os -lmsettings -lpthread -lrt -ldl -Wl,--gc-sections -s

all:
	$(CC) $(TARGET).c -o $(PRODUCT) $(FLAGS)
clean:
	rm -rf $(PRODUCT)

