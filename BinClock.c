/*
 * BinClock.c
 * Jarrod Olivier
 * Modified for EEE3095S/3096S by Keegan Crankshaw
 * August 2019
 * 
 * NCHKEA001 HMRSAS001
 * 20 Aug 2019
*/

#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <softPwm.h>
#include <stdio.h> //For printf functions
#include <stdlib.h> // For system functions
#include "BinClock.h"
#include "CurrentTime.h"

//Global variables
int hours, mins, secs;
long lastInterruptTime = 0; //Used for button debounce
int RTC; //Holds the RTC instance
int HOURS, MINS, SECSS; // Must edit from RTC
int write_hour, write_min, write_sec; // Store to RTC

int HH,MM,SS;

int decSeconds(int secs); // Function declaration

void initGPIO(void){
	/* 
	 * Sets GPIO using wiringPi pins. see pinout.xyz for specific wiringPi pins
	 * You can also use "gpio readall" in the command line to get the pins
	 * Note: wiringPi does not use GPIO or board pin numbers (unless specifically set to that mode)
	 */
	printf("Setting up\n");
	wiringPiSetup(); //This is the default mode. If you want to change pinouts, be aware
	
	RTC = wiringPiI2CSetup(RTCAddr); //Set up the RTC
	
	//Set up the LEDS
	for(int i; i < sizeof(LEDS)/sizeof(LEDS[0]); i++){
	    pinMode(LEDS[i], OUTPUT);
	}
	
	//Set Up the Seconds LED for PWM
        //Set range from 0 to 60 on WPi pin 1
        softPwmCreate (1, 0, 60);
	
	printf("LEDS done\n");
	
	//Set up the Buttons
	for(int j; j < sizeof(BTNS)/sizeof(BTNS[0]); j++){
		pinMode(BTNS[j], INPUT);
		pullUpDnControl(BTNS[j], PUD_UP);
	}
	
	//Attach interrupts to Buttons
        wiringPiISR (5, INT_EDGE_FALLING,  minInc);
        wiringPiISR (30, INT_EDGE_FALLING,  hourInc); 

	printf("BTNS done\n");
	printf("Setup done\n");
}


/*
 * The main function
 * This function is called, and calls all relevant functions we've written
 */
int main(void){
	initGPIO();
	//Set random time (3:54PM)
	wiringPiI2CWriteReg8(RTC, HOUR, 0x13+TIMEZONE);
	wiringPiI2CWriteReg8(RTC, MIN, 0x54);
	wiringPiI2CWriteReg8(RTC, SEC, 0x00);

        // Turn on oscillator for seconds led
	wiringPiI2CWriteReg8(RTC,0x00, 0b10000000);
	
	// Repeat this until we shut down
	for (;;){
		//Fetch the time from the RTC
		HOURS = wiringPiI2CReadReg8 (RTC, HOUR);
		hours = hexCompensation(HOURS);
		hours = hFormat(hours);
		MINS = wiringPiI2CReadReg8 (RTC, MIN);
		mins = hexCompensation(MINS);
		SECSS = wiringPiI2CReadReg8 (RTC, SEC);
		secs = decSeconds(SECSS);
		
		//Function calls to toggle LEDs
		lightHours(hours);
		lightMins(mins);
		secPWM(secs);

		// Print out the time we have stored on our RTC
		printf("The current time is: %d:%d:%d\n", hours, mins, secs);

		//using a delay to make our program "less CPU hungry"
		delay(1000);
	}
	return 0;
}

/*
 * Converts the seconds value read from the RTC into decimal form
*/
int decSeconds(int secs){
	int digits = secs &0b1111;
	int tens = ((secs&0b1110000)>>4);
	return ((tens*10) + digits);

}

/*
 * Change the hour format to 12 hours
 */
int hFormat(int hours){
	/*formats to 12h*/
	if (hours >= 24){
		hours = 0;
	}
	else if (hours > 12){
		hours -= 12;
	}
	return (int)hours;
}

/*
 * Turns on corresponding LED's for hours
 */
void lightHours(int units){
	digitalWrite (0, ((hours&0b1000)>>3)); // 2^3 position
        digitalWrite (2, ((hours&0b100)>>2)); // 2^2 position
        digitalWrite (3, ((hours&0b10)>>1)); // 2^1 position
        digitalWrite (25, (hours&0b1)); // 2^0 position
        //printf("Write values to hours LEDS: %d%d%d%d\n",((hours&0b1000)>>3), ((hours&0b100)>>2), ((hours&0b10)>>1), (hours&0b1));
}

/*
 * Turn on the Minute LEDs
 */
void lightMins(int units){
	digitalWrite (7, ((mins&0b100000)>>5)); // 2^5 position
	digitalWrite (22, ((mins&0b10000)>>4)); // 2^4 position
        digitalWrite (21, ((mins&0b1000)>>3)); // 2^3 position
        digitalWrite (27, ((mins&0b100)>>2)); // 2^2 position
        digitalWrite (4, ((mins&0b10)>>1)); // 2^1 position
        digitalWrite (6, (mins&0b1)); // 2^0 position
}

/*
 * PWM on the Seconds LED
 * The LED should have 60 brightness levels
 * The LED should be "off" at 0 seconds, and fully bright at 59 seconds
 */
void secPWM(int units){
	// Displays seconds on LED
	softPwmWrite (1, secs);
}

/*
 * hexCompensation
 * This function may not be necessary if you use bit-shifting rather than decimal checking for writing out time values
 */
int hexCompensation(int units){
	/*Convert HEX or BCD value to DEC where 0x45 == 0d45 
	  This was created as the lighXXX functions which determine what GPIO pin to set HIGH/LOW
	  perform operations which work in base10 and not base16 (incorrect logic) 
	*/
	int unitsU = units%0x10;

	if (units >= 0x50){
		units = 50 + unitsU;
	}
	else if (units >= 0x40){
		units = 40 + unitsU;
	}
	else if (units >= 0x30){
		units = 30 + unitsU;
	}
	else if (units >= 0x20){
		units = 20 + unitsU;
	}
	else if (units >= 0x10){
		units = 10 + unitsU;
	}
	return units;
}


/*
 * decCompensation
 * This function "undoes" hexCompensation in order to write the correct base 16 value through I2C
 */
int decCompensation(int units){
	int unitsU = units%10;

	if (units >= 50){
		units = 0x50 + unitsU;
	}
	else if (units >= 40){
		units = 0x40 + unitsU;
	}
	else if (units >= 30){
		units = 0x30 + unitsU;
	}
	else if (units >= 20){
		units = 0x20 + unitsU;
	}
	else if (units >= 10){
		units = 0x10 + unitsU;
	}
	return units;
}


/*
 * hourInc
 * Fetch the hour value off the RTC, increase it by 1, and write back
 * Be sure to cater for there only being 23 hours in a day
 * Software Debouncing should be used
 */
void hourInc(void){
	//Debounce
	long interruptTime = millis();

	if (interruptTime - lastInterruptTime>200){
		printf("Interrupt 1 triggered, %x\n", hours);
		//Fetch RTC Time
		HOURS = wiringPiI2CReadReg8 (RTC, HOUR);
                hours = hexCompensation(HOURS);
		//Increase hours by 1
		if (hours >= 23)
		{
			hours =0;
		}
		else
		{
			hours+=1;
		}
		write_hour = decCompensation(hours);
		wiringPiI2CWriteReg8(RTC, HOUR, write_hour);
	}
	lastInterruptTime = interruptTime;
}

/* 
 * minInc
 * Fetch the minute value off the RTC, increase it by 1, and write back
 * Be sure to cater for there only being 60 minutes in an hour
 * Software Debouncing should be used
 */
void minInc(void){
	long interruptTime = millis();

	if (interruptTime - lastInterruptTime>200){
		printf("Interrupt 2 triggered, %x\n", mins);
		//Fetch RTC Time
                MINS = wiringPiI2CReadReg8 (RTC, MIN);
                mins = hexCompensation(MINS);
                //Increase mins by 1
                if (mins >= 59)
                {
                        mins =0;
                }
                else
                {
                        mins+=1;
                }
                write_min = decCompensation(mins);
                wiringPiI2CWriteReg8(RTC, MIN, write_min);
	}
	lastInterruptTime = interruptTime;
}

//This interrupt will fetch current time from another script and write it to the clock registers
//This functions will toggle a flag that is checked in main
void toggleTime(void){
	long interruptTime = millis();

	if (interruptTime - lastInterruptTime>200){
		HH = getHours();
		MM = getMins();
		SS = getSecs();

		HH = hFormat(HH);
		HH = decCompensation(HH);
		wiringPiI2CWriteReg8(RTC, HOUR, HH);

		MM = decCompensation(MM);
		wiringPiI2CWriteReg8(RTC, MIN, MM);

		SS = decCompensation(SS);
		wiringPiI2CWriteReg8(RTC, SEC, 0b10000000+SS);

	}
	lastInterruptTime = interruptTime;
}
