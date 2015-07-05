#include "iostm8s003f3.h"

//Код кнопок
#define CODE_BUT1 0x8EBA
#define CODE_BUT2 0x8EEA

//От повтрного срабатывания
#define DEAD_TIME 200

/*Выбор генератора (0xE1 - HSI; 0xD2 - LSI; 0xB4 - HSE) нужное раскомментировать*/
//#define HSE 0xB4
#define HSI 0xE1
#define LSI 0xD2

#define CLK_DEF HSI

unsigned short int BitTicks;
unsigned char PreAmb = 1;
unsigned long Data = 0;
unsigned char BitCount;
unsigned short int TIM1_PSCR = 4;
unsigned char Flag;

//---------------------------------------------------------

void delay_ms(unsigned int t)
{
  CLK_SWR = LSI; //Выбор LSI генератора
  while (CLK_SWCR_SWBSY);
  
  while (t > 0) 
  {
    if (TIM4_SR_UIF == 1) { t--; TIM4_SR_UIF = 0;};
  }
  
  CLK_SWR = CLK_DEF;
  while (CLK_SWCR_SWBSY);
};

void init(void){
#ifdef LSI
  CLK_ICKR_LSIEN = 1;
#else
  CLK_ICKR_LSIEN = 0;
#endif
#ifdef HSI
  CLK_ICKR_HSIEN = 1;
#else
  CLK_ICKR_HSIEN = 0;
#endif
#ifdef HSE  
  CLK_ECKR_HSEEN = 1;
  while (!CLK_ECKR_HSERDY);    
#else
  CLK_ECKR_HSEEN = 0;
#endif
  CLK_CKDIVR = 0; //Делитель  
  CLK_SWCR = 0; //Reset the clock switch control register.
  CLK_SWCR_SWEN = 1; //Переключение на выбранный генератор  
  CLK_SWR = CLK_DEF; 
  
  while (CLK_SWCR_SWBSY);
  
  //Настройка входа для сигнала
  EXTI_CR1_PAIS = 2; //Прерывание на порт (0: падающий и низкий уровень, 1: возрастающий, 2: падающий, 3: оба)  
  PA_CR1_C13=1; //Подтяжка вверх
  PA_CR2_C23=1; //Разрешаем прерывания
  CPU_CFG_GCR_AL = 1; //Прерывания во сне
  
  //Таймер 4 для задержки ms
  TIM4_PSCR = 0; //Предделитель
  TIM4_ARR = 127;
  TIM4_CR1_URS = 1;
  TIM4_EGR_UG = 1;  //Вызываем Update Event
  TIM4_CR1_CEN = 1;
  
  //Таймер 1
  TIM1_CR1_URS = 1; //Прерывание только по переполнению счетчика  
  TIM1_IER_UIE = 1; // Разрешаем прерывания
  
  //Таймер 2 F_CPU/2^PSCR/ARR
  TIM2_PSCR = 7; // 2^PSCR = 128
  TIM2_ARRH = (125*DEAD_TIME) >> 8;
  TIM2_ARRL = (125*DEAD_TIME) & 0xFF;
  TIM2_CR1_OPM = 1; //Останавливаем таймер при переполнении
  TIM1_IER_UIE = 1;
  TIM1_CR1_URS = 1;
    
  //GPIO 
  PC_DDR_DDR7 = 1;
  PC_CR1_C17 = 1;
  PC_DDR_DDR6 = 1;
  PC_CR1_C16 = 1;
};

void RESET (void)
{
  TIM1_CR1_CEN = 0;
  TIM1_CNTRH = 0;
  TIM1_CNTRL = 0;  
  TIM1_PSCRH = (TIM1_PSCR << 1) >> 8;
  TIM1_PSCRL = (TIM1_PSCR << 1) & 0xFF;
  TIM1_ARRH = 21845 >> 8;
  TIM1_ARRL = 21845 & 0xFF;
  TIM1_EGR_UG = 1;
  TIM1_SR1_UIF = 0;
  
  PreAmb = 1;
  Data = 0;
  BitCount = 0;
  Flag = 0;
  EXTI_CR1_PAIS = 2;
  PA_CR2_C23 = 1;
};

unsigned char DTime()
{
  if (TIM2_CR1_CEN) {
    TIM2_EGR_UG = 1;
    TIM2_SR1_UIF = 0;
    TIM2_CR1_CEN = 1;
    return 1;
  } else
  {
    TIM2_SR1_UIF = 0;
    TIM2_CR1_CEN = 1;
    return 0;
  }
};

void Button1(void)
{
    PC_ODR_ODR7 ^= 1;
};

void Button2(void)
{
    PC_ODR_ODR6 ^= 1;
};

#pragma vector = TIM1_OVR_UIF_vector 
__interrupt void TIM1_Interrupt(void) 
{
  switch (Flag){
  case 1: RESET(); break;
  case 2:
    TIM1_CR1_CEN = 0;
    if (PreAmb == 0x15)
    {
      BitCount++;
      Data = (Data << 1) ^ !PA_IDR_IDR3;
      if (BitCount >= 36)
      {
        switch (Data & 0xFFFF)
        {
        case CODE_BUT1: // Кнопка 1
          if (!DTime()) Button1();
          break;
        case CODE_BUT2: // Кнопка 2
          if (!DTime()) Button2();
          break;
        }
        RESET();
      }
    } else if ((PreAmb & 0x1) == PA_IDR_IDR3)
    {
      PreAmb = (PreAmb << 1) ^ !PA_IDR_IDR3;
    } else RESET();
    PA_CR2_C23 = 1;
    break;
  default:
    Flag = 1;
    TIM1_CR1_CEN = 0;
    TIM1_EGR_UG = 1;
    break;
  }
 TIM1_SR1_UIF = 0;
};

#pragma vector=0x05
__interrupt void PORTA_interrupt(void) 
{
  PA_CR2_C23 = 0;
  switch (Flag){
  case 2:
    TIM1_CR1_CEN = 1;
    break;
  case 1:
    if (TIM1_CR1_CEN == 1)
    {
      TIM1_CR1_CEN = 0;
      BitTicks = (TIM1_CNTRH << 8 | TIM1_CNTRL) * 3;
      if ( BitTicks != 0 )
      {
        TIM1_ARRH = BitTicks >> 8;
        TIM1_ARRL = BitTicks & 0xFF;
        TIM1_PSCRH = TIM1_PSCR >> 8;
        TIM1_PSCRL = TIM1_PSCR & 0xFF;
        TIM1_EGR_UG = 1;
        TIM1_CR1_CEN = 1;
        Flag = 2;
      } else RESET();
    } else
    {
      TIM1_CR1_CEN = 1;
      EXTI_CR1_PAIS = 2;
      PA_CR2_C23 = 1;
    }
    break;  
  default:
    if (TIM1_CR1_CEN == 1) RESET();
    else 
    {
      TIM1_CR1_CEN = 1;
      EXTI_CR1_PAIS = 1;
      PA_CR2_C23 = 1;
    }
    break;
  }
};

void main( void )
{
  init();
  RESET();
  asm("WFI");
};
