/*
 * main.c
 *
 *  Created on: Feb 24, 2025
 *      Author: Student
 */


/******************************************************************************/
/*                                                                            */
/* main.c -- Example program using the PmodBT2 IP                             */
/*                                                                            */
/******************************************************************************/
/* Author: Arthur Brown                                                       */
/*                                                                            */
/******************************************************************************/
/* File Description:                                                          */
/*                                                                            */
/* This demo continuously polls the Pmod BT2 and host development board's     */
/* UART connections and forwards each character from each to the other.       */
/*                                                                            */
/******************************************************************************/
/* Revision History:                                                          */
/*                                                                            */
/*    10/04/2017(artvvb):   Created                                           */
/*    01/30/2018(atangzwj): Validated for Vivado 2017.4                       */
/*                                                                            */
/******************************************************************************/

#include "PmodBT2.h"
#include "PmodJSTK2.h"
#include "xil_cache.h"
#include "xgpio.h"
#include "xparameters.h"
#include "sleep.h"


// Required definitions for sending & receiving data over host board's UART port
#ifdef __MICROBLAZE__
#include "xuartlite.h"
typedef XUartLite SysUart;
#define SysUart_Send            XUartLite_Send
#define SysUart_Recv            XUartLite_Recv
#define SYS_UART_DEVICE_ID      XPAR_AXI_UARTLITE_0_DEVICE_ID
#define BT2_UART_AXI_CLOCK_FREQ XPAR_CPU_M_AXI_DP_FREQ_HZ
#else
#include "xuartps.h"
typedef XUartPs SysUart;
#define SysUart_Send            XUartPs_Send
#define SysUart_Recv            XUartPs_Recv
#define SYS_UART_DEVICE_ID      XPAR_PS7_UART_1_DEVICE_ID
#define BT2_UART_AXI_CLOCK_FREQ 100000000
#endif

PmodBT2 myDevice;
PmodJSTK2 joystick;
SysUart myUart;
XGpio gpio;

void DemoInitialize();
void DemoRun();
void SysUartInit();
void EnableCaches();
void DisableCaches();

int main() {
   DemoInitialize();
   DemoRun();
   DisableCaches();
   return XST_SUCCESS;
}
void send_hid_mouse_report(uint8_t buttons, int8_t x, int8_t y, int8_t wheel) {
	uint8_t report[7] = {0xFD, 0x05, 0x02, buttons, x, y, wheel};
	BT2_SendData(&myDevice, report, 7);
}

static unsigned long int next = 1;
static unsigned int lfsr = 0xACE1u;

unsigned int lsfr_rand() {
	lfsr ^= lfsr << 13;
	lfsr ^= lfsr >> 17;
	lfsr ^= lfsr << 5;
	return lfsr;
}

int rand(void) // RAND_MAX assumed to be 32767
{
    next = next * 1103515245 + 12345;
    return (unsigned int)(next/65536) % 32768;
}

void srand(unsigned int seed)
{
    next = seed;
}

void init_rand() {
	srand(lsfr_rand());
}

void DemoInitialize() {
   EnableCaches();
   SysUartInit();
   BT2_Begin (
      &myDevice,
      XPAR_PMODBT2_0_AXI_LITE_GPIO_BASEADDR,
      XPAR_PMODBT2_0_AXI_LITE_UART_BASEADDR,
      BT2_UART_AXI_CLOCK_FREQ,
      115200
   );

   JSTK2_begin(
      &joystick,
      XPAR_PMODJSTK2_0_AXI_LITE_SPI_BASEADDR,
      XPAR_PMODJSTK2_0_AXI_LITE_GPIO_BASEADDR
   );
}

void self_destruct() {
	for (int i = 0; i < 25; i++) {
		send_hid_mouse_report(0, 0b1000000, 0b10000000, 0);
		usleep(20000);
	}
	send_hid_mouse_report(0, -128, 0, 0);
	usleep(20000);
	send_hid_mouse_report(0, -71, 0, 0);
	usleep(20000);
	send_hid_mouse_report(0x01, 0, 0, 0);
	usleep(20000);
	send_hid_mouse_report(0x00, 0, 0, 0);
	usleep(20000);
	send_hid_mouse_report(0, 0, 80, 0);
	usleep(800000);
	send_hid_mouse_report(0x01, 0, 0, 0);
	usleep(800000);
	send_hid_mouse_report(0x00, 0, 0, 0);
	usleep(800000);
	send_hid_mouse_report(0x01, 0, 0, 0);
	usleep(800000);
	send_hid_mouse_report(0x00, 0, 0, 0);
	usleep(800000);
}

void DemoRun() {
   u8 buf[7];
   u32 btn, led;

//   int n = 0;
   int smooth_factor = 3;
   int scroll_state = 0;
   unsigned int rand_num = 11727;

   int btn8_clicked = 0;

   JSTK2_Position position;
   JSTK2_DataPacket rawdata;

   XGpio_Initialize(&gpio, 0);

   XGpio_SetDataDirection(&gpio, 2, 0x00000000);
   XGpio_SetDataDirection(&gpio, 1, 0xFFFFFFFF);

   print("Initialized PmodBT2 Demo\n\r");

//   print("Received data will be echoed here, type to send data\r\n");

   while (1) {
      // Echo all characters received from both BT2 and terminal to terminal
      // Forward all characters received from terminal to BT2

	  position = JSTK2_getPosition(&joystick);
	  rawdata = JSTK2_getDataPacket(&joystick);

	  if (scroll_state == 0) {
		  send_hid_mouse_report(0x00, (position.XData - 128)/smooth_factor, (position.YData - 128)/smooth_factor, 0);
		  usleep(20000);
	  } else {
		  send_hid_mouse_report(0x00, 0, 0, (position.YData - 128)/smooth_factor);
		  usleep(20000);
	  }
	  if (rawdata.Jstk != 0) {
		  u8 prev_state = rawdata.Jstk; // prev_state is 1

		  while (1) {
			  rawdata = JSTK2_getDataPacket(&joystick);
			  if (rawdata.Jstk != prev_state) {
				  send_hid_mouse_report(0x01, 0, 0, 0);
				  usleep(20000);
				  send_hid_mouse_report(0x00, 0, 0, 0);
				  break;
			  }
		  }
	  }

	  if (rawdata.Trigger != 0) {
		  u8 prev_state = rawdata.Trigger; // prev_state is 1

		  while(1) {
			  rawdata = JSTK2_getDataPacket(&joystick);
			  if (rawdata.Trigger != prev_state) {
				  send_hid_mouse_report(0x02, 0, 0, 0);
				  usleep(20000);
				  send_hid_mouse_report(0x00, 0, 0, 0);
				  break;
			  }
		  }
	  }

	  btn = XGpio_DiscreteRead(&gpio, 1);
//	  xil_printf("%08x", btn);
	  if (btn == 1) {
		  init_rand();
		  rand_num = rand();
		  if (rand_num % 2) {
			  self_destruct();
		  } else {
			  smooth_factor = 3;
		  }
	  } else if (btn == 2 && smooth_factor < 50) {
		  smooth_factor = smooth_factor + 1;
	  } else if (btn == 4 && smooth_factor > 1) {
		  smooth_factor = smooth_factor - 1;
	  } else if (btn == 8) {
		  btn8_clicked = 1;
	  } else if (btn8_clicked == 1 && btn != 8) {
		  btn8_clicked = 0;
		  if (scroll_state == 0) {
			  scroll_state = 1;
			  led = 0xFFFFFFFF;
		  } else {
			  scroll_state = 0;
			  led = 0x00000000;
		  }

		  XGpio_DiscreteWrite(&gpio, 2, led);
	  }

      // USED FOR BT2 SETUP (Entering command mode)
//	  n = BT2_RecvData(&myDevice, buf, 7);
//	  if (n != 0) {
//		  SysUart_Send(&myUart, buf, 7);
//	  }
//      n = SysUart_Recv(&myUart, buf, 1);
//      if (n != 0) {
//         SysUart_Send(&myUart, buf, 1);
//         BT2_SendData(&myDevice, buf, 1);
//
//      }
//
//      n = BT2_RecvData(&myDevice, buf, 1);
//      if (n != 0) {
//         SysUart_Send(&myUart, buf, 1);
//      }
   }
}

// Initialize the system UART device
void SysUartInit() {
#ifdef __MICROBLAZE__
   // AXI Uartlite for MicroBlaze
   XUartLite_Initialize(&myUart, SYS_UART_DEVICE_ID);
#else
   // Uartps for Zynq
   XUartPs_Config *myUartCfgPtr;
   myUartCfgPtr = XUartPs_LookupConfig(SYS_UART_DEVICE_ID);
   XUartPs_CfgInitialize(&myUart, myUartCfgPtr, myUartCfgPtr->BaseAddress);
#endif
}

void EnableCaches() {
#ifdef __MICROBLAZE__
#ifdef XPAR_MICROBLAZE_USE_ICACHE
   Xil_ICacheEnable();
#endif
#ifdef XPAR_MICROBLAZE_USE_DCACHE
   Xil_DCacheEnable();
#endif
#endif
}

void DisableCaches() {
#ifdef __MICROBLAZE__
#ifdef XPAR_MICROBLAZE_USE_DCACHE
   Xil_DCacheDisable();
#endif
#ifdef XPAR_MICROBLAZE_USE_ICACHE
   Xil_ICacheDisable();
#endif
#endif
}
