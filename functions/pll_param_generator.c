// this pll param generator is designed for output frequency of 1MHz to 10MHz

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "hwlib.h"
#include "socal/socal.h"
#include "socal/hps.h"
#include "socal/alt_gpio.h"
#include "pll_param_generator.h"

#include "../hps_soc_system_backup(3).h"
#include "reconfig_functions.h"
#include "pll_calculator.h"

// counter C read address (write address is different from read address)
uint32_t CNT_READ_ADDR [18] = {
	0x28,	// address C00
	0x2C,   // address C01
	0x30,   // address C02
	0x34,   // address C03
	0x38,   // address C04
	0x3C,   // address C05
	0x40,   // address C06
	0x44,   // address C07
	0x48,   // address C08
	0x4C,   // address C09
	0x50,   // address C10
	0x54,   // address C11
	0x58,   // address C12
	0x5C,   // address C13
	0x60,   // address C14
	0x64,   // address C15
	0x68,   // address C16
	0x6C    // address C17
};

void Set_M (void *addr, uint32_t * pll_param, uint32_t enable_message) {
	uint32_t high_byte, low_byte, bypass_enable, odd_division;
	
	high_byte = 0;
	low_byte = 0;
	bypass_enable = 0;
	odd_division = 0;
	
	if (*(pll_param+M_COUNTER_ADDR) == 1) {
		bypass_enable = 1;
	}
	else {
		low_byte = *(pll_param+M_COUNTER_ADDR)>>1;		// divide by 2, in other words, remove 1 LSB
		if ( (low_byte<<1) == *(pll_param+M_COUNTER_ADDR)) {	// shift M to left to check if the previous LSB is 1 or 0, if it is 0, then high-byte should be equal to low-byte
			high_byte = low_byte;
		}
		else {	// M must be odd number, so the high byte should be incremented by one, and set the odd division to get 50% duty cycle
			high_byte = low_byte+1;
			odd_division = 1;
		}
	}
	
	Reconfig_M (addr, low_byte, high_byte, bypass_enable, odd_division);
	if (enable_message) {
		printf ("-- M --\n\tlow_byte\t\t: %3d\n\thigh_byte\t\t: %3d\n\tbypass_enable\t: %3d\n\todd_division\t: %3d\n", low_byte, high_byte, bypass_enable, odd_division);
	}
}

void Set_N (void *addr, uint32_t * pll_param, uint32_t enable_message) {
	uint32_t high_byte, low_byte, bypass_enable, odd_division;
	
	high_byte = 0;
	low_byte = 0;
	bypass_enable = 0;
	odd_division = 0;
	
	if (*(pll_param+N_COUNTER_ADDR) == 1) {
		bypass_enable = 1;
	}
	else {
		low_byte = *(pll_param+N_COUNTER_ADDR)>>1;		// divide by 2, in other words, remove 1 LSB
		if ( (low_byte<<1) == *(pll_param+N_COUNTER_ADDR)) {	// shift N to left to check if the previous LSB is 1 or 0, if it is 0, then high-byte should be equal to low-byte
			high_byte = low_byte;
		}
		else {	// N must be odd number, so the high byte should be incremented by one, and set the odd division to get 50% duty cycle
			high_byte = low_byte+1;
			odd_division = 1;
		}
	}
	
	Reconfig_N (addr, low_byte, high_byte, bypass_enable, odd_division);
	if (enable_message) {
		printf ("-- N --\n\tlow_byte\t\t: %3d\n\thigh_byte\t\t: %3d\n\tbypass_enable\t: %3d\n\todd_division\t: %3d\n", low_byte, high_byte, bypass_enable, odd_division);
	}
}

void Set_C (void *addr, uint32_t * pll_param, uint32_t counter_select, double duty_cycle, uint32_t enable_message) {
	uint32_t high_byte, low_byte, bypass_enable, odd_division;
	
	high_byte = 0;
	low_byte = 0;
	bypass_enable = 0;
	odd_division = 0;
	
	unsigned int i = 0;
	
	if (*(pll_param+C_COUNTER_ADDR) == 1) {
		bypass_enable = 1;
	}
	else {
		double prev_duty_cycle = 0;
		double mid_duty_cycle = 0;
		double current_duty_cycle = 0;
		
		// this loop is trying to find the closest fraction match from duty cycle user input to duty cycle can be generated by the pll system
		// it is tightly connected to the number of C counter that's available from pll_calculator function
		// it is iterating from i from 1 to C-counter-parameter to try to find the right i/C-counter-parameter value that is closest to the user input duty cycle
		for (i = 1; i<=*(pll_param+C_COUNTER_ADDR); i++) {
			prev_duty_cycle = current_duty_cycle;
			current_duty_cycle = (double)i/(double)(*(pll_param+C_COUNTER_ADDR));
			mid_duty_cycle = (prev_duty_cycle+current_duty_cycle)/2;
			if (current_duty_cycle == duty_cycle) {
				break;
			}
			if (mid_duty_cycle == duty_cycle) {
				odd_division = 1;
				break;
			}
			if (duty_cycle<current_duty_cycle && duty_cycle>mid_duty_cycle) {
				if (current_duty_cycle-duty_cycle <= duty_cycle-mid_duty_cycle) {
					break;
				}
				else {
					odd_division = 1;
					break;
				}
			}
			if (duty_cycle<mid_duty_cycle && duty_cycle>prev_duty_cycle) {
				if (mid_duty_cycle-duty_cycle <= duty_cycle-prev_duty_cycle) {
					odd_division = 1;
					break;
				}
				else {
					i--;
					break;
				}
			}
		}
		
		
	}
	
	high_byte = i;
	low_byte = (*(pll_param+C_COUNTER_ADDR)) - high_byte;
	// printf("\nhigh_byte: %d, low_byte: %d",high_byte,low_byte);
	// printf("\nduty cycle: %f\n",duty_cycle);
	
	Reconfig_C (addr, counter_select, low_byte, high_byte, bypass_enable, odd_division);
	if (enable_message) {
		printf ("-- C --\n\tlow_byte\t\t: %3d\n\thigh_byte\t\t: %3d\n\tbypass_enable\t: %3d\n\todd_division\t: %3d\n", low_byte, high_byte, bypass_enable, odd_division);
	}
}

void Set_DPS (void *addr, uint32_t counter_select, uint32_t phase, uint32_t enable_message) { // phase is 0 to 360
	double DPS;
	double c_counter = Read_C_Counter(addr,counter_select);		// read the current C Counter value
	uint32_t DPS_direction = 1;
	
	DPS = ((double)phase*(double)8*(double)c_counter)/(double)360; // 1/8 VCO is the minimum phase shift you could get, so multiply by 8 and also c_counter to get 360 degrees phase shift
	//DPS_direction = 1;

	Reconfig_DPS (addr, counter_select, (uint32_t)DPS, DPS_direction);
	Start_Reconfig(addr,0x00);
	
	if (enable_message) {
		double temp; // general variable for value to be printed
		temp = (double)((uint32_t)DPS)/(double)(8*c_counter)*360;
		printf("Actual phase shift : %f\n",temp);
	}
}

void Set_MFrac (void *addr, uint32_t * pll_param, uint32_t enable_message) {
	uint32_t MFRAC = *(pll_param+M_FRAC_ADDR);
	Reconfig_MFrac(addr, MFRAC);
	if (enable_message) {
		printf ("-- MFRAC --\n\tmfrac : %u/4294967296 = %4.3f\n", MFRAC, (double)MFRAC/(double)4294967296);
	}
}

void Set_PLL (void *addr, uint32_t counter_select, double out_freq, double duty_cycle, uint32_t enable_message) {
	uint32_t pll_param [TOTAL_PLL_PARAM];
	//printf("\nduty cycle: %f\n",duty_cycle);
	if (pll_calculator (pll_param, out_freq, INPUT_FREQ)) { // frequency can be implemented
		Set_M(addr, pll_param, enable_message);
		Set_MFrac (addr, pll_param, enable_message);
		Set_N(addr, pll_param, enable_message);
		Set_C (addr, pll_param, counter_select, duty_cycle, enable_message);
		//Set_DPS (addr, pll_param, counter_select, phase);

		Start_Reconfig(addr,0x00);
		
		if (enable_message) {
			double temp; // general variable to print value
			temp = (double)INPUT_FREQ / (double)*(pll_param+N_COUNTER_ADDR) * ((double)*(pll_param+M_COUNTER_ADDR)+((double)*(pll_param+M_FRAC_ADDR)/(double)(4294967296))) / (double)*(pll_param+C_COUNTER_ADDR);
			printf("Actual frequency\t: %5.2f MHz\n",temp);
			uint32_t reg_value = alt_read_word(addr+CNT_READ_ADDR[counter_select]);
			temp = (double)((reg_value & 0xFF00) >> 8)/(double)((reg_value & 0xFF) + ((reg_value & 0xFF00) >> 8));
			printf("Actual duty cycle\t: %5.2f %%\n",temp*100);
		}
	}
	else {	// frequency cannot be implemented
		printf("Set_PLL failed! Desired frequency was failed to be found!\n");
	}
	
}
