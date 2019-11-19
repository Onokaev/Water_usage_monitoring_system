#include <SIM808.h>
#include <SIM808.Types.h>
#include <SIMComAT.Common.h>
#include <SIMComAT.h>

byte statusLed    = 13;
byte sensorInterrupt = 0;  // 0 = digital pin 2
byte sensorPin       = 2;

#define BUFFER_SIZE 100
float calibrationFactor = 5.5;  //For 1.5 inch sensor
float totalLitres = 0.000;
float totalLitresToSend = 0.00;

char packed_data_to_transmit[512] = {0};
char cell_information[350] = {0};

volatile byte pulseCount;

float flowRate;
unsigned int flowMilliLitres;
unsigned long totalMilliLitres;
unsigned long system_runtime = 0;
unsigned long oldTime;

#define SIM_RST     3 //PB0
#define SIM_PWR     8 //PB0

SIM808 sim808 = SIM808(SIM_RST, SIM_PWR);

int transmission_time = 0;
int trialRuns = 0;

char sim808_data_buffer[BUFFER_SIZE];
char network_time[BUFFER_SIZE];
char sim808_imei_buffer[BUFFER_SIZE];



void setup() {
  // put your setup code here, to run once:
      Serial.begin(9600);
      sim808.begin(Serial);
      sim808.powerOnOff(true);
      sim808.getImei(sim808_imei_buffer, BUFFER_SIZE);
      delay(200);

      pinMode(sensorPin, INPUT);
      digitalWrite(sensorPin, HIGH);

      pulseCount        = 0;
      flowRate          = 0.0;
      flowMilliLitres   = 0;
      totalMilliLitres  = 0;
      oldTime           = 0;

      attachInterrupt(sensorInterrupt, pulseCounter, FALLING);
}




void loop() 
{
  if (millis() - oldTime > 500)
  {
        transmission_time = transmission_time +1;
        detachInterrupt(sensorInterrupt);
        flowRate = ((1000.0 / (millis() - oldTime)) * pulseCount) / calibrationFactor;
        oldTime = millis();
        flowMilliLitres = (flowRate / 60) * 1000;
        totalMilliLitres += flowMilliLitres;
    
        unsigned int frac;
    
        totalLitres = (float)totalMilliLitres / 1000.000;
        totalLitresToSend = totalLitres;
        totalLitres = 0.00;
        pulseCount = 0;
        attachInterrupt(sensorInterrupt, pulseCounter, FALLING);

        if (transmission_time == 60)
        {
          trialRuns = trialRuns + 1;

          if (trialRuns < 10)
          {
              sim808.getImei(sim808_imei_buffer, BUFFER_SIZE);
              delay(200);
              system_runtime = millis();
              char battery_level[20] = {0};
              sim808.sendCommand("+CBC\r", battery_level, 20);
              delay(200);
              sim808.sendCommand("+CCLK?\r", network_time, BUFFER_SIZE);
              delay(200);
              char cell_infor_res[20] = {0};
              sim808.sendCommand("+CNETSCAN=1\r", cell_infor_res, 20);
              delay(200);
              sim808.sendCommand("+CNETSCAN\r", cell_information, BUFFER_SIZE);
              delay(200);
              char waterVal[20] = {0};
              dtostrf((double)totalLitresToSend, 15, 2, waterVal);

              int j = 0;
        
                  for (j = 0; j < sizeof(packed_data_to_transmit); j++)
                  {
                    packed_data_to_transmit[j] = 0;
                  }

              int aquifer_level = 99;
              int knocks_detected = 0;

              sprintf(packed_data_to_transmit, "device_id=%s&system_runtime=%ld&dispensed_water_volume=%s&aquifer_level=%d&knocks_detected=%d&battery_level=%s&mobile_network_information=%s",
                      sim808_imei_buffer,
                      system_runtime,
                      waterVal,
                      aquifer_level,
                      knocks_detected,
                      battery_level,
                      cell_information
                     );

               delay(1000);
               http_post_transmit_google_sheets(packed_data_to_transmit);
               transmission_time = 0;
               totalMilliLitres = 0;
               sim808.sendCommand("+CNETSCAN\r", cell_information, BUFFER_SIZE); 
      }

        else
        {
          //do nothing since the transmission is complete
        }
      
    }

     if (transmission_time == 7200)
     {
           sim808.getImei(sim808_imei_buffer, BUFFER_SIZE);
           delay(200);
           system_runtime = millis();
           char battery_level[20] = {0};
           sim808.sendCommand("+CBC\r", battery_level, 20);
           delay(200);
           sim808.sendCommand("+CCLK?\r", network_time, BUFFER_SIZE);
           delay(200);
           char cell_infor_res[20] = {0};
           sim808.sendCommand("+CNETSCAN=1\r", cell_infor_res, 20);
           char waterVal[20] = {0};
           dtostrf((double)totalLitresToSend, 15, 2, waterVal);
           int j = 0;
              for (j = 0; j < sizeof(packed_data_to_transmit); j++)
              {
                packed_data_to_transmit[j] = 0;
              }

           int aquifer_level = 99;
           int knocks_detected = 0;

           //Pack data to transmit as text
            sprintf(packed_data_to_transmit, "device_id=%s&system_runtime=%ld&dispensed_water_volume=%s&aquifer_level=%d&knocks_detected=%d&battery_level=%s&mobile_network_information=%s",
                    sim808_imei_buffer,
                    system_runtime,
                    waterVal,
                    aquifer_level,
                    knocks_detected,
                    battery_level,
                    cell_information
                   );
           
            delay(1000);
            http_post_transmit_google_sheets(packed_data_to_transmit);
            transmission_time = 0;
            totalMilliLitres = 0;
            sim808.sendCommand("+CNETSCAN\r", cell_information, BUFFER_SIZE);         
     }
  }
}


void pulseCounter()
{
      // Increment the pulse counter
      pulseCount++;
}


void http_post_transmit_google_sheets(const char *msg)
{
        /*
      
          AT+SAPBR=3,1,"APN","internet"
          AT+SAPBR=1,1
          AT+HTTPINIT
          AT+HTTPPARA="CID",1
          AT+HTTPPARA=\"URL\",\"https://script.google.com/macros/s/AKfycbz8EccbE9zDA_IvOl4iLf9woupHBNU_uZ3El2TTVMIHF_g1bBc/exec
          AT+HTTPPARA="CONTENT","application/text"
          AT+HTTPPARA=\"CONTENT\",\"application/x-www-form-urlencoded\"
          AT+HTTPDATA=422,20000        //Means the system should expect 422 bytes within 20 seconds
          //Wait for SIM808 to respond with "DOWNLOAD" word. Thereafter send data
          e.g. if json, send {"location_id": 238, "fill_percent": 90}
          AT+HTTPACTION=1
          AT+HTTPREAD
          AT+HTTPTERM
          AT+SAPBR=0,1
        */
      
        Serial.print("AT\r");
        delay(300);
        Serial.print("AT\r");
        delay(300);
        Serial.print("AT\r");
        delay(300);
        Serial.print("ATE0\r");
        delay(300);
        Serial.print("ATE0\r");
        delay(300);
        //debug_printd("Set Connection type GPRS\n");
        Serial.print("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"\r");
        delay(300);
        //debug_printd("Set Safaricom APN\n");
        Serial.print("AT+SAPBR=3,1,\"APN\",\"safaricom\"\r");
        delay(300);
        Serial.print("AT+SAPBR=3,1,\"USER\",\"saf\"\r");
        delay(300);
        Serial.print("AT+SAPBR=3,1,\"PWD\",\"data\"\r");
        delay(300);
        //debug_printd("Open Bearer\n");
        Serial.print("AT+SAPBR=1,1\r");
        delay(2000);
        //debug_printd("Initialize\n");
        Serial.print("AT+HTTPINIT\r");
        //debug_printd("Set CID\n");
        delay(2000);
        Serial.print("AT+HTTPPARA=\"CID\",1\r");
        delay(1000);
      
        //debug_printd("Set URL\n");
        //Serial.print(http_param_url_google_sheets_script);
        Serial.print("AT+HTTPPARA=\"URL\",\"http://api.pushingbox.com/pushingbox?devid=vDB9019FDE95823D\"\r");
        delay(300);
      
        char http_param_content_type_text[] = "AT+HTTPPARA=\"CONTENT\",\"application/x-www-form-urlencoded\"\r";
        Serial.print(http_param_content_type_text);
      
        uint16_t data_size = 0;
        data_size = strlen((const char*) msg);
        char http_data_qty_duration[50] = {0};  //TO BE SET DURING DATA PACKING FOR TRANSMISSION SINCE DATA LENGTH IS VARIABLE
        //debug_printd("Set Data length and wait duration\n");
      
        sprintf(http_data_qty_duration, "AT+HTTPDATA = %d,%d\r", data_size, 10000);
        //Set the number of bytes to send and the duration to wait for that data
        Serial.print(http_data_qty_duration);
        delay(2000);
        //debug_printd("Send data\n");
        Serial.print(msg);                   //Send the command
        Serial.print("\r");
      
        delay(2000);
        //debug_printd("Post Action\n");
        Serial.print("AT+HTTPACTION=1\r");
        delay(10000);
        Serial.print("AT+HTTPREAD\r");
        delay(500);
        //debug_printd("Read result and terminate connection\n");
        Serial.print("AT+HTTPTERM\r");
        delay(500);
        //debug_printd("Close bearer\n");
        Serial.print("AT+SAPBR=0,1\r");
        delay(500);
        //debug_printd("GPRS transmission done\n");
}
