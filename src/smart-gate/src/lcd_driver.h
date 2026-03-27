#pragma once

/* I2C Configuration */
#define I2C_MASTER_SCL_IO           7 /* GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           5 /* GPIO number used for I2C master data */
#define I2C_MASTER_NUM              0
#define I2C_MASTER_FREQ_HZ          50000 /* I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0 /* I2C master doesn't need buffer for transmission */
#define I2C_MASTER_RX_BUF_DISABLE   0 /* I2C master doesn't need buffer for reception */

/* LCD Configuration */
#define LCD_ADDR 0x27 /* I2C address of the LCD */
// https://en.wikipedia.org/wiki/HD44780_(integrated_circuit)#Instruction_set
#define LCD_CMD_CLEAR_DISPLAY 0x01
#define LCD_CMD_RETURN_HOME 0x02
#define LCD_CMD_ENTRY_MODE_SET 0x06
#define LCD_CMD_DISPLAY(display_flag, show_cursor_flag, blink_cursor_flag) (0x0C | display_flag << 2 | show_cursor_flag << 1 | blink_cursor_flag)
#define LCD_CMD_DISPLAY_ON LCD_CMD_DISPLAY(1, 0, 0)
#define LCD_CMD_DISPLAY_OFF LCD_CMD_DISPLAY(0, 0, 0)
#define LCD_CMD_FUNCTION_SET(eight_bit_flag, two_line_flag, font_flag) (0x20 | eight_bit_flag << 4 | two_line_flag << 3 | font_flag << 2 )
#define LCD_CMD_INIT_8_BIT_MODE LCD_CMD_FUNCTION_SET(1, 0, 0)
#define LCD_CMD_INIT_4_BIT_MODE LCD_CMD_FUNCTION_SET(0, 0, 0)
#define LCD_CMD_WRITE 0x80

void lcd_driver_set_cursor(int row, int col);

void lcd_driver_clear(void);

void lcd_driver_print(const char *str);

void lcd_driver_init(void);
