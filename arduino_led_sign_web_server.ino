/*
LED Adapter
 Date Created:  2014-11-19
 Using COM 6 on CB workstation
 
 TODO:
 - xxx consider a message queue that cycles so user can see every message
 - xxx continuously check ethernet_on flag, try to connect if false
 - xxx likewise, continuously check SD "enabled" flag
 - xxx consider replacing all String class uses with char arrays/pointers
 - xxx short ints
 - xxx triple-check memory allocation + deallocation
 */

#include <LiquidCrystal.h>
#include <SPI.h>
#include <Ethernet.h>
#include <SD.h>

//Create LCD screen instance ***
LiquidCrystal lcd(A5, A4, A3, A2, A1, A0); // (rs, enable, d4, d5, d6, d7) 

// Ethernet ***
byte mac[] = {
  0x90, 0xA2, 0xDA, 0x0F, 0x96, 0xBE};
byte ip[] = {
  10, 10, 151, 121};
unsigned short int server_port = 80;
EthernetServer server = EthernetServer(server_port);
EthernetClient client = EthernetClient();

// diagnostic ***
unsigned short int         num_requests = 0;   // counter for requests
unsigned long              num_bytes_sent = 0; // counter for num bytes sent
unsigned long              num_bytes_rec = 0;  // counter for num bytes received
unsigned short int         status_code = 0;
String                     serial_debug_string_slow = "";
unsigned long              time_of_last_serial_debug_print = millis();
unsigned short int         d = 500;

// Parsing, Flags, State ***
unsigned long              cur_line_num = 1;   // 1-based. 0 means no lines yet. (lines of incoming header)
unsigned short int         cur_token_index = 0;  // 0-based. e.g. GET will have token index 0. Filename 1. HTTP version 2.
char *                     file_type; // text/html, text/xml, image, etc.
String                     file_path_name_buf_string = ""; // xxx consider using char* only to save memory
char *                     file_path_name_buf; // feed into SD.exists()
File                       f; // the file to be served to the client
boolean                    file_ready = false;
char*                      http_header = ""; // header to send to client, including final CRLF
byte                       cur_byte;          // byte read in from client
boolean                    get_or_post = false;        // false=GET true=POST
String                     token = "";      // to hold bytes read from client
boolean                    receive_or_send = false; // 0=receive 1=send
boolean                    appending_token = true; // false=no the first space; true=this is the first space

void lcd_print(int x, int y, String msg)
{
  lcd.setCursor(x, y);
  lcd.print(msg);
}

void lcd_print(int x, int y, IPAddress msg)
{
  lcd.setCursor(x, y);
  lcd.print(msg);
}

// by David A. Mellis / Rob Faludi http://www.faludi.com
int availableMemory() {
  int size = 2048; // Use 2048 with ATmega328
  byte *buf;
  while ((buf = (byte *) malloc(--size)) == NULL)
    ;
  free(buf);
  return size;
}

void setup()
{
  Serial.begin(9600); 

  // LCD screen
  lcd.begin(16,2);

  lcd_print(0, 0, "starting up...");

  // Ethernet
  while ( ! Ethernet.begin(mac) )
  {
    lcd.clear();
    lcd_print(0,0,"Ethernet failure");  
    delay(d);
  }
  server.begin(); // starts listening for incoming connections
  lcd.clear();
  lcd_print(0,0,"Ethernet on.");     
  delay(d);
  lcd.clear();
  lcd_print(0,0, Ethernet.localIP()); 
  delay(d);
  // SD card
  pinMode(10, OUTPUT);
  while ( ! SD.begin(4) )
  {
    lcd.clear();
    lcd_print(0,0,"SD failure.     "); 
    delay(d);
  }
  lcd.clear();
  lcd_print(0,0,"SD initialized. "); 
  delay(d);  
  lcd.clear();
}

void loop()
{
  //  if (millis() - time_of_last_serial_debug_print > 100)
  //  {
  //    Serial.print(serial_debug_string_slow);
  //    time_of_last_serial_debug_print = millis();
  //    // clear serial message buffer
  //    serial_debug_string_slow = "";
  //  }
  lcd_print(0, 0, String(availableMemory()));
  //lcd_print(0, 0, "xxx");
  if (status_code < 10)
  {
    lcd_print(14, 1, " "); // clear error code
  }
  lcd_print(13, 1, String(status_code)); // print error code
  lcd_print(0, 1, String(num_requests) + "q" );    // num requests
  lcd_print(4, 1, String(num_bytes_sent) + "s");   // num bytes sent
  lcd_print(9, 1, String(num_bytes_rec) + "r");    // num bytes received

  if (client)
  {
    lcd_print(15, 1, "c");
    // sending or receiving?
    if (!receive_or_send) 
    {
      // receiving data from client.    
      cur_byte = client.read();
      num_bytes_rec++;

      if (cur_byte == 0xFF) // 0xFF == EOF == -1 as char
      {
        // EOF
        // reset counters  xxx
        token = "";
        cur_token_index = 0;
        cur_line_num = 1;
        appending_token = false;
        num_requests++; // tally this "request"
        free(file_path_name_buf); // super-important memory deallocation
      }
      else
      {
        // if whitespace
        if (char(cur_byte) == char(' '))
        {
          appending_token = false;
          // process token that just finished arriving
          switch (cur_line_num)
          {
          case 1: // cur_line_num
            switch (cur_token_index)
            {
            case 0: // cur_token_index
              //Serial.print( String(cur_line_num == 0 ? 69 : cur_line_num) + ":" + String(cur_token_index == 0 ? 69 : cur_token_index) + "\n");
              Serial.print("1:0\n");
              // GET or POST?
              if (token == "GET")
              {
                get_or_post = false;
                lcd_print(13,0,"GET");
              }
              else if (token == "POST")
              {
                get_or_post = true;
                lcd_print(13,0,"POST");
              }
              else
              {
                ;
              }
              break;

            case 1: // cur_token_index
              //Serial.print( String(cur_line_num) + ":" + String(cur_token_index) + "\n" );
              Serial.print("1:1\n");
              // This token should be the file requested.
              //file_path_name_buf_string = token; 
              file_path_name_buf = (char*)malloc((token.length() * sizeof(char)) + 1);
              token.toCharArray( file_path_name_buf, token.length() + 1 );
              break;

            case 2:
              Serial.print("1 : 2\n");
              // protocol version should be followed by CRLF, not (SP)
              // xxx    400 (Bad Request) error
              break;

            default: // cur_token_index
              //Serial.print( String(cur_line_num) + ":" + String(cur_token_index) + "\n" );
              Serial.print("1:d\n");
              break;  
            }
            break;

          default: // cur_line_num
            //Serial.print(  String(cur_line_num) + ":" + String(cur_token_index) + "\n" );
            Serial.print("d:x\n");
            break;
          }
          appending_token = false; // cur byte is a space, so no longer 
        }
        else if( char(cur_byte) == char('\r') )
        {
          Serial.print( "\\r\n" );
          Serial.print("    finished token:  " + token + "\n"); 
          if (cur_line_num == 1 && cur_token_index == 2)
          {
            // This token should be protocol (HTTP) version 
            if (token.length() >= 4)
            {
              if (token.startsWith("HTTP"))
              {
                ; // good to go 
                receive_or_send = true; // send response
              }
              else
              {
                ; // xxx    505  (HTTP Version Not Supported) 
              }
            }
            else
            { 
              ; // xxx    505  (HTTP Version Not Supported) 

            }            
          }
          else
          {
            ; // xxx handle other lines here as needed
          }
        }
        else if(char(cur_byte) == char('\n'))
        {        
          Serial.print( String(cur_line_num) + ":" + String(cur_token_index) + "  \\n\n" );
          cur_line_num++; // increment the \n
          appending_token = false; // reset just in case spaces at end of last line
          cur_token_index = 0; // back to first token
        }
        else
        {
          // non-whitespace, so append to token
          if (!appending_token)
          {
            // starting a new token
            //Serial.print( "    incrementing token index: " + String(cur_token_index) + " -> " + String(cur_token_index+1) + "\n" );
            Serial.print("    finished token:  " + token + "\n"); 
            Serial.print("    incrementing cur_token_index to ");
            Serial.print(cur_token_index + 1);
            Serial.print("\n");
            //Serial.print("    incrementing cur_token_index to " + String(cur_token_index + 1) + "\n");
            //Serial.print("    incrementing cur_token_index\n");
            cur_token_index++;
            token = "";
            appending_token = true;
          }
          token += String(char(cur_byte));
        }
      }
    }
    else
    {
      // sending data to client(s)
      num_bytes_sent++;

      //server.write(cur_byte);   

      Serial.print("attempting to serve ");
      Serial.print(String(file_path_name_buf));
      Serial.print("\n");
      
      // Serve the file requested.
      if (SD.exists(file_path_name_buf))
      {
        lcd_print(6, 0, "sd+");
        f = SD.open(file_path_name_buf, FILE_READ);
        if (!f)
        {
          // error
          lcd_print(4,0, "f-"); 
        }
        else
        {
          lcd_print(4,0, "f+"); 
          server.println("HTTP/1.1 200 OK");
          server.println("Content-Type: text/html");
          server.println("Content-Length: " + String(f.size()) + "\r\n");
          while (f.available())
          {
            server.write(f.read()); 
          }
        }
      }
      else
      {
        lcd_print(6, 0, "sd-");
        server.println("HTTP/1.1 404 Not Found\r\n\r\n");
      }


      // if done sending, disconnect the client
      if (client.connected())
      {
        client.stop();  // disconnect the client
      }
    }
  }
  else
  {
    // No client. 
    lcd_print(15, 1, " "); // no 'c'

    // Check for incoming client.
    client = server.available(); 
  }
}






















































































