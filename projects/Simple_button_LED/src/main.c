#include "stm32f4xx.h"

#define LED_PIN 14 //PD14
#define BTN_PIN 0  //PA0


// Simple downward counter to act as a delay
static void delay(volatile uint32_t n)
{
    while (n--) { }          // volatile stops the compiler removing the empty loop
}

// Simple Button + LED
int main(void)
{
    // Enable the necessary clocks
    // RCC: Peripheral that manages the chip's clock and resets
    // AHB1ENR: Register that enables the clocks of peripherals at AHB1
    // RCC_AHB1ENR_GPIOAEN: Macro that enables GPIOA
    // RCC_AHB1ENR_GPIODEN: Macro that enables GPIOD
    // GPIOA: Port where the button pin is located
    // GPIOD: Port where the LED pin is located
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;        // Clock on for GPIOA
    (void)RCC->AHB1ENR;                         // Short settle delay
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;        // Clock on for GPIOD
    (void)RCC->AHB1ENR;                         // Short settle delay

    // Clear GPIOA and GPIOD register and then set the pin modes in the mode register
    // MODER: 32-bit register that holds the mode of all 16 pins of a port
    GPIOA->MODER &= ~(3U << (BTN_PIN * 2));     // Set BTN bits to 00
    GPIOD->MODER &= ~(3U << (LED_PIN * 2));     // Clear LED bits to 00
    GPIOD->MODER |=  (1U << (LED_PIN * 2));     // Set LED bits to 01

    while (1) {
        // Make the LED turn on for a fixed time upon a button press
        // IDR: Input data register that holds the level of the input pin
        if (GPIOA->IDR & (1U << BTN_PIN)) {
            // ODR: Output data register that holds the level of the output pin
            GPIOD->ODR |= (1U << LED_PIN);       // Set the value of the LED_PIN
            delay(2000000);                      // Delay function to adjust time
            GPIOD->ODR &= ~(1U << LED_PIN);      // Clear the value of the LED_PIN        
        }
    }
}