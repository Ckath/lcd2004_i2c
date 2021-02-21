#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

#include "lcd2004_i2c.h"

#define LCD_ROW0 0x80
#define LCD_ROW1 0xC0
#define LCD_ROW2 0x94
#define LCD_ROW3 0xD4
#define LCD_ENABLE_BIT 0x04

struct lcd {
	int fd;
	uint8_t bl;
};

static void
i2c_write(int fd, uint8_t b)
{
	write(fd, &b, 1);
}

static void
lcd_toggle_enable(int fd, uint8_t val)
{
	usleep(666); /* generous delay to not mess up controller */
	i2c_write(fd, val | LCD_ENABLE_BIT);
	usleep(666);
	i2c_write(fd, val & ~LCD_ENABLE_BIT);
	usleep(666);
}

LCD *
lcd_init(int bus, int addr)
{
	/* create lcd and associate with i2c device */
	LCD *lcd = malloc(sizeof(LCD));
	lcd->bl = LCD_BACKLIGHT;
	char dev[33];
	sprintf(dev, "/dev/i2c-%d", bus); 
	if ((lcd->fd = open(dev, O_RDWR)) < 0) {
		free(lcd);
		return NULL;
	} if (ioctl(lcd->fd, I2C_SLAVE, addr) < 0) { 
		lcd_delete(lcd);
		return NULL;
	}

	/* required magic taken from pi libs */
	lcd_send_cmd(lcd, 0x03);
	lcd_send_cmd(lcd, 0x03);
	lcd_send_cmd(lcd, 0x03);
	lcd_send_cmd(lcd, 0x02);

	/* initialize display config */
	lcd_send_cmd(lcd, LCD_ENTRYMODESET | LCD_ENTRYLEFT);
	lcd_send_cmd(lcd, LCD_FUNCTIONSET | LCD_2LINE);
	lcd_on(lcd);
	lcd_clear(lcd);
	return lcd;
}

void
lcd_delete(LCD *lcd)
{
	close(lcd->fd);
	free(lcd);
}

void
lcd_send_byte(LCD *lcd, uint8_t val, uint8_t mode) {
	uint8_t buf = mode | (val & 0xF0) | lcd->bl;
	i2c_write(lcd->fd, buf); /* write first nibble */
	lcd_toggle_enable(lcd->fd, buf);
	buf = mode | ((val << 4) & 0xF0) | lcd->bl;
	i2c_write(lcd->fd, buf); /* write second nibble */
	lcd_toggle_enable(lcd->fd, buf);
}

void
lcd_backlight(LCD *lcd, uint8_t on)
{
	lcd->bl = on ? LCD_BACKLIGHT : LCD_NOBACKLIGHT;
	lcd_on(lcd);
}

void
lcd_write(LCD *lcd, char *data)
{
	for (uint8_t i = 0; i < strlen(data); ++i) {
		lcd_send_chr(lcd, data[i]);
	}
}

void
lcd_move(LCD *lcd, int x, int y)
{
	uint8_t pos_addr = x;
	switch(y) {
		case 0:
			pos_addr += LCD_ROW0;
			break;
		case 1:
			pos_addr += LCD_ROW1;
			break;
		case 2:
			pos_addr += LCD_ROW2;
			break;
		case 3:
			pos_addr += LCD_ROW3;
			break;
	}
	lcd_send_cmd(lcd, pos_addr);
}
