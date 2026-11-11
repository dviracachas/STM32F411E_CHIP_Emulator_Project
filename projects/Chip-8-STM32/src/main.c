#include <stdint.h>
#include <stdio.h>
#include "stm32f411xe.h"
#include "stm32f4xx.h"
#include "../../Chip-8/core/chip8.h"

#define UART_TX_PIN 2 // PA2

// Pins for ILI9341 display
#define DISP_RST_PIN 3  // PA3
#define DISP_CS_PIN 4   // PA4
#define DISP_SCK_PIN 5  // PA5
#define DISP_DC_PIN 6   // PA6
#define DISP_MOSI_PIN 7 // PA7

// Macros for setting pins
#define DISP_RST_HIGH() (GPIOA->BSRR = (1UL << DISP_RST_PIN))
#define DISP_RST_LOW() (GPIOA->BSRR = (1UL << (DISP_RST_PIN + 16)))
#define DISP_CS_HIGH() (GPIOA->BSRR = (1UL << DISP_CS_PIN))
#define DISP_CS_LOW() (GPIOA->BSRR = (1UL << (DISP_CS_PIN + 16)))
#define DISP_DC_HIGH() (GPIOA->BSRR = (1UL << DISP_DC_PIN))
#define DISP_DC_LOW() (GPIOA->BSRR = (1UL << (DISP_DC_PIN + 16)))

#define DISP_W 320
#define DISP_H 240
#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
#define COLOR_RED 0xF800
#define COLOR_GREEN 0x07E0
#define COLOR_BLUE 0x001F
#define COLOR_BLACK 0x0000
#define COLOR_WHITE 0xFFFF

#define PCLK1_HZ 16000000U
#define BAUD 115200U

static Chip8 chip;

static const uint8_t timer_rom[] = {
    0x60, 0x3C, // 0x200: V0 = 60 (Hz)
    0xF0, 0x15, // 0x202: delay_timer = V0
    0xF1, 0x07, // 0x204: V1 = delay_timer
    0x31, 0x00, // 0x206: skip next if V1 == 0
    0x12, 0x04, // 0x208: Jump to 0x204 (Wait for delay)
    0x62, 0x00, // 0x20A: V2 = 0
    0x63, 0x00, // 0x20C: V3 = 0
    0xA0, 0x91, // 0x20E: I = 0x91 (Font digit D)
    0xD2, 0x35, // 0x210: Draw 5 rows at (V2, V3)
    0x12, 0x12  // 0x212: Jump to itself (Repeat)
};

volatile uint32_t ms_ticks;
void SysTick_Handler(void)
{
    ms_ticks++;
}

static void delay_ms(uint32_t ms)
{
    uint32_t t = ms_ticks;
    while ((uint32_t)(ms_ticks - t) < ms) {
    }
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
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;  // Clock on for GPIOA
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN; // Clock on for USART2
    (void)RCC->APB1ENR;                   // Short settle delay

    // Clear GPIOA register and then set the pin modes in the mode register
    // MODER: 32-bit register that holds the mode of all 16 pins of a port
    // AFR: 32-bit register that holds alternate functions for pins. AFR[0] has pins 0-7
    GPIOA->MODER &= ~(3U << (UART_TX_PIN * 2));    // Set PA2 bits to 00
    GPIOA->MODER |= (2U << (UART_TX_PIN * 2));     // Set PA2 bits to 10
    GPIOA->AFR[0] &= ~(0xFU << (UART_TX_PIN * 4)); // Clear the 4-bit field
    GPIOA->AFR[0] |= (7U << (UART_TX_PIN * 4));    // Write 7 = AF7 = USART2

    // Set baudrate and enable USART
    USART2->BRR = PCLK1_HZ / BAUD;              // 115200 Bauds
    USART2->CR1 |= USART_CR1_UE | USART_CR1_TE; // Enable USART and Tx

    setvbuf(stdout, NULL, _IONBF, 0); // No buffering
}

// Send byte using UART
void uart_putc(char c)
{
    while (!(USART2->SR & USART_SR_TXE)) {
    }
    USART2->DR = (uint8_t)c;
}

// Send string using UART
void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

// Implement write to print in UART using printf
int _write(int fd, char *buf, int len)
{
    for (int i = 0; i < len; i++) {
        if (buf[i] == '\n')
            uart_putc('\r');
        uart_putc(buf[i]);
    }
    return len;
}

// Print display using UART
void print_display(const Chip8 *c)
{
    uart_puts("\x1b[2J");
    uart_puts("\x1b[H");
    for (unsigned y = 0; y < CHIP8_DISP_H; y++) {
        for (unsigned x = 0; x < CHIP8_DISP_W; x++) {
            if (c->display[x + y * CHIP8_DISP_W]) {
                uart_putc('#');
            } else {
                uart_putc('.');
            }
        }
        uart_putc('\r');
        uart_putc('\n');
    }
}

// Initialize SPI1 peripheral
static void spi1_init(void)
{
    // Enable the necessary clocks for SPI
    // APB2ENR: Register that enables the clocks of peripherals at APB2ENR
    // RCC_APB2ENR_SPI1EN: Macro that enables SPI1
    // GPIOA: Port where other pins are located
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;  // Clock on for SPI1
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN; // Clock on for GPIOA
    (void)RCC->APB2ENR;                  // Short settle delay

    // Clear GPIOA register and then set the pin modes in the mode register
    // MODER: 32-bit register that holds the mode of all 16 pins of a port
    // OSPEEDR: 32-bit register that holds the velocity mode of all 16 pins of a port
    // AFR: 32-bit register that holds alternate functions for pins. AFR[0] has pins 0-7

    // Set up pins SCK and MOSI
    GPIOA->MODER &= ~(3U << (DISP_SCK_PIN * 2));  // Set PA5 bits to 00
    GPIOA->MODER &= ~(3U << (DISP_MOSI_PIN * 2)); // Set PA7 bits to 00

    GPIOA->MODER |= (2U << (DISP_SCK_PIN * 2));  // Set PA5 bits to 10 (Alt. Function)
    GPIOA->MODER |= (2U << (DISP_MOSI_PIN * 2)); // Set PA7 bits to 10 (Alt. Function)

    GPIOA->OSPEEDR |= (3U << (DISP_SCK_PIN * 2));  // Set PA5 bits to 11 (High speed)
    GPIOA->OSPEEDR |= (3U << (DISP_MOSI_PIN * 2)); // Set PA7 bits to 11 (High speed)

    GPIOA->AFR[0] &= ~(0xFU << (DISP_SCK_PIN * 4));  // Clear the PA5 4-bit field
    GPIOA->AFR[0] &= ~(0xFU << (DISP_MOSI_PIN * 4)); // Clear the PA7 4-bit field

    GPIOA->AFR[0] |= (5U << (DISP_SCK_PIN * 4));  // Write 5 = AF5 for PA5 = SPI1
    GPIOA->AFR[0] |= (5U << (DISP_MOSI_PIN * 4)); // Write 5 = AF5 for PA7 = SPI1

    // Set up CS pin
    GPIOA->MODER &= ~(3U << (DISP_CS_PIN * 2)); // Set PA4 bits to 00
    GPIOA->MODER |= (1U << (DISP_CS_PIN * 2));  // Set PA4 bits to 01 (Output)
    DISP_CS_HIGH();                             // Set PA4 to 1 (CS: Inactive)

    // Set up DC pin
    GPIOA->MODER &= ~(3U << (DISP_DC_PIN * 2)); // Set PA6 bits to 00
    GPIOA->MODER |= (1U << (DISP_DC_PIN * 2));  // Set PA6 bits to 01 (Output)

    // Set up RESET pin
    GPIOA->MODER &= ~(3U << (DISP_RST_PIN * 2)); // Set PA3 bits to 00
    GPIOA->MODER |= (1U << (DISP_RST_PIN * 2));  // Set PA3 bits to 01 (Output)

    // Set up SPI1 Control 1
    SPI1->CR1 = 0;                              // Clean register
    SPI1->CR1 |= SPI_CR1_MSTR;                  // Set as master
    SPI1->CR1 |= SPI_CR1_SSM;                   // Software slave management
    SPI1->CR1 |= SPI_CR1_SSI;                   // Internal slave select (Master mode)
    SPI1->CR1 &= ~(7U << SPI_CR1_BR_Pos);       // Set baud rate bits to 000 (Fclk/2)
    SPI1->CR1 &= ~(1U << SPI_CR1_DFF_Pos);      // Set Data frame bit to 0 (8 bits)
    SPI1->CR1 &= ~(1U << SPI_CR1_LSBFIRST_Pos); // Set LSB bit to 0 (Most significant bit first)

    SPI1->CR1 |= SPI_CR1_SPE; // SPI Enable
}

static void spi_write(uint8_t b)
{
    while (!(SPI1->SR & SPI_SR_TXE)) {
    }
    SPI1->DR = b;
}

static void spi_wait_idle(void)
{
    while (!(SPI1->SR & SPI_SR_TXE)) {
    }
    while (SPI1->SR & SPI_SR_BSY) {
    }
}

static void disp_cmd(uint8_t c)
{
    DISP_DC_LOW();
    DISP_CS_LOW();
    spi_write(c);
    spi_wait_idle();
    DISP_CS_HIGH();
}

static void disp_data(uint8_t d)
{
    DISP_DC_HIGH();
    DISP_CS_LOW();
    spi_write(d);
    spi_wait_idle();
    DISP_CS_HIGH();
}

static void disp_rst(void)
{
    DISP_RST_HIGH();
    delay_ms(5);
    DISP_RST_LOW();
    delay_ms(20);
    DISP_RST_HIGH();
    delay_ms(120);
}

typedef struct {
    uint8_t cmd;
    uint8_t len; // Number of data bytes
    uint8_t data[16];
    uint16_t delay_ms;
} disp_sequence;

static const disp_sequence init_seq[] = {
    {0x01, 0, {0}, 120},  // Software reset
    {0x3A, 1, {0x55}, 0}, // Pixel format (16 bits per pixel)
    {0x36, 1, {0x28}, 0}, // Memory Access Control
    {0x11, 0, {0}, 120},  // Sleep Out
    {0x29, 0, {0}, 0},    // Dispay on
    {0x00, 0, {0}, 0}     // End of sequence
};

static void disp_execute_seq(const disp_sequence *seq)
{
    for (const disp_sequence *step = seq; step->cmd != 0x00; step++) {
        disp_cmd(step->cmd);
        for (uint8_t byte = 0; byte < step->len; byte++) {
            disp_data(step->data[byte]);
        }
        delay_ms(step->delay_ms);
    }
}

static void disp_init()
{
    disp_execute_seq(init_seq);
};

void disp_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    disp_cmd(0x2A);
    disp_data(x0 >> 8);
    disp_data(x0 & 0xFF);
    disp_data(x1 >> 8);
    disp_data(x1 & 0xFF);
    disp_cmd(0x2B);
    disp_data(y0 >> 8);
    disp_data(y0 & 0xFF);
    disp_data(y1 >> 8);
    disp_data(y1 & 0xFF);
    disp_cmd(0x2C);
    DISP_DC_HIGH();
    DISP_CS_LOW();
};

void disp_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    // Clip rectangle to screen
    if (x >= DISP_W || x >= DISP_W) {
        return;
    }
    uint16_t x1 = x + w;
    if (x1 >= DISP_W) {
        x1 = DISP_W;
    }
    uint16_t y1 = y + h;
    if (y1 >= DISP_H) {
        y1 = DISP_H;
    }
    // Fill window
    disp_set_window(x, y, x1, y1);
    for (uint32_t n = 0; n < (uint32_t)w * h; n++) {
        spi_write(color >> 8);
        spi_write(color & 0xFF);
    }
    spi_wait_idle();
    DISP_CS_HIGH();
};

void disp_clear(uint16_t color)
{
    disp_fill_rect(0, 0, DISP_W, DISP_H, color);
};

void disp_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    disp_fill_rect(x, y, 1, 1, color);
};

void disp_selftest()
{
    disp_clear(COLOR_BLACK);
    delay_ms(200);
    disp_clear(COLOR_RED);
    delay_ms(200);
    disp_clear(COLOR_BLUE);
    delay_ms(200);
    disp_clear(COLOR_GREEN);
    delay_ms(200);
    disp_clear(COLOR_WHITE);
    delay_ms(200);
    disp_fill_rect(50, 50, 50, 50, COLOR_RED);
    disp_fill_rect(200, 50, 50, 50, COLOR_BLUE);
    disp_fill_rect(50, 150, 50, 50, COLOR_GREEN);
    disp_fill_rect(200, 150, 50, 50, COLOR_BLACK);
};

// Main loop
int main(void)
{
    SysTick_Config(SystemCoreClock / 1000U);

    chip8_init(&chip);
    chip8_load_rom(&chip, timer_rom, sizeof(timer_rom));

    uart_init();
    uart_puts("\x1b[2J");

    spi1_init();

    disp_rst();
    disp_init();
    disp_selftest();

    uint32_t last_ms = ms_ticks;
    uint32_t cpu_acc = 0;
    uint32_t timer_acc = 0;

    uint32_t start = ms_ticks;
    bool reported = false;

    while (1) {
        while (ms_ticks != last_ms) { // One millisecond has passed
            last_ms++;
            cpu_acc += 700;
            timer_acc += 60;
            while (cpu_acc >= 1000) {
                Chip8Status s = chip8_step(&chip);
                if (s != CHIP8_OK) {
                    fprintf(stderr, "stopped at pc=0x%03X, status %d\n", chip.pc, s);
                    while (1) {
                    } // Crash until reset
                }
                cpu_acc -= 1000;
            }
            while (timer_acc >= 1000) {
                chip8_tick_timers(&chip);
                timer_acc -= 1000;
            }
            if (!reported && chip.display[0]) {
                reported = true;
                printf("Time elapsed: %lu ms\n", (unsigned long)(ms_ticks - start));
            }
        }
        print_display(&chip);
    }
}