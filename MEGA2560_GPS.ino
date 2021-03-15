/**

	A7      ----    ARDUINO MEGA2560
    ------------------------------------
	GND 	  --->		GND
	U_TXD	  --->		RX3
	U_RXD	  <---		TX3

	GPS_TXD	  --->		RX2

**/
#include <TimerOne.h>

#define DEBUG

#ifdef DEBUG
#define DEBUGSERIAL(x)  Serial.println((x))
#else
#define DEBUGSERIAL(x)  (x)
#endif
#define GPRS_SERIAL  Serial2
#define GPS_SERIAL   Serial3
#define SUCCESS      1U
#define FAILURE      0U
#define LED          LED_BUILTIN

struct
{
  bool isGetData,
       isParseData,
       isUsefull;
  char UTCTime[11],
       GPS_Buffer[80],
       latitude[11],
       N_S[2],    // N/S
       longitude[12],
       E_W[2];    // E/W
} save_data_t;

const unsigned int gpsRxBufferLength = 600,
                   gprsRxBufferLength = 500;
char gpsRxBuffer[gpsRxBufferLength], gprsRxBuffer[gprsRxBufferLength],
     messageBuffer[100] = {}, send_buf[100] = {0};
unsigned int gpsRxCount = 0, count = 0;
unsigned long time_count = 0;
unsigned int gprsBufferCount = 0;

const char TCPServer[] = "122.228.19.57", Port[] = "30396";

void setup(void)
{
  DEBUGSERIAL.begin(9600);
  GPRS_SERIAL.begin(115200);
  GPS_SERIAL.begin(9600);

  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);

  Timer1.initialize(1000);
  Timer1.attachInterrupt(timer1Handler);

  initGprs();

  DEBUGSERIAL.println(F("\r\nsetup end!"));
  clrGprsRxBuffer();
}

void loop(void)
{
  gpsRead();
  //  parseGpsBuffer();
  //  printGpsBuffer();
}


void initGprs(void)
{
  if (sendCommand("AT+RST\r\n", "OK\r\n", 3000, 10) == SUCCESS);
  else errorLog(1);

  if (sendCommand("AT\r\n", "OK\r\n", 3000, 10) == SUCCESS);
  else errorLog(2);

  delay(10);

  if (sendCommand("AT+GPS=1\r\n", "OK\r\n", 1000, 10) == SUCCESS);
  else errorLog(3);
  delay(10);

  //  if (sendCommand("AT+GPSRD=1\r\n", "OK\r\n", 1000, 10) == SUCCESS);
  //  else errorLog(3);
  //  delay(10);
}

void(* resetFunc) (void) = 0;

void errorLog(int num)
{
  DEBUGSERIAL.print("ERROR");
  DEBUGSERIAL.println(num);
  while (1)
  {
    digitalWrite(LED, HIGH);
    delay(100);
    digitalWrite(LED, LOW);
    delay(100);
    digitalWrite(LED, HIGH);
    delay(100);
    digitalWrite(LED, LOW);
    delay(300);

    if (sendCommand("AT\r\n", "OK", 100, 10) == SUCCESS)
    {
      DEBUGSERIAL.print(F("\r\nRESET!!!!!!\r\n"));
      resetFunc();
    }
  }
}

void printGpsBuffer(void)
{
  static String str = "";

  while (Serial3.available()) {
    char c = (char)Serial3.read();
    if (c != ' ')
      str += c;
    if (c == '\n' && str.indexOf("GPVTG") > 0)
      break;
  }
  str.trim();
  Serial.println(str);
  Serial.println();

  /*  if (save_data_t.isParseData)
    {
    save_data_t.isParseData = false;

    DEBUGSERIAL.print("save_data_t.UTCTime = ");
    DEBUGSERIAL.println(save_data_t.UTCTime);
    if (save_data_t.isUsefull)
    {
     save_data_t.isUsefull = false;
     DEBUGSERIAL.print("save_data_t.latitude = ");
     DEBUGSERIAL.println(save_data_t.latitude);
     DEBUGSERIAL.print("save_data_t.N_S = ");
     DEBUGSERIAL.println(save_data_t.N_S);
     DEBUGSERIAL.print("save_data_t.longitude = ");
     DEBUGSERIAL.println(save_data_t.longitude);
     DEBUGSERIAL.print("save_data_t.E_W = ");
     DEBUGSERIAL.println(save_data_t.E_W);
     digitalWrite(LED, !digitalRead(LED));//发送一组数据翻转一次
    } else DEBUGSERIAL.println("GPS DATA is not usefull!");
    }*/
}

void parseGpsBuffer(void)
{
  char *subString;
  char *subStringNext;
  if (save_data_t.isGetData)
  {
    save_data_t.isGetData = false;
    DEBUGSERIAL.println(F("**************"));
    DEBUGSERIAL.println(save_data_t.GPS_Buffer);
    for (int i = 0 ; i <= 6 ; i++)
    {
      if (i == 0)
        if ((subString = strstr(save_data_t.GPS_Buffer, ",")) == NULL)
          errorLog(12);
        else
        {
          subString++;
          if ((subStringNext = strstr(subString, ",")) != NULL)
          {
            char usefullBuffer[2];
            switch (i)
            {
              case 1: memcpy(save_data_t.UTCTime, subString, subStringNext - subString); break;
              case 2: memcpy(usefullBuffer, subString, subStringNext - subString); break;
              case 3: memcpy(save_data_t.latitude, subString, subStringNext - subString); break;
              case 4: memcpy(save_data_t.N_S, subString, subStringNext - subString); break;
              case 5: memcpy(save_data_t.longitude, subString, subStringNext - subString); break;
              case 6: memcpy(save_data_t.E_W, subString, subStringNext - subString); break;
              default: break;
            }

            subString = subStringNext;
            save_data_t.isParseData = true;
            if (usefullBuffer[0] == 'A')
              save_data_t.isUsefull = true;
            else if (usefullBuffer[0] == 'V')
              save_data_t.isUsefull = false;
          }
          else
            errorLog(13);
        }
    }
  }
}

void gpsRead(void)
{
  while (GPS_SERIAL.available())
  {
    gpsRxBuffer[gpsRxCount] = GPS_SERIAL.read();
    if (gpsRxBuffer[gpsRxCount++] == '\n')
    {
      char* GPS_BufferHead;
      char* GPS_BufferTail;
      if ((GPS_BufferHead = strstr(gpsRxBuffer, "$GPRMC,")) != NULL || (GPS_BufferHead = strstr(gpsRxBuffer, "$GNRMC,")) != NULL )
      {
        if (((GPS_BufferTail = strstr(GPS_BufferHead, "\r\n")) != NULL) && (GPS_BufferTail > GPS_BufferHead))
        {
          memcpy(save_data_t.GPS_Buffer, GPS_BufferHead, GPS_BufferTail - GPS_BufferHead);
          save_data_t.isGetData = true;
          clrGpsRxBuffer();
        }
      }
      clrGpsRxBuffer();
    }
    if (gpsRxCount == gpsRxBufferLength)
      clrGpsRxBuffer();
  }
}

void clrGpsRxBuffer(void)
{
  memset(gpsRxBuffer, 0, gpsRxBufferLength);
  gpsRxCount = 0;
}

unsigned int sendCommand(char *Command, char *Response, unsigned long Timeout, unsigned char Retry)
{
  clrGprsRxBuffer();
  for (unsigned char n = 0; n < Retry; n++)
  {
    DEBUGSERIAL.print(F("\r\n---------send AT Command:---------\r\n"));
    DEBUGSERIAL.write(Command);
    GPRS_SERIAL.write(Command);

    time_count = 0;
    while (time_count < Timeout)
    {
      gprsReadBuffer();
      if (strstr(gprsRxBuffer, Response) != NULL)
      {
        DEBUGSERIAL.print(F("\r\n==========receive AT Command:==========\r\n"));
        DEBUGSERIAL.print(gprsRxBuffer);
        clrGprsRxBuffer();
        return SUCCESS;
      }
    }
    time_count = 0;
  }
  DEBUGSERIAL.print(F("\r\n==========receive AT Command:==========\r\n"));
  DEBUGSERIAL.print(gprsRxBuffer);
  clrGprsRxBuffer();
  return FAILURE;
}

unsigned int sendCommandReceive2Keyword(char *Command, char *Response, char *Response2, unsigned long Timeout, unsigned char Retry)
{
  clrGprsRxBuffer();
  for (unsigned char n = 0; n < Retry; n++)
  {
    DEBUGSERIAL.print(F("\r\n---------send AT Command:---------\r\n"));
    DEBUGSERIAL.write(Command);
    GPRS_SERIAL.write(Command);

    time_count = 0;
    while (time_count < Timeout)
    {
      gprsReadBuffer();
      if (strstr(gprsRxBuffer, Response) != NULL && strstr(gprsRxBuffer, Response2) != NULL)
      {
        DEBUGSERIAL.print(F("\r\n==========receive AT Command:==========\r\n"));
        DEBUGSERIAL.print(gprsRxBuffer);
        clrGprsRxBuffer();
        return SUCCESS;
      }
    }
    time_count = 0;
  }
  DEBUGSERIAL.print(F("\r\n==========receive AT Command:==========\r\n"));
  DEBUGSERIAL.print(gprsRxBuffer);
  clrGprsRxBuffer();
  return FAILURE;
}

void timer1Handler(void)
{
  time_count++;
}

void gprsReadBuffer()
{
  while (GPRS_SERIAL.available())
  {
    gprsRxBuffer[gprsBufferCount++] = GPRS_SERIAL.read();
    if (gprsBufferCount == gprsRxBufferLength)
      clrGprsRxBuffer();
  }
}

void clrGprsRxBuffer(void)
{
  memset(gprsRxBuffer, 0, gprsRxBufferLength);
  gprsBufferCount = 0;
}
