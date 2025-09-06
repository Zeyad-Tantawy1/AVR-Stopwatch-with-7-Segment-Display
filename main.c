#include <avr/io.h>
#include <avr/interrupt.h>

#define MAX_SECONDS 60
#define MAX_MINUTES 60
#define MAX_HOURS   24

#define COUNT_UP   1
#define COUNT_DOWN 0
#define MULTI_SEG_COUNT 6

typedef unsigned char uint8;

// Global variables
volatile uint8 hours = 0;
volatile uint8 minutes = 0;
volatile uint8 seconds = 0;

volatile uint8 running = 1;
volatile uint8 countMode = COUNT_UP;
volatile uint8 tick = 0;
volatile uint8 modeBtnFlag = 0;

volatile uint8 currentDigit = 0;

// flags for buttons
unsigned char hour_inc_flag=0, hour_dec_flag=0;
unsigned char min_inc_flag=0,  min_dec_flag=0;
unsigned char sec_inc_flag=0,  sec_dec_flag=0;

// lookup table for BCD
const uint8 digit_to_bcd[10] = {0,1,2,3,4,5,6,7,8,9};

// Timer + INT
void timer1CTC_init(void) {
    TCCR1A = 0;
    TCCR1B = (1<<WGM12)|(1<<CS10)|(1<<CS12); // CTC, clk/1024
    OCR1A  = 15624; // 1 sec
    TIMSK |= (1<<OCIE1A);
}

void timer0CTC_init(void) {
    TCCR0 |= (1<<WGM01);
    OCR0   = 250;
    TIMSK |= (1<<OCIE0);
    TCCR0 |= (1<<CS01)|(1<<CS00);
}

void exInt_init(void) {
    // INT0 Reset
    MCUCR |= (1<<ISC01);
    GICR  |= (1<<INT0);

    // INT1 Pause
    MCUCR |= (1<<ISC11)|(1<<ISC10);
    GICR  |= (1<<INT1);

    // INT2 Resume
    MCUCSR &= ~(1<<ISC2);
    GICR   |= (1<<INT2);
}

void io_init(void) {
    DDRA |= 0x3F;   // digit enables
    DDRC |= 0x0F;   // BCD outputs
    DDRD |= (1<<PD0)|(1<<PD4)|(1<<PD5); // buzzer + LEDs
    DDRB  = 0x00;   // buttons input
    PORTB = 0xFF;   // pull-ups
}

// ISRs
ISR(TIMER1_COMPA_vect) {
    tick = 1;
}

ISR(TIMER0_COMP_vect) {
    uint8 digits[6];
    digits[0] = hours / 10;
    digits[1] = hours % 10;
    digits[2] = minutes / 10;
    digits[3] = minutes % 10;
    digits[4] = seconds / 10;
    digits[5] = seconds % 10;

    PORTA = (1 << currentDigit);                 // enable digit
    PORTC = digit_to_bcd[digits[currentDigit]];  // output BCD
    currentDigit++;
    if (currentDigit >= MULTI_SEG_COUNT) currentDigit = 0;
}

ISR(INT0_vect) {
    hours=0; minutes=0; seconds=0;  // reset
}
ISR(INT1_vect) {
    running = 0;  // pause
}
ISR(INT2_vect) {
    running = 1;  // resume
}

// Helper Functions
void stopwatch_toggle_mode(void) {
    countMode ^= 1;
}

void Hour_Inc(void){
    if(!hour_inc_flag){
        hours++;
        if(hours>=MAX_HOURS) hours=0;
        hour_inc_flag=1;
    }
}
void Hour_Dec(void){
    if(!hour_dec_flag){
        if(hours>0) hours--;
        else hours=MAX_HOURS-1;
        hour_dec_flag=1;
    }
}
void Min_Inc(void){
    if(!min_inc_flag){
        minutes++;
        if(minutes>=MAX_MINUTES) minutes=0;
        min_inc_flag=1;
    }
}
void Min_Dec(void){
    if(!min_dec_flag){
        if(minutes>0) minutes--;
        else minutes=MAX_MINUTES-1;
        min_dec_flag=1;
    }
}
void Sec_Inc(void){
    if(!sec_inc_flag){
        seconds++;
        if(seconds>=MAX_SECONDS) seconds=0;
        sec_inc_flag=1;
    }
}
void Sec_Dec(void){
    if(!sec_dec_flag){
        if(seconds>0) seconds--;
        else seconds=MAX_SECONDS-1;
        sec_dec_flag=1;
    }
}

int main(void) {
    io_init();
    timer1CTC_init();
    timer0CTC_init();   // for display multiplexing
    exInt_init();
    sei();

    while(1) {
        // LEDs indicator
        if (countMode == COUNT_UP) {
            PORTD |= (1<<PD4);
            PORTD &= ~(1<<PD5);
        } else {
            PORTD |= (1<<PD5);
            PORTD &= ~(1<<PD4);
        }

        // Handle tick
        if (tick) {
            if (running) {
                if (countMode == COUNT_UP) {
                    seconds++;
                    if (seconds >= MAX_SECONDS) {
                        seconds = 0;
                        minutes++;
                    }
                    if (minutes >= MAX_MINUTES) {
                        minutes = 0;
                        hours++;
                    }
                    if (hours >= MAX_HOURS) {
                        hours = 0;
                    }
                } else { // COUNT_DOWN
                    if (hours==0 && minutes==0 && seconds==0) {
                        PORTD |= (1<<PD0); // buzzer ON
                        running = 0;
                    } else {
                        if (seconds == 0) {
                            if (minutes == 0) {
                                if (hours > 0) {
                                    hours--;
                                    minutes = 59;
                                    seconds = 59;
                                }
                            } else {
                                minutes--;
                                seconds = 59;
                            }
                        } else {
                            seconds--;
                        }
                    }
                }
            }
            tick = 0;
        }

        // Mode toggle button (PB7)
        if(!(PINB & (1<<PB7))) {
            if(!modeBtnFlag){
                stopwatch_toggle_mode();
                modeBtnFlag=1;
            }
        } else {
            modeBtnFlag=0;
        }

        // Adjust buttons
        if(!(PINB & (1<<PB1))) { Hour_Inc(); } else { hour_inc_flag=0; }
        if(!(PINB & (1<<PB0))) { Hour_Dec(); } else { hour_dec_flag=0; }

        if(!(PINB & (1<<PB4))) { Min_Inc(); }  else { min_inc_flag=0; }
        if(!(PINB & (1<<PB3))) { Min_Dec(); }  else { min_dec_flag=0; }

        if(!(PINB & (1<<PB6))) { Sec_Inc(); }  else { sec_inc_flag=0; }
        if(!(PINB & (1<<PB5))) { Sec_Dec(); }  else { sec_dec_flag=0; }
    }
}
