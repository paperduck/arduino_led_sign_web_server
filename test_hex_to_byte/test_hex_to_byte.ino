

void setup()
{
   Serial.begin(9600);
      
   Serial.println("hello");
      
   char s[3] = "11";
   long l;
   //l = strtol(s, NULL, 16);   
   Serial.println( (byte)l );
   
   Serial.println("goodbye");
   
}

void loop()
{
  
}
