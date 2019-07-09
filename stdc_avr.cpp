#define F_CPU 16000000L 

#include "Arduino.h"
#include "Servo.h"
#include "IRReadOnlyRemote.h"

// there is a remote IR sensor on pin 2
IRReadOnlyRemote remote(2);

// there is a Servo on pin A4
Servo servo;
#define SERVO_MIN_POS 82

extern "C" {
	float READ_IR();
	float SERVO_POS_INC();
	float SERVO_POS_DEC();
	void setup();
}

void setup() {
	// any arduino like code that you need to setup your robot!

	pinMode(2, INPUT);

	pinMode(A0, OUTPUT);

	for(int i = 3; i <= 13; i++)
		pinMode(i, OUTPUT);

	// connect to servo in A4 port
	servo.attach(A4);
	servo.write(SERVO_MIN_POS);
}

float READ_IR() {
	return remote.read();
}

float READ_MICROS() {
	return micros();
}

float SERVO_POS_INC() {
	int a = servo.read();
	if (a < SERVO_MIN_POS + 40)
		servo.write(a+5);
	delay(15);
	return 0;
}

float SERVO_POS_DEC() {
	int a = servo.read();
	if (a > SERVO_MIN_POS)
		servo.write(a-5);
	delay(15);
	return 0;
}

