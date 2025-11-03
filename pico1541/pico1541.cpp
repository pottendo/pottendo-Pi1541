#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"


extern "C" void kernel_main(unsigned int r0, unsigned int r1, unsigned int atags);
// Pico W devices use a GPIO on the WIFI chip for the LED,
// so when building for Pico W, CYW43_WL_GPIO_LED_PIN will be defined
#ifdef CYW43_WL_GPIO_LED_PIN
#include "pico/cyw43_arch.h"
#endif

#ifndef LED_DELAY_MS
#define LED_DELAY_MS 250
#endif

//char psram_test[512 * 1024 * 1024]; // 5MB should be enough to test PSRAM presence
// Perform initialisation
int pico_led_init(void) {
#if defined(PICO_DEFAULT_LED_PIN)
    // A device like Pico that uses a GPIO for the LED will define PICO_DEFAULT_LED_PIN
    // so we can use normal GPIO functionality to turn the led on and off
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    return PICO_OK;
#elif defined(CYW43_WL_GPIO_LED_PIN)
    // For Pico W devices we need to initialise the driver etc
    return cyw43_arch_init();
#endif
}

// Turn the led on or off
void pico_set_led(bool led_on) {
#if defined(PICO_DEFAULT_LED_PIN)
    // Just set the GPIO on or off
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
#elif defined(CYW43_WL_GPIO_LED_PIN)
    // Ask the wifi "driver" to set the GPIO on or off
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
#endif
}
#include <ff.h>
extern void list_directory(const char *path);
int main()
{
    stdio_init_all();

    int rc = pico_led_init();
    hard_assert(rc == PICO_OK);
    pico_set_led(true);
    sleep_ms(LED_DELAY_MS);
    pico_set_led(false);

    int _i = 7;
    while (_i--)
    {
        printf("counting %s: - %d\n", __FUNCTION__, _i);
        sleep_ms(1000);
        fflush(stdout);
    }
    FATFS fileSystemSD;
    FRESULT fr = f_mount(&fileSystemSD, "SD:", 1);
    if (FR_OK != fr) {
       	printf("f_mount error: (%d)\n", fr);
		return -1;
    }		
	list_directory("/");
    fflush(stdout);
    sleep_ms(1000);
    kernel_main(0, 0, 0);
    while (true) {
        printf("Hello, world!\n");
        sleep_ms(10000);
    }
}

void not_implemented(const char *fn)
{
    fprintf(stderr, "%s for Pico2\n", fn);
}

/* --------- PICO2 specific Pi1541 functions here --------- */
extern "C" {
void SetACTLed(int v) 
{ 
    if (v) pico_set_led(true);
    else pico_set_led(false);
}
void usDelay(unsigned nMicroSeconds)
{
    sleep_us(nMicroSeconds);
}
} /* extern "C" */

void reboot_now(void)
{
    not_implemented(__FUNCTION__);
}

void InitialiseLCD(void)
{
    not_implemented(__FUNCTION__);
}
