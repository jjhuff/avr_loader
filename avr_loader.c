/*
    avr_loader

    AVR Hexfile Boot Loader

    Copyright (C) 2012 Justin Huff

    This is heavily based on kavr by Frank Edelhaeuser.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <avr/boot.h>
#include <avr/common.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <util/setbaud.h>


// macros
#define PACKED          __attribute__((__packed__))

// definitions
#define XON             0x11
#define XOFF            0x13
#define TIMEOUT_COUNT   0

// data
struct HEX_RECORD
{
    uint8_t             length;
    uint16_t            address;
    uint8_t             type;
    uint8_t             data[255 + 1];
} PACKED record;

uint8_t                 page[SPM_PAGESIZE];


// prototypes
static void flash_write(uint16_t addr);
static void out_str_P(const char* ptr);
static void out_char(char c);
static void app_start(void);

// implementation
__attribute__ ((OS_main)) __attribute__ ((section (".init9")))
void boot(void)
{
    uint16_t            new_page;
    uint16_t            cur_page;
    uint16_t            addr;
    uint8_t*            rx_ptr;
    uint8_t             rx_dat;
    uint8_t             rx_crc;
    uint8_t             rx_len;
    uint8_t             rx_val;
    uint8_t             i;
    uint8_t*            dst;

    asm volatile ("clr r1");
    MCUSR = 0;
    wdt_disable();

    SREG = 0;
    SP   = RAMEND;
#if defined(RAMPD)
    RAMPD = 0;
#endif
#if defined(RAMPX)
    RAMPX = 0;
#endif
#if defined(RAMPY)
    RAMPY = 0;
#endif
#if defined(RAMPZ)
    RAMPZ = 0;
#endif

    // Configure UART for $(BAUD),8N1
    UBRR0H = UBRRH_VALUE;
    UBRR0L = UBRRL_VALUE;

    // Enable receiver and transmitter
    #if USE_2X
    UCSR0A = (1 << U2X0);
    #else
    UCSR0A = (0 << U2X0);
    #endif
    UCSR0B = (1<<RXEN0)|(1<<TXEN0);

    // Set frame format: 8 bit, no parity, 1 stop bit,
    UCSR0C = (1<<UCSZ00)|(1<<UCSZ01);

    // Hello world!
    out_str_P(PSTR("LOADER\r\n"));

    TCCR1A = 0;                   // start the timer
    TCCR1B = (1<<CS02)|(1<<CS00); // set prescaler (1024)
    TIMSK1 = 0;
    TCNT1 = TIMEOUT_COUNT;
    TIFR1 = (1<<TOV1); //clear TOV1

    // XON
    out_char(XON);

    // Initialize overall state
    cur_page  = BOOTADDR;

    // (Re-)Initialize line state
restart_line:
    rx_val = 0;
    rx_len = 0;
    rx_crc = 0;
    rx_ptr = (uint8_t*) &record;

    // Download data records until timeout
    while(1)
    {
        // Wait for data or timeout
        while( !(TIFR1 & (1<<TOV1)) && !(UCSR0A & (1<<RXC0)) );

        if(TIFR1 & (1<<TOV1))
            break;

        rx_dat = UDR0;

        // Convert to binary nibble
        rx_dat -= '0';
        if (rx_dat > 9)
        {
            rx_dat -= 7;
            if ((rx_dat < 10) || (rx_dat > 15))
                goto restart_line;
        }

        // Valid hex character. Restart timeout.
        TCNT1 = TIMEOUT_COUNT;

        // Two nibbles = 1 byte
        rx_val = (rx_val << 4) + rx_dat;
        if ((++ rx_len & 1) == 0)
        {
            rx_crc += rx_val;
            *rx_ptr ++ = rx_val;

            if (rx_ptr == (uint8_t*) &record.data[record.length + 1])
            {
                if (rx_crc)
                {
                    // Checksum error. Abort
                    out_char('?');
                    break;
                }
                else if (record.type == 0)
                {
                    // Data record
                    addr = (record.address << 8) | (record.address >> 8);
                    for (i = 0; i < record.length; i ++)
                    {
                        // Determine page base for current address
                        new_page = addr & ~(SPM_PAGESIZE - 1);
                        if (new_page != cur_page)
                        {
                            out_char(XOFF);
                            out_char('.');
                            // Write updated RAM page buffer into flash
                            flash_write(cur_page);

                            // Load page at new address into RAM page buffer
                            cur_page = new_page;

                            // memcpy_P(page, (PGM_P) new_page, SPM_PAGESIZE);
                            dst = page;
                            while (dst < &page[SPM_PAGESIZE])
                                *dst ++ = pgm_read_byte((PGM_P) new_page ++);
                            out_char(XON);
                        }

                        // Update RAM page buffer from data record
                        page[addr & (SPM_PAGESIZE - 1)] = record.data[i];
                        addr ++;
                    }

                    goto restart_line;
                }
                else if (record.type == 1)
                {
                    // End of file record. Finish
                    flash_write(cur_page);
                    break;
                }
            }
        }
    }

    out_str_P(PSTR("EXIT\r\n"));

    // Wait for the last byte to transmit
    loop_until_bit_is_set(UCSR0A, UDRE0);

    // Boot the app
    app_start();
}

static void app_start()
{
    __asm__ __volatile__ (
    // Jump to RST vector
    "clr r30\n"
    "clr r31\n"
    "ijmp\n"
    );
}

static void out_str_P(const char* ptr)
{
    while (pgm_read_byte(ptr) != 0x00)
        out_char(pgm_read_byte(ptr++));
}

static void out_char(char c)
{
    //if (c == '\n') out_char('\r');
    loop_until_bit_is_set(UCSR0A, UDRE0);
    UDR0 = c;
}

static void flash_write(uint16_t addr)
{
    uint16_t  dst;
    uint16_t* src;
    uint8_t   i;

    if (addr < BOOTADDR)
    {
        // Erase and program
        boot_page_erase(addr);
        boot_spm_busy_wait();

        // Copy RAM addr buffer into SPM addr buffer
        dst = addr;
        src = (uint16_t*) page;
        i = 0;
        do
        {
            boot_page_fill(dst, *src ++);
            dst += 2;
        }
        while (++ i < SPM_PAGESIZE / 2);

        boot_page_write(addr);
        boot_spm_busy_wait();

        boot_rww_enable();
    }
}


