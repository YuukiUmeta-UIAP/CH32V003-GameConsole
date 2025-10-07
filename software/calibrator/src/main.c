// ===================================================================================
// Project:   Joypad Calibrator
// Version:   v1.0
// Year:      2023
// Author:    Stefan Wagner
// Github:    https://github.com/wagiminator
// EasyEDA:   https://easyeda.com/wagiminator
// License:   http://creativecommons.org/licenses/by-sa/3.0/
// ===================================================================================
//
// Description:
// ------------
// Prints ADC-values of joypad-buttons on OLED for calibration.

// ===================================================================================
// Libraries, Definitions and Macros
// ===================================================================================
#include <driver.h>           // TinyJoypad conversion driver

// ===================================================================================
// Main Function
// ===================================================================================
int main(void) {

  // Seamless Switch
  if (FLASH->STATR & (1<<14)) NVIC_SystemReset();
  FLASH->KEYR = 0x45670123;
  FLASH->KEYR = 0xCDEF89AB;
  FLASH->BOOT_MODEKEYR = 0x45670123;
  FLASH->BOOT_MODEKEYR = 0xCDEF89AB;
  FLASH->STATR |= (1<<14);
  FLASH->CTLR = 0x00000080;
  PIN_output( PD4 );

  // Setup
  JOY_init();

  OLED_println("Hello World!");
  OLED_println("Tiny CH32 Pad Lite has NO ADC when default conf. You can re-Upload after push 'TrR' button in the upper right of this board.");

  // Loop
  while(1) {
    OLED_printD(ADC_read()); OLED_write('\n');
    DLY_ms(500);
  }
}
