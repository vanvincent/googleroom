#include <avr/io.h>
#include <stdio.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <string.h>

#include <groom/button.h>

#include "groom/usart.h"
#include "groom/com.h"


#define FOSC 9830400            // Clock frequency = Oscillator freq.
#define BAUD 9600               // UART0 baud rate
#define MYUBRR FOSC/16/BAUD-1   // Value for UBRR0 register
#define BDIV (FOSC / 100000 - 16) / 2 + 1    // Puts I2C rate just below 100kHz

volatile int i=0;
volatile uint8_t buffer[20];
volatile uint8_t StrRxFlag=0;
volatile char c='0';
char buf[256]; //string for thermometer
volatile uint8_t interruptstate=0; //interrupt state
volatile uint8_t buttonstate=0;

//Output Relay Swap Buffers
int thermoBuf[3] = {0, 0, 0};
int lightBuf[3] = {0, 0, 0};
int blindBuf[3] = {0, 0, 0};
int *currentBuf = thermoBuf;
//


//I/O init
void io_pin_init()
{
  usart_outstring("Initializing I/O pins...\r\n");

  DDRC |= 1 << DDC0;          // Set PORTC bit 0 for output
  DDRC |= 1 << DDC3;          // Set PORTC bit 3 for output
  DDRC |= 1 << DDC4;          // Set PORTC bit 4 for output
  DDRC |= 1 << DDC5;          // Set PORTC bit 5 for output
  
  
  DDRC = 0xff;
  
  DDRB |= 1 << DDB1;
  DDRB |= 1 << DDB2;
  DDRC |= 1 << DDC2;    
}

//adc 
void adc_init(void){ 
  DDRC &= ~(1 << DDC1);

	ADCSRA |= ((1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0));    //16Mhz/128 = 125Khz the ADC reference clock
	ADMUX |= (1<<REFS0);                //Voltage reference from Avcc (5v)
	ADCSRA |= (1<<ADEN);                //Turn on ADC
	ADCSRA |= (1<<ADSC);                //Do an initial conversion because this one is the slowest and to ensure that everything is up and running
}
uint16_t read_adc(uint8_t channel){
	ADMUX &= 0xF0;                    //Clear the older channel that was read
	ADMUX |= channel;                //Defines the new ADC channel to be read
	ADCSRA |= (1<<ADSC);                //Starts a new conversion
	while(ADCSRA & (1<<ADSC));            //Wait until the conversion is done
	return ADCW;                    //Returns the ADC value of the chosen channel
}


static uint8_t button_pressed = 0;
static uint8_t button_pressed_stack = 0;

void button_init(void)
{
	/* set pin C1 as an input */
	DDRB &= ~(1 << 0);
	
	PORTC |= 1 << PC3;      // Set PC3 to a 1                                                                            
	/* pull C1 up */
	PORTB |= (0 << 0);
	
	/* enable pin change interrupt */
	PCICR |= (1 << PCIE0);
	PCMSK0 |= (1 << PCINT0);
}

uint8_t button_was_pressed(void)
{
	if (button_pressed_stack) {
		button_pressed_stack = 0;
		return 1;
	}
	
	return 0;
}

uint8_t button_val(void)
{
	if (PINB & (1 << 0)) {
		return 1;
	}
	
	return 0;
}



//Buffer Output Swapping
void buffersUpdated()
{
  showRelayBuffer(currentBuf);
}

void relaysOff()
{
  PORTC &= ~(1 << PC3);  //Set PC3 to 0
  PORTC &= ~(1 << PC4);  //Set PC4 to 0
  PORTC &= ~(1 << PC5);  //Set PC5 to 0
}

void showRelayBuffer(int buf[3])
{
  relaysOff();

  if(buf == NULL)
  {
    return;
  }

  PORTC &= ~(buf[0] << PC3);
  PORTC &= ~(buf[1] << PC4);
  PORTC &= ~(buf[2] << PC5);

  PORTC |= (buf[0] << PC3);
  PORTC |= (buf[1] << PC4);
  PORTC |= (buf[2] << PC5);
}

void showNone()
{
  currentBuf = NULL;
  buffersUpdated();
}

void showThermo()
{
  currentBuf = thermoBuf;
  buffersUpdated();
}

void showBlinds()
{
  currentBuf = blindBuf;
  buffersUpdated();
}

void showLights()
{
  currentBuf = lightBuf;
  buffersUpdated();
}
//===

//LED control
void redLedOn()
{
  PORTB |= 1 << PB1;
}

void greenLedOn()
{
  PORTB |= 1 << PB2;
}

void blueLedOn()
{
  PORTC |= 1 << PC2;
}

void allLedOff()
{
  PORTB &= ~(1 << PB1);
  PORTB &= ~(1 << PB2);
  PORTC &= ~(1 << PC2);
}
//

ISR(PCINT0_vect)
{
	
	if (button_val() == 0) {
		button_pressed = 0;
	}
	else {
		button_pressed = 1;

		switch(buttonstate)
		{
		default:
		  usart_printf("unknown buttonstate: %d", buttonstate);
		case 0:
		  allLedOff();
		  showNone();
		  break;
		case 1:
		  allLedOff();
		  redLedOn();
		  showThermo();
		  break;
		case 2:
		  allLedOff();
		  greenLedOn();
		  showLights();
		  break;
		case 3:
		  allLedOff();
		  blueLedOn();
		  showBlinds();
		  break;
		}

		buttonstate = buttonstate + 1;
		if(buttonstate == 4)
		{
		  buttonstate = 0;
		}
	}
	
	//usart_printf("INTERRUPTED thing buttonpressed = %d, button_val = %d, buttonstate = %d \r\n", button_pressed, button_val(), buttonstate);

}

void relayInit()
{
  currentBuf = NULL;
  buttonstate = 0;
  buffersUpdated();
  allLedOff();
}

//Thermostat controls
//PC5: Cool - Blue Wire - Light Three
//PC4: Heat - Green Wire - Light Two
//PC3: Fan - Yellow Wire - Light One
//
void thermo_fan_on()
{
  usart_printf("Turning fan on\r\n");
  //PORTC |= (1 << PC3);      // Set PC3 to a 1
  thermoBuf[0] = 1;
  buffersUpdated();
}

void thermo_fan_off()
{
  usart_printf("Turning fan off\r\n");
  //PORTC &= ~(1 << PC3);  //Set PC3 to 0
  thermoBuf[0] = 0;
  buffersUpdated();
}

void thermo_turn_off()
{
  usart_printf("Turning all thermostat systems off\r\n");
  //PORTC &= ~(1 << PC4);  //Set PC4 to 0
  //PORTC &= ~(1 << PC5);  //Set PC5 to 0
  thermoBuf[1] = 0;
  thermoBuf[2] = 0;
  buffersUpdated();
}

void thermo_call_for_heat()
{
  thermo_turn_off();
  usart_printf("Turning heat on\r\n");
  //PORTC |= (1 << PC4);  //Set PC4 to 1
  thermoBuf[1] = 1;
  buffersUpdated();
}

void thermo_call_for_cool()
{
  thermo_turn_off();
  usart_printf("Turning cool on\r\n");
  //PORTC |= (1 << PC5);  //Set PC5 to 1
  thermoBuf[2] = 1;
  buffersUpdated();
}

//Light controls
void lights_off()
{
  usart_printf("Turning lights off\r\n");
  lightBuf[0] = 0;
  lightBuf[1] = 0;
  lightBuf[2] = 0;
  buffersUpdated();
}

void lights_full_power()
{
  usart_printf("Turning lights on full power\r\n");
  lightBuf[0] = 1;
  lightBuf[1] = 1;
  lightBuf[2] = 1;
  buffersUpdated();
}

void lights_low_power()
{
  usart_printf("Turning lights on low power\r\n");
  lightBuf[0] = 1;
  lightBuf[1] = 0;
  lightBuf[2] = 0;
  buffersUpdated();
}

//Blind Controls
void blinds_up()
{
  usart_printf("Putting blinds up\r\n");
  blindBuf[0] = 0;
  blindBuf[1] = 1;
  blindBuf[2] = 0;
  buffersUpdated();
}

void blinds_down()
{
  usart_printf("Putting blinds down\r\n");
  blindBuf[0] = 0;
  blindBuf[1] = 0;
  blindBuf[2] = 1;
  buffersUpdated();
}

void blinds_stop()
{
  usart_printf("Stopping blinds\r\n");
  blindBuf[0] = 1;
  blindBuf[1] = 0;
  blindBuf[2] = 0;
  buffersUpdated();
}


// data sender general
void senddata(){
	
	uint16_t result = read_adc(1);	
	char buf[128];
	sprintf(buf, "%d\r", result);
	usart_outstring(buf);
	c=DEFAULT;
	
}

//write the control you want to do reacting to commands form master in this function
void control(char *command){
  unsigned int numCommands = strlen(command);
  int i = 0;

  for(i = 0; i < numCommands; i++)
  {
    char cmd = command[i];
    usart_out(cmd);
    switch(cmd)
    {
    case HEAT_ON:
      thermo_call_for_heat();
      break;
    case HEAT_OFF:
      thermo_turn_off();
      break;
    case COOL_ON:
      thermo_call_for_cool();
      break;
    case COOL_OFF:
      thermo_turn_off();
      break;
    case FAN_ON:
      thermo_fan_on();
      break;
    case FAN_OFF:
      thermo_fan_off();
      break;
    case BLINDS_UP:
      blinds_up();
      break;
    case BLINDS_DOWN:
      blinds_down();
      break;
    case LIGHTS_FULL:
      lights_full_power();
      break;
    case LIGHTS_HALF:
      lights_low_power();
      break;
    case LIGHTS_OFF:
      lights_off();
      break;
    default:
      //usart_printf("Invalid command character received: %d from command string: %s", cmd, command);
      break;
    }
  }
}

void receivecommand(){

	int count=0;
	
	while (1) {
		_delay_ms(10);
		count++;
		if(StrRxFlag || count>500){    //time_out
			if(StrRxFlag){
            	StrRxFlag=0;                // Reset String received flag
				count=0;
			}else{
				sprintf(buffer,"TIME_OUT");
				count=0;
				i=0;  
				interruptstate=COMMAND_MODE;     //Reset buffer index
			}
			break;
		}
	}
	
	control(buffer);	
} 

int main(void)
{
  int count = -1;

  /* for 9600 baud on with 9.304MHz clock */
  usart_init();
  io_pin_init();
  /* adc_init must happen after io_pin_init because reasons */
  adc_init();
  button_init();
  relayInit();
  
  /*while(1)
  {
    if(command == 0) { allLedOff(); }
    else if(command == 1) { allLedOff(); redLedOn(); }
    else if(command == 2) { allLedOff(); greenLedOn(); }
    else if(command == 3) { allLedOff(); blueLedOn(); }

    command++;
    if(command == 4) { command = 0; }

    _delay_ms(500);
    }*/


  while(1)
  {
    
    switch(interruptstate){
      case TRANSMIT_MODE:
        receivecommand();
        break;
      default:
        break;
    }
    

    if(count % 2 == 0) 
    {
      PORTC |= 1 << PC0;   
    }
    else 
    {
      PORTC &= ~(1 << PC0);
    }

    switch(count)
    {
    case 0:
      lights_low_power();
      thermo_call_for_cool();
      blinds_up();
      break;
    case 1:
      lights_off();
      thermo_fan_on();
      blinds_stop();
      break;
    case 2:
      lights_full_power();
      thermo_call_for_heat();
      blinds_down();
      break;
    case 3:
      lights_off();
      thermo_fan_off();
      blinds_stop();
      break;
    default:
    case 4:
      lights_off();
      thermo_turn_off();
      blinds_stop();
      break;
    case -1:
      //Do Nothing
      break;
    }


    if(count != -1)
    {
      count = count + 1;
      if(count == 5)
      {
	count = 0;
      }
    }

  } 
    
}

//interrpt handler
ISR(USART_RX_vect)
{
	switch(interruptstate){
		case COMMAND_MODE:
			while (!(UCSR0A & (1<<RXC0)));
			c=UDR0; 
			//Read USART data register
			switch (c) {
				case HB_BETA:
					usart_out(ACTIVE_RESPONSE);  //no motion detected
					break;
				case READ_BETA:
					senddata();   //receive data request
					break;
				case SEND_BETA:
					interruptstate=1;
					usart_out(ACK);
				    break;
				default:
					break;
			}
			break;
		case TRANSMIT_MODE:
			while (!(UCSR0A & (1<<RXC0)));
			buffer[i]=UDR0;         //Read USART data register
			if(buffer[i++]=='\r')   //check for carriage return terminator and increment buffer index
			{
				// if terminator detected
				StrRxFlag=1;        //Set String received flag
				buffer[i-1]=0x00;   //Set string terminator to 0x00
				i=0;  
				interruptstate=COMMAND_MODE;              //Reset buffer index
			}
			break;	
	}
	
}
