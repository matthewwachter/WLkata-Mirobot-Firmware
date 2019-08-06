/*
  spindle_control.c - spindle control methods
  Part of Grbl
  Copyright (c) 2012-2015 Sungeun K. Jeon
  Copyright (c) 2009-2011 Simen Svale Skogsrud
  Grbl is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  Grbl is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with Grbl.  If not, see <http://www.gnu.org/licenses/>.
*/


/* RC-Servo PWM modification: switch between 0.6ms and 2.5ms pulse-width at 61Hz
   16000000/1024 = 15625Hz   2的8次方，TOP值为255
   Prescaler 1024 = 15625Hz / 256Steps =  61Hz	64碌s/step -> Values 15 / 32 for 1ms / 2ms
   Reload value = 0x07
   Replace this file in C:\Program Files (x86)\Arduino\libraries\GRBL
*/


#include "grbl.h"

#define RC_SERVO_SHORT     5       // Timer ticks for 0.6ms pulse duration  (9 for 0.6ms)
//(1/15625)*5 = 0.00032(s) = 0.32(ms)
#define RC_SERVO_LONG      250       // Timer ticks for 2.5 ms pulse duration  (39 for 2.5ms)

#define RC_SERVO_SHORT_2     5       // Timer ticks for 0.6ms pulse duration  (9 for 0.6ms)
#define RC_SERVO_LONG_2      250       // Timer ticks for 2.5 ms pulse duration  (39 for 2.5ms)

//#define RC_SERVO_INVERT     1     // Uncomment to invert servo direction


void spindle_init()
{
  // Configure variable spindle PWM and enable pin, if requried. On the Uno, PWM and enable are
  // combined unless configured otherwise.
  #ifdef VARIABLE_SPINDLE
    SPINDLE_PWM_DDR |= (1<<SPINDLE_PWM_BIT); // Configure as PWM output pin.G0设置为输出
    #if defined(CPU_MAP_ATMEGA2560) || defined(USE_SPINDLE_DIR_AS_ENABLE_PIN)
      SPINDLE_ENABLE_DDR |= (1<<SPINDLE_ENABLE_BIT); // Configure as output pin.
    #endif
  // Configure no variable spindle and only enable pin.
  #else
    SPINDLE_ENABLE_DDR |= (1<<SPINDLE_ENABLE_BIT); // Configure as output pin.
  #endif

  #ifndef USE_SPINDLE_DIR_AS_ENABLE_PIN
    SPINDLE_DIRECTION_DDR |= (1<<SPINDLE_DIRECTION_BIT); // Configure as output pin.
  #endif
  spindle_stop();
}

#ifdef VARIABLE_SPINDLE_2
void spindle_init_2()
{
  // Configure variable spindle PWM and enable pin, if requried. On the Uno, PWM and enable are
  // combined unless configured otherwise.
    SPINDLE_PWM_DDR_2 |= (1<<SPINDLE_PWM_BIT_2); // Configure as PWM output pin.E4端口设置为输出
  spindle_stop_2();
}
#endif


void spindle_stop()
{     // On the Uno, spindle enable and PWM are shared. Other CPUs have seperate enable pin.
       #ifdef RC_SERVO_INVERT
          OCR_REGISTER = RC_SERVO_LONG;
      #else
          OCR_REGISTER = RC_SERVO_SHORT;
      #endif
}


#ifdef VARIABLE_SPINDLE_2
void spindle_stop_2()
{     // On the Uno, spindle enable and PWM are shared. Other CPUs have seperate enable pin.
       #ifdef RC_SERVO_INVERT
          OCR_REGISTER_2 = RC_SERVO_LONG;
      #else
          OCR_REGISTER_2 = RC_SERVO_SHORT;
      #endif
}
#endif


void spindle_run(uint8_t direction, float rpm)
{
  if (sys.state == STATE_CHECK_MODE) { return; }

  // Empty planner buffer to ensure spindle is set when programmed.
  protocol_auto_cycle_start();  //temp fix for M3 lockup
  protocol_buffer_synchronize();//先把缓冲区之前的都执行完，思考，机械臂有无必要这么办，如果是雕刻机模式
  //则有必要！！！

  if (direction == SPINDLE_DISABLE) {

    spindle_stop();

  } else {

	#ifdef VARIABLE_SPINDLE

      // TODO: Install the optional capability for frequency-based output for servos.
      #define SPINDLE_RPM_RANGE (SPINDLE_MAX_RPM-SPINDLE_MIN_RPM)//主轴转速的范围
      #define RC_SERVO_RANGE (RC_SERVO_LONG-RC_SERVO_SHORT)//舵机的范围


        TCCRA_REGISTER = (1<<COMB_BIT)  | (1<<WGM40); //ESTO ESTA HARDCODEADO
        //TCCR4A：COM4B1置1，WGM40置1,    0010 0001
        TCCRB_REGISTER = (TCCRB_REGISTER & 0b11101000) | 0b00000101 | (1<<WGM42); // set to 1/1024 Prescaler //ESTO TAMBIEN, el wgm42 y el 0b00000101
		//TCCR4B：xxx0 1101
		uint8_t current_pwm;


	   if ( rpm < SPINDLE_MIN_RPM ) { rpm = 0; }
      else {
        rpm -= SPINDLE_MIN_RPM;
        if ( rpm > SPINDLE_RPM_RANGE ) { rpm = SPINDLE_RPM_RANGE; } // Prevent integer overflow
      }

      #ifdef RC_SERVO_INVERT
          current_pwm = floor( RC_SERVO_LONG - rpm*(RC_SERVO_RANGE/SPINDLE_RPM_RANGE));
          OCR_REGISTER = current_pwm;
      #else
         current_pwm = floor( rpm*(RC_SERVO_RANGE/SPINDLE_RPM_RANGE) + RC_SERVO_SHORT);
          OCR_REGISTER = current_pwm;//这里的比较值从5到29.5来回切换，占空比从1.9到11.7
      #endif
	  #ifdef MINIMUM_SPINDLE_PWM
        if (current_pwm < MINIMUM_SPINDLE_PWM) { current_pwm = MINIMUM_SPINDLE_PWM; }
	     OCR_REGISTER = current_pwm;
      #endif
    #endif
  }
}

#ifdef VARIABLE_SPINDLE_2
void spindle_run_2(uint8_t direction, float rpm)
{
  if (sys.state == STATE_CHECK_MODE) { return; }

  // Empty planner buffer to ensure spindle is set when programmed.
  protocol_auto_cycle_start();  //temp fix for M3 lockup
  protocol_buffer_synchronize();

  if (direction == SPINDLE_DISABLE) {

    spindle_stop_2();

  } else {



      // TODO: Install the optional capability for frequency-based output for servos.
      #define SPINDLE_RPM_RANGE_2 (SPINDLE_MAX_RPM-SPINDLE_MIN_RPM)
      #define RC_SERVO_RANGE_2 (RC_SERVO_LONG_2-RC_SERVO_SHORT_2)


        TCCRA_REGISTER_2 = (1<<COMB_BIT_2)  | (1<<WGM30); //ESTO ESTA HARDCODEADO
        TCCRB_REGISTER_2 = (TCCRB_REGISTER_2 & 0b11101000) | 0b00000101 | (1<<WGM32); // set to 1/1024 Prescaler //ESTO TAMBIEN, el wgm42 y el 0b00000101
	    uint8_t current_pwm;


	   if ( rpm < SPINDLE_MIN_RPM ) { rpm = 0; }
      else {
        rpm -= SPINDLE_MIN_RPM;
        if ( rpm > SPINDLE_RPM_RANGE_2 ) { rpm = SPINDLE_RPM_RANGE_2; } // Prevent integer overflow
      }

      #ifdef RC_SERVO_INVERT
          current_pwm = floor( RC_SERVO_LONG_2 - rpm*(RC_SERVO_RANGE_2/SPINDLE_RPM_RANGE_2));
          OCR_REGISTER_2 = current_pwm;
      #else
         current_pwm = floor( rpm*(RC_SERVO_RANGE_2/SPINDLE_RPM_RANGE_2) + RC_SERVO_SHORT_2);
          OCR_REGISTER_2 = current_pwm;
      #endif
	 // #ifdef MINIMUM_SPINDLE_PWM
     //   if (current_pwm < MINIMUM_SPINDLE_PWM) { current_pwm = MINIMUM_SPINDLE_PWM; }
	 //    OCR_REGISTER = current_pwm;
     // #endif
    
  }
}
#endif

spindle_set_state(uint8_t state, float rpm){
}
