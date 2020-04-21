#include <stdio.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "MKL02Z4.h"
#include "fsl_clock.h"

volatile uint32_t timer_counter = 0;

void init_timer()
{
	CLOCK_EnableClock(kCLOCK_Lptmr0);

	LPTMR0->CSR = 0;
	LPTMR0->PSR = LPTMR_PSR_PRESCALE(1) | LPTMR_PSR_PCS(1); // LPO 1khz divide by 4
	LPTMR0->CMR = 25; // 0.1 seconds
	LPTMR0->CSR |= LPTMR_CSR_TIE_MASK | LPTMR_CSR_TEN_MASK;
	NVIC_EnableIRQ(LPTMR0_IRQn);
}

extern "C" void LPTMR0_IRQHandler(void)
{
	timer_counter++;
	LPTMR0->CSR |= LPTMR_CSR_TCF_MASK;
}

int enable_switch()
{
	return (BOARD_INITPINS_ENABLE_GPIO->PDIR & (1 << BOARD_INITPINS_ENABLE_PIN)) == 0;
}

void power_switch(int state)
{
	if (!state) {
		BOARD_INITPINS_POWERSW_GPIO->PDDR |= 1 << BOARD_INITPINS_POWERSW_PIN;
		BOARD_INITPINS_POWERSW_GPIO->PDOR &= ~(1 << BOARD_INITPINS_POWERSW_PIN);
	}
	else {
		BOARD_INITPINS_POWERSW_GPIO->PDDR |= 1 << BOARD_INITPINS_POWERSW_PIN;
		BOARD_INITPINS_POWERSW_GPIO->PDOR |= 1 << BOARD_INITPINS_POWERSW_PIN;
	}
}

int red_led_on()
{
	return (BOARD_INITPINS_REDLED_GPIO->PDIR & (1 << BOARD_INITPINS_REDLED_PIN)) != 0;
}

int blue_led_on()
{
	return (BOARD_INITPINS_BLUELED_GPIO->PDIR & (1 << BOARD_INITPINS_BLUELED_PIN)) != 0;

}

int main(void) {
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();

    init_timer();

    //printf("Hello World\n");

    enum {
    	state_startup,
		state_powering_up,
		state_power_on,
		state_powering_down,
		state_power_off
    } state = state_startup;
    uint32_t timer_start_count = timer_counter;

	power_switch(0);

    while(1) {
    	__WFE();

    	if (state == state_startup) {
    		if (timer_counter - timer_start_count < 4) {
        		if (red_led_on()) {
            		state = state_power_on;
            		timer_start_count = timer_counter;
        		}
    			continue;
    		}

    		state = state_power_off;
    		continue;
    	}

    	if (state == state_power_off) {
    		if (timer_counter - timer_start_count < 5) {
    			continue;
    		}
    		if (red_led_on()) {
    			state = state_power_on;
    			timer_start_count = timer_counter;
        		power_switch(0);
    		}
    		else if (enable_switch()) {
    			state = state_powering_up;
    			timer_start_count = timer_counter;
    			power_switch(1);
    		}
    		continue;
    	}

    	if (state == state_powering_up) {
    		if (red_led_on()) {
    			state = state_power_on;
    			timer_start_count = timer_counter;
        		power_switch(0);
    		}
    		continue;
    	}

    	if (state == state_power_on) {
    		if (timer_counter - timer_start_count < 5) {
    			continue;
    		}
    		if (!enable_switch()) {
    			state = state_powering_down;
    			timer_start_count = timer_counter;
    			power_switch(1);
    		}
    		continue;
    	}

    	if (state == state_powering_down) {
    		if (timer_counter - timer_start_count < 30) {
    			continue;
    		}
    		state = state_power_off;
			timer_start_count = timer_counter;
			power_switch(0);
    		continue;
    	}
    }

    return 0;
}
