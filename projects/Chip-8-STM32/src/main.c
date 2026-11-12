#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "stm32f411xe.h"
#include "stm32f4xx.h"
#include "../../Chip-8/core/chip8.h"
#include "roms.h"

#define UART_TX_PIN 2 // PA2

#define BTN_PIN 0 // PA0

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

// Definitions for the screen
#define DISP_W 320
#define DISP_H 240
#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
#define COLOR_RED 0xF800
#define COLOR_GREEN 0x07E0
#define COLOR_BLUE 0x001F
#define COLOR_BLACK 0x0000
#define COLOR_WHITE 0xFFFF

#define SCALE 5
#define OFFSET_Y ((DISP_H - CHIP8_DISP_H * SCALE) / 2)
#define FG RGB565(0x33, 0xFF, 0x66)
#define BG COLOR_BLACK
#define STRIP_BYTES (CHIP8_DISP_W * SCALE * 2 * SCALE)

// Definitions for UART
#define PCLK1_HZ 50000000U
#define BAUD 115200U

static Chip8 chip;

static int clock_init_100mhz(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC->APB1ENR;
    PWR->CR |= PWR_CR_VOS;

    FLASH->ACR = FLASH_ACR_LATENCY_3WS | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;
    if ((FLASH->ACR & FLASH_ACR_LATENCY) != FLASH_ACR_LATENCY_3WS)
        return -1;

    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2)) |
                RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_PPRE2_DIV1;

    uint32_t p = RCC->PLLCFGR;
    p &= ~(RCC_PLLCFGR_PLLM | RCC_PLLCFGR_PLLN | RCC_PLLCFGR_PLLP | RCC_PLLCFGR_PLLQ |
           RCC_PLLCFGR_PLLSRC);
    p |= (8U << RCC_PLLCFGR_PLLM_Pos) | (100U << RCC_PLLCFGR_PLLN_Pos) |
         (0U << RCC_PLLCFGR_PLLP_Pos) | (4U << RCC_PLLCFGR_PLLQ_Pos);
    RCC->PLLCFGR = p;

    RCC->CR |= RCC_CR_PLLON;
    for (uint32_t t = 0; !(RCC->CR & RCC_CR_PLLRDY); t++)
        if (t > 100000U)
            return -2;

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    for (uint32_t t = 0; (RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL; t++)
        if (t > 100000U)
            return -3;

    SystemCoreClock = 100000000U;
    return 0;
}

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

volatile uint32_t tick60;

void timer60_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    (void)RCC->APB1ENR;

    TIM3->PSC = 99;    // Prescaler gives 1MHz
    TIM3->ARR = 16666; // Auto-reload at 59.9988 Hz
    TIM3->EGR = TIM_EGR_UG;
    TIM3->SR = 0;
    TIM3->DIER |= TIM_DIER_UIE;
    NVIC_EnableIRQ(TIM3_IRQn);

    TIM3->CR1 |= TIM_CR1_CEN; // Enable counter
}

void TIM3_IRQHandler(void)
{
    if (TIM3->SR & TIM_SR_UIF) {
        TIM3->SR = ~TIM_SR_UIF; // Clear flag
        tick60++;
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
    SPI1->CR1 &= ~(7U << SPI_CR1_DFF_Pos);      // Set baud rate bits to 000 (Fclk/2)
    SPI1->CR1 &= ~(1U << SPI_CR1_DFF_Pos);      // Set Data frame bit to 0 (8 bits)
    SPI1->CR1 &= ~(1U << SPI_CR1_LSBFIRST_Pos); // Set LSB bit to 0 (Most significant bit first)

    SPI1->CR1 |= SPI_CR1_SPE; // SPI Enable
}

// Initialize DMA2 for use with SPI1
static uint8_t strips[2][STRIP_BYTES];
static uint8_t snap[CHIP8_DISP_W * CHIP8_DISP_H];
static volatile bool dma_done;

static struct {
    bool busy;
    bool pending;  // Built strip is waiting for the DMA
    uint8_t cur;   // Buffer currently (or last) sent
    uint8_t built; // Number of CHIP-8 rows built so far
} rs;

static void dma2_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
    (void)RCC->AHB1ENR;

    DMA2_Stream3->CR = 0;
    while (DMA2_Stream3->CR & DMA_SxCR_EN) {
    }

    DMA2_Stream3->CR |= (3U << DMA_SxCR_CHSEL_Pos); // Assign Channel 3
    DMA2_Stream3->CR |= DMA_SxCR_DIR_0;             // Direction Memory-to-peripheral
    DMA2_Stream3->CR |= DMA_SxCR_MINC;              // Increment memory address
    DMA2_Stream3->CR |= DMA_SxCR_TCIE;              // Interrupt on transfer complete

    SPI1->CR2 |= SPI_CR2_TXDMAEN; // SPI asks the DMA for data
    NVIC_EnableIRQ(DMA2_Stream3_IRQn);
}

static void dma_start(const uint8_t *buf, uint16_t len)
{
    DMA2_Stream3->CR &= ~DMA_SxCR_EN;
    while (DMA2_Stream3->CR & DMA_SxCR_EN) {
    }

    DMA2->LIFCR = DMA_LIFCR_CTCIF3 | DMA_LIFCR_CHTIF3 | DMA_LIFCR_CTEIF3 | DMA_LIFCR_CDMEIF3 |
                  DMA_LIFCR_CFEIF3; // Clear stale flags

    DMA2_Stream3->PAR = (uint32_t)&SPI1->DR;
    DMA2_Stream3->M0AR = (uint32_t)buf;
    DMA2_Stream3->NDTR = len;
    DMA2_Stream3->CR |= DMA_SxCR_EN;
}

void DMA2_Stream3_IRQHandler(void)
{
    if (DMA2->LISR & DMA_LISR_TCIF3) {
        DMA2->LIFCR = DMA_LIFCR_CTCIF3;
        dma_done = true;
    }
    if (DMA2->LISR & DMA_LISR_TEIF3) { // Transfer error
        DMA2->LIFCR = DMA_LIFCR_CTEIF3;
    }
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

static uint16_t line[CHIP8_DISP_W * SCALE];

void render(const Chip8 *c)
{
    disp_set_window(0, OFFSET_Y, DISP_W - 1, OFFSET_Y + CHIP8_DISP_H * SCALE - 1);

    for (unsigned y = 0; y < CHIP8_DISP_H; y++) {
        for (unsigned x = 0; x < CHIP8_DISP_W; x++) {
            uint16_t col = c->display[x + y * CHIP8_DISP_W] ? FG : BG;
            for (unsigned k = 0; k < SCALE; k++) {
                line[x * SCALE + k] = col;
            }
        }
        for (unsigned r = 0; r < SCALE; r++) {
            for (unsigned i = 0; i < CHIP8_DISP_W * SCALE; i++) {
                spi_write(line[i] >> 8);
                spi_write(line[i] & 0xFF);
            }
        }
    }
    spi_wait_idle();
    DISP_CS_HIGH();
}

static void strip_build(uint8_t *dst, unsigned y)
{
    const uint8_t fg_hi = FG >> 8, fg_lo = FG & 0xFF;
    const uint8_t bg_hi = BG >> 8, bg_lo = BG & 0xFF;
    uint8_t *p = dst;

    for (unsigned x = 0; x < CHIP8_DISP_W; x++) {
        uint8_t hi = snap[x + y * CHIP8_DISP_W] ? fg_hi : bg_hi;
        uint8_t lo = snap[x + y * CHIP8_DISP_W] ? fg_lo : bg_lo;
        for (unsigned k = 0; k < SCALE; k++) {
            *p++ = hi;
            *p++ = lo;
        }
    }
    for (unsigned r = 1; r < SCALE; r++) {
        memcpy(dst + r * CHIP8_DISP_W * SCALE * 2, dst, CHIP8_DISP_W * SCALE * 2);
    }
}

bool render_busy(void)
{
    return rs.busy;
}

bool render_start(Chip8 *c)
{
    if (rs.busy || !c->display_dirty)
        return false;

    c->display_dirty = false; // Clear before copying
    memcpy(snap, c->display, sizeof snap);

    disp_set_window(0, OFFSET_Y, DISP_W - 1, OFFSET_Y + CHIP8_DISP_H * SCALE - 1);
    // Polled SPI is no longer used until the frame ends

    rs.busy = true;
    dma_done = false;
    rs.cur = 0;
    strip_build(strips[0], 0);
    dma_start(strips[0], STRIP_BYTES); // Row 0 is sending
    strip_build(strips[1], 1);         // Row 1 is built meanwhile
    rs.built = 2;
    rs.pending = true;
    return true;
}

void render_poll(void)
{
    if (!rs.busy || !dma_done)
        return;
    dma_done = false;

    if (rs.pending) {
        rs.cur ^= 1;
        dma_start(strips[rs.cur], STRIP_BYTES);
        rs.pending = false;
        if (rs.built < CHIP8_DISP_H) {
            strip_build(strips[rs.cur ^ 1], rs.built++);
            rs.pending = true;
        }
    } else { // The last strip is sent
        while (!(SPI1->SR & SPI_SR_TXE)) {
        }
        while (SPI1->SR & SPI_SR_BSY) {
        }
        DISP_CS_HIGH();
        rs.busy = false;
    }
}

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

static void btn_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN; // Clock on for GPIOA
    (void)RCC->AHB1ENR;                  // Short settle delay

    GPIOA->MODER &= ~(3U << (BTN_PIN * 2)); // Set BTN bits to 00 (Input)
}

typedef enum { BTN_NONE, BTN_SHORT, BTN_LONG } btn_event_t;

static btn_event_t button_poll(void) // Call once per ms
{
    static uint16_t held;
    static bool long_sent;
    bool down = GPIOA->IDR & 1U;

    if (down) {
        if (held < 0xFFFF)
            held++;
        if (held == 500 && !long_sent) {
            long_sent = true;
            return BTN_LONG;
        }
    } else {
        btn_event_t e = (held >= 30 && held < 500) ? BTN_SHORT : BTN_NONE;
        held = 0;
        long_sent = false;
        return e;
    }
    return BTN_NONE;
}

static size_t rom_index;

static void start_rom(size_t index)
{
    const rom_entry_t *e = rom_get(index);
    if (e == NULL) {
        return;
    }

    chip8_init(&chip);
    if (chip8_load_rom(&chip, e->data, e->len) != CHIP8_OK) {
        uart_puts("ROM load failed\r\n");
        return;
    }
    uart_puts("Running: ");
    uart_puts(e->name);
    uart_puts("\r\n");

    render_start(&chip);
}

// Main loop
int main(void)
{
    clock_init_100mhz();

    timer60_init();
    SysTick_Config(SystemCoreClock / 1000U);

    btn_init();

    uart_init();
    uart_puts("\x1b[2J");

    spi1_init();
    dma2_init();

    disp_rst();
    disp_init();
    disp_clear(COLOR_BLACK);
    // disp_selftest();

    start_rom(0);

    static uint32_t last_tick60;
    uint32_t last_ms = ms_ticks;
    uint32_t cpu_acc = 0;

    uint32_t start = ms_ticks;
    bool reported = false;

    while (1) {
        while (last_tick60 != tick60) {
            last_tick60++;
            chip8_tick_timers(&chip);
        }
        while (ms_ticks != last_ms) { // One millisecond has passed
            last_ms++;
            cpu_acc += 700;
            while (cpu_acc >= 1000) {
                Chip8Status s = chip8_step(&chip);
                if (s != CHIP8_OK) {
                    fprintf(stderr, "stopped at pc=0x%03X, status %d\n", chip.pc, s);
                    while (1) {
                    } // Crash until reset
                }
                cpu_acc -= 1000;
            }
            if (!reported && chip.display[0]) {
                reported = true;
                printf("Time elapsed: %lu ms\n", (unsigned long)(ms_ticks - start));
            }
            switch (button_poll()) {
            case BTN_SHORT:
                rom_index = (rom_index + 1) % rom_count();
                uart_puts(rom_get(rom_index)->name);
                uart_puts("\r\n");
                break;
            case BTN_LONG:
                start_rom(rom_index);
                break;
            default:
                break;
            }
        }
        // print_display(&chip);
        static uint32_t last_render;
        render_poll();
        if (!render_busy() && chip.display_dirty && (uint32_t)(ms_ticks - last_render) >= 16) {
            if (render_start(&chip)) {
                printf("Time to render: %lu ms\n", (unsigned long)(ms_ticks - last_render));
                last_render = ms_ticks;
            }
        }
    }
}