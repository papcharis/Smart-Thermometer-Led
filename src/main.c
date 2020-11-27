#include <platform.h>
#include <delay.h>
#include <stdio.h>
#include <gpio.h>
#include <leds.h>
#include <lcd.h>
#include <i2c.h>

//Pins define
#define TRIGGER_PIN PA_0
#define ECHO_PIN PA_1
#define TEMP_PIN PA_4

//Critical temperature and distance
#define TEMP_LOW  20;
#define TEMP_HIGH  30;
#define DIST  15;

// ARM_CM  to calculate number of clock cycles
#define  ARM_CM_DEMCR      (*(uint32_t *)0xE000EDFC)
#define  ARM_CM_DWT_CTRL   (*(uint32_t *)0xE0001000)
#define  ARM_CM_DWT_CYCCNT (*(uint32_t *)0xE0001004)


uint16_t distance;
uint32_t distance_time;


uint8_t presence;
uint8_t temp_byte1;
uint8_t temp_byte2;
float temp;
float temperature;

float temperaturesArray[24];
int  temperaturesArrayIdx  = 0;
float avgTemperature ;
char * temperatureState ;
int switchButton ;


//Volatile variables changed at the execution time
volatile int read_temp;                       //Interrupt -> Read Temperature every 5 seconds
volatile int new_sec;                         //Interrupt -> Update the LCD Screen every second
volatile int t_dist;                          //Interrupt -> Calculate the distance every 200ms
volatile int count_secs=0;                      //Interrupt -> Auxilary variable for calculating the above




//Function decleration
void init_timer();
uint32_t distance_read();
float read_temperature();
uint8_t temperature_start(void);
void temperature_write(uint8_t data);
uint8_t temperature_read(void);
void my_isr(void);
void updateLCD(int message);




int main(void){

	leds_init();
	lcd_init();
	adc_init(TEMP_PIN);
	timer_init(200000);
	timer_set_callback(my_isr);
	timer_enable();
	gpio_set_mode(TRIG_PIN, Output);
	gpio_set_mode(ECHO_PIN, Input);

	while(1){


	//Get distance value every 200ms
	if(t_dist ==1){
		t_dist = 0;
		distance_time = distance_read();
		distance = sensor_time_read * .034/2;
	}

	//Get temperature value every 5s
	if(read_temp){
		presence = temperature_start();
		delay_ms(1);
		temperature_write(0xCC);
		temperature_write(0x44);
		delay_ms(1);

		presence = temperature_start();
		delay_ms(1);
		temperature_write(0xCC);
		temperature_write(0xBE);

		temp_byte1 = temperature_read();
		temp_byte2 = temperature_read();


		temp = (temp_byte2<<8) | temp_byte1;
		temperature = (float)temp / 16;

		temperaturesArray[temperaturesArrayIdx]= temperature;
		if(temperaturesArrayIdx == 23){
			avgTemperature =  0 ;
      //calculate the average temperature
			for(int i = 0 ; i < 24 ; i ++){
				avgTemperature += temperaturesArray[i];
			}
			avgTemperature = avgTemperature/24;
			temperaturesArrayIdx = 0;
		}
		else{
			temperaturesArrayIdx++;
		}

	}

  //Check for High/Low Temperatures
if(temperature > TEMP_HIGH){
	leds_set(1,0,0);
	temperatureState = 'Overheating' ;
	switchButton = 1 ;

}
else if (temperature < TEMP_LOW){
	leds_set(0,0,1);
	temperatureState = 'Underheating' ;
	switchButton  = 0 ;

}
else{
	leds_set(0,1,0);
	temperatureState = 'Normal Temperature' ;
  switchButton = 0 ;
}

	//Find the message to be printed
	if (distance <= DIST){                                        //If user close to device
		message = 1 ;
	}
	else if(distance > DIST && count_temps>=2 && count_temps<24){  //Print LEDs' state (main LCD message)
	  message = 4;
	}
	else if(distance > DIST && count_temps<=2 && mean_temp==0){   //Wait for the first mean temp to be calculated
   message = 4 ;
  }
	else if(distance < DIST  && mean_temp == 0  ){
    message = 3 ;
	}
	else{                                                         //New mean temp is calculated, print for 10 secs
   message = 2;
	}
	}

}



void updateLCD(int message){

	case 1:
		char str[20] = {0};
		char str2[20] = {0};
		lcd_set_cursor(0,1);
		sprintf(str, "Temperature: %.2f ",temperature);
		lcd_print(str);
		lcd_print("C");
		lcd_set_cursor(0,0);
		sprintf(str2, "Mean Temperature:%.2f",avgTemperature);
		lcd_print(str2);
		lcd_print("C");
		lcd_set_cursor(15,1);
		lcd_print(temperatureState);
			 break;

	 case 2:
			lcd_clear();
			char str[20] = {0};
			lcd_set_cursor(0,0);
			sprintf(str,"Mean Temperature:%.2f ",avgTemperature);
			lcd_print(str);
			lcd_print("C");
			lcd_set_cursor(0,1);
			lcd_print(temperatureState);
			break;

		case 3:
		lcd_clear();
		lcd_set_cursor(0,0);
		temperatureState ='Wait for first measurment';
		lcd_print(temperatureState);

		 break;

		default:
		lcd_clear();
		lcd_set_cursor(0,0);
		lcd_print(temperatureState);



}


void my_isr(void)
{
	count_secs = count_secs+1;
	if (count_secs == 25){
		read_temp=1;
		count_secs = 0;
	}
	if (count_secs%5==0){
		new_sec=1;
	}
	t_dist = 1;
}


void init_timer(){
	if (ARM_CM_DWT_CTRL != 0) {      // See if DWT is available
		ARM_CM_DEMCR      |= 1 << 24;  // Set bit 24

		ARM_CM_DWT_CYCCNT = 0;

		ARM_CM_DWT_CTRL   |= 1 << 0;   // Set bit 0
	}
}


float read_temperature(){

  uint16_t res = (int)adc_read(TEMP_PIN);
	float voltage =  res * 3300 / 4096 ;
	float Temperature = (voltage-500)/10;
	return Temperature ;
}


uint32_t distance_read() {
	init_timer();
	uint32_t start;
	uint32_t end;
	uint32_t total_cycles;
	uint32_t total_time;

	uint32_t time=0;

	gpio_set(TRIG_PIN, 1);
	delay_us(10);
	gpio_set(TRIG_PIN, 0);

	while(!(gpio_get(ECHO_PIN)));

//ena apo ta 2 den prepei na xreiazete
	start = ARM_CM_DWT_CYCCNT;

	while(gpio_get(ECHO_PIN));

	end = ARM_CM_DWT_CYCCNT;
	total_cycles = end - start;
	total_time = total_cycles / (SystemCoreClock * 1e-6);

	return 2*total_time;
}


uint8_t temperature_start(void) {
	uint8_t response = 0;
	gpio_set_mode(TEMP_PIN, Output);
	gpio_set(TEMP_PIN, 0);
	delay_us(480);

	gpio_set_mode(TEMP_PIN, Input);
	delay_us(80);

	if(!(gpio_get(TEMP_PIN))){
		response = 1;
		printf(" >> temperature_start -> pulse detected \n");
	}
	else{
		response = -1;
	}
	delay_us(400);

	return response;
}

void temperature_write(uint8_t data) {

	gpio_set_mode(TEMP_PIN, Output);

	for(int i=0; i<8; i++) {
		if((data & (1<<i)) !=0 ){
			gpio_set_mode(TEMP_PIN, Output);
			gpio_set(TEMP_PIN, 0);
			delay_us(1);

			gpio_set_mode(TEMP_PIN, Input);
			delay_us(40);
		}
		else {
			gpio_set_mode(TEMP_PIN, Output);
			gpio_set(TEMP_PIN, 0);
			delay_us(40);

			gpio_set_mode(TEMP_PIN, Input);

		}
	}
}

uint8_t temperature_read(void) {

	uint8_t value = 0;
	gpio_set_mode(TEMP_PIN, Input);

	for(int i=0; i<8; i++) {
		gpio_set_mode(TEMP_PIN, Output);
		gpio_set(TEMP_PIN, 0);
		delay_us(2);

		gpio_set_mode(TEMP_PIN, Input);
		if(gpio_get(TEMP_PIN)) {
			value |= 1<<i;
		}

		delay_us(60);
	}

	return value;
}
