

#include "pwm.h"
sctimer_config_t sctimerInfo;
sctimer_pwm_signal_param_t pwmParam;
uint32_t event_temp;
uint32_t event_light;
uint32_t sctimerClock;

void pwm_init(void)
{
    sctimerClock = SCTIMER_CLK_FREQ;
    SCTIMER_GetDefaultConfig(&sctimerInfo);

    /* Initialize SCTimer module */
    SCTIMER_Init(SCT0, &sctimerInfo);

    /* Configure first PWM with frequency 24kHZ from first output */
    pwmParam.output           = DEMO_FIRST_SCTIMER_OUT;
    pwmParam.level            = kSCTIMER_HighTrue;
    pwmParam.dutyCyclePercent = 0;
    if (SCTIMER_SetupPwm(SCT0, &pwmParam, kSCTIMER_CenterAlignedPwm, 100U, sctimerClock, &event_temp) == kStatus_Fail)
    {

    }
    /* Configure second PWM with different duty cycle but same frequency as before */
       pwmParam.output           = DEMO_SECOND_SCTIMER_OUT;
       pwmParam.level            = kSCTIMER_HighTrue;
       pwmParam.dutyCyclePercent = 0;
       if (SCTIMER_SetupPwm(SCT0, &pwmParam, kSCTIMER_CenterAlignedPwm, 100U, sctimerClock, &event_light) == kStatus_Fail)
       {
           //return -1;
       }
    /* Start the 32-bit unify timer */
    SCTIMER_StartTimer(SCT0, kSCTIMER_Counter_U);

}

