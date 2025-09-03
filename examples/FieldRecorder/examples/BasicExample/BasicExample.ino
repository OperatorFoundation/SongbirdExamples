/*
 * BasicExample.ino
 * 
 * Example sketch for FieldRecorder library
 * 
 * Created: 2025-08-30
 */

#include <FieldRecorder.h>

FieldRecorder myInstance;

void setup() {
    Serial.begin(9600);
    while (!Serial) {
        ; // Wait for serial port to connect
    }
    
    Serial.println("FieldRecorder Example");
    myInstance.begin();
}

void loop() {
    // Your code here
    delay(1000);
}
