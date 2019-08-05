#include "msp430g2553.h"

#include <stdbool.h>
#include <inttypes.h>

#define TEMP1_INCH INCH_4
#define TEMP2_INCH INCH_5

// P1
#define ADC   BIT0
#define VAZA  BIT3
#define TEMP1 BIT4
#define TEMP2 BIT5
//#define TIMER BIT7

// P2
#define LED1  BIT0
#define LED2  BIT1
#define LED3  BIT2
#define LED4  BIT3
#define LED5  BIT4

#define OFF     0
#define ON      1
#define ATENCAO 2

#define VERY_LOW    136 // 136
#define LOW         272 // 272
#define MEDIUM      408 // 408
#define HIGH        544 // 544
#define VERY_HIGH   680 // 680

volatile uint8_t estado = OFF;
uint32_t contador;
uint16_t adc;

bool terminou_media = false;

unsigned int  timer_cont = 0;

volatile char tx_buf[32];
unsigned char tx_index;

#define abs(x) x < 0 ? -x : x;

uint8_t intlen(int16_t const value)
{
  //return log10(abs(value)) + (value < 0 ? 2 : 1);
  int16_t const absolute = abs(value);
  if (absolute < 10) return (value < 0 ? 2 : 1);
  if (absolute < 100) return (value < 0 ? 3 : 2);
  if (absolute < 1000) return (value < 0 ? 4 : 3);
  if (absolute < 10000) return (value < 0 ? 5 : 4);
  return (absolute < 0 ? 6 : 5);
}

char* itoa(int16_t value, char* const str)
{
  uint8_t length = intlen(value);
 
  str[length] = '\0';
  while (length > 0)
  {
    str[--length] = value % 10 + '0';
    value /= 10;
  }

  return str;
}

void print(char const * const str)
{
    while (tx_buf[0] != '\0');
    //strcpy(tx_buf, str);
    for (tx_index = 0; str[tx_index]; tx_index++)
    {
        tx_buf[tx_index] = str[tx_index];
    }
    
    tx_index = 0;
    IFG2 |= UCA0TXIFG;
    //UC0IE |= UCA0TXIE;
}

void println(char const * const str)
{
    while (tx_buf[0] != '\0');
    for (tx_index = 0; str[tx_index]; tx_index++)
    {
        tx_buf[tx_index] = str[tx_index];
    }
    tx_buf[tx_index++] = '\n';
    tx_buf[tx_index] = '\0';
    
    tx_index = 0;
    IFG2 |= UCA0TXIFG;
    //UC0IE |= UCA0TXIE;
}

void config_timer(uint32_t interval, uint8_t divider)
{
  /*TA0CTL  |= TASSEL_2;      // submain clock
  TA0CTL  |= divider;
  TA0CCR0  = interval - 1;
  TA0CCTL0 = CCIE;*/
    // TASSELx:
    //      00: TACLK
    //      01: ACLK
    //      10: SMCLK
    //      11: INCLK
    // MCx:
    //      00: Stop mode: the timer is halted.
    //      01: Up mode: the timer counts up to TACCR0
    //      10: Continuous mode: the timer counts up to 0xFFFF
    //      11: Up/down mode: the timer counts up to TACCR0 then down to 0x0000
    /*P1DIR  &= ~TIMER;
    P1SEL  |=  TIMER;
    P1SEL2 &= ~TIMER;*/
    TACCR0 = interval - 1;
    TACTL |=
         TASSEL_2
        | divider
        | MC_1
        | TAIE;      // Enables general TimerA0 interrupt
    TACCTL0 |= CCIE; // Enables CCR0 interrupt. Enables TAIFG interrupt request.
}

int8_t temperature(uint16_t adc)
{
    return adc * 20 / 136;
}

void config_adc(uint16_t pin, uint16_t inch)
{
  ADC10AE0 = pin;       // sets the pin as the ADC10 input
  ADC10CTL1 =
      inch              // configures the pin
    | SHS_0             // sampleandhold controlled by the ADC10SC bit
    | ADC10DIV_7        // selects clock division by 8
    | ADC10SSEL_0       // selects internal clock ADC10SC
    | CONSEQ_0;         // selects 0 mode
  ADC10CTL0 =
      SREF_1            // selects Vref+ as VR+ and GND as VR-
    | ADC10SHT_0        // sampleandhold time of 4 clock cycles
    | ADC10ON           // enables the ADC as an interrupt after the conversion
    | ADC10IE           // IE
    | REFON;            // Reference generator on
  ADC10CTL0 &= ~REF2_5V;
}

void config_uart(uint16_t baudrate)
{
    const uint32_t SMCLOCK = 1000000ull;
    P1SEL  |= BIT1 | BIT2;
    P1SEL2 |= BIT1 | BIT2;

    DCOCTL  = 0;
    BCSCTL1 = CALBC1_1MHZ;
    DCOCTL  = CALDCO_1MHZ;
    
    UCA0CTL1 |=  UCSSEL_2;                      // submain clock (SMCLK) as source for UART
    UCA0BR0   =  (SMCLOCK / baudrate) % 256;    // 1 MHZ 9600
    UCA0BR1   =  (SMCLOCK / baudrate) / 256;    // 1 MHZ 9600
    UCA0MCTL  =  UCBRS2 + UCBRS0;               // Modulation UCBRSx = 5
    UCA0CTL1 &= ~UCSWRST;                       // Initialize USCI
    UC0IE    |=  UCA0TXIE | UCA0RXIE;           // IE
}

int main(void)
{
  WDTCTL = WDTPW + WDTHOLD;
  
  P2DIR |= LED1 | LED2 | LED3 | LED4 | LED5;
  
  config_adc(TEMP1, TEMP1_INCH);
  config_timer(10e3, ID_0);

  tx_index = 0;
  tx_buf[0] = '\0';

  // teste gerador
  P1REN |= VAZA;
  P1OUT |= VAZA;

  config_uart(9600);

  P1IE  |=  VAZA;
  P1IES |=  VAZA;
  P1IFG &= ~VAZA;
  
  _BIS_SR(GIE);
  
  while (true)
  {
    switch (estado)
    {
      case OFF:
        //println("OFF");
     
      case ATENCAO:
        //println("ATENCAO");
        //P1IE &= ~VAZA;
        P1OUT &= ~(TEMP1 | TEMP2 | VAZA);
        break;
      case ON:
        ADC10CTL0 |= ENC | ADC10SC;     // initializes ADC convert
        //println("ON");
        //P1IE |= VAZA;
        P1OUT |=  (TEMP1 | TEMP2 | VAZA);

        __delay_cycles(1e6);
        char stradc[8];
        itoa(temperature(adc), stradc);
        //print("Temperatura media: ");
        println(stradc);
        break;
    }
  }

  return 0;
}

#pragma vector=PORT1_VECTOR
__interrupt void PORT1_ISR(void)
{
  if ((P1IFG & VAZA) == VAZA)
  {
    timer_cont = 0;
    P1IFG &= ~VAZA;
  }
}

#pragma vector=TIMER0_A0_VECTOR
__interrupt void TIMER0_A0_ISR(void)
{
  if (estado != ON)
  {
    timer_cont = 0;
    TACTL &= ~TAIFG;
    TACCTL0 &= ~CCIFG;
    return;
  }
  
  timer_cont++;
  if (timer_cont >= 1000) // 10s
  {
    if (estado != ATENCAO)
    {
      estado = ATENCAO;
      println("Vazamento detectado!");
    }

    timer_cont = 0;
  }
  
  TACTL &= ~TAIFG;
  TACCTL0 &= ~CCIFG;
}

/*
0 -> < 20
1 -> 20~40
2 -> 40~60
3 -> 60~80
4 -> 80~100
5 -> > 100
*/

void led_control(unsigned int adc)
{
  if (adc < VERY_LOW)
  {
    P2OUT &= ~(LED1 | LED2 | LED3 | LED4 | LED5);
  }
  else if (adc < LOW)
  {
    P2OUT |= (LED1);
    P2OUT &= ~(LED2 | LED3 | LED4 | LED5);
  }
  else if (adc < MEDIUM)
  {
    P2OUT |= (LED1 | LED2);
    P2OUT &= ~(LED3 | LED4 | LED5);
  }
  else if (adc < HIGH)
  {
    P2OUT |= (LED1 | LED2 | LED3);
    P2OUT &= ~(LED4 | LED5);
  }
  else if (adc < VERY_HIGH)
  {
    P2OUT |= (LED1 | LED2 | LED3 | LED4);
    P2OUT &= ~(LED5);
  }
  else
  {
    P2OUT |= (LED1 | LED2 | LED3 | LED4 | LED5);
  }
}

#pragma vector=ADC10_VECTOR
__interrupt void ADC10_ISR(void)
{
  if ((ADC10CTL1 & TEMP1_INCH) == TEMP1_INCH)
  {
    adc = ADC10MEM;
    config_adc(TEMP2, TEMP2_INCH);
    terminou_media = false;
  }
  else if ((ADC10CTL1 & TEMP2_INCH) == TEMP2_INCH)
  {
    adc = (ADC10MEM + adc) / 2;
    config_adc(TEMP1, TEMP1_INCH);
    terminou_media = true;
  }
  
  led_control(adc);
  ADC10CTL0 &= ~ADC10IFG;
}

#pragma vector=USCIAB0RX_VECTOR
__interrupt void USCIAB0RX_ISR(void)
{
  char rx_buf = UCA0RXBUF;
  if (rx_buf == 'S' || rx_buf == 's')
  {
    estado = ON;
  }
}

#pragma vector=USCIAB0TX_VECTOR
__interrupt void USCIA0TX_ISR(void)
{
  if (tx_buf[tx_index])
  {
    UCA0TXBUF = tx_buf[tx_index++];
  }
  else
  {
    tx_index = 0;
    tx_buf[0] = '\0';
    IFG2 &= ~UCA0TXIFG;
  }
}