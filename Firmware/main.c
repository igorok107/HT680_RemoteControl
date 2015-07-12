#include "iostm8s003f3.h"

//Код кнопок
#define CODE_BUT1 0x8EBA
#define CODE_BUT2 0x8EEA

//Задержка от повтрного срабатывания
#define DEAD_TIME 200

#define HSE 0xB4
#define HSI 0xE1
#define LSI 0xD2

unsigned short int BitTicks;
unsigned char PreAmb = 1;
unsigned long Data = 0;
unsigned char BitCount;
unsigned short int TIM1_PSCR = 4;
unsigned char Flag;

//---------------------------------------------------------

void init(void){
  CLK_ICKR_LSIEN = 0;
  CLK_ICKR_HSIEN = 1;
  CLK_ECKR_HSEEN = 0;
  CLK_CKDIVR = 0; //Делитель  
  CLK_SWCR = 0; //Reset the clock switch control register.
  CLK_SWCR_SWEN = 1; //Переключение на выбранный генератор  
  CLK_SWR = HSI; 
  while (CLK_SWCR_SWBSY);
  
  CPU_CFG_GCR_AL = 1; //Разрешить прерывания во сне  
  
  //Настройка входа приёмника
  EXTI_CR1_PAIS = 2; //Прерывание на порт (0: падающий и низкий уровень, 1: возрастающий, 2: падающий, 3: оба)  
  PA_CR1_C13=1; //Подтяжка вверх
  PA_CR2_C23=1; //Разрешаем прерывания
  
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
    
  //Канал 1 
  PC_DDR_DDR7 = 1;
  PC_CR1_C17 = 1;
  PC_ODR_ODR7 = 1; //Высокий уровень по умолчанию
  //Канал 2
  PC_DDR_DDR6 = 1;
  PC_CR1_C16 = 1;
  
  //AC Detect вход
  PD_DDR_DDR3 = 0;
  PC_CR1_C13 = 1;
  EXTI_CR1_PDIS = 2;
  PD_CR2_C23 = 1;
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

#pragma vector=0x08
__interrupt void AC_DET(void) 
{
  if (!DTime()) //Прогамный антидребезг
  {
    unsigned char State = PC_ODR >> 6;
    if (State != 0)
    {
      PC_ODR = ((((State >> 1) ^ (State & 1)) << 2) | State) << 5; // (10) -> (11) -> (10) cycle
    } else 
      PC_ODR_ODR7 = 1;
  }
}

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
__interrupt void RECIVER(void) 
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
