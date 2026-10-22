#include "stm32f4xx.h"

#define LED_PIN 14

// Simple downward counter to act as a delay
static void delay(volatile uint32_t n)
{
    while (n--) {
    } // volatile stops the compiler removing the empty loop
}

// Simple LED blinker
int main(void)
{
    // Enable the necessary clocks
    // RCC: Peripheral that manages the chip's clock and resets
    // AHB1ENR: Register that enables the clocks of peripherals at AHB1
    // RCC_AHB1ENR_GPIODEN: Macro that enables GPIOD
    // GPIOD: Port where the LED pin is located
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN; // Clock on
    (void)RCC->AHB1ENR;                  // Short settle delay

    // Clear GPIOD register and then set the LED pin as output in the mode register
    // MODER: 32-bit register that holds the mode of all 16 pins of a port
    GPIOD->MODER &= ~(3U << (LED_PIN * 2)); // Clear bits (LED_PIN*2)+1:(LED_PIN*2) to 00
    GPIOD->MODER |= (1U << (LED_PIN * 2));  // Set bits (LED_PIN*2)+1:(LED_PIN*2) to 01

    while (1) {
        // Make the LED blink
        // ODR: Output data register that holds the level of the output pin
        GPIOD->ODR ^= (1U << LED_PIN); // Toggle the value of the LED_PIN
        delay(500000);                 // Delay function to adjust blink timing
    }
}