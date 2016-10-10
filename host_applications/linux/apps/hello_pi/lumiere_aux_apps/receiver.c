#include "bcm2835.h"
#include <stdio.h>

#define PIN 30

int main(int argc, char **argv)
{
    if (!bcm2835_init())
        return 1;

    bcm2835_gpio_fsel(PIN, BCM2835_GPIO_FSEL_INPT);
    // bcm2835_gpio_set_pud(PIN, BCM2835_GPIO_PUD_UP);
    // And a low detect enable
    bcm2835_gpio_len(PIN);

    while (1) {
        if (bcm2835_gpio_eds(PIN)) {
            // Now clear the eds flag by setting it to 1
            bcm2835_gpio_set_eds(PIN);
            printf("low event detect for pin 30\n");
        }

        // wait a bit
        delay(10);
    }

    bcm2835_close();
    return 0;
}

