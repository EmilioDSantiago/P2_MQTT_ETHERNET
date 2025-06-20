#ifndef PWM_H_
#define PWM_H_

#include "fsl_debug_console.h"
#include "board.h"
#include "app.h"
#include "fsl_sctimer.h"

#define SCTIMER_CLK_FREQ        CLOCK_GetCoreSysClkFreq()
#define DEMO_FIRST_SCTIMER_OUT  kSCTIMER_Out_0
#define DEMO_SECOND_SCTIMER_OUT kSCTIMER_Out_1

extern sctimer_config_t sctimerInfo;
extern sctimer_pwm_signal_param_t pwmParam;
extern uint32_t event_temp;
extern uint32_t event_light;
extern uint32_t sctimerClock;

void pwm_init(void);

#endif /* PWM_H_ */
