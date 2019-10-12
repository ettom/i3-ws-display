#include <Arduino.h>

#define DISPLAY_LENGTH 4
#define ALIGN_TO_RIGHT true

#define WORKSPACES_SENT_DELIMITER 'w'
#define VISIBLE_SENT_DELIMITER    'f'
#define VISIBLE_NOT_FOUND         '-'

String inputString = "";
String toPrint = "8888";

char visibleWorkspace = VISIBLE_NOT_FOUND;

unsigned long startMillis;
unsigned long currentMillis;
const short refresh_interval = 100;

void lightSegments(char number)
{
	switch (number) {
	case '0':
		PORTD = 0b11011111;
		PORTC = 0b00000001;
		break;
	case '1':
		PORTD = 0b01010000;
		PORTC = 0b00000000;
		break;
	case '2':
		PORTD = 0b11101100;
		PORTC = 0b00000000;
		break;
	case '3':
		PORTD = 0b11111000;
		PORTC = 0b00000000;
		break;
	case '4':
		PORTD = 0b01110000;
		PORTC = 0b00000001;
		break;
	case '5':
		PORTD = 0b10111000;
		PORTC = 0b00000001;
		break;
	case '6':
		PORTD = 0b10111100;
		PORTC = 0b00000001;
		break;
	case '7':
		PORTD = 0b11010000;
		PORTC = 0b00000000;
		break;
	case '8':
		PORTD = 0b11111100;
		PORTC = 0b00000001;
		break;
	case '9':
		PORTD = 0b11111000;
		PORTC = 0b00000001;
		break;
	}
}

void lightDigit(int index)
{
	switch (index) {
	case 1:
		PORTB = 0b11111110;
		break;
	case 2:
		PORTB = 0b11111101;
		break;
	case 3:
		PORTB = 0b11111011;
		break;
	case 4:
		PORTB = 0b11110111;
		break;
	}
}

void lightDotIfVisible(char number)
{
	if (number == visibleWorkspace) {
		PORTC = PORTC | (1 << 1);
	} else {
		PORTC = PORTC & (1 << 0);
	}
}

void printDigit(int index, char number)
{
	lightSegments(number);
	lightDigit(index);
	lightDotIfVisible(number);
	delay(5);
}

void printNumber(String number)
{
	while (currentMillis - startMillis < refresh_interval) {
		currentMillis = millis();

		int positionShift = 1;
		if (ALIGN_TO_RIGHT) {
			positionShift = number.length() - (DISPLAY_LENGTH + 1);
			positionShift = abs(positionShift);
		}

		for (size_t i = 0; i < number.length(); ++i) {
			printDigit(i + positionShift, number[i]);
		}
	}

	startMillis = currentMillis;
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

	// turn everything on
	PORTD = 0b11111100;
	PORTC = 0b00000011;
	PORTB = 0b00000000;

	Serial.begin(9600);
} // 1}}}

void loop()
{
	while (Serial.available() > 0) {
		char inChar = Serial.read();

		if (inputString.length() < DISPLAY_LENGTH + 1 && isDigit(inChar)) {
			inputString += inChar;
		} else if (inChar == WORKSPACES_SENT_DELIMITER) {
			toPrint = inputString;
			inputString = "";
		} else if (inChar == VISIBLE_SENT_DELIMITER) {
			visibleWorkspace = inputString[0];
			inputString = "";
		}
	}
	printNumber(toPrint);
}

// vim:fdm=marker:fmr={{{,}}}
