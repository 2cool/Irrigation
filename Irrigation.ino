
//добавить команди включить и отключить воду
// добавить флаг отработки тамймера
//программу включения таймера через определенние промежутки времени на определенное время
//изменить программу так чтоби в одной строке било время включения и время отключения.

//#define __DEBUG

#define PROGS_AMOUNT 8
#define VELVES 2
#define TIME_2_OPEN_MAX 10000
#define MAX_DELAY 15000
#define OFFLINE (sizeof(PROG)* PROGS_AMOUNT)

//////////////////////////////
int64_t correction4 =    987470;//,19122609673790776152980877
//int64_t correction4 = 1151496;
int64_t correction = -1000;
///////////////////////////////

uint32_t throttle[VELVES] = { 0, 0 };


uint32_t secs_now;
#include <EEPROM.h>
//#define Serial Serial3
struct TIME{
    uint8_t days, hours,minutes,sec;
};

TIME now_t;
struct LOGS
{
	uint8_t velve	: 1;
	uint8_t open	: 1;
	uint8_t day		: 3;
	uint8_t h		: 5;
	uint8_t m		: 6;
	
};

//LOG_SIZE = 2^n
#define LOG_SIZE 32

LOGS logs[LOG_SIZE];
uint8_t logsI = 0;


struct PROG{  
	uint32_t start_time_in_sec,stop_time_in_sec;
	uint32_t time_2_open;// 0 - 86399  the velve need to be closed in day it was opened
	uint32_t time_2_close;
    uint16_t throttle; //velve opening im msec 
	uint8_t day; //0-6 or 7 for everyday
	
    uint8_t velveN;
	uint8_t opened_for_day; // if n == n-day then done
	uint8_t colosed_for_day;  
    bool active;
};


int64_t time_;
int64_t last_correction_time=0;
uint32_t old_time=0;
PROG prog[PROGS_AMOUNT];
PROG *active_prog[VELVES];
float volt = 5, moisture_mark = 800, do_not_water_level_L = 677, do_water_level_H=750;

uint8_t offline_cnt=0;
uint8_t this_session_offline_cnt = 0;
bool online = true;
bool moisture = false;
// d>6 = eny day
const String d[] = { "Mon.","Tue.","Wed.","Thu.","Fri.","Sat.","Sun." };

uint64_t get_time()
{
	return time_ + millis();
}
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
uint16_t opened_thr[2] = { 0, 0 };


void close2()
{
	Serial.println(" CLOSED2");
#ifndef __DEBUG
	digitalWrite(6 + 2, LOW);
	digitalWrite(7 + 2, HIGH);
	digitalWrite(6 , LOW);
	digitalWrite(7 , HIGH);
	delay(MAX_DELAY);
	digitalWrite(7 + 2, LOW);
	digitalWrite(7 , LOW);
#endif
}
void close(const uint8_t v)
{
	if (opened_thr[v] > 0)
	{
		logs[logsI].velve = v;
		logs[logsI].open = 0;
		logs[logsI].day = now_t.days;
		logs[logsI].h = now_t.hours;
		logs[logsI].m = now_t.minutes;
		logsI = (logsI + 1) & (LOG_SIZE-1);
		
		opened_thr[v] = 0;
		Serial.print(" CLOSED ");
		Serial.println(v);
#ifndef __DEBUG
		digitalWrite(6 + v * 2, LOW);
		digitalWrite(7 + v * 2, HIGH);
		delay(MAX_DELAY);
		digitalWrite(7 + v * 2, LOW);
#endif
	}
}



//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void open(const uint8_t v, const uint16_t thr)
{
	if (opened_thr[v] == 0)
	{
		opened_thr[v] = thr;
		logs[logsI].velve = v;
		logs[logsI].open = 1;
		logs[logsI].day = now_t.days;
		logs[logsI].h = now_t.hours;
		logs[logsI].m = now_t.minutes;
		logsI = (logsI + 1) & (LOG_SIZE-1);
		
		Serial.print(" OPENED ");
		Serial.print(v);
		Serial.print(" ");
		Serial.println(thr);
#ifndef __DEBUG
		digitalWrite(7 + v * 2, LOW);
		digitalWrite(6 + v * 2, HIGH);
		delay(thr);
		digitalWrite(6 + v * 2, LOW);
		delay(MAX_DELAY - thr + 1);
#endif
	}
	else
	{
		close(v);
		open(v, thr);
	}
}

void close_(PROG &p){
	if (online) {
		p.stop_time_in_sec = secs_now;
		close(p.velveN);
		p.colosed_for_day = now_t.days;	
		active_prog[p.velveN] = 0;
		p.opened_for_day = now_t.days; // in case it was not opened
	}
	
	
	
}
void open_(PROG &p){

	if (online && !moisture) {
		p.start_time_in_sec = secs_now;
		open(p.velveN, p.throttle);
		active_prog[p.velveN] = &p;
		p.opened_for_day = now_t.days;
	}
	
}
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
bool get_time(TIME& _time, const String t) {//T 3 22:30:44
    bool ret = true;
    _time.days = t[2] - '0';
    ret &= _time.days < 7;
    _time.hours = (t[4] - '0') * 10 + t[5] - '0';
    ret &= _time.hours < 24;
    _time.minutes = (t[7] - '0') * 10 + t[8] - '0';
    ret &= _time.minutes < 60;
    _time.sec = (t[10] - '0') * 10 + t[11] - '0';
    ret &= _time.sec < 60;
  //  if (ret)
   //     Serial.println("get_time OK");
    return ret;
}
//////////////////////////////////////////////////////////

void get_time(TIME&_time){

  const uint32_t t=millis();
  if (t < old_time){
    time_+=4294967296LL;   
  }
  old_time=t; 
  
  const int64_t now=time_ + t;

  if ((now - last_correction_time) >= correction4){
    last_correction_time+=correction4;
    time_+=correction;
  }

  
  _time.sec=(now/1000) % 60;
  _time.minutes=(now/60000) % 60;
  _time.hours = (now /3600000 ) % 24;
  _time.days = (now / 86400000) % 7;

}
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void set_time(const TIME &t){
  time_ = ((uint64_t)t.days)*86400000;
  time_+=((uint64_t)t.hours)*3600000;
  time_+=((uint64_t)t.minutes)*60000;
  time_+=((uint64_t)t.sec)*1000;
  last_correction_time = time_;
  time_-= old_time = millis();

}
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void Print_Time(const TIME& t) {

     Serial.print(d[t.days]);
     Serial.print(" ");
     if (t.hours < 10)
         Serial.print('0');
     Serial.print(t.hours);
     Serial.print(":");
     if (t.minutes < 10)
         Serial.print('0');
     Serial.print(t.minutes);
     Serial.print(":");
     if (t.sec < 10)
         Serial.print('0');
     Serial.println(t.sec);
 
}
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void print_prog(const PROG &prog)
{
	Serial.print(prog.active ? " + " : " - ");
	Serial.print(prog.velveN);
	Serial.print(" ");
	Serial.print(prog.day);
	Serial.print(" o ");
	uint8_t hours = prog.time_2_open / 3600;
	uint8_t minutes = (prog.time_2_open % 3600)/60;
	if (hours < 10)
		Serial.print('0');
	Serial.print(hours);
	Serial.print(":");
	if (minutes < 10)
		Serial.print('0');
	Serial.print(minutes);
	Serial.print(" thr: ");
	Serial.print(prog.throttle);
	Serial.print(" s4o:");
	Serial.print(prog.opened_for_day);
	Serial.print(" closed:");
	hours = prog.time_2_close / 3600;
	minutes = (prog.time_2_close % 3600)/60;
	if (hours < 10)
		Serial.print('0');
	Serial.print(hours);
	Serial.print(":");
	if (minutes < 10)
		Serial.print('0');
	Serial.print(minutes);
	Serial.print(" s4c:");
	Serial.println(prog.colosed_for_day );
}

void print_time(const TIME& t) {
    Serial.print(t.days);
	Serial.print(" ");
    if (t.hours < 10)
        Serial.print('0');
    Serial.print(t.hours);
	Serial.print(":");
    if (t.minutes < 10)
        Serial.print('0');
    Serial.print(t.minutes);
}


int tokens(const uint8_t maxN,String token[], const String delimiter, String inputString)
{
	int pos = 0;
	int i = 0;
	while (i < maxN-1 && (pos = inputString.indexOf(delimiter)) != -1) {
		token[i++] = inputString.substring(0, pos);
	//	Serial.println(token[i - 1]);
		inputString.remove(0, pos + delimiter.length());
	}
	token[i++] = inputString.substring(0, inputString.length()-1);
//	Serial.println(token[i - 1]);
	return i;
}
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//n 0 0 7 22 30 6000 360
void set_prog(String inputString) {
	//Serial.println(t);
	String token[8];
	int i = 0;
	int len = tokens(8, token, " ", inputString);
//	Serial.println(len);
	if (len == 8)
	{
		bool active = true;
		uint32_t opened_time;
		uint32_t hours, minutes;
		active &= (PROGS_AMOUNT > (i = token[1].toInt()));
		active &= (VELVES > (prog[i].velveN = token[2].toInt()));
		active &= (7 >= (prog[i].day = token[3].toInt()));
		active &= (24 > (hours = token[4].toInt()));
		active &= (60 > (minutes = token[5].toInt()));
		active &= (TIME_2_OPEN_MAX >= (prog[i].throttle = token[6].toInt()));	
		active &= (1440 >= (opened_time = token[7].toInt()));	
		prog[i].time_2_open = 3600*hours + 60*minutes;
		prog[i].time_2_close = prog[i].time_2_open + opened_time*60;
		if (prog[i].time_2_close > (86399 - 60))
		{
			active = false;
			Serial.println("ERROR closed in next day");
		}
		prog[i].colosed_for_day = prog[i].opened_for_day = 7;

		prog[i].active = active;
	
		if (active)
		{
			uint8_t* a = (uint8_t*)&prog[i];
			for (int m = 0; m < sizeof(PROG); m++) {
				EEPROM.write(m + i*sizeof(PROG), a[m]);
			}
		}
		if (active)
			Serial.println("Prog added");
	}
}




void setup() {
	offline_cnt = EEPROM.read(OFFLINE);
	this_session_offline_cnt = 0;
    old_time = millis();
    Serial.begin(9600);
    time_=0;
    pinMode(6, OUTPUT);
    pinMode(7, OUTPUT);
    pinMode(8, OUTPUT);
    pinMode(9, OUTPUT);
    for (int i = 0; i < PROGS_AMOUNT; i++) {
        uint8_t* a = (uint8_t*)&prog[i];
        for (int m = 0; m < sizeof(PROG); m++) {
            a[m] = EEPROM.read(m + i * sizeof(PROG));
        }
        prog[i].active = false;
    }


    close2();

  
  
}
bool vm = true;
void test_voltage_and_Moisture()
{
	for (int i = 0; i < 100; i++)
	{
		
		delay(1);
		if (vm)
			volt += ((0.0048875855 * (float)analogRead(A0)) - volt) * 0.1f;
		else
			moisture_mark += (((float)analogRead(A1)) - moisture_mark) * 0.1;
		
		
	}
	vm ^= true;
}
void do_some(String &msg)
{
	switch (msg[0]) {
		
	case 's':
		for (int i = 0; i < VELVES; i++)
		{
			Serial.print(i);
			Serial.print(" ");
			Serial.println(opened_thr[i]);

		}
		break;
	case 'M': 
		{
			//M 600 600
			int l, h;
			l = msg.substring(2, 5).toInt();
			h = msg.substring(6, 9).toInt();
			if (l > 500 && h > l) {
				do_not_water_level_L = l;
				do_water_level_H = h;
			}
			else{
				Serial.println("error");
        Serial.println(l);
        Serial.println(h);
		  }
		}
	case 'm': {
		Serial.print("rain at ");
		Serial.print(do_not_water_level_L);
		Serial.print(", dry at ");
		Serial.print(do_water_level_H);
		Serial.print(", m = ");
		Serial.println(moisture_mark);
			break;
		}	
	case 't':
	case 'T': //T 3 22:30:44
		{
			TIME t;
			if (get_time(t, msg)) {
				set_time(t);
				get_time(now_t);
				Serial.println("TIME SET UP");
			}
			break;
		}
	case 'n':
	case 'N'://N 0 0 7 22 30 6000 60
		{
			set_prog(msg);
			break;
		}
	case 'a':
	case 'A': //A 10
		{
			int i = msg.substring(2).toInt();
			if (i < PROGS_AMOUNT) {
				prog[i].active = true;
				prog[i].colosed_for_day = prog[i].opened_for_day = 7;
				Serial.println("PROG ACTIVATED");
			}
			break;
		}
	case 'd':
	case 'D': //D 00
		{
			int i = msg.substring(2).toInt();
			if (i < PROGS_AMOUNT) {
				prog[i].active = false;
				Serial.println("PROG DEACTIVATED");
			}
			break;
		}
	case 'L':
		for (int i = 0; i < LOG_SIZE; i++)
		{
			const LOGS &p = logs[(logsI - i - 1)&(LOG_SIZE - 1)];
			Serial.print(p.velve);
			Serial.print(p.open?" opened ":" closed ");
			Serial.print(p.day);
			Serial.print(" ");
			if (p.h < 10)
				Serial.print('0');
			Serial.print(p.h);
			Serial.print(":");
			if (p.m < 10)
				Serial.print('0');
			Serial.println(p.m);
		}
		break;
	case 'l':
		{
			for (int i = 0; i < PROGS_AMOUNT; i++)
			{
				Serial.print("N ");
				Serial.print(i);
				print_prog(prog[i]);
			}
			break;
		}
	case 'v':
	case 'V':
		Serial.println(volt);
		break;

	case 'o':
	case 'O':
		{
			Serial.print("all:");
			Serial.print(offline_cnt);
			Serial.print(" ");
			Serial.println(this_session_offline_cnt);
			break;
		}
	case 'g':
		print_time(now_t);
		Serial.println();
		break;
	case 'G': {
			Print_Time(now_t);
			Serial.println();
			break;
		}
	case '1': 
		{
			//1 00 5000
			uint8_t velve = msg[2] - '0';
			int i = 4;
			if (msg[3] != ' ') {
				velve = velve * 10 + msg[3] - '0';
				i++;
			}
			const uint16_t t = msg.substring(i).toInt();
			open(velve, t);
			break;
		}
	case '0':  //0 00
		uint8_t velve = msg[2] - '0';
		if (msg[3] != ' ') {
			velve = velve * 10 + msg[3] - '0';
		}
		close(velve);
		break;
	}

}



void test_and_do(PROG &p)
{
	
	if (p.active  && (p.day > 6 || p.day == now_t.days))
	{
		if (p.opened_for_day != now_t.days && p.time_2_open < secs_now)
		{
			
			open_(p);
		}

		if (p.colosed_for_day != now_t.days && p.time_2_close < secs_now)
		{

			close_(p);		  
		}
	}
}




void loop() {

  // put your main code here, to run repeatedly:
	get_time(now_t);
	secs_now = (uint32_t)now_t.hours * 3600 + (uint32_t)now_t.minutes * 60 + now_t.sec;

	test_voltage_and_Moisture();
	//Serial.println(moisture_mark);
	  if (online && volt < 4.3) {
		  online = false;
		  print_time(now_t);
		  Serial.println(" OFFLINE");
		  EEPROM.write(OFFLINE, ++offline_cnt);
		  this_session_offline_cnt++;
	  }
	if (!online && volt > 4.6) {
		online = true;
		print_time(now_t);
		Serial.println(" ONLINE");
	}
	if (do_not_water_level_L > moisture_mark)
	{
		if (!moisture)
		{
			Serial.println("REIN");
			moisture = true;
		}
	}
	else if (do_water_level_H < moisture_mark){
		if (moisture) 
		{
			moisture = false;
			Serial.println("DRY");
		}
	}

	if (Serial.available()) {
		String msg = Serial.readString();
		do_some(msg);
	}
	
	if (online)
	{
		for (int i = 0; i < VELVES; i++)
		{
			if (active_prog[i] && active_prog[i]->start_time_in_sec > secs_now)
			{
				close(i);
				active_prog[i] = 0;
			}
		
			if (moisture)
			{
				if (active_prog[i])
					active_prog[i]->opened_for_day = 7;
				close(i);
			}
		}
	}
  for (int i=0; i< PROGS_AMOUNT; i++){
	  test_and_do(prog[i]);
  }
}
