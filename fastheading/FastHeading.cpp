

/*
 FastHeading.cpp : Defines the entry point for the console application.
 A simple apolication converting NMEA2000 Fastheading PGN127250 to
 NMEA183 HDM messages.
 The NGT-1 should be connected to COM port 7
 The NMEA183 output is at COM port 20.
 Both COM ports operate at 115000 Bps.
 Based on Håkan Svensson's SEND_NMEA_COM, see https://github.com/Hakansv/Send_NMEA_COM
 and
 Kees Verruyt's Canboat, see https://github.com/canboat/canboat/tree/master/actisense-serial

 This software is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 You should have received a copy of the GNU General Public License.
 If not, see <http://www.gnu.org/licenses/>.
 
*/ 
#include "stdafx.h"
#include <iostream>
#include <fstream>
#include "actisense.h"
#include <winsock.h>
using namespace std;


int testsum(string);
int WPL_count = 0;

string userdata = getenv("USERPROFILE");
HANDLE hSerialin, hSerialout; //COM port handler
DWORD bytes_written, total_bytes_written = 0;



static void writeMessage(HANDLE handle, unsigned char command, const unsigned char * cmd, const size_t len);
static int readNGT1(HANDLE handle);
static int readIn(unsigned char * msg, size_t msgLen);
static void messageReceived(const unsigned char * msg, size_t msgLen);
static void n2kMessageReceived(const unsigned char * msg, size_t msgLen);
static void ngtMessageReceived(const unsigned char * msg, size_t msgLen);
static void readNGT1Byte(unsigned char c);
static void parseAndWriteIn(HANDLE handle, const unsigned char * cmd);
bool OpenSerialPort(wchar_t* pcCommPort, HANDLE* handle);
void NMEA_HDM(void);
void FormatCourseData(double heading);
void OnKeyPress(void);
bool esc = false;
bool Quit = false;
char HDM_NMEA[80];
string s_Mag; //Heading for NMEA



/* The following startup command reverse engineered from Actisense NMEAreader.
* It instructs the NGT1 to clear its PGN message TX list, thus it starts
* sending all PGNs.
*/
static unsigned char NGT_STARTUP_SEQ[] =
{ 0x11   /* msg byte 1, meaning ? */
, 0x02   /* msg byte 2, meaning ? */
, 0x00   /* msg byte 3, meaning ? */
};

#define BUFFER_SIZE 900
#define PI 3.1415926535
enum ReadyDescriptor
{
  FD1_Ready = 0x0001,
  FD2_Ready = 0x0002
};

enum MSG_State
{
  MSG_START,
  MSG_ESCAPE,
  MSG_MESSAGE
};


string msg = "\n\n****************** Send NMEA data to a COM port. *****************\n";
string msg1 = "And read NMEA RAHDT from the same port and if available using it as course\n";

int main()
{
  cout << msg << msg1;
  // Define some static NMEA messages
  char NMEA_MTW[] = "$IIMTW,14.7,C*11\r\n";
  //char NMEA_DBT[] = "$IIDBT,37.9,f,11.5,M,6.3,F*1C\r\n";

  // Declare variables and structures
  //   bool Last = false;
  DCB dcbSerialParams = { 0 };
  
  char s[20];
  wchar_t pcCommPort[20];
  char Port[10];

  //ReadNavData(); //Read Navdata from file
  fprintf_s(stderr, "\nNow searching for a useable port.....\n");

  // Use FileOpen()
  // Try all 255 possible COM ports, check to see if it can be opened, or if
  // not, that an expected error is returned.

  for (int j = 7; j < 8; j++)  //  in this case only port 7
  {
    sprintf_s(s, "\\\\.\\COM%d", j);
    mbstowcs(pcCommPort, s, strlen(s) + 1); //Plus null
    sprintf_s(Port, "COM%d", j);
    // Open the port tentatively
    HANDLE hComm = ::CreateFile(pcCommPort, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);

    //  Check for the error returns that indicate a port is there, but not currently useable
    if (hComm == INVALID_HANDLE_VALUE)
    {
      DWORD dwError = GetLastError();

      if (dwError == ERROR_ACCESS_DENIED ||
        dwError == ERROR_GEN_FAILURE ||
        dwError == ERROR_SHARING_VIOLATION ||
        dwError == ERROR_SEM_TIMEOUT)
        fprintf_s(stderr, "\nFound serial port COM%d. But it's not ready to use\n", j);
    }
    else
    {
      CloseHandle(hComm);
      fprintf_s(stderr, "\nFound serial port COM%d and it's ready to use", j);
      // with the following chang manual port selection cen be made
      /*_cputs("\nPress key \"y\" to use that port. Any other key continuing search\n");
      if (toupper(_getch()) == 'Y')*/ break;
      //else:
      //continue;
    }

    Quit = j == 255 ? true : false; //When for is ended and no port found
  }

  if (Quit) {
    _cputs("\nNo useable port found or selected - Press any key to exit program");
    int Dummy = toupper(_getch());
    return 0; //No port found - exit pgm normally
  }
  // Some old stuff for info........
  //char Port[] = "COM1"; //Work around to get text
  //TCHAR *pcCommPort = TEXT("\\\\.\\COM1");
  // The \\\\.\\ is only needed for COM10 and above

 
  // here we really start!
  if (!OpenSerialPort(pcCommPort, &hSerialin)){
    fprintf(stderr, "Error making serial port for NGT-1, quit.\n");
    return 1;
  }
  // open output port
  sprintf_s(s, "\\\\.\\COM20");
  mbstowcs(pcCommPort, s, strlen(s) + 1); //Plus null
  fprintf(stderr, "Opening %s\n", s);
  
  fprintf(stderr, "Opening Serial Port 20 for output .\n");
  if (!OpenSerialPort(pcCommPort, &hSerialout)){
    fprintf(stderr, "Error opening output port, quit.\n");
    return 1;
  }

  fprintf(stderr, "Device is a serial port, send the startup sequence.\n");

  writeMessage(hSerialin, NGT_MSG_SEND, NGT_STARTUP_SEQ, sizeof(NGT_STARTUP_SEQ));
  Sleep(2000);


  // Main loop
  while (!esc){
    unsigned char msg[BUFFER_SIZE];
    size_t msgLen;
    
      if (!readNGT1(hSerialin))
      {
        break;
      }
      if (_kbhit()) { //Check the buffer for a key press to exit the program or enter a new course
        OnKeyPress();
      }
  }

  // Close serial ports and exit
  fprintf_s(stderr, "Closing serial ports...");
  if (CloseHandle(hSerialin) == 0)
  {
    fprintf_s(stderr, "Error. Press a key to exit\n");
    int Dummy = toupper(_getch());
    return 1;
  }
  if (CloseHandle(hSerialout) == 0)
  {
    fprintf_s(stderr, "Error. Press a key to exit\n");
    int Dummy = toupper(_getch());
    return 1;
  }
  fprintf_s(stderr, "OK\n");
  Sleep(1000);
 
    // Quit normally
    return 0;
} //End of main()




// Calculates the Checksum for the NMEA string
int testsum(string strN) {
int i;
int XOR = 0;
int c;
// Calculate testsum ignoring any $'s in the string
for ( i = 0; i < 80; i++) {  //strlen(strN)
  c = (unsigned char)strN[i];
  if (c == '*') break;
  if (c != '$') XOR ^= c;
  }
return XOR;
}


  double GetUserInput(const double &NavData, const int &min, const int &max) {
          double x;
          cin >> x;
          if ( x < 1 ) x += 0.1; //Avoid 0 (int 0 = false)
          cin.clear();
          cin.ignore(10000, '\n');
          if ( x >= min && x <= max ) {
              return x;
          }
          else {
              cout << "No change to: " << NavData << "\n\n";
              return NULL;
          }
  }
  
  
  void OnKeyPress(void) {
      char key = _getch();
      //cout << key << "\n";
      bool pIsTouched = false;
      switch (key) {
      case 27: //Esc
      case 32: //Space
          esc = true;
          break;
     
      default:;
      }
  }
  
  

  static void writeMessage(HANDLE handle, unsigned char command, const unsigned char * cmd, const size_t len)
  {
    unsigned char bst[255];
    unsigned char *b = bst;
    unsigned char *lenPtr;
    unsigned char crc;

    int i;

    *b++ = DLE;
    *b++ = STX;
    *b++ = command;
    crc = command;
    lenPtr = b++;

    for (i = 0; i < len; i++)
    {
      if (cmd[i] == DLE)
      {
        *b++ = DLE;
      }
      *b++ = cmd[i];
      crc += (unsigned char)cmd[i];
    }

    *lenPtr = i;
    crc += i;

    *b++ = (unsigned char)(256 - (int)crc);
    *b++ = DLE;
    *b++ = ETX;

    if (!WriteFile(handle, bst, b - bst, &bytes_written, NULL)) {
      fprintf_s(stderr, "Error. Writing to serial port. Press a key to exit\n");
      CloseHandle(hSerialin);
      int Dummy = toupper(_getch());
    }

  //  if (write(handle, bst, b - bst) != b - bst)
    
  }

  static int readNGT1(HANDLE handle)
  {
    size_t i;
 //   ssize_t r;
    int r;
    bool printed = 0;
    unsigned char c;
    unsigned char buf[500];
    DWORD dwBytesRead;
    
    r = ReadFile(handle, buf, sizeof(buf), &dwBytesRead, NULL);

    if (r <= 0) /* No char read, abort message read */
    {
      fprintf_s(stderr, "Unable to read from NGT1 device\n");
    }
    

 //   fprintf_s(stderr, "Read %d bytes from device \n", (int)dwBytesRead);
   /* if (debug)
    {
      fprintf(stderr, "read: ");
      for (i = 0; i < dwBytesRead; i++)
      {
        c = buf[i];
        fprintf(stderr, " %02X", c);
      }
      fprintf(stderr, "\n");
    }*/

    for (i = 0; i < dwBytesRead; i++)
    {
      c = buf[i];
      readNGT1Byte(c);
    }

    return r;
  }

  static void readNGT1Byte(unsigned char c)
  {
    static enum MSG_State state = MSG_START;
    static bool startEscape = false;
    static bool noEscape = false;
    static unsigned char buf[500];
    static unsigned char * head = buf;

  //  fprintf(stderr, "received byte %02x state=%d offset=%d\n", c, state, head - buf);

   /* if (state == MSG_START)
    {
      if ((c == ESC) && isFile)
      {
        noEscape = true;
      }
    }*/

    if (state == MSG_ESCAPE)
    {
      if (c == ETX)
      {
        messageReceived(buf, head - buf);
        head = buf;
        state = MSG_START;
      }
      else if (c == STX)
      {
        head = buf;
        state = MSG_MESSAGE;
      }
      else if ((c == DLE) || noEscape)
      {
        *head++ = c;
        state = MSG_MESSAGE;
      }
      else
      {
        fprintf(stderr, "DLE followed by unexpected char %02X, ignore message\n", c);
        state = MSG_START;
      }
    }
    else if (state == MSG_MESSAGE)
    {
      if (c == DLE)
      {
        state = MSG_ESCAPE;
      }
     /* else if (isFile && (c == ESC) && !noEscape)
      {
        state = MSG_ESCAPE;
      }
      else*/
  //    {
        *head++ = c;
    //  }
    }
    else
    {
      if (c == DLE)
      {
        state = MSG_ESCAPE;
      }
    }
  }

  
  static int readIn(unsigned char * msg, size_t msgLen)
  {
    bool printed = 0;
    char * s;

    s = fgets((char *)msg, msgLen, stdin);

    if (s)
    {
    //  if (debug)
   //   {
        fprintf(stderr, "in: %s", s);
    //  }

      return 1;
    }
    return 0;
  }



  static void messageReceived(const unsigned char * msg, size_t msgLen)
  {
    unsigned char command;
    unsigned char checksum = 0;
    unsigned char * payload;
    unsigned char payloadLen;
    size_t i;
 //   fprintf(stderr, "messageReceived len = %i\n", msgLen);
    if (msgLen < 3)
    {
      fprintf(stderr, "Ignore short command len = %zu\n", msgLen);
      return;
    }

    for (i = 0; i < msgLen; i++)
    {
      checksum += msg[i];
    }
   /* if (checksum)
    {
      fprintf(stderr, "Ignoring message with invalid checksum\n");
      return;
    }*/

    command = msg[0];
    payloadLen = msg[1];

//    fprintf(stderr, "X message command = %02x len = %u\n", command, payloadLen);

    if (command == N2K_MSG_RECEIVED)
    {
      n2kMessageReceived(msg + 2, payloadLen);
    }
    else if (command == NGT_MSG_RECEIVED)
    {
    //  ngtMessageReceived(msg + 2, payloadLen);  // not needed
    }
  }


  static void n2kMessageReceived(const unsigned char * msg, size_t msgLen)
  {
    unsigned int prio, src, dst;
    unsigned int pgn;
    size_t i;
    unsigned int id;
    unsigned int len;
    unsigned int data[8];
    char line[800];
    char * p;

    if (msgLen < 11)
    {
      fprintf(stderr, "Ignoring N2K message - too short\n");
      return;
    }
    prio = msg[0];
    pgn = (unsigned int)msg[1] + 256 * ((unsigned int)msg[2] + 256 * (unsigned int)msg[3]);
    if (pgn != 127250){
      return;
    }
    dst = msg[4];
    src = msg[5];
    /* Skip the timestamp logged by the NGT-1-A in bytes 6-9 */
    len = msg[10];
    double heading = ((unsigned int)msg[12] + 256 * (unsigned int)msg[13]) * 360. / PI / 20000;
    fprintf(stderr, " heading= %f \n", heading);
    if (len > 223)
    {
      fprintf(stderr, "Ignoring N2K message - too long (%u)\n", len);
      return;
    }

    FormatCourseData(heading);
    NMEA_HDM();
    if (!WriteFile(hSerialout, HDM_NMEA, strlen(HDM_NMEA), &bytes_written,
                   NULL)) {
      fprintf_s(stderr, "Error. Press a key to exit\n");
      CloseHandle(hSerialin);
      int Dummy = toupper(_getch());
      return ;
    }
    fprintf_s(stderr, "NMEA send. %s\n", HDM_NMEA);
  }


  static void ngtMessageReceived(const unsigned char * msg, size_t msgLen)
  {
    size_t i;
    char line[1000];
    char * p;
 //   char dateStr[DATE_LENGTH];

    if (msgLen < 12)
    {
      fprintf(stderr, "Ignore short msg len = %zu\n", msgLen);
      return;
    }

    sprintf(line, "%s,%u,%u,%u,%u,%u", "date ", 0, 0x40000 + msg[0], 0, 0, (unsigned int)msgLen - 1);
    p = line + strlen(line);
    for (i = 1; i < msgLen && p < line + sizeof(line) - 5; i++)
    {
      sprintf(p, ",%02x", msg[i]);
      p += 3;
    }
    *p++ = 0;

    puts(line);
    fflush(stdout);
  }

  // for later use
  static void parseAndWriteIn(HANDLE handle, const unsigned char * cmd)
  {
    unsigned char msg[500];
    unsigned char * m;

    unsigned int prio;
    unsigned int pgn;
    unsigned int src;
    unsigned int dst;
    unsigned int bytes;

    char * p;
    int i;
    int b;
    unsigned int byt;
    int r;

    if (!cmd || !*cmd || *cmd == '\n')
    {
      return;
    }

    p = strchr((char *)cmd, ',');
    if (!p)
    {
      return;
    }

    r = sscanf(p, ",%u,%u,%u,%u,%u,%n", &prio, &pgn, &src, &dst, &bytes, &i);
    if (r == 5)
    {
      p += i - 1;
      m = msg;
      *m++ = (unsigned char)prio;
      *m++ = (unsigned char)pgn;
      *m++ = (unsigned char)(pgn >> 8);
      *m++ = (unsigned char)(pgn >> 16);
      *m++ = (unsigned char)dst;
      //*m++ = (unsigned char) 0;
      *m++ = (unsigned char)bytes;
      for (b = 0; m < msg + sizeof(msg) && b < bytes; b++)
      {
        if ((sscanf(p, ",%x%n", &byt, &i) == 1) && (byt < 256))
        {
          *m++ = byt;
        }
        else
        {
     //     logError("Unable to parse incoming message '%s' at offset %u\n", cmd, b);
          return;
        }
        p += i;
      }
    }
    else
    {
  //    logError("Unable to parse incoming message '%s', r = %d\n", cmd, r);
      return;
    }

    writeMessage(handle, N2K_MSG_SEND, msg, m - msg);
  }

  void FormatCourseData(double heading) {
    double d_Mag = heading;
    if (d_Mag > 360.0){ d_Mag = d_Mag - 360.0; }
    else { if (d_Mag < 0) d_Mag = 360.0 + d_Mag; }
    s_Mag = static_cast<ostringstream*>(&(ostringstream() << setprecision(1) << fixed << d_Mag))->str();
  }


  bool OpenSerialPort(wchar_t* pcCommPort, HANDLE* handle){
    // Open serial port number
    DCB dcbSerialParams = { 0 };
    COMMTIMEOUTS timeouts = { 0 };

//    fprintf_s(stderr, "\nOpening serial port: %s ......... at 115200", pcCommPort);
    *handle = CreateFile(
      pcCommPort, GENERIC_READ | GENERIC_WRITE, 0, NULL,
      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (*handle == INVALID_HANDLE_VALUE)
    {
      fprintf_s(stderr, "Error com port. Press a key to exit\n");
      int Dummy = toupper(_getch());
      return false;
    }
    else fprintf_s(stderr, "OK\n");

    // Set device parameters (115200 baud, 1 start bit,
    // 1 stop bit, no parity)
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (GetCommState(*handle, &dcbSerialParams) == 0)
    {
      fprintf_s(stderr, "Error getting device state. Press a key to exit\n");
      CloseHandle(*handle);
      int Dummy = toupper(_getch());
      return false;
    }

    dcbSerialParams.BaudRate = CBR_115200;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;
    if (SetCommState(*handle, &dcbSerialParams) == 0)
    {
      fprintf_s(stderr, "Error setting device parameters. Press a key to exit\n");
      CloseHandle(*handle);
      int Dummy = toupper(_getch());
      return false;
    }

    // Set COM port timeout settings
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    if (SetCommTimeouts(*handle, &timeouts) == 0)
    {
      fprintf_s(stderr, "Error setting timeouts. Press a key to exit\n");
      CloseHandle(*handle);
      int Dummy = toupper(_getch());
      return false;
    }
    return true;
  }

  //Create NMEA string: $HCHDM,238.5,M*hh/CR/LF
  void NMEA_HDM() {
    string nmea = "$HCHDM,";
    nmea += s_Mag;
    nmea += ',';
    nmea += 'M';
    nmea += '*';
    nmea += static_cast<ostringstream*>(&(ostringstream() << hex << testsum(nmea)))->str();
    nmea += '\r';
    nmea += '\n';
    int Lens = nmea.size();
    memset(HDM_NMEA, NULL, sizeof(HDM_NMEA)); //Clear the array
    for (int a = 0; a < Lens; a++) {
      HDM_NMEA[a] = nmea[a];
    }
  }