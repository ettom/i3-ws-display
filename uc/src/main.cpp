#include "serial-commands.h"

#include <Arduino.h>

constexpr bool align_to_right = true;
constexpr uint8_t display_length = 4;
constexpr uint16_t urgent_blink_interval_ms = 700;

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
unsigned long start_millis = 0, current_millis = 0, urgent_millis = 0;

String rx_buf = "";
String to_print = "8888", visible_workspaces = "8888", urgent_workspaces = "";

bool urgent_on = false;

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

void light_dot(int index, int digit)
{
	static auto contains = [](const String& s, int to_search) {
		for (size_t i = 0; i < s.length(); ++i) {
			if (s[i] - '0' == to_search) {
				return true;
			}
		}
		return false;
	};

	if (contains(visible_workspaces, digit) || (urgent_on && contains(urgent_workspaces, digit))) {
		PORTC |= seg_DP;
	} else {
		PORTC &= ~seg_DP;
	}
}

void print_digit(int index, int digit)
{
	light_dot(index, digit);
	light_segments(digit);
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
	return isDigit(c) || (c == serial_commands::workspaces_sent) || (c == serial_commands::visible_sent)
	       || (c == serial_commands::urgent_sent);
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

		switch (in_char) {
		case serial_commands::workspaces_sent:
			to_print = rx_buf;
			rx_buf = "";
			break;
		case serial_commands::visible_sent:
			visible_workspaces = rx_buf;
			rx_buf = "";
			break;
		case serial_commands::urgent_sent:
			urgent_workspaces = rx_buf;
			rx_buf = "";
			break;
		default:
			rx_buf += in_char;
		}
	}

	if (to_print.length() == 0) {
		all_off();
	} else {
		print_number(to_print);
	}

	if (millis() - urgent_millis > urgent_blink_interval_ms) {
		urgent_millis = millis();
		urgent_on = !urgent_on;
	}
}
// vim:fdm=marker:fmr={{{,}}}
