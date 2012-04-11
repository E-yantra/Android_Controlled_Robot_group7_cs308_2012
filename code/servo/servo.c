/********************************************************************************
Written by: Vinod Desai, NEX Robotics Pvt. Ltd.
Edited by: Sachitanand Malewar, NEX Robotics Pvt. Ltd.
AVR Studio Version 4.17, Build 666

Date: 26th December 2010
This experiment demonstrates Servo motor control using 10 bit fast PWM mode.

Concepts covered: Use of timer to generate PWM for servo motor control

Fire Bird V ATMEGA2560 microcontroller board has connection for 3 servo motors (S1, S2, S3).
Servo motors move between 0 to 180 degrees proportional to the pulse train
with the on time of 0.6 to 2 ms with the frequency between 40 to 60 Hz. 50Hz is most recommended.

We are using Timer 1 at 10 bit fast PWM mode to generate servo control waveform.
In this mode servo motors can be controlled with the angular resolution of 1.86 degrees.
Although angular resolution is less this is very simple method.
There are better ways to produce very high resolution PWM but it involves interrupts at the frequency of the PWM.
High resolution PWM is used for servo control in the Hexapod robot.
Connection Details: PORTB 5 OC1A --> Servo 1: Camera pod pan servo
PORTB 6 OC1B --> Servo 2: Camera pod tile servo
PORTB 7 OC1C --> Servo 3: Reserved
Note:
1. Make sure that in the configuration options following settings are
done for proper operation of the code

Microcontroller: atmega2560
Frequency: 14745600
Optimization: -O0 (For more information read section: Selecting proper optimization
options below figure 2.22 in the Software Manual)

2. 5V supply to these motors is provided by separate low drop voltage regulator "5V Servo" which can
supply maximum of 800mA current. It is a good practice to move one servo at a time to reduce power surge
in the robot's supply lines. Also preferably take ADC readings while servo motor is not moving or stopped
moving after giving desired position.
*********************************************************************************/

/********************************************************************************

Copyright (c) 2010, NEX Robotics Pvt. Ltd. -*- c -*-
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in
the documentation and/or other materials provided with the
distribution.

* Neither the name of the copyright holders nor the names of
contributors may be used to endorse or promote products derived
from this software without specific prior written permission.

* Source code can be used for academic purpose.
For commercial use permission form the author needs to be taken.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

Software released under Creative Commence cc by-nc-sa licence.
For legal information refer to:
http://creativecommons.org/licenses/by-nc-sa/3.0/legalcode

********************************************************************************/
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include <stdio.h>
#include <stdlib.h>
#include "Motion.c"

#include "LCD.c"

unsigned char mov_array[1000]; 
int val_array[1000];

int data_pos;
int bot_pos;

int prin = 0;

//Configure PORTB 6 pin for servo motor 2 operation
void servo2_pin_config (void)
{
	DDRB = DDRB | 0x40; //making PORTB 6 pin output
	PORTB = PORTB | 0x40; //setting PORTB 6 pin to logic 1
}

//TIMER1 initialization in 10 bit fast PWM mode
//prescale:256
// WGM: 7) PWM 10bit fast, TOP=0x03FF
// actual value: 52.25Hz
void timer1_init(void)
{
	TCCR1B = 0x00; //stop
	TCNT1H = 0xFC; //Counter high value to which OCR1xH value is to be compared with
	TCNT1L = 0x01; //Counter low value to which OCR1xH value is to be compared with
	OCR1BH = 0x03; //Output compare Register high value for servo 2
	OCR1BL = 0xFF; //Output Compare Register low Value For servo 2
	ICR1H = 0x03;
	ICR1L = 0xFF;
	TCCR1A = 0xAB; /*{COM1A1=1, COM1A0=0; COM1B1=1, COM1B0=0; COM1C1=1 COM1C0=0}
	For Overriding normal port functionality to OCRnA outputs.
	{WGM11=1, WGM10=1} Along With WGM12 in TCCR1B for Selecting FAST PWM Mode*/
	TCCR1C = 0x00;
	TCCR1B = 0x0C; //WGM12=1; CS12=1, CS11=0, CS10=0 (Prescaler=256)
}

void uart3_init(void)
{
 UCSR3B = 0x00; //disable while setting baud rate
 UCSR3A = 0x00;
 UCSR3C = 0x06;
 //This is for 14745600
 //UBRR3L = 0x5F; //set baud rate lo
 UBRR3L = 0x5F;
 UBRR3H = 0x00; //set baud rate hi
 UCSR3B = 0x98;
}


void buzzer_pin_config (void)
{
 DDRC = DDRC | 0x08;		//Setting PORTC 3 as output
 PORTC = PORTC & 0xF7;		//Setting PORTC 3 logic low to turnoff buzzer
}

void buzzer_on (void)
{
 unsigned char port_restore = 0;
 port_restore = PINC;
 port_restore = port_restore | 0x08;
 PORTC = port_restore;
}

void buzzer_off (void)
{
 unsigned char port_restore = 0;
 port_restore = PINC;
 port_restore = port_restore & 0xF7;
 PORTC = port_restore;
}

int flag = 0; 

SIGNAL(SIG_USART3_RECV) 		// ISR for receive complete interrupt
{
	unsigned char data1 = UDR3;

	int i = data1 - '0';
	

	if(data1 == 0x24){
		data_pos++;
	}

	else if (data1 == 0x46){
		mov_array[data_pos] = 'F';
		flag  = 1;
	}

	else if (data1 == 0x42){
		mov_array[data_pos] = 'B';
		//flag  = 1;
	}

	else if (data1 == 0x52){
		mov_array[data_pos] = 'R';
		//flag  = 1;
	}

	else if (data1 == 0x4C){
		mov_array[data_pos] = 'L';
		//flag  = 1;
	}
	
	else if (data1 == 0x44){
		mov_array[data_pos] = 'D';
		//flag  = 1;
	}
	
	else if (data1 == 0x55){
		mov_array[data_pos] = 'U';
		//flag  = 1;
	}

	else if ( i <= 10 &&  i > -1 && flag == 1){
		val_array[data_pos] = val_array[data_pos]*10 + i;
	}
}

//Function to rotate Servo 2 by a specified angle in the multiples of 1.86 degrees
void servo_2(float degrees)
{
	float PositionTiltServo = 0;
	PositionTiltServo = ((float)degrees / 1.86) + 35.0;
	OCR1BH = 0x00;
	OCR1BL = (unsigned char) PositionTiltServo;
}


//servo_free functions unlocks the servo motors from the any angle
//and make them free by giving 100% duty cycle at the PWM. This function can be used to
//reduce the power consumption of the motor if it is holding load against the gravity.


void servo_2_free (void) //makes servo 2 free rotating
{
	OCR1BH = 0x03;
	OCR1BL = 0xFF; //Servo 2 off
}

void pen_up(void)
{
	int degree = 50;
	servo_2(degree);
	_delay_ms(2000);
	servo_2_free();
}

void pen_down(void)
{
	int degree = 92;
	servo_2(93);
	_delay_ms(2000);
	servo_2_free();
}

//Initialize the ports
void port_init(void)
{
	servo2_pin_config(); //Configure PORTB 6 pin for servo motor 2 operation
 	buzzer_pin_config();
}

void init_devices(void)
{
	cli(); //disable all interrupts
	port_init();
	timer1_init();
	lcd_init();
	uart3_init();
	motion_port_init();
	sei(); //re-enable interrupts
}

//Main function
void main(void)
{
	init_devices();

	data_pos = 0;
	bot_pos = 0;

	int i;

	for(i=0;i<1000;i++){
		val_array[i] = 0;
	}

//	mov_array[1] = 'F'; mov_array[2] = 'R'; mov_array[3] = 'F';	mov_array[4] = 'L'; mov_array[5] = 'B'; 
//	val_array[1] = 100; val_array[2] = 90; val_array[3] = 200;	val_array[4] = 40; val_array[5] = 150;

	//LCD_Reset_4bit();
	lcd_cursor(2,1);
	lcd_string("DRAWOID");
	while(1){
		//lcd_print(1,9,val_array[bot_pos],3);
		
		if (bot_pos < data_pos){
			lcd_print(1,9,val_array[bot_pos],3);
			lcd_cursor(1,1);
			if (mov_array[bot_pos] == 'F'){
				lcd_string("F");
				move_straight(val_array[bot_pos],1);
			}
			else if (mov_array[bot_pos] == 'B'){
				lcd_string("B");
				move_straight(val_array[bot_pos],0);
			}
			else if (mov_array[bot_pos] == 'R'){
				lcd_string("R");
				right_degrees(val_array[bot_pos]);
			}
			else if (mov_array[bot_pos] == 'L'){
				lcd_string("L");
				left_degrees(val_array[bot_pos]);
			}
			else if (mov_array[bot_pos] == 'U'){
				lcd_string("U");
				pen_up();
			}
			else if (mov_array[bot_pos] == 'D'){
				lcd_string("D");
				pen_down();
			}
			bot_pos++;
			_delay_ms(1000);
		}
	}
}
