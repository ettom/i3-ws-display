#include "serial-commands.h"

#include <Arduino.h>

constexpr bool align_to_right = true;
constexpr uint8_t display_length = 4;

enum segment {
	seg_A = 0x80,
	seg_B = 0x40,
	seg_G = 0x20,
	seg_C = 0x10,
	seg_D = 0x08,
	seg_E = 0x04,
	seg_F = 0x01,
	seg_DP = 0x02
};

enum common { com_1 = 0xfe, com_2 = 0xfd, com_3 = 0xfb, com_4 = 0xf7 };

const uint8_t decimals[10] = {seg_A | seg_B | seg_C | seg_D | seg_E | seg_F,
			      seg_B | seg_C,
			      seg_A | seg_B | seg_G | seg_E | seg_D,
			      seg_A | seg_B | seg_G | seg_C | seg_D,
			      seg_B | seg_C | seg_F | seg_G,
			      seg_A | seg_F | seg_G | seg_C | seg_D,
			      seg_A | seg_C | seg_D | seg_E | seg_F | seg_G,
			      seg_A | seg_B | seg_C,
			      seg_A | seg_B | seg_C | seg_D | seg_E | seg_F | seg_G,
			      seg_A | seg_B | seg_C | seg_D | seg_F | seg_G};

const uint8_t coms[display_length] = {com_1, com_2, com_3, com_4};

const uint8_t refresh_interval = 100;
unsigned long start_millis = 0, current_millis = 0;

String to_print = "8888", visible_workspaces = "8888";
String rx_buf = "";

void all_off()
{
	PORTB = PORTD = PORTC = 0;
}

void light_segments(int number)
{
	PORTD = decimals[number];
	if ((decimals[number] & seg_F) == seg_F) {
		PORTC |= seg_F;
	} else {
		PORTC &= ~seg_F;
	}
}

void light_index(int index)
{
	PORTB = coms[index];
}

void light_dot_if_visible(int digit)
{
	for (size_t i = 0; i < visible_workspaces.length(); ++i) {
		if (visible_workspaces[i] - '0' == digit) {
			PORTC |= seg_DP;
			return;
		} else {
			PORTC &= ~seg_DP;
		}
	}
}

void print_digit(int index, int digit)
{
	light_segments(digit);
	light_dot_if_visible(digit);
	light_index(index);
	delay(2);
}

void print_number(const String& number)
{
	while (current_millis - start_millis < refresh_interval) {
		current_millis = millis();

		int position_shift = 0;
		if (align_to_right) {
			position_shift = number.length() - (display_length);
			position_shift = abs(position_shift);
		}

		for (size_t i = 0; i < number.length(); ++i) {
			print_digit(i + position_shift, number[i] - '0');
		}
	}

	start_millis = current_millis;
}

bool is_valid_cmd(char c)
{
	return isDigit(c) || (c == serial_commands::workspaces_sent) || (c == serial_commands::visible_sent);
}

void setup() // {{{1
{
	/*
	     A
	  |-----|
	F |  G  | B
	  |-----|
	E |     | C
	  |-----|
	     D    [•]

	   */

	// declare outputs
	// segment control
	//-------ABGCDE--
	DDRD = 0b11111100; // pins D7-D2
	//-------------•F
	DDRC = 0b00000011; // pins A1-A0

	// digit control
	//-----------1234
	DDRB = 0b00001111; // pins D11-D8

	Serial.begin(19200);
} // 1}}}

void loop()
{
	while (Serial.available() > 0) {
		char in_char = Serial.read();
		if (!is_valid_cmd(in_char)) {
			continue;
		}

		if (in_char == serial_commands::workspaces_sent) {
			to_print = rx_buf;
			rx_buf = "";
		} else if (in_char == serial_commands::visible_sent) {
			visible_workspaces = rx_buf;
			rx_buf = "";
		} else {
			rx_buf += in_char;
		}
	}

	if (to_print.length() == 0) {
		all_off();
	} else {
		print_number(to_print);
	}
}
// vim:fdm=marker:fmr={{{,}}}
