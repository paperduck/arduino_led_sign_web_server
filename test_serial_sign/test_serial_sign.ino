/*
 RS232 shield test program
 Created: 2014-12-05 
 */

#include <SoftwareSerial.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>


byte   sign_packet [22] = {
  0x00 , 0xFF , 0xFF , 0x00 , 0x0B , 0x01 , 0xFF , 0x01 , 0x30 , 0x31 , 0xEF , 0xB0 , 0xEF , 0xA2 ,
  0x43 , 0x65 , 0x6C , 0x6C , 0x6F , 
  0xFF , 0xFF , 0x00 };

byte     sign_packet_buffer[100];

unsigned long     time_since_last_rec_byte = 0;
// Communication with sign doesn't seem to work without null modem cable,
// even if the pins are switched here.
SoftwareSerial    s(2,3); // RX, TX
SoftwareSerial    s_in(5,6);
byte              b;
//boolean         temp_bool = true;
boolean           temp_bool2 = true;
long              time_of_last_programming = millis() + 60000;
unsigned long     num_bytes_rec = 0;
unsigned long     num_bytes_sent = 0;
LiquidCrystal     lcd(A5, A4, A3, A2, A1, A0); // (rs, enable, d4, d5, d6, d7) 
boolean           buf_ready = false;
int               bytes_per_buffer_chunk = 1;
int               eeprom_write_counter = 0;
bool              serial_buffer_empty = true;

/****************************************************************/
void setup()
{
  lcd.begin(16,2);
  lcd.clear();

  Serial.begin(9600); 
  s.begin(2400);
  s_in.begin(2400);
}

/****************************************************************/
void loop()
{    
  //  if (millis() - time_of_last_programming > 60000)
  //  {
  //    time_of_last_programming = millis();    
  //    s.write( sign_packet, 22 ); 
  //    Serial.write( sign_packet, 22 );
  //  }


  //--------------------------------------------------------------
  // NO BUFFER
  // does nothing
  
//  if (s_in.available())
//  {
//    s.write( (byte)(s_in.read()) );
//  }
//  return;
  
  //--------------------------------------------------------------
  // NO BUFFER
  // Works sometimes, but sign occasionally does a long (~2 sec) beep.
  // Also puts a weird graphic before text on sign.


  //  if( s_in.available() )
  //  {
  //    while (s_in.available())
  //    {
  //      b = s_in.read();
  //      
  //      num_bytes_rec ++;      
  //      lcd.setCursor(0,0);
  //      lcd.print( String(num_bytes_rec) + String(" recvd") );   
  //      
  //      s.write(b); 
  //      Serial.write( b );
  //    }
  //  }

  //--------------------------------------------------------------
  // CHUNKED BUFFER IN EEPROM - ITERATIVE FOR MICROCONTROLLER
  // works 90% of the time

  if (s_in.available())
  {
    Serial.print("x");
    
    // read byte into EEPROM
    b = s_in.read();
    if (eeprom_write_counter < 100)
    {
      EEPROM.write(num_bytes_rec, b );
      eeprom_write_counter++;
    }
    else
    {
      lcd.setCursor(0,0);
      lcd.print( "EEPROM write max" );  
      return;
    }
    num_bytes_rec++;
    serial_buffer_empty = false;

    lcd.setCursor(0,0);
    lcd.print( num_bytes_rec );    
  }
  else
  {       
    // see if bytes are waiting to be sent to sign
    if (!serial_buffer_empty)
    {
      // send bytes to sign, one chunk at a time
      if (num_bytes_sent < num_bytes_rec)
      {
        // check if "chunk-size" bytes should be sent, or the lesser remainder bytes. 
        if (bytes_per_buffer_chunk < (num_bytes_rec - num_bytes_sent) )
        {
          // load "chunk-size" bytes from EEPROM into SRAM
          for (int i = 0; i < bytes_per_buffer_chunk; i++)
          {
            sign_packet_buffer[i] = EEPROM.read(num_bytes_sent + i);
          }
          // send chunk
          num_bytes_sent += ( s.write( sign_packet_buffer, bytes_per_buffer_chunk) ); 
          
          lcd.setCursor(0,1);
          lcd.print ( num_bytes_sent );
        }
        else
        {        
          // load remaining bytes from EEPROM into SRAM
          for (int i = 0; i < (num_bytes_rec - num_bytes_sent); i++)
          {
            sign_packet_buffer[i] = EEPROM.read(num_bytes_sent + i);
          }
          // send chunk
          num_bytes_sent += ( s.write( sign_packet_buffer, (num_bytes_rec - num_bytes_sent)) ); 
          
          lcd.setCursor(0,1);
          lcd.print ( num_bytes_sent );
        }
      }
      else
      {
        // done; reset state variables
        serial_buffer_empty = true;
        num_bytes_rec = 0;
        num_bytes_sent = 0;
      }
    }
  }  

  //--------------------------------------------------------------
  // CHUNKED BUFFER IN EEPROM
  // works fine with chunk=4,6

  //  if (s_in.available())
  //  {
  //    num_bytes_rec = 0;
  //    // read all bytes into EEPROM
  //    while(s_in.available() )
  //    {
  //      b = s_in.read();
  //      EEPROM.write(num_bytes_rec, b );
  //      num_bytes_rec++;
  //
  //      lcd.setCursor(0,0);
  //      lcd.print( num_bytes_rec );
  //    }
  //
  //    // send bytes to sign, one chunk at a time
  //    num_bytes_sent = 0;
  //    while (num_bytes_sent < num_bytes_rec)
  //    {
  //      lcd.setCursor(0,1);
  //      if (bytes_per_buffer_chunk < (num_bytes_rec - num_bytes_sent) )
  //      {
  //        // load chunk from EEPROM into SRAM
  //        for (int i = 0; i < bytes_per_buffer_chunk; i++)
  //        {
  //          sign_packet_buffer[i] = EEPROM.read(num_bytes_sent + i);
  //        }
  //        // send chunk
  //        lcd.print ( s.write( sign_packet_buffer, bytes_per_buffer_chunk) ); 
  //        num_bytes_sent += bytes_per_buffer_chunk;
  //      }
  //      else
  //      {        
  //        // load chunk from EEPROM into SRAM
  //        for (int i = 0; i < (num_bytes_rec - num_bytes_sent); i++)
  //        {
  //          sign_packet_buffer[i] = EEPROM.read(num_bytes_sent + i);
  //        }
  //        // send chunk
  //        lcd.print ( s.write( sign_packet_buffer, (num_bytes_rec - num_bytes_sent)) ); 
  //        num_bytes_sent += (num_bytes_rec - num_bytes_sent);
  //      }
  //    }
  //  }

  //--------------------------------------------------------------
  // BUFFER IN EEPROM

  //  num_bytes_rec = 0;
  //  if (s_in.available() > 0)
  //  {
  //    while(s_in.available() > 0)
  //    {
  //      b = s_in.read();
  ////      EEPROM.write(num_bytes_rec, b);
  //      EEPROM.write(num_bytes_rec, char('a'));
  //      num_bytes_rec ++;
  //
  //      lcd.setCursor(0,0);
  //      lcd.print( num_bytes_rec );
  //    }
  //    
  //    lcd.setCursor(0,1);
  //    //lcd.print( s.write( (char*)EEPROM.read(0), num_bytes_rec ) );
  //    Serial.write( (char*)(EEPROM.read(0)), num_bytes_rec );
  //  }

  //--------------------------------------------------------------
  // PARTIAL BUFFER IN SRAM
  // Programs sign OK; tested with chunks: 3 byte, 4 byte

  //  num_bytes_rec = 0;
  //  if (s_in.available())
  //  {
  //    // read bytes into buffer
  //    while(s_in.available() > 0)
  //    {
  //      b = s_in.read();
  //      sign_packet_buffer[num_bytes_rec] = b;
  //      num_bytes_rec++;
  //
  //      lcd.setCursor(0,0);
  //      lcd.print( num_bytes_rec );
  //    }
  //    
  //    // write bytes to sign, one chunk at a tim
  //    num_bytes_sent = 0;
  //    while (num_bytes_sent < num_bytes_rec)
  //    {
  //      lcd.setCursor(0,1);
  //      if (bytes_per_buffer_chunk < (num_bytes_rec - num_bytes_sent) )
  //      {
  //        lcd.print ( s.write( sign_packet_buffer + num_bytes_sent, bytes_per_buffer_chunk) ); 
  //        num_bytes_sent += bytes_per_buffer_chunk;
  //      }
  //      else
  //      {
  //        lcd.print ( s.write( sign_packet_buffer + num_bytes_sent, (num_bytes_rec - num_bytes_sent) ) ); 
  //        num_bytes_sent += (num_bytes_rec - num_bytes_sent);
  //      }
  //    }
  //  }

  //--------------------------------------------------------------
  // FULL BUFFER IN SRAM - SHORT VERSION
  // Programs sign OK

  //  num_bytes_rec = 0;
  //  if (s_in.available())
  //  {
  //    while(s_in.available() > 0)
  //    {
  //      b = s_in.read();
  //      sign_packet_buffer[num_bytes_rec] = b;
  //      num_bytes_rec++;
  //
  //      lcd.setCursor(0,0);
  //      lcd.print( num_bytes_rec );
  //    }
  //    lcd.setCursor(0,1);
  //    lcd.print ( s.write( sign_packet_buffer, num_bytes_rec) );
  //  }

  //--------------------------------------------------------------
  // BUFFER IN SRAM - LONG VERSION

  //  if(s_in.available() > 0)
  //  { 
  //    b = s_in.read();    
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
  //    lcd.print(String(num_bytes_rec) + String(" rec") );
  //  }
  //
  //  // forward buffer to sign
  //  if (millis() - time_since_last_rec_byte > 1000)
  //  {
  //    if (buf_ready)
  //    {     
  //      lcd.setCursor(0, 1);
  //      //lcd.print( String( s.write( sign_packet_buffer, num_bytes_rec ) ) + String(" sent") ); // doesn't program sign 
  //      //num_bytes_sent = s.write( sign_packet_buffer, num_bytes_rec ); // doesn't program sign
  //      s.write( sign_packet_buffer, num_bytes_rec ); // programs sign OK
  //      lcd.print(String(num_bytes_sent) + String(" sent") );
  //
  //      //      lcd.setCursor(0,0);
  //      //      for (int a = 0; a < 16; a++)
  //      //      {
  //      //        lcd.print( String(char(sign_packet_buffer[a])) );
  //      //        s.write( sign_packet_buffer[a]); 
  //      //      }
  //      //      lcd.setCursor(0, 1);
  //      //      for (int b = 16; b < num_bytes_rec; b++)
  //      //      {
  //      //        lcd.print( String(char(sign_packet_buffer[b])) );
  //      //        s.write( sign_packet_buffer[b] ); 
  //      //      }
  //
  //      buf_ready = false;    
  //      num_bytes_rec = 0;    
  //      num_bytes_rec = 0;      
  //    }
  //  }

  //--------------------------------------------------------------
}

























