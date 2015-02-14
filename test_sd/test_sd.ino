
/****************************************************************/
//#include <LiquidCrystal.h>
#include <SPI.h>
#include <Ethernet.h>
#include <SD.h>

/****************************************************************/


// SD card -----------------------------------
File                       f; // file to read from SD card
unsigned long              counter = 0; 
char *                     dname = "/TMP/";
char *                     fname = "ALERT.TXT"; // needs to be 8.3 filename


/****************************************************************/
void setup()
{

//  // LED
//  pinMode(A4, OUTPUT);
//  pinMode(7, OUTPUT);
//  pinMode(8, OUTPUT);
//  pinMode(9, OUTPUT);
//
//  digitalWrite(A4, HIGH);
//  digitalWrite(7, HIGH);
//  digitalWrite(8, HIGH);
//  digitalWrite(9, HIGH);
//
//  delay(500);
//
//  digitalWrite(A4, LOW);
//  digitalWrite(7, LOW);
//  digitalWrite(8, LOW);
//  digitalWrite(9, LOW);

  Serial.begin(9600);

  SD.begin(4);
} 


/****************************************************************/
void loop()
{
  test_2();
}

/****************************************************************/
void test_2()
{
  Serial.println("--");
  if (SD.exists(dname))
  {
    Serial.println("dir exists");
  }
  else
  {
    Serial.println("dir doesn't exist");
  }    

  if (SD.exists(fname))
  {
    Serial.println("file exists");
  }
  else
  {
    Serial.println("file doesn't exist");
  }      
  
//  SD.open(fname, FILE_WRITE);
  f = SD.open(fname, FILE_WRITE);
  if (f)
  {
    Serial.println("file open");
    if (f.available())
    {
      Serial.println("avail");
    }
    else
    {
      Serial.println("not avail");
    }
    f.close();
  }
  else
  {
    Serial.println("didn't open file");
  }

  delay(3000);
}

/****************************************************************/
void test_1()
{  
  // ------------------------------------------------------------
  // open file over and over, see if attempt ever fails
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
}









