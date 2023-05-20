// -------------------------------
// CpuFanControl, V0.01 26.08.2022
// -------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

#define HIGH    1
#define LOW     0

#define closeLOG    fclose(logfile);

extern int errno;

// -------------------------

struct Config {
  int FAN_PIN;
  int HIGH_TEMP;
  int LOW_TEMP;
};

FILE *logfile;

void writeLog(char *s, uint cmd);   // s string, cmd = 1 print to stdout, 0 - quiet

// GPIO control
int gpioInit(int apin, char *amode);
int gpioFree(int apin);
int gpioWrite(int apin, int val);

// get current cpu temperature
int cpuTemp();

// get parameters from *.conf file param_name=value
int getParam(char *buf, char *pname);

// return s with current date+time
void curDT(char *s);

// --------------------------------------------------------

int main(int argc, char *argv[])
{
  char *cfile = "cfc.conf";
  char *logName="/var/log/cpufanctrl.log";
  char buf[100];
  FILE *f;
  struct Config Cfg;
  int ctemp;

  printf("CpuFanControl, V0.01\n");
  printf("--------------------\n");

  for(int i = 0; i < 100; i++) buf[i] = 0x00;

// log file

  logfile = fopen(logName, "a");

  if (!logfile)
  {
    printf("Error open log file: %s\n", logName);
    exit(1);
  }

// conf file

  strcat(buf, argv[0]);
  strcat(buf, ".conf");

  f = fopen(buf, "r");

  if (!f)
  {
    sprintf(buf, "Error open %s! %s\n", cfile, strerror(errno));
    writeLog(buf, 1);
    closeLOG;
    exit(1);
  }

  fread(buf, 100, 1, f);
  fclose(f);

  if (!buf)
  {
    sprintf(buf, "Wrong format config file: %s\n", cfile);
    writeLog(buf, 1);
    closeLOG;
    exit(1);
  }

// FAN GPIO!!! PIN

  Cfg.FAN_PIN = getParam(buf, "FAN_PIN");
  if (Cfg.FAN_PIN <= 0 || Cfg.FAN_PIN > 255){
    sprintf(buf, "Wrong settings for FAN_PIN %d!\n", Cfg.FAN_PIN); 
    writeLog(buf, 1);
    closeLOG;
    exit(1);
  }

// cpu high temperature

  Cfg.HIGH_TEMP = getParam(buf, "HIGH_TEMP");
  if (Cfg.HIGH_TEMP <= 0 || Cfg.HIGH_TEMP > 100)
  {
    sprintf(buf, "Wrong settings for HIGH_TEMP %d!\n", Cfg.HIGH_TEMP);
    writeLog(buf, 1);
    closeLOG;
    exit(1);
  }

// cpu low temperature

  Cfg.LOW_TEMP = getParam(buf, "LOW_TEMP");
  if (Cfg.LOW_TEMP <= 0 || Cfg.LOW_TEMP > 100)
  {
    sprintf(buf, "Wrong settings for LOW_TEMP %d!\n", Cfg.LOW_TEMP);
    writeLog(buf, 1);
    closeLOG;
    exit(1);
  }

  if (Cfg.LOW_TEMP >= Cfg.HIGH_TEMP)
  {
    sprintf(buf, "LOW_TEMP >= HIGH_TEMP!\n");
    writeLog(buf, 1);
    closeLOG;
    exit(1);
  }

  printf("FAN_PIN=%d HIGH_TEMP=%d LOW_TEMP=%d\n", Cfg.FAN_PIN, Cfg.HIGH_TEMP, Cfg.LOW_TEMP);

  ctemp = cpuTemp();

  printf("CPU temp=%d\n", ctemp);

// ---

  if (ctemp >= Cfg.HIGH_TEMP)
  {
    sprintf(buf, "cputemp(%d) >= HIGH_TEMP(%d) --> CPU_FAN ON!\n", ctemp, Cfg.HIGH_TEMP);
    writeLog(buf, 1);

    puts("init gpio pin...");

    gpioInit(Cfg.FAN_PIN, "out");

    puts("write HIGH");

    gpioWrite(Cfg.FAN_PIN, HIGH);
  }
  else
  {
    printf("CPUtemp(%d) below HIGH_TEPM(%d)\n", ctemp, Cfg.HIGH_TEMP);
    closeLOG;
    exit(0);
  }

  while ( ctemp > Cfg.LOW_TEMP )
  {
    ctemp = cpuTemp();

    printf("cpuTemp=%d\n", ctemp);

    sleep(5);
  }

    sprintf(buf, "cputemp(%d) < LOW_TEMP(%d) --> CPU_FAN OFF!\n", ctemp, Cfg.LOW_TEMP);
    writeLog(buf, 1);

    puts("write LOW");
    gpioWrite(Cfg.FAN_PIN, LOW);
    puts("free gpio pin...");
    gpioFree(Cfg.FAN_PIN);

    fclose(logfile);
}

// ----------------------------------------------------------
// get parameter from conf file

int getParam(char *buf, char *pname)
{
  char ch;
  char *r;
  char s[10];
  int res = 0;
  int i = 0;

  r = strstr(buf, pname);
  if (!r) return -1;

  r = strchr(r , '=');
  if (!r) return -2;

  ch = *r++;

  while ((ch = *r++))
  {
    if (ch == '\n') break;
    s[i++] = ch;
  }

  s[i++] = 0x00;

  return atoi(s);
}

// ---------------------------------------

int gpioInit(int apin, char *amode)
{
  FILE *f;
  char buf[50];

  sprintf(buf, "/sys/class/gpio/gpio%d/value", apin);

  if (!access(buf, F_OK))
  {
    printf("GPIO pin %d already in use!\n", apin);
    exit(1);
  }

  f = fopen("/sys/class/gpio/export", "w");

  if (!f)
  {
    printf("Error open GPIO /sys/class/gpio/export or GPIO not installed!\n");
    fprintf(stderr, "Error opening file: %s\n", strerror(errno));
    exit(1);
  }

  if (fprintf(f, "%d", apin) < 0)
  {
    printf("Error initialize GPIO PIN %d!\n", apin);
    fprintf(stderr, "Error writing GPIO: %s\n", strerror(errno));
    fclose(f);
    exit(1);
  }

  fclose(f);

  return 0;
}

// ---------------------------------------

int gpioFree(int apin)
{
  FILE *f;

  f = fopen("/sys/class/gpio/unexport", "w");

  if (!f)
  {
    printf("Error open GPIO /sys/class/gpio/unexport or GPIO not installed!\n");
    fprintf(stderr, "%s\n", strerror(errno));
    exit(1);
  }

  if (fprintf(f, "%d", apin) < 0)
  {
    fclose(f);
    printf("Error unitialize GPIO PIN %d!\n",apin);
    fprintf(stderr, "%s\n", strerror(errno));
    exit(1);
  }

  fclose(f);

  return 0;
}

// ---------------------------------------

int gpioWrite(int apin, int val)
{
  FILE *f;
  char fName[100];

  sprintf(fName, "/sys/class/gpio/gpio%d", apin);
  strcat(fName, "/value");

  f = fopen(fName, "w");

  if (!f)
  {
    printf("Error open GPIO %s\n", fName);
    fprintf(stderr, "Error opening file: %s\n", strerror(errno));
    exit(1);
  }

  if (fprintf(f, "%d", val) < 0)
  {
    fclose(f);
    printf("Error write %d into GPIO PIN %d!\n", val, apin);
    exit(1);
  }

  fclose(f);

  return 0;
}

// --------------------------------------------------------------
// get current CPU temperature

int cpuTemp()
{
  char *cfile = "/sys/class/thermal/thermal_zone0/temp";
  char buf[20];
  FILE *f;
  int ctemp;

  f = fopen(cfile, "r");

  if (!f)
  {
    printf("Error open file: %s\n", cfile);
    return -1;
  }

  fread(buf, 20, 1, f);
  fclose(f);

  for(int i = 0; i < strlen(buf); i++)
  {
    if( buf[i] == 0x0A)
    {
      buf[i] = 0x00;
      break;
    }
  }

  ctemp = atoi(buf) / 1000;

  return ctemp;
}

// -----------------------------------

void writeLog(char *s, uint cmd)
{
  char dt[80];

  curDT(dt);

  if (cmd) puts(s);

  fprintf(logfile, "%s %s", dt, s);
}

// ----------------------------------
// current date time to s

void curDT(char *s)
{
  time_t rawtime;
  struct tm *info;
  char buffer[80];

  time( &rawtime );

  info = localtime( &rawtime );

  strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", info);

  strcpy(s, buffer);

  return;
}