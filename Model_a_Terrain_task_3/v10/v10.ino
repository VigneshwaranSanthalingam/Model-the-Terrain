#define __OPTIMISE__ -O0
#define F_CPU 14745600
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include <math.h> //included to support power function
#include "lcd.c"
#include <Servo.h>

Servo myservo;  // create servo object to control a servo
// twelve servo objects can be created on most boards

int pos = 0;    // variable to store the servo position


#define THRESHOLD 0x10 //   set threshold for White_line_Sensor
#define V_MAX 150
#define V_MIN 50
#define V_SOFT 120
#define NODE_1 (Left_white_line>THRESHOLD && Center_white_line>THRESHOLD && Right_white_line<THRESHOLD)
#define NODE_2 (Left_white_line<THRESHOLD && Center_white_line>THRESHOLD && Right_white_line>THRESHOLD)
#define NODE_3 (Left_white_line>THRESHOLD && Center_white_line>THRESHOLD && Right_white_line>THRESHOLD)
#define PATH (Left_white_line<THRESHOLD && Center_white_line>THRESHOLD && Right_white_line<THRESHOLD)
#define NODE (NODE_1||NODE_2||NODE_3)
#define WHITE (Left_white_line<THRESHOLD && Center_white_line<THRESHOLD && Right_white_line<THRESHOLD)
#define LEFT_DEVIATION (Right_white_line>THRESHOLD)
#define RIGHT_DEVIATION (Left_white_line>THRESHOLD)
#define OBJECT_DIS 38


unsigned char ADC_Conversion(unsigned char);
unsigned char ADC_Value;
unsigned char sharp, distance, adc_reading;
unsigned int value;
unsigned char flag = 0;
unsigned char Left_white_line = 0;
unsigned char Center_white_line = 0;
unsigned char Right_white_line = 0;
int x,y,p,q,l,r,SHARP_IR;
volatile unsigned long int ShaftCountLeft = 0;
volatile unsigned long int ShaftCountRight = 0;
int flag_NODE;
//Function to configure LCD port
void lcd_port_config (void)
{
 DDRC = DDRC | 0xF7; //all the LCD pin's direction set as output
 PORTC = PORTC & 0x80; // all the LCD pins are set to logic 0 except PORTC 7
}

//ADC pin configuration
void adc_pin_config (void)
{
 DDRF = 0x00; 
 PORTF = 0x00;
 DDRK = 0x00;
 PORTK = 0x00;
}

//Function to configure ports to enable robot's motion
void motion_pin_config (void) 
{
 DDRA = DDRA | 0x0F;
 PORTA = PORTA & 0xF0;
 DDRL = DDRL | 0x18;   //Setting PL3 and PL4 pins as output for PWM generation
 PORTL = PORTL | 0x18; //PL3 and PL4 pins are for velocity control using PWM.
}


void timer5_init()
{
  TCCR5B = 0x00;  //Stop
  TCNT5H = 0xFF;  //Counter higher 8-bit value to which OCR5xH value is compared with
  TCNT5L = 0x01;  //Counter lower 8-bit value to which OCR5xH value is compared with
  OCR5AH = 0x00;  //Output compare register high value for Left Motor
  OCR5AL = 0xFF;  //Output compare register low value for Left Motor
  OCR5BH = 0x00;  //Output compare register high value for Right Motor
  OCR5BL = 0xFF;  //Output compare register low value for Right Motor
  OCR5CH = 0x00;  //Output compare register high value for Motor C1
  OCR5CL = 0xFF;  //Output compare register low value for Motor C1
  TCCR5A = 0xA9;  /*{COM5A1=1, COM5A0=0; COM5B1=1, COM5B0=0; COM5C1=1 COM5C0=0}
            For Overriding normal port functionality to OCRnA outputs.
              {WGM51=0, WGMVELOCITY_MIN=1} Along With WGM52 in TCCR5B for Selecting FAST PWM 8-bit Mode*/
  
  TCCR5B = 0x0B;  //WGM12=1; CS12=0, CS11=1, CS10=1 (Prescaler=64)
}

/*
 * Function Name: void left_encoder_pin_config (void)
 * Input : void
 * Output : void
 * Logic: Function to configure INT4 (PORTE 4) pin as input for the left position encoder
 * PE4 (INT4): External interrupt for left motor position encoder
 
 * Example Call: left_encoder_pin_config();
 */
void left_encoder_pin_config (void)
{
  DDRE  = DDRE & 0xEF;  //Set the direction of the PORTE 4 pin as input
  PORTE = PORTE | 0x10; //Enable internal pull-up for PORTE 4 pin
}

/*
 * Function Name: void right_encoder_pin_config (void)
 * Input : void
 * Output : void
 * Logic: Function to configure INT5 (PORTE 5) pin as input for the right position encoder
 * PE5 (INT5): External interrupt for the right position encoder
 
 * Example Call: right_encoder_pin_config();
 */
void right_encoder_pin_config (void)
{
  DDRE  = DDRE & 0xDF;  //Set the direction of the PORTE 5 pin as input
  PORTE = PORTE | 0x20; //Enable internal pull-up for PORTE 5 pin
}

//Function to Initialize PORTS
void port_init()
{
  lcd_port_config();
  adc_pin_config();
  timer5_init();
  motion_pin_config();
  left_encoder_pin_config();
  right_encoder_pin_config();
}

/*
 * Function Name: void left_position_encoder_interrupt_init (void)
 * Input : void
 * Output : void
 * Logic: Function to enable Interrupt 4 (INT4)
 * Example Call: left_position_encoder__interrupt_init();
 */
void left_position_encoder_interrupt_init (void)
{
  cli(); //Clears the global interrupt
  EICRB = EICRB | 0x02; // INT4 is set to trigger with falling edge
  EIMSK = EIMSK | 0x10; // Enable Interrupt INT4 for left position encoder
  sei();   // Enables the global interrupt
}

/*
 * Function Name: void right_position_encoder_interrupt_init (void)
 * Input : void
 * Output : void
 * Logic: Function to enable Interrupt 5 (INT5)
 * Example Call: right_position_encoder__interrupt_init();
 */
void right_position_encoder_interrupt_init (void)
{
  cli(); //Clears the global interrupt
  EICRB = EICRB | 0x08; // INT5 is set to trigger with falling edge
  EIMSK = EIMSK | 0x20; // Enable Interrupt INT5 for right position encoder
  sei();   // Enables the global interrupt
}

//ISR for right position encoder
ISR(INT5_vect)
{
  ShaftCountRight++;  //increment right shaft position count
}

//ISR for left position encoder
ISR(INT4_vect)
{
  ShaftCountLeft++;  //increment left shaft position count
}

void adc_init()
{
  ADCSRA = 0x00;  
  ADCSRB = 0x00;    //MUX5 = 0
  ADMUX = 0x20;   //Vref=5V external --- ADLAR=1 --- MUX4:0 = 0000
  ACSR = 0x80;
  ADCSRA = 0x86;    //ADEN=1 --- ADIE=1 --- ADPS2:0 = 1 1 0
}


void init_devices (void)
{
  cli();   //Clears the global interrupts
  port_init();
  adc_init();
  left_position_encoder_interrupt_init();
  right_position_encoder_interrupt_init();
  sei();   //Enables the global interrupts
}

unsigned char ADC_Conversion(unsigned char Ch) 
{
  unsigned char a;
  if(Ch>7)
  {
    ADCSRB = 0x08;
  }
  Ch = Ch & 0x07;       
  ADMUX= 0x20| Ch;        
  ADCSRA = ADCSRA | 0x40;   //Set start conversion bit
  while((ADCSRA&THRESHOLD)==0);  //Wait for conversion to complete
  a=ADCH;
  ADCSRA = ADCSRA|THRESHOLD; //clear ADIF (ADC Interrupt Flag) by writing 1 to it
  ADCSRB = 0x00;
  return a;
}

//Function To Print Sesor Values At Desired Row And Coloumn Location on LCD
void print_sensor(char row, char coloumn,unsigned char channel)
{
    ADC_Value = ADC_Conversion(channel);
  lcd_print(row, coloumn, ADC_Value, 3);
}

void getinput_sharp()
{
  sharp = ADC_Conversion(9);           //Stores the Analog value of front sharp connected to ADC channel 11 into variable "sharp"
  value = Sharp_GP2D12_estimation(sharp);       //Stores Distance calsulated in a variable "value".
  
  lcd_wr_command(0x01);
  lcd_print(1,1,value,3);
}

unsigned int Sharp_GP2D12_estimation(unsigned char adc_reading)
{
  float distance;
  unsigned int distanceInt;
  distance = (int)(10.00*(2799.6*(1.00/(pow(adc_reading,1.1546)))));
  distanceInt = (int)distance;
  if(distanceInt>800)
   {
     distanceInt=800;
   }
  return distanceInt;
}

void motion_set (unsigned char Direction)
{
  unsigned char PortARestore = 0;

  Direction &= 0x0F;    // removing upper nibbel for the protection
  PortARestore = PORTA;     // reading the PORTA original status
  PortARestore &= 0xF0;     // making lower direction nibbel to 0
  PortARestore |= Direction; // adding lower nibbel for forward command and restoring the PORTA status
  PORTA = PortARestore;     // executing the command
}

void forward (void) //both wheels forward
{
  motion_set(0x06);
}

void stop (void) //both wheels backward
{
  motion_set(0x00);
}

void left (void) //Left wheel backward, Right wheel forward
{
  motion_set(0x05);
}

void right (void) //Left wheel forward, Right wheel backward
{
  motion_set(0x0A);
}

void velocity (unsigned char left_motor, unsigned char right_motor)
{
  OCR5AL = (unsigned char)left_motor;
  OCR5BL = (unsigned char)right_motor;
}
  
  
void setup() 
{
  DDRK  = DDRK | 0xF7; //set PJ3 as input for color sensor output
  PORTK = PORTK | 0x08;//Enable internal pull-up for PORTJ 3 pin
 
  init_devices();
  lcd_set_4bit();
  lcd_init();
  
  myservo.attach(11);  // attaches the servo on pin 9 to the servo object
}


void getinput_WL()
{
    Left_white_line = ADC_Conversion(3);  //Getting data of Left WL Sensor
    Center_white_line = ADC_Conversion(2);  //Getting data of Center WL Sensor
    Right_white_line = ADC_Conversion(1);   //Getting data ofRight WL Sensor

    print_sensor(1,1,3);  //Prints value of White Line Sensor1
    print_sensor(1,5,2);  //Prints Value of White Line Sensor2
    print_sensor(1,9,1);
 }
 
 void follow_blk(unsigned char speed)
{
  while(1)
  {
    getinput_WL();
    if (Center_white_line>0x10 && Left_white_line>0x10 && Right_white_line>0x10)
    {
        flag_NODE = 1;
      ShaftCountLeft = 0;
      ShaftCountRight = 0;  
      break;      
    }
    
    if(Center_white_line>0x10)
    {
      forward();
      velocity(speed,speed);
    }

    if(Left_white_line<0x10 && Center_white_line<0x10)
    {
      forward();
      velocity(speed+20,speed-30);
    }

    if(Right_white_line<0x10 && Center_white_line<0x10)
    {
      forward();
      velocity(speed-30,speed+20);
    }
  }
}

void loop()
{
   follow_blk(150);

   lcd_print(1,5,flag_NODE,1);
    
     if ((ShaftCountLeft>=(OBJECT_DIS-2))&&(ShaftCountLeft<=(OBJECT_DIS+2))&&(flag_NODE == 1))
     {  
       myservo.write(0);
       getinput_sharp();
       if(value<100)
        { 
        stop();
        lcd_wr_command(0x01);
        lcd_string("object");
        delay(2000);
        }
        myservo.write(180);
        getinput_sharp();
        if(value<100)
        { 
        stop();
        lcd_wr_command(0x01);
        lcd_string("object");
        delay(2000);
        }
     }
     if ((ShaftCountLeft>(OBJECT_DIS+2))&&(flag_NODE = 1))
     {
       flag_NODE = 0;
     }  
       
 }

