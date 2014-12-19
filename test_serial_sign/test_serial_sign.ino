/*

 RS232 shield test program
 Created: 2014-12-05
 
 */

#include <SoftwareSerial.h>
#include <LiquidCrystal.h>


byte   sign_packet [22] = {
  0x00 , 0xFF , 0xFF , 0x00 , 0x0B , 0x01 , 0xFF , 0x01 , 0x30 , 0x31 , 0xEF , 0xB0 , 0xEF , 0xA2 ,
  0x43 , 0x65 , 0x6C , 0x6C , 0x6F , 
  0xFF , 0xFF , 0x00 };

byte     sign_packet_buffer[100];

unsigned long     time_since_last_rec_byte = 0;
SoftwareSerial    s(2,3); // RX, TX
byte              b;
//boolean         temp_bool = true;
boolean           temp_bool2 = true;
long              time_of_last_programming = millis() + 60000;
unsigned long     num_bytes_rec = 0;
unsigned long     num_bytes_sent = 0;
LiquidCrystal     lcd(A5, A4, A3, A2, A1, A0); // (rs, enable, d4, d5, d6, d7) 
boolean           buf_ready = false;

/****************************************************************/
void setup()
{
  lcd.begin(16,2);
  lcd.clear();

  Serial.begin(9600); 
  s.begin(2400);
}

/****************************************************************/
void loop()
{    
  if (millis() - time_of_last_programming > 15000)
  {
    time_of_last_programming = millis();    
    s.write( sign_packet, 22 );
  }

  if (s.available() > 0)
  {
    b = s.read();   
    Serial.print( String(" ") + String( b, HEX ) ); 

    num_bytes_rec++;
    lcd.setCursor(0, 1);
    lcd.print(String(num_bytes_rec) + String(" rec fr sgn   ") );
  }
  
//
//  if(Serial.available() > 0)
//  { 
//    b = Serial.read();    
//    //Serial.print( String(" ") + String( b, HEX ) ); 
//    // get current time
//    time_since_last_rec_byte = millis();
//    // write byte to buffer
//    sign_packet_buffer[num_bytes_rec] = b; 
//    // buffer has content
//    buf_ready = true;
//    // increment byte counter
//    num_bytes_rec++;
//    // debugging on lcd
//    lcd.setCursor(0, 0);  
//    lcd.print(String(num_bytes_rec) + String(" in buf       ") );
//    //lcd.setCursor(0, 0);
//    //lcd.print(String(num_bytes_rec) + String(" rec   ") );
//  }
//
//  // forward buffer to sign
//  if (millis() - time_since_last_rec_byte > 1000)
//  {
//    if (buf_ready)
//    { 
//      lcd.setCursor(11, 0);
//      //lcd.print( String( s.write( sign_packet_buffer, num_bytes_rec ) ) ); 
//      s.write( sign_packet_buffer, num_bytes_rec );
//
////      lcd.setCursor(0,0);
////      for (int a = 0; a < 16; a++)
////      {
////        lcd.print( String(char(sign_packet_buffer[a])) );
////        s.write( sign_packet_buffer[a]); 
////      }
////      lcd.setCursor(0, 1);
////      for (int b = 16; b < num_bytes_rec; b++)
////      {
////        lcd.print( String(char(sign_packet_buffer[b])) );
////        s.write( sign_packet_buffer[b] ); 
////      }
//
//      buf_ready = false;    
//      num_bytes_rec = 0;      
//    }
//  }

}





