/*
 * PlasmaDriver.c
 *
 *  Created on: Jan 30, 2023
 *      Author: Nicole Brown
 */

#include "main.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"
#include "stm32h7xx_hal.h"

//External handles
extern UART_HandleTypeDef huart3;
extern TIM_HandleTypeDef htim1;
extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern ADC_HandleTypeDef hadc3;

#define MAX_INPUT 20

// Debug output if debug = 1
static uint16_t debug = 1;

// *** Menu table ***
#define MAX_MENU_SIZE 12
static char *menu[MAX_MENU_SIZE];
static uint16_t menu_size;
#define CONFIG_MENU_SIZE 2
static char *config_menu[CONFIG_MENU_SIZE];

// ADC clock frequency (Hz)
#define ADC_CLOCK 48000000 // ADC clock = 96MHz/2 = 48MHz

// *** ADC1 and ADC2 setup ***
// The two ADC's, ADC1 and ADC2 are setup to read data at the same time with ADC1 as master and ADC2 as slave.
// Each ADC will read 3 channels in sequence.
// It will take 8.5 cycles to sample and 7.5 cycles for conversion.
// This gives a total of 16 cycles per read.  The ADC frequency is 96MHz/2 = 48MHz, so each read takes 16/48MHz = 0.3333usec.
// The complete time for 3 reads are 3*0.3333usec = 1.000usec
// The DMA is connected to ADC1, and will transfer 32bit for each read [16bit ADC2][16bit ADC3].
// This group of data will consist of 6 data elements of size 16 bit; one from each channel. The read time for one group of data is 1.0000usec.
#define ADC12_NO_CHANNELS 3		// 3 ADC channels for ADC1 and ADC2

//Definition for 6 channels measured by ADC1 & ADC2
#define ADC1_TIM1_CH1		0	//Output from Timer TIM1 CH1 (Used to time when to sample the bridge current)
#define ADC2_Is				1	//Bridge current
#define ADC1_VbriS1			2	//Bridge voltage S1
#define ADC2_VbriS2			3	//Bridge voltage S2
#define ADC1_VplaL1			4	//Plasma voltage L1
#define ADC2_VplaL2			5	//Plasma voltage L2

// The data above defines a group. One group of data will consist of 6 data elements of size 16 bit; one from each channel.
// The number of groups is defined by
#define ADC12_MAX_GROUP 100				// Maximum number of groups
#define ADC12_GROUP_READTIME 1.0000E-6	// Read time for a group in seconds

// The ADC's are setup in continues mode and will repeat reading until all DMA requests have been handled.
// The number of DMA requests is given by (each request contains data from ADC1 and ADC2)
#define ADC12_DMA_REQUESTS  (ADC12_NO_CHANNELS*ADC12_MAX_GROUP)

// The number of 16bit values is given by
#define ADC12_ARRAY_SIZE (2*ADC12_DMA_REQUESTS)

// *** ADC3 setup ***
// ADC3 is setup to read 11 channels, ADC3_INP0 to ADC3_INP10
// It will take 6.5 cycles to sample and 12.5 cycles for conversion.
// This gives a total of 19 cycles per read.  The ADC frequency is 96MHz/2 = 48MHz, so each read takes 19/48MHz = 0.3958usec.
// Reading all 11 channels will take 11*0.3958 = 4.35usec
// The DMA is connected to ADC3, and will transfer 16bit for each channel read.
#define ADC3_NO_CHANNELS 11		// 11 ADC channels for ADC3

// ADC3 is setup up in scan mode to read the 11 channels
// The number of DMA requests is given by
#define ADC3_DMA_REQUESTS  ADC3_NO_CHANNELS

// The number of 16bit values is given by
#define ADC3_ARRAY_SIZE ADC12_DMA_REQUESTS

//Definition for 11 channels measured by ADC3
#define ADC3_VBAT			0	//48V battery: Vbat
#define ADC3_15V			1	//15V Power Supply: V_15V
#define ADC3_3_3V 			2	//3.3V Power Supply: V_3.3V
#define ADC3_NC3			3	//Not Connected
#define ADC3_BridgeTemp		4	//Bridge temp: Rbritemp
#define	ADC3_500VDC			5	//500VDC: Vdc
#define ADC3_Thermistor1	6	//Thermistor1
#define ADC3_Thermistor2	7	//Thermistor2
#define ADC3_Thermistor3	8	//Thermistor3
#define ADC3_Thermistor4	9	//Thermistor4
#define ADC3_NC10			10	//Not Connected

// Timer base clock frequency (Hz)
#define TIMER_BASE_CLOCK 34375000

//ADC data definition
typedef struct
{
	uint16_t adc12_data[ADC12_ARRAY_SIZE];	//Array containing the ADC1 and ADC2 data
	uint16_t adc3_data[ADC3_ARRAY_SIZE];	//Array containing the ADC3 data
	uint32_t nADC12Read;					//Number of ADC12 group reads
	volatile uint16_t adc3_reading;			//Set to 1 when ADC3 starts reading and to 0 when it is done (interrupt)
	volatile uint16_t adc12_reading;		//Set to 1 when ADC1 and ADC2 starts reading and to 0 when it is done (interrupt)
} ADC_t;
static ADC_t sADC;

//ADC3 Threshold data (TODO This should be part of flash memory)
uint16_t sADC3threshold[] = {3252,	// Vbat:   48V*9.76k/(9.76k+169k) = 2.62V, 2.62V/3.3V*4096 = 3252
							 3600,	// V_15V:  14.5V*30k/(30k+120k) = 2.90V, 2.90V/3.3V*4096 = 3600
							 3389,	// V_3.3V: 3V*30k/(30k+3k) = 2.73V, 2.73V/3.3V*4096 = 3389
							 0,		// NC
							 0,		// Bridge temp
							 3326,	// 500VDC: 450V*12k/(12k+2M) = 2.68V, 2.68V/3.3V*4096 = 3326
							 		// 30VDC Test board limit is 2.68V*(220k+2M)/220k = 27.0V
							 0,		// Thermistor1
							 0,		// Thermistor2
							 0,		// Thermistor3
							 0,		// Thermistor4
							 0};	// NC


// H-bridge data definition
#define MIN_FREQUENCY 15000
#define MAX_FREQUENCY 65000
#define MIN_DEADTIME  1
#define MAX_DEADTIME  40
typedef struct
{
	uint16_t on;		//1=On 0=Off
	uint16_t frequency;	//Current frequency (Hz)
	uint16_t deadtime;  //Current dead time (%)
} Hbridge_t;
static Hbridge_t sHbridge = {0, 30000, 35};

#define POWERON_SUCCEEDED 1	//Power on succeeded
#define POWERON_FAILED 	  0	//Power on failed
#define V500_OFF 0	//Powered off
#define V500_ON  1	//Powered On (550V)
static uint16_t powerStatus = V500_OFF;


#define FLASH_WORD 32	// One flash word is 8*4 = 32 bytes
#define FLASH_SECTOR7_START_ADDR 0x080E0000	// Flash sector 7 start address

// Data used to erase flash sector 7, bank 1: 0x080E0000- 0x080FFFFF (128 K)
static FLASH_EraseInitTypeDef sFlashErase = {FLASH_TYPEERASE_SECTORS, FLASH_BANK_1, 7, 1, FLASH_VOLTAGE_RANGE_3};

// Plasma configuration stored in flash memory
#define TEST_MODE 0		//This enables the test mode menu, so each component can be tested individually
#define RUN_MODE  1		//This enables the run mode.
typedef struct
{
	uint8_t mode;		//Test mode or Run mode
} FlashConfig_t;
static FlashConfig_t sFlashConfig = {TEST_MODE};

// Prototypes
void measureVoltagesTemperaturesADC3(void);

// Write configuration to flash
static uint32_t writeConfigFlash(void)
{
	uint32_t faultySector;	//Contains error code for faulty sector
	uint32_t error_code = 0;

	// Unlock Flash
	HAL_FLASH_Unlock();

	// Erase flash sector 7
	if (HAL_FLASHEx_Erase(&sFlashErase, &faultySector) == HAL_OK)
	{
		for (uint32_t offset = 0; offset < sizeof(sFlashConfig); offset += FLASH_WORD)
		{
			// Program one flash word (8*4 bytes)
			if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, FLASH_SECTOR7_START_ADDR + offset, ((uint32_t) &sFlashConfig) + offset) != HAL_OK)
			{
				// Error during flash write
				error_code = HAL_FLASH_GetError();
			}
		}
	}
	else
	{
		error_code = HAL_FLASH_GetError();
	}

	  // Lock Flash
	  HAL_FLASH_Lock();

	  return error_code;
}

// Read configuration from flash
static void readConfigFlash(void)
{
	// Read configuration from flash
	memcpy(&sFlashConfig, (void *) FLASH_SECTOR7_START_ADDR, sizeof(sFlashConfig));
}

// Print CR
static void printCR(void)
{
	HAL_UART_Transmit(&huart3, (uint8_t *) "\n\r", 2, 1000);
}

// Print string on UART3
static void printString(char *str)
{
	HAL_UART_Transmit(&huart3, (uint8_t *) str, strlen(str), 1000);
}

// Print configuration on UART3
static void printConfigFlash(void)
{
	printString("\n\rCurrent configuration:");
	printString("\n\r  Mode = ");
	if (sFlashConfig.mode == TEST_MODE)
		printString("TEST");
	else if (sFlashConfig.mode == RUN_MODE)
		printString("RUN");
	else
		printString("UNKNOWN");
	printCR();
}

// Print 16bit unsigned integer on UART3
static void printNumber(const char *text, uint16_t number, uint8_t CR)
{
	char s_output[100];
	char s_number[7];
	strcpy(s_output, text);
	itoa(number, s_number, 10);
	strcat(s_output, s_number);
	if (CR)
	{
		strcat(s_output, "\n\r");
	}
	HAL_UART_Transmit(&huart3, (uint8_t *) s_output, strlen(s_output), 1000);
}

// Print HAL error status on UART3
static void	printHALErrorStatus(HAL_StatusTypeDef HALresp, const char *text)
{
	char s_output[100];

	switch (HALresp)
	{
		case HAL_ERROR:
			strcpy(s_output, "** HAL ERROR **: ");
		break;

		case HAL_BUSY:
			strcpy(s_output, "** HAL BUSY **: ");
		break;

		case HAL_TIMEOUT:
			strcpy(s_output, "** HAL TIMEOUT **: ");
		break;

		case HAL_OK:
		break;
	}
	strcat(s_output, text);
	printString(s_output);
}

void stopHbridge(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	//Stop TIMER 1 PWM & interrupts
	HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);  //This will stop channel PWM1
	HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_1); //This will stop channel PWM1N

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOE, TIM1_CH1N_Pin|TIM1_CH1_Pin, GPIO_PIN_SET);
	/*Configure GPIO pins : LED_GREEN_Pin LINE_DRIVER1_ENABLE_Pin LINE_DRIVER2_ENABLE_Pin LED_RED_Pin */
	GPIO_InitStruct.Pin = TIM1_CH1N_Pin|TIM1_CH1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

}

// Print H-bridge data on UART3
static void printHbridgeData(void)
{
	char s_output[100];
	sprintf(s_output, "\n\rH-bridge 1=On 0=Off: %u, Frequency: %u (Hz), Dead time: %u (%%)\n\r", sHbridge.on, sHbridge.frequency, sHbridge.deadtime);
	HAL_UART_Transmit(&huart3, (uint8_t *) s_output, strlen(s_output), 1000);
}

// Program TIMER 1 controlling the H-bridge
static void programHbridge(void)
{
	uint8_t DT, DTG;
	float tDTS = 1E6/((float) TIMER_BASE_CLOCK);  //Minimum step in usec
	float req_dtime_us;
	float timARR_f;
	uint32_t value_int;
	uint32_t timARR, timCCR1, timBDTR;
	char s_output[100];

	//HAL_GPIO_WritePin(TEST_OUTPUT_GPIO_Port, TEST_OUTPUT_Pin, GPIO_PIN_SET);

	// TIMER 1 has already been initialized using HAL.
	// Program the H-bridge TIMER 1 - Clock TIMER_BASE_CLOCK

	// Calculate the period and duty cycle based on the frequency requested.
	timARR_f = ((float) TIMER_BASE_CLOCK) / ((float) sHbridge.frequency);
	timARR = (uint32_t) (timARR_f + 0.5);							// Set period counter
	timCCR1 = timARR / 2;  											// 50% duty cycle

	// Print out the set frequency
	if (debug == 1)
	{
		value_int = ((uint32_t) TIMER_BASE_CLOCK) / timARR;
		sprintf(s_output, "\n\rSet frequency (Hz): %lu\n\r", value_int);
		printString(s_output);
	}


	//  Reference manual page 1643: https://www.st.com/resource/en/reference_manual/rm0468-stm32h723733-stm32h725735-and-stm32h730-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf
	//  Bits 7:0 DTG[7:0]: Dead-time generator setup
	//  This bit-field defines the duration of the dead-time inserted between the complementary
	//  outputs. DT correspond to this duration.
	//  Since the timer clock is 34.375 MHz, tDTS = 29.091 nsec
	//  DTG[7:5] = 0xx => DT = DTG[7:0] x tDTG with tDTG = tDTS.			 max 127*tDTS   		: 0.0000 usec - 3.6946 usec
	//  DTG[7:5] = 10x => DT = (64 + DTG[5:0]) x tDTG with tDTG =2xtDTS.	 max (64 + 63)*2*tDTS  	: 3.7236 usec - 7.3890 usec
	//  DTG[7:5] = 110 => DT = (32 + DTG[4:0]) x tDTG with tDTG =8xtDTS.     max (32 + 31)*8*tDTS   : 7.4472 usec - 14.662 usec
	//  DTG[7:5] = 111 => DT = (32 + DTG[4:0]) x tDTG with tDTG = 16 x tDTS. max (32 + 31)*16*tDTS 	: 14.895 usec - 29.324 usec

	// Calculate dead time in usec
	req_dtime_us = (((float) sHbridge.deadtime))*(10000./(float) sHbridge.frequency);

/*
	if (debug == 1)
	{
		value_int = (uint32_t) 1000*req_dtime_us;
		sprintf(s_output, "\n\rRequested dead time (ns): %lu\n\r", value_int);
		printString(s_output);

		value_int = (uint32_t) 127*1000*tDTS;
		uint32_t res_int = 1000*tDTS;
		sprintf(s_output, "Max 1 (ns): %6lu %6lu\n\r", value_int, res_int);
		printString(s_output);

		value_int = (uint32_t) 127*2*1000*tDTS;
		res_int = 1000*2*tDTS;
		sprintf(s_output, "Max 2 (ns): %6lu %6lu\n\r", value_int, res_int);
		printString(s_output);

		value_int = (uint32_t) 63*8*1000*tDTS;
		res_int = 1000*8*tDTS;
		sprintf(s_output, "Max 3 (ns): %6lu %6lu\n\r", value_int, res_int);
		printString(s_output);

		value_int = (uint32_t) 63*16*1000*tDTS;
		res_int = 1000*16*tDTS;
		sprintf(s_output, "Max 4 (ns): %6lu %6lu\n\r", value_int, res_int);
		printString(s_output);
	}
*/

	// Minimum allowed dead time is 1 usec.
	if (req_dtime_us < 1.0)
		req_dtime_us = 1.0;

	if (req_dtime_us <= 127*tDTS) //3.6946
	{
		DT = (uint8_t) (req_dtime_us/tDTS + 0.5);
		if (DT > 127)
			DT = 127;
		DTG = DT;
	}
	else if (req_dtime_us <= 127*2*tDTS) //7.3890
	{
		DT = (uint8_t) ((req_dtime_us/(2*tDTS)) - 64 + 0.5);
		if (DT > 63)
			DT = 63;
		DTG = DT + 0x80;
	}
	else if (req_dtime_us <= 63*8*tDTS) //14.662
	{
		DT = (uint8_t) ((req_dtime_us/(8*tDTS)) - 32 + 0.5);
		if (DT > 31)
			DT = 31;
		DTG = DT + 0xC0;
	}
	else if (req_dtime_us <= 63*16*tDTS) //29.324
	{
		DT = (uint8_t) ((req_dtime_us/(16*tDTS)) - 32 + 0.5);
		if (DT > 31)
			DT = 31;
		DTG = DT + 0xE0;
	}
	else
		DTG = 255;

	uint32_t temp = TIM1->BDTR & 0xFFFFFF00;		//Mask out DTG
	timBDTR = temp | DTG;							//Add new DTG

	//Change timer1 settings
	htim1.Init.Period = timARR;   // Updating internal structure for timer
	TIM1->ARR = timARR;			  // Update period
	TIM1->CCR1 = timCCR1;		  // Update duty cycle
	TIM1->BDTR = timBDTR;	      // Update dead time

	if (debug == 1) {
		//sprintf(s_output, "ARR %lu CCR1 %lu BDTR %lu", timARR, timCCR1, timBDTR & 0xFF);
		//printString(s_output);

		float calcDT = 0;
		//  DTG[7:5] = 0xx => DT = DTG[7:0] x tDTG with tDTG = tDTS.			 max 127*tDTS   		: 0.0000 usec - 3.6946 usec
		if ((DTG & 0x80) == 0) {
			calcDT = DTG*tDTS;
		}
		//  DTG[7:5] = 10x => DT = (64 + DTG[5:0]) x tDTG with tDTG =2xtDTS.	 max (64 + 63)*2*tDTS  	: 3.7236 usec - 7.3890 usec
		if ((DTG & 0xC0) == 0x80) {
			calcDT = (64 + (DTG & 0x3F))*2*tDTS;
		}
		//  DTG[7:5] = 110 => DT = (32 + DTG[4:0]) x tDTG with tDTG =8xtDTS.     max (32 + 31)*8*tDTS   : 7.4472 usec - 14.662 usec
		if ((DTG & 0xE0) == 0xC0) {
			calcDT = (32 + (DTG & 0x1F))*8*tDTS;
		}
		//  DTG[7:5] = 111 => DT = (32 + DTG[4:0]) x tDTG with tDTG = 16 x tDTS. max (32 + 31)*16*tDTS 	: 14.895 usec - 29.324 usec
		if ((DTG & 0xE0) == 0xE0) {
			calcDT = (32 + (DTG & 0x1F))*16*tDTS;
		}
		value_int = (uint32_t) 1000*calcDT;
		sprintf(s_output, "\n\rSet dead time: %lu (ns)\n\r", value_int);
		printString(s_output);
	}

	//Start driving the H-bridge
	if (sHbridge.on && (TIM_CHANNEL_STATE_GET(&htim1, TIM_CHANNEL_1) == HAL_TIM_CHANNEL_STATE_READY))
	{
		HAL_TIM_MspPostInit(&htim1);	//Setup GPIO for timer alternate function
		//Start TIMER 1 PWM & interrupts
		HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);  //This will start channel PWM1
		HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1); //This will start channel PWM1N
	}

	//Stop driving the H-bridge
	if (!sHbridge.on)
	{
		stopHbridge();
	}

	//HAL_GPIO_WritePin(TEST_OUTPUT_GPIO_Port, TEST_OUTPUT_Pin, GPIO_PIN_RESET);

}

#define ADC1_TIM1_CH1		0	//Output from Timer TIM1 CH1 (Used to time when to sample the bridge current)
#define ADC2_Is				1	//Bridge current
#define ADC1_VbriS1			2	//Bridge voltage S1
#define ADC2_VbriS2			3	//Bridge voltage S2
#define ADC1_VplaL1			4	//Plasma voltage L1
#define ADC2_VplaL2			5	//Plasma voltage L2

//Convert ADC1 & ADC2 data to voltages and current
float convertADC12data(uint32_t item, char **text)
{
	float result = 0;
	float V;

	switch (item % (2*ADC12_NO_CHANNELS)) {
		case ADC1_TIM1_CH1:
			result = sADC.adc12_data[item];
			if (text)
				*text ="ADC1_TIM1_CH1";
			break;

		case ADC2_Is:
			V = 3.3*(((float) sADC.adc12_data[item])/65536.0);
			result =  2000*(V - 1.585714)/3.594286;
			if (text)
				*text ="ADC2_Is(mA)";
			break;

		case ADC1_VbriS1:
			result =  1000*((12.0+2000.0)/12.0)*3.3*(((float) sADC.adc12_data[item])/65536.0);
			if (text)
				*text ="ADC1_VbriS1(mV)";
			break;

		case ADC2_VbriS2:
			result =  1000*((12.0+2000.0)/12.0)*3.3*(((float) sADC.adc12_data[item])/65536.0);
			if (text)
				*text ="ADC2_VbriS2(mV)";
			break;

		case ADC1_VplaL1:
			V = 3.3*(((float) sADC.adc12_data[item])/65536.0);
			result =  1E6*(V-1.648348)/0.999;
			if (text)
				*text ="ADC1_VplaL1(mV)";
			break;

		case ADC2_VplaL2:
			V = 3.3*(((float) sADC.adc12_data[item])/65536.0);
			result =  1E6*(V-1.648348)/0.999;
			if (text)
				*text ="ADC2_VplaL2(mV)";
			break;
	}

	return(result);
}

//Print measured ADC1 and ADC2 data on UART3
void printADC12data(void)
{
	char *p_text[2*ADC12_NO_CHANNELS];
	float result[2*ADC12_NO_CHANNELS];

	if (debug == 1)
	{
		printCR();
		printNumber("No data: ", sADC.nADC12Read, 1);
		for (int i=0; i<2*ADC12_NO_CHANNELS*sADC.nADC12Read; i++)
		{
			int ii = i % (2*ADC12_NO_CHANNELS);
			result[ii] = convertADC12data(i, &p_text[ii]);
			char text[300];
			if (ii == 5)
			{
				sprintf(text, "%2u ADC12: %s (%5u) %s %4i (%5u) %s %6i (%5u) %s %6i (%5u) %s %7i (%5u) %s %7i (%5u)\n\r", i / 6,
						p_text[ii-5],                     sADC.adc12_data[i-5],
						p_text[ii-4], (int) result[ii-4], sADC.adc12_data[i-4],
						p_text[ii-3], (int) result[ii-3], sADC.adc12_data[i-3],
						p_text[ii-2], (int) result[ii-2], sADC.adc12_data[i-2],
						p_text[ii-1], (int) result[ii-1], sADC.adc12_data[i-1],
						p_text[ii],   (int) result[ii],   sADC.adc12_data[i]);
				printString(text);
			}
		}
	}
}

//Calculate frequency correction
//Returns 1 if a valid frequency correction is calculated, otherwise 0
uint8_t freqCorrection(int16_t *freqCorr)
{
	int start_index=0;
	int stop_index=0;
	int number_of_lows=0;
	int lowDetected = 0;
	int highDetected = 0;
	float min = 100000;
	float max = -100000;
	float norm = 0;

	//Find when MOSFET branch is on (start and stop time)
	//Find minimum and maximum value of bridge current
	for (int i=0; i<2*ADC12_NO_CHANNELS*sADC.nADC12Read; i=i+6)
	{
		// Find minimum of bridge current
		float data = convertADC12data(i+ADC2_Is, NULL);
		if (data < min)
			min = data;
		// Find maximum
		if (data > max)
			max = data;
		// Check for low
		if (!lowDetected && sADC.adc12_data[i+ADC1_TIM1_CH1] < 500)
		{
			//First low detected
			start_index = i; 		// Store index of first low detected
			lowDetected = 1;
		}
		//Check for high
		if (!highDetected && lowDetected && sADC.adc12_data[i+ADC1_TIM1_CH1] > 65000)
		{
			//High after first low detected
			stop_index = i-6;		// Store index of last low
			highDetected = 1;
		}
	}

	// Check maximum and minimum difference
	norm = max - min;
	if (norm < 10)
		norm = max;

	number_of_lows = (stop_index - start_index)/6 + 1;
	if (lowDetected && highDetected && (number_of_lows >= 5))
	{
		float upper = convertADC12data(start_index+ADC2_Is+6, NULL);
		float lower = convertADC12data(stop_index+ADC2_Is-6, NULL);
		*freqCorr = (int16_t) 1000*(upper - lower)/norm;
		return(1);
	}
	else
		return(0);
}


// Measure bridge current, plasma voltage, and bridge current using ADC1 and ADC2 for one period
// After the measurement is done the function doneMeasuringBridgePlasmaADC12 is called
void measureBridgePlasmaADC12(void)
{
	//HAL_GPIO_WritePin(TEST_OUTPUT_GPIO_Port, TEST_OUTPUT_Pin, GPIO_PIN_SET);

	//Calculate number of reads needed for one period
	sADC.nADC12Read = (uint32_t) ((1/(float) sHbridge.frequency)/ADC12_GROUP_READTIME);
	sADC.nADC12Read +=2; //Add to see the start of next period

	//Start ADC1 and ADC2 measurements
	if (sADC.nADC12Read <= ADC12_MAX_GROUP)
	{
		//Calculate the number DMA transfers needed
		uint32_t noDMARequests = ADC12_NO_CHANNELS*sADC.nADC12Read;

		// Start reading ADC1 and ADC2
		sADC.adc12_reading = 1;

		//This will start the ADC1 and ADC2 measurements when H-BRIDGE_B_CTRL (TIM1_CH1) goes from 0 to 1.
		//When the measurements are done doneMeasuringBridgePlasmaADC12 is called.
		HAL_StatusTypeDef HALresp = HAL_ADCEx_MultiModeStart_DMA(&hadc1, (uint32_t *) sADC.adc12_data, noDMARequests);
		if (HALresp != HAL_OK)
		{
			printHALErrorStatus(HALresp, "measureBridgePlasmaADC12");
		}
	}
	else
		printString("** ERROR ** pADC.nRead > ADC12_MAX_GROUP");

	//HAL_GPIO_WritePin(TEST_OUTPUT_GPIO_Port, TEST_OUTPUT_Pin, GPIO_PIN_RESET);
}

// Done measuring bridge current, plasma voltage, and bridge current
void doneMeasuringBridgePlasmaADC12(uint32_t errorCode)
{
	static uint32_t count = 0;

	//HAL_GPIO_WritePin(TEST_OUTPUT_GPIO_Port, TEST_OUTPUT_Pin, GPIO_PIN_SET);

	count++;

	// Done reading ADC1 and ADC2
	sADC.adc12_reading = 0;

	if (errorCode == HAL_ADC_ERROR_NONE)
	{
		if (sFlashConfig.mode == RUN_MODE)
		{
			//TODO Calculate bridge voltage Vmax and Vmin
			//TODO Check bridge voltage VbriS1 and VbriS2 (To high? Not present?)

			//TODO Calculate bridge current Irms, Imax and Imin
			//TODO Check bridge current Is (To high?)

			//TODO Check plasma voltage VplaL1 and VplaL2 (To high? Not present?)
			//TODO Calculate plasma voltage Vrms, Vmax and Vmin

			//Adjust H-bridge frequency
			if (powerStatus == V500_ON)
			{
				//Adjust H-bridge frequency
				int16_t freqCorr = 0;
				if (freqCorrection(&freqCorr))
					sHbridge.frequency += freqCorr;
				//TODO Adjust H-bridge dead time
				//sHbridge.deadtime = new setting;
				programHbridge();
				HAL_Delay(1);	//Allow H-bridge to settle with new settings
				if (count % 2048)
					printHbridgeData();
			}
		}
	}
	else
	{
		printNumber("ADC12 Error Code: ", errorCode, 1);
	}

	//Measure ADC3 voltages
	if (sFlashConfig.mode == RUN_MODE)
		measureVoltagesTemperaturesADC3();

	//HAL_GPIO_WritePin(TEST_OUTPUT_GPIO_Port, TEST_OUTPUT_Pin, GPIO_PIN_RESET);
}

//Convert ADC3 data to voltages
float convertADC3data(uint32_t item, char **text)
{
	float result = 0;

	switch (item) {
		case ADC3_VBAT:
			result =  1000*((9.76+169.0)/9.76)*3.3*(((float) sADC.adc3_data[ADC3_VBAT])/4096.0);
			*text ="ADC3_VBAT (mV)";
			break;
		case ADC3_15V:
			result =  1000*((30.0+120.0)/30.0)*3.3*(((float) sADC.adc3_data[ADC3_15V])/4096.0);
			*text ="ADC3_15V (mV)";
			break;
		case ADC3_3_3V:
			result =  1000*((30.0+3.0)/30.0)*3.3*(((float) sADC.adc3_data[ADC3_3_3V])/4096.0);
			*text ="ADC3_3_3V (mV)";
			break;
		case ADC3_NC3:
			result = 0;
			*text ="ADC3_NC3";
			break;
		case ADC3_BridgeTemp:
			result = 1000*((float) sADC.adc3_data[ADC3_BridgeTemp])*3.3/4096.0;
			*text ="ADC3_BridgeTemp (mV)";
			break;
		case ADC3_500VDC:
			result =  1000*((12.0+2000.0)/12.0)*3.3*(((float) sADC.adc3_data[ADC3_500VDC])/4096.0);
			*text ="ADC3_500VDC (mV)";
			break;
		case ADC3_Thermistor1:
			result = 0;
			*text ="ADC3_Thermistor1";
			break;
		case ADC3_Thermistor2:
			result = 0;
			*text ="ADC3_Thermistor2";
			break;
		case ADC3_Thermistor3:
			result = 0;
			*text ="ADC3_Thermistor3";
			break;
		case ADC3_Thermistor4:
			result = 0;
			*text ="ADC3_Thermistor4";
			break;
		case ADC3_NC10:
			result = 0;
			*text ="ADC3_NC10";
			break;
	}

	return(result);
}

//Print measured ADC3 data on UART3
void printADC3data(void)
{
	if (debug == 1)
	{
		printCR();
		printNumber("No data: ", ADC3_DMA_REQUESTS, 1);
		for (int i=0; i<ADC3_DMA_REQUESTS; i++)
		{
			char text[100];
			char *p_text;
			float result = convertADC3data(i, &p_text);
			sprintf(text, "%2u %20s: %7u    (%6u)\n\r", i, p_text, (int) result, sADC.adc3_data[i]);
			printString(text);
		}
	}
}

// Measure voltages and temperatures using ADC3
// After the measurement is done the function doneMeasuringVoltagesTemperaturesADC3 is called
void measureVoltagesTemperaturesADC3(void)
{
	//HAL_GPIO_WritePin(TEST_OUTPUT_GPIO_Port, TEST_OUTPUT_Pin, GPIO_PIN_SET);

	// This will start the measurement of voltages and temperatures using ADC3
	sADC.adc3_reading = 1; //Started to read ADC3
	HAL_ADC_Start_DMA(&hadc3, (uint32_t *) sADC.adc3_data, ADC3_DMA_REQUESTS);

	//HAL_GPIO_WritePin(TEST_OUTPUT_GPIO_Port, TEST_OUTPUT_Pin, GPIO_PIN_RESET);
}


// Done measuring voltages and temperatures
void doneMeasuringVoltagesTemperaturesADC3(uint32_t errorCode)
{
	//HAL_GPIO_WritePin(TEST_OUTPUT_GPIO_Port, TEST_OUTPUT_Pin, GPIO_PIN_SET);

	sADC.adc3_reading = 0; //Done reading ADC3

	if (errorCode == HAL_ADC_ERROR_NONE)
	{

		// TODO Check voltages

		// TODO Check temperatures

	}
	else
	{
		printNumber("ADC12 Error Code: ", errorCode, 1);
	}


	//Start ADC1 and ADC2 measurements
	if (sFlashConfig.mode == RUN_MODE)
		measureBridgePlasmaADC12();

	//HAL_GPIO_WritePin(TEST_OUTPUT_GPIO_Port, TEST_OUTPUT_Pin, GPIO_PIN_RESET);
}

// Get an integer number from UART3. If the number is valid, the function returns 1, otherwise 0.
static uint8_t GetNumber(int *number)
{
	char input;
	char s_input[MAX_INPUT];
	int pos = 0;
	//Wait for user input
	HAL_UART_Receive(&huart3, (uint8_t *) &input, 1, 100000);
	while (input != 13)
	{
		// Backspace?
		if ((input == 127) && (pos > 0))
		{
			// Delete digit
			HAL_UART_Transmit(&huart3, (uint8_t *) &input, 1, 1000);
			pos--;
		}
		// A digit?
		else if ((input >= 48) && (input <=57))
		{
			// Echo digit and store it
			HAL_UART_Transmit(&huart3, (uint8_t *) &input, 1, 1000);
			s_input[pos++] = input;
		}

		// Get next character
		if (pos < MAX_INPUT-1)
		{
			HAL_UART_Receive(&huart3, (uint8_t *) &input, 1, 100000);
		}
		else
		{
			input = 13; // Terminate while loop
		}
	}
	s_input[pos] = 0;
	if (pos > 0)
		*number = atoi(s_input);
	return(pos > 0);
}

// Get an 16bit unsigned number from UAR3 and validate the input againts min and max values
static uint8_t GetUint16Input(uint16_t *input, uint8_t bValidate, uint16_t min, uint16_t max)
{
	int number;
	char s_output[100];
	uint8_t result;

	result = GetNumber(&number);
	if (bValidate)
	{
		if (result && (number >= min) && (number <= max))
		{
			*input =  number;
			HAL_UART_Transmit(&huart3, (uint8_t *) " - Ok\n\r", 7, 1000);
		}
		else
		{
			result = 0; //Indicate failure
			sprintf(s_output, " - Invalid number, valid range %u - %u\n\r", min, max);
			HAL_UART_Transmit(&huart3, (uint8_t *) s_output, strlen(s_output), 1000);
		}
	}
	return(result);
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef* hadc)
{
	// ADC1 is master and ADC2 is slave
	if (hadc->Instance == ADC1)
	{
		printNumber("** ERROR ** ADC12 Error Code: ", hadc->ErrorCode, 1);
	}
	else if ((hadc->Instance == ADC3))
	{
		printNumber("** ERROR ** ADC3 Error Code: ", hadc->ErrorCode, 1);
	}
}

// ADC conversion and DMA transfer complete
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
	// ADC1 is master and ADC2 is slave
	if (hadc->Instance == ADC1)
	{
		//HAL_ADCEx_MultiModeStop_DMA(&hadc1);
		doneMeasuringBridgePlasmaADC12(hadc->ErrorCode);
	}
	else if (hadc->Instance == ADC3)
	{
		//HAL_ADCEx_MultiModeStop_DMA(&hadc3);
		doneMeasuringVoltagesTemperaturesADC3(hadc->ErrorCode);
	}
	else
	{
		HAL_UART_Transmit(&huart3, (uint8_t *) "** ERROR ** Unknown ADC\n\r", 25, 1000);
		printCR();
	}
}

//Power Off Supplies in order3.3V switch and 15V
void PowerOffLowSupplies(void)
{
	if (powerStatus == V500_OFF)
	{
		//Power off 3.3V switch voltage
		HAL_GPIO_WritePin(OUT_3V3_SWITCH_GPIO_Port, OUT_3V3_SWITCH_Pin, GPIO_PIN_SET);
		HAL_Delay(1);	//Wait 1msec  TODO - Might need to be changed

		//Power off 15V
		HAL_GPIO_WritePin(OUT_15V_ENABLE_GPIO_Port, OUT_15V_ENABLE_Pin, GPIO_PIN_SET);		//There is an inverter between MCU and the output, thus SET
		HAL_Delay(1);	//Wait 1msec  TODO - Might need to be changed
	}
	else
	{
		printString("** ERROR ** PowerOffLowSupplies: 500V is On\n\r");
		printCR();
	}
}


//Power Off Supply 500V
void PowerOffHighSupplies(void)
{
	//Make sure the H-bridge outputs are zero before turning off power
	stopHbridge();

	//Power off 500V
	HAL_GPIO_WritePin(OUT_500V_ENABLE_GPIO_Port, OUT_500V_ENABLE_Pin, GPIO_PIN_SET);	//There is an inverter between MCU and the output, thus SET
	HAL_Delay(1);	//Wait 1msec  TODO - Might need to be changed

	//Disable Line Drive 1
	HAL_GPIO_WritePin(LINE_DRIVER1_ENABLE_GPIO_Port, LINE_DRIVER1_ENABLE_Pin, GPIO_PIN_SET);
	HAL_Delay(1);	//Wait 1msec  TODO - Might need to be changed

	//Signal to robot controller all power supplies are inactive.
	HAL_GPIO_WritePin(LED_ACTIVE_GPIO_Port, LED_ACTIVE_Pin, GPIO_PIN_SET);			//There is an inverter between MCU and the output, thus SET

	powerStatus = V500_OFF;
}

void PowerOffSupplies(void)
{
	PowerOffHighSupplies();
	PowerOffLowSupplies();
}

//Power On Supplies in order 15V, 3.3V switch
//Returns 1 if the power up sequence was successful, and 0 if it failed
int PowerOnLowSupplies(void)
{
	//Power on 15V
	//printString("\n\rPower on 15V - ");
	HAL_GPIO_WritePin(OUT_15V_ENABLE_GPIO_Port, OUT_15V_ENABLE_Pin, GPIO_PIN_RESET);	//There is an inverter between MCU and the output, thus RESET
	printString("\n\rPower on 15V - ");
	HAL_Delay(1);	//Wait 1msec  TODO - Might need to be changed

	//Start reading ADC3 channels
	measureVoltagesTemperaturesADC3();
	//Wait until ADC3 reading is done
	while (sADC.adc3_reading) ;

	//Check 15V voltage
	if (sADC.adc3_data[ADC3_15V] >= sADC3threshold[ADC3_15V])
		printString("Ok");
	else
	{
		printString("Fail");
		PowerOffLowSupplies();
		return(0);
	}

	//Power on 3.3V switch voltage
	printString("\n\rPower on 3.3V switch");
	HAL_GPIO_WritePin(OUT_3V3_SWITCH_GPIO_Port, OUT_3V3_SWITCH_Pin, GPIO_PIN_RESET);
	HAL_Delay(1);	//Wait 1msec  TODO - Might need to be changed

	return(1);
}

//Power On Supply 500V
//Returns 1 if the power up was successful, and 0 if it failed
int PowerOnHighSupplies(void)
{
	stopHbridge(); 	//Make sure the H-bridge outputs are zero before enabling the line driver

	//Enable Line Drive 1
	printString("\n\rEnable Line Drive 1");
	HAL_GPIO_WritePin(LINE_DRIVER1_ENABLE_GPIO_Port, LINE_DRIVER1_ENABLE_Pin, GPIO_PIN_RESET);
	HAL_Delay(1);	//Wait 1msec  TODO - Might need to be changed

	//Power on 500V
	printString("\n\rPower on 500V - ");
	HAL_GPIO_WritePin(OUT_500V_ENABLE_GPIO_Port, OUT_500V_ENABLE_Pin, GPIO_PIN_RESET);	//There is an inverter between MCU and the output, thus RESET
	HAL_Delay(1);	//Wait 1msec  TODO - Might need to be changed

	//Start reading ADC3 channels
	measureVoltagesTemperaturesADC3();
	//Wait until ADC reading is done
	while (sADC.adc3_reading) ;

	//Check 500V voltage
	if (sADC.adc3_data[ADC3_500VDC] >= sADC3threshold[ADC3_500VDC])
		printString("Ok");
	else
	{
		printString("Fail");
		PowerOffHighSupplies();
		return(0);
	}

	//Signal to robot controller all power supplies are active
	HAL_GPIO_WritePin(LED_ACTIVE_GPIO_Port, LED_ACTIVE_Pin, GPIO_PIN_RESET);	//There is an inverter between MCU and the output, thus RESET

	powerStatus = V500_ON;

	return(1);
}

// GPIO interrupt handler
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if (sFlashConfig.mode == RUN_MODE)
	{
		// Power off request received
		if(GPIO_Pin == POWER_OFF_IRQ_Pin)
			PowerOffSupplies();
	}
}

// Initialize the UART3 configuration menu
static void InitializeConfigMenu(void)
{
	int item = 0;

	config_menu[item++] = "\n\rPlasma Driver Configuration Menu";
	config_menu[item++] = "   m: Change mode (Test/Run)";

	if (item > CONFIG_MENU_SIZE)
		HAL_UART_Transmit(&huart3, (uint8_t *) "\n\rIncrease CONFIG_MENU_SIZE", 20, 1000);

}

// Initialize the UART3 menu
static void InitializeMenu(void)
{
	int item = 0;

	//Clear previous menu
	for (int i=0; i < MAX_MENU_SIZE; ++i)
	{
		menu[i] = "";
	}

	if (sFlashConfig.mode == TEST_MODE)
	{
		menu[item++] = "\n\rPlasma Driver TEST Menu";
		menu[item++] = "   p: Power on supplies";
		menu[item++] = "   o: Power off supplies";
		menu[item++] = "   s: Start/stop driving H-Bridge";
		menu[item++] = "   f: Set H-bridge frequency (Hz)";
		menu[item++] = "   d: Set H-bridge dead time (%)";
		menu[item++] = "   a: Start ADC1 and ADC2 measurement";
		menu[item++] = "   b: Start ADC3 measurement";
		menu[item++] = "   q: Frequency correction";
		menu[item++] = "   z: Debug output (On/Off)";
		menu[item++] = "   c: Show/Change configuration";
		menu[item++] = "   t: Test GPIO";
	}
	else if ((sFlashConfig.mode == RUN_MODE))
	{
		menu[item++] = "\n\rPlasma Driver RUN Menu";
		menu[item++] = "   c: Show/Change current configuration";
	}
	else
	{
		menu[item++] = "\n\rERROR SETTING UP MENU";
	}
	menu_size = item;

	if (menu_size> MAX_MENU_SIZE)
		HAL_UART_Transmit(&huart3, (uint8_t *) "\n\rIncrease MENU_SIZE", 20, 1000);

}

// Show configuration. Add option to change configuration
void ShowChangeConfigFlash(void)
{
	uint16_t aYes;
	uint16_t mode;
	char input;

	printConfigFlash();
	printString("\n\rChange configuration (0:No 1:Yes)? ");
	if (GetUint16Input(&aYes, 1, 0, 1))
	{
		if (aYes)
		{
			for (int i=0; i<CONFIG_MENU_SIZE; i++)
			{
				HAL_UART_Transmit(&huart3, (uint8_t *) config_menu[i], strlen(config_menu[i]), 1000);
				printCR();
			}
			printCR();

			//Wait for user input
			if (HAL_UART_Receive(&huart3, (uint8_t *) &input, 1, 60000) == HAL_OK)
			{
				switch (input)
				{
					case 'm': //Change mode
						printString("\n\rChange mode (0:Test 1:Run)? ");
						if (GetUint16Input(&mode, 1, 0, 1))
						{   // Data entry valid
							if (sFlashConfig.mode != mode)
							{
								sFlashConfig.mode = mode;
								InitializeMenu();
								writeConfigFlash();
							}
						}
					break;
				}
			}
		}
	}
}

//Test GPIO settings
void testGPIO(void)
{
/*
		uint16_t aYes;

		printString("\n\rSet OUT_500V_ENABLE");
		HAL_GPIO_WritePin(OUT_500V_ENABLE_GPIO_Port, OUT_500V_ENABLE_Pin, GPIO_PIN_SET);
		GetUint16Input(&aYes, 1, 0, 1);

		printString("\n\rClear OUT_500V_ENABLE");
		HAL_GPIO_WritePin(OUT_500V_ENABLE_GPIO_Port, OUT_500V_ENABLE_Pin, GPIO_PIN_RESET);
		GetUint16Input(&aYes, 1, 0, 1);

		printString("\n\rSet LINE_DRIVER1_ENABLE");
		HAL_GPIO_WritePin(LINE_DRIVER1_ENABLE_GPIO_Port, LINE_DRIVER1_ENABLE_Pin, GPIO_PIN_SET);
		GetUint16Input(&aYes, 1, 0, 1);

		printString("\n\rClear LINE_DRIVER1_ENABLE");
		HAL_GPIO_WritePin(LINE_DRIVER1_ENABLE_GPIO_Port, LINE_DRIVER1_ENABLE_Pin, GPIO_PIN_RESET);
		GetUint16Input(&aYes, 1, 0, 1);

		printString("\n\rSet LINE_DRIVER2_ENABLE");
		HAL_GPIO_WritePin(LINE_DRIVER2_ENABLE_GPIO_Port, LINE_DRIVER2_ENABLE_Pin, GPIO_PIN_SET);
		GetUint16Input(&aYes, 1, 0, 1);

		printString("\n\rClear LINE_DRIVER2_ENABLE");
		HAL_GPIO_WritePin(LINE_DRIVER2_ENABLE_GPIO_Port, LINE_DRIVER2_ENABLE_Pin, GPIO_PIN_RESET);
		GetUint16Input(&aYes, 1, 0, 1);

		printString("\n\rSet OUT_3V3_SWITCH");
		HAL_GPIO_WritePin(OUT_3V3_SWITCH_GPIO_Port, OUT_3V3_SWITCH_Pin, GPIO_PIN_SET);
		GetUint16Input(&aYes, 1, 0, 1);

		printString("\n\rClear OUT_3V3_SWITCH");
		HAL_GPIO_WritePin(OUT_3V3_SWITCH_GPIO_Port, OUT_3V3_SWITCH_Pin, GPIO_PIN_RESET);
		GetUint16Input(&aYes, 1, 0, 1);

		printString("\n\rSet OUT_15V_ENABLE");
		HAL_GPIO_WritePin(OUT_15V_ENABLE_GPIO_Port, OUT_15V_ENABLE_Pin, GPIO_PIN_SET);
		GetUint16Input(&aYes, 1, 0, 1);

		printString("\n\rClear OUT_15V_ENABLE");
		HAL_GPIO_WritePin(OUT_15V_ENABLE_GPIO_Port, OUT_15V_ENABLE_Pin, GPIO_PIN_RESET);
		GetUint16Input(&aYes, 1, 0, 1);

		printString("\n\rSet LED_ACTIVE");
		HAL_GPIO_WritePin(LED_ACTIVE_GPIO_Port, LED_ACTIVE_Pin, GPIO_PIN_SET);
		GetUint16Input(&aYes, 1, 0, 1);

		printString("\n\rClear LED_ACTIVE");
		HAL_GPIO_WritePin(LED_ACTIVE_GPIO_Port, LED_ACTIVE_Pin, GPIO_PIN_RESET);
		GetUint16Input(&aYes, 1, 0, 1);

		printString("\n\rSet LED_GREEN");
		HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);
		GetUint16Input(&aYes, 1, 0, 1);

		printString("\n\rClear LED_GREEN");
		HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
		GetUint16Input(&aYes, 1, 0, 1);
*/
}

// Action table for the run mode menu
static void RunModeAction(char input)
{
	switch (input)
	{
		case 'c': //Show/Change current configuration
			ShowChangeConfigFlash();
			break;
	}
}

// Action table for the test mode menu
static void TestModeAction(char input)
{
	switch (input)
	{
		case 'p': //Power supplies on
			if (powerStatus == V500_OFF)
			{
				if (PowerOnHighSupplies() == POWERON_FAILED)
				{
					printString("\n\rPower on failed\n\r");
				}
				else
				{
					printString("\n\rPower on succeeded\n\r");
				}
			}
			else
			{
				printString("\n\rPower supplies are on\n\r");
			}
			break;

		case 'o': //Power supplies off
			printString("\n\rPower off supplies\n\r");
			PowerOffHighSupplies();
			break;

		case 'a': //Start ADC1 and ADC2 analog voltage measurement
			if (sHbridge.on)
			{
				//Setup to read ADC1 and ADC2 channels
				//Reading will start at TIM1 CH1 0->1 interrupt
				measureBridgePlasmaADC12();
				//Wait until ADC3 reading is done
				while (sADC.adc12_reading) ;
				//Print ADC3 data on UART3
				printADC12data();
			}
			else
			{
				printString("\n\r ---- H-Bridge needs to be on");
			}
			break;

		case 'b': //Start ADC3 analog voltage measurement
				//Start reading ADC3 channels
				measureVoltagesTemperaturesADC3();
				//Wait until ADC3 reading is done
				while (sADC.adc3_reading) ;
				//Print ADC3 data on UART3
				printADC3data();
			break;

		case 'f': //Enter frequency (Hz)
			printHbridgeData();
			printString("\n\rEnter frequency (Hz): ");
			if (GetUint16Input(&sHbridge.frequency, 1, MIN_FREQUENCY, MAX_FREQUENCY))
			{   // Data entry valid
				programHbridge();
				printHbridgeData();
			}
			break;

		case 'd': //Enter dead time (%)
			printHbridgeData();
			printString("\n\rEnter dead time (%): ");
			if (GetUint16Input(&sHbridge.deadtime, 1, MIN_DEADTIME, MAX_DEADTIME))
			{   // Data entry valid
				programHbridge();
				printHbridgeData();
			}
			break;

		case 's': //Start/stop driving H-bridge
			printHbridgeData();
			printString("\n\rDrive H-bridge 1:Yes 0:No : ");
			if (GetUint16Input(&sHbridge.on, 1, 0, 1))
			{   // Data entry valid
				programHbridge();
				printHbridgeData();
			}
			break;

		case 'q': //Frequency correction
			int16_t freqCorr;
			if (freqCorrection(&freqCorr))
			{
				char text[100];
				sprintf(text, "\n\rFrequency correction: %i", (int) freqCorr);
				printString(text);
			}
			break;

		case 'z': //Debug output
			printNumber("\n\rCurrent Debug output: ", debug, 0);
			printString("\n\rSet Debug output 1:On 0:Off : ");
			GetUint16Input(&debug, 1, 0, 1);
			break;

		case 'c': //Show/Change current configuration
			ShowChangeConfigFlash();
			break;

		case 't': //Test GPIO
			testGPIO();
			break;
	}
}

// Print the plasma driver menu on UART3, and execute entered command.
static void PlasmaDriverMenu(void)
{
	char input;

	//Wait for user input
	if (HAL_UART_Receive(&huart3, (uint8_t *) &input, 1, 1) == HAL_OK)
	{
		// Return pressed -- Display Menu
		if (input == 13)
		{
			for (int i=0; i<menu_size; i++)
			{
				HAL_UART_Transmit(&huart3, (uint8_t *) menu[i], strlen(menu[i]), 1000);
				printCR();
			}
			printCR();
		}
		else
		{
			if (sFlashConfig.mode == TEST_MODE)
				TestModeAction(input);
			else if (sFlashConfig.mode == RUN_MODE)
				RunModeAction(input);
			else
				printString("\n\r*** ERROR *** Invalid mode");
		}
	}
}

// Initialize the plasma driver
void PlasmaDriverInit(void)
{
	//Enable line driver 2 (HAL has initialized all GPIO)
	HAL_GPIO_WritePin(LINE_DRIVER2_ENABLE_GPIO_Port, LINE_DRIVER2_ENABLE_Pin, GPIO_PIN_RESET);  //Enable = Low
	printString("\n\rEnable Line Drive 2");

	//Power On Supplies in order 15V, 3.3V switch
	PowerOnLowSupplies();

	//Read configuration from flash
	readConfigFlash();

	//Has the flash configuration been initialized?
	if (sFlashConfig.mode == 0xFF)
	{
		// write default configuration to flash
		if (writeConfigFlash() != 0)
			printString("\n\r*** ERROR FLASH");
	}

	//Initialize menu and configuration menu
	InitializeMenu();
	InitializeConfigMenu();

	//Calibrate ADC1, ADC2 and ADC3
	printString("\n\rCalibrate ADC1, ADC2 and ADC3");
	HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED);
	HAL_ADCEx_Calibration_Start(&hadc2, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED);
	HAL_ADCEx_Calibration_Start(&hadc3, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED);

	//Power On Supply 500V
	if (sFlashConfig.mode == RUN_MODE)
	{
		//POWER_OFF_IRQ needs to be high before powering On
		if (HAL_GPIO_ReadPin(POWER_OFF_IRQ_GPIO_Port, POWER_OFF_IRQ_Pin) == GPIO_PIN_RESET)		//There is an inverter between MCU and the input, thus check RESET
			PowerOnHighSupplies();
		else
		printString("\n\rPOWER_OFF_IRQ line is not high - 500V is not powered on");
	}
}

// This command is called from a while loop, and will execute any needed task.
void PlasmaDoTask(void)
{
	PlasmaDriverMenu(); //Check UART3 and execute command

	if (sFlashConfig.mode == RUN_MODE)
	{
		//Check if robot controller requested the plasma On or Off
		if (HAL_GPIO_ReadPin(TURN_PLASMA_ON_GPIO_Port, TURN_PLASMA_ON_Pin) == GPIO_PIN_RESET)		//There is an inverter between MCU and the input, thus check RESET
		{
			if (powerStatus == V500_ON)
			{
				sHbridge.on = 1; // Turn Hbridge on
				programHbridge();
			}
			else
				printString("\n\rUnable to turn H-bridge on, since 500V is not powered on");
		}
	}
}

