#include "serial-commands.h"

#include <Arduino.h>

#define DISPLAY_LENGTH 4
#define ALIGN_TO_RIGHT true

#define SEG_A  0x80
#define SEG_B  0x40
#define SEG_G  0x20
#define SEG_C  0x10
#define SEG_D  0x08
#define SEG_E  0x04
#define SEG_F  0x01
#define SEG_DP 0x02

#define COM_1 0xfe
#define COM_2 0xfd
#define COM_3 0xfb
#define COM_4 0xf7

const uint8_t decimals[10] = {SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,
			      SEG_B | SEG_C,
			      SEG_A | SEG_B | SEG_G | SEG_E | SEG_D,
			      SEG_A | SEG_B | SEG_G | SEG_C | SEG_D,
			      SEG_B | SEG_C | SEG_F | SEG_G,
			      SEG_A | SEG_F | SEG_G | SEG_C | SEG_D,
			      SEG_A | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,
			      SEG_A | SEG_B | SEG_C,
			      SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,
			      SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G};

const uint8_t coms[DISPLAY_LENGTH] = {COM_1, COM_2, COM_3, COM_4};

const uint8_t refreshInterval = 100;
unsigned long startMillis = 0, currentMillis = 0;

String inputString = "";
String toPrint = "8888", visibleWorkspaces = "8888";

void allOff()
{
	PORTB = PORTD = PORTC = 0;
}

void lightSegments(int number)
{
	PORTD = decimals[number];
	PORTC = PORTD;
}

void lightDigit(int index)
{
	PORTB = coms[index];
}

void lightDotIfVisible(int digit)
{
	for (int i = 0; i < visibleWorkspaces.length(); ++i) {
		if (visibleWorkspaces[i] - '0' == digit) {
			PORTC |= SEG_DP;
			return;
		} else {
			PORTC &= ~SEG_DP;
		}
	}
}

void printDigit(int index, int digit)
{
	lightSegments(digit);
	lightDigit(index);
	lightDotIfVisible(digit);
	delay(2);
}

void printNumber(String number)
{
	while (currentMillis - startMillis < refreshInterval) {
		currentMillis = millis();

		int positionShift = 0;
		if (ALIGN_TO_RIGHT) {
			positionShift = number.length() - (DISPLAY_LENGTH);
			positionShift = abs(positionShift);
		}

		for (size_t i = 0; i < number.length(); ++i) {
			printDigit(i + positionShift, number[i] - '0');
		}
	}

	startMillis = currentMillis;
}

bool isValidCmd(char c)
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
		char inChar = Serial.read();
		if (!isValidCmd(inChar)) {
			continue;
		}

		if (inChar == serial_commands::workspaces_sent) {
			toPrint = inputString;
			inputString = "";
		} else if (inChar == serial_commands::visible_sent) {
			visibleWorkspaces = inputString;
			inputString = "";
		} else {
			inputString += inChar;
		}
	}

	if (toPrint.length() == 0) {
		allOff();
	} else {
		printNumber(toPrint);
	}
}
// vim:fdm=marker:fmr={{{,}}}
