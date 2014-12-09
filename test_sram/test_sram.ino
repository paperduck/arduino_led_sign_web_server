/****************************************************************/
#include <LiquidCrystal.h>

/****************************************************************/
//Create LCD screen instance ***
LiquidCrystal lcd(A5, A4, A3, A2, A1, A0); // (rs, enable, d4, d5, d6, d7) 

/****************************************************************/
// by David A. Mellis / Rob Faludi http://www.faludi.com
int availableMemory() {
  int size = 2048; // Use 2048 with ATmega328
  byte *buf;
  while ((buf = (byte *) malloc(--size)) == NULL)
    ;
  free(buf);
  return size;
}

/****************************************************************/
void setup()
{
  // LCD screen
  lcd.begin(16,2);
  
  int m = availableMemory();
  
  lcd.setCursor(0, 0);
  lcd.print(m);
  
  lcd.setCursor(0, 1);
  lcd.print(F("hello world!"));
} 

/****************************************************************/
void loop()
{

}

























































