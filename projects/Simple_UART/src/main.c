#include <stdio.h>
#include "stm32f4xx.h"

#define UART_TX_PIN 2  //PA2

#define PCLK1_HZ   16000000U
#define BAUD       115200U

// Simple downward counter to act as a delay
static void delay(volatile uint32_t n)
{
    while (n--) { }          // volatile stops the compiler removing the empty loop
}

// Initialize UART
static void uart_init(void)
{
    // Enable the necessary clocks
    // RCC: Peripheral that manages the chip's clock and resets
    // AHB1ENR: Register that enables the clocks of peripherals at AHB1
    // APB1ENR: Register that enables the clocks of peripherals at AHB1
    // RCC_AHB1ENR_GPIOAEN: Macro that enables GPIOA
    // RCC_APB1ENR_USART2EN: Macro that enables USART
    // GPIOA: Port where the UART pin is located
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;        // Clock on for GPIOA
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;       // Clock on for USART2
    (void)RCC->APB1ENR;                         // Short settle delay

    // Clear GPIOA register and then set the pin modes in the mode register
    // MODER: 32-bit register that holds the mode of all 16 pins of a port
    // AFR: 32-bit register that holds alternate functions for pins. AFR[0] has pins 0-7
    GPIOA->MODER &= ~(3U << (UART_TX_PIN * 2));     // Set PA2 bits to 00
    GPIOA->MODER |=  (2U << (UART_TX_PIN * 2));     // Set PA2 bits to 10
    GPIOA->AFR[0] &= ~(0xFU << (UART_TX_PIN * 4));  // Clear the 4-bit field
    GPIOA->AFR[0] |=  (7U << (UART_TX_PIN * 4));    // Write 7 = AF7 = USART2
    
    // Set baudrate and enable USART
    USART2->BRR = PCLK1_HZ/BAUD;                    // 115200 Bauds
    USART2->CR1 |= USART_CR1_UE | USART_CR1_TE;     // Enable USART and Tx

    setvbuf(stdout, NULL, _IONBF, 0); // No buffering
}

// Send byte
void uart_putc(char c)
{
    while (!(USART2->SR & USART_SR_TXE)) { }
    USART2->DR = (uint8_t)c;
}

// Send string
void uart_puts(const char *s) { while (*s) uart_putc(*s++); }

// Implement write to print in UART using printf
int _write(int fd, char *buf, int len)
{
    for (int i = 0; i < len; i++) {
        if (buf[i] == '\n') uart_putc('\r');
        uart_putc(buf[i]);
    }
    return len;
}

// Main loop
int main(void)
{
    uart_init();
    while (1) {
        printf("UART working!\n");
        for (volatile uint32_t i = 0; i < 500000; i++) { }
    }
}