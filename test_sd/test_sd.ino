
/****************************************************************/
//#include <LiquidCrystal.h>
#include <SPI.h>
#include <Ethernet.h>
#include <SD.h>

/****************************************************************/


// SD card -----------------------------------
File                       f; // file to read from SD card
unsigned long       counter = 0;



/****************************************************************/
void setup()
{
  File f;

  // LED
  pinMode(A4, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(8, OUTPUT);
  pinMode(9, OUTPUT);

  digitalWrite(A4, HIGH);
  digitalWrite(7, HIGH);
  digitalWrite(8, HIGH);
  digitalWrite(9, HIGH);

  delay(500);

  digitalWrite(A4, LOW);
  digitalWrite(7, LOW);
  digitalWrite(8, LOW);
  digitalWrite(9, LOW);

  Serial.begin(9600);

  SD.begin(4);
} 


/****************************************************************/
void loop()
{
  if (SD.exists("/index.htm"))
  {
    //Serial.println("  +");
    if (f)
    {
      //Serial.println("   f");
      if (f.available())
      {
        //Serial.println("    a");
        Serial.println((char)f.read());
        delay(300);
      }
      else
      {
        Serial.println("    .");
      }
    }
    else
    {
      f = SD.open("/index.htm", FILE_READ);
      //Serial.println("     -");
    }
  }
  else
  {
    SD.begin(4);
    Serial.println("     x");
  }
  return;





  //  if (SD.begin(4) )
  //  {
  if (SD.exists("/index.htm"))
  {
    f = SD.open("/index.htm", FILE_READ);
    if (f) 
    {
      Serial.println(counter);
      if(f.available())
      {
        counter ++;
      }
      else
      {
        Serial.println("f.available() failed");
        digitalWrite(9, HIGH);
        delay(100);
        digitalWrite(A4, LOW);   
      }
      f.close();
    }
    else
    { 
      Serial.println("if(f) failed");
      digitalWrite(8, HIGH);
      delay(100);
      digitalWrite(A4, LOW);   
    }
  }
  else
  {
    Serial.println("SD.exists() failed");
    digitalWrite(7, HIGH);
    delay(100);
    digitalWrite(A4, LOW);   
  }
  //  }
  //  else
  //  {
  //    Serial.println("SD.begin() failed");
  //    digitalWrite(A4, HIGH);
  //    delay(100);
  //    digitalWrite(A4, LOW);    
  //  }

}







