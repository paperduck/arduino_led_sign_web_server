

#include <SoftwareSerial.h>  

SoftwareSerial             serial_pc(5,6);


void setup()
{
  serial_pc.begin(2400);  
  Serial.begin(9600);
}

void loop()
{
  if (serial_pc.available())
  {
    Serial.print( (char)serial_pc.read() );
  }
}

