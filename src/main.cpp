#include "Arduino.h"

void setup(){
    Serial.begin(9600);
}

void loop(){
    Serial.println("Welcome to AstroJay");
    delay(1000);
}