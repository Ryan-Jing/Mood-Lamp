/**************************************************************************************************/
/**
 * @file led.cpp
 * @author  Ryan Jing
 * @brief WS2812B LED driver implementation.
 *
 * @version 0.1
 * @date 2026-07-03
 *
 * @copyright Copyright (c) 2026
 *
 */
/**************************************************************************************************/

/*------------------------------------------------------------------------------------------------*/
/* HEADERS                                                                                        */
/*------------------------------------------------------------------------------------------------*/

#include "hal/led.h"

#include <Adafruit_NeoPixel.h>

#include "hal/led_effects.h"

/*------------------------------------------------------------------------------------------------*/
/* MACROS                                                                                         */
/*------------------------------------------------------------------------------------------------*/

#define LED_DIN_PIN D0

/*------------------------------------------------------------------------------------------------*/
/* GLOBAL VARIABLES                                                                               */
/*------------------------------------------------------------------------------------------------*/

static Adafruit_NeoPixel led(1, LED_DIN_PIN, NEO_GRB + NEO_KHZ800);

/*------------------------------------------------------------------------------------------------*/
/* FUNCTION PROTOTYPES                                                                            */
/*------------------------------------------------------------------------------------------------*/



/*------------------------------------------------------------------------------------------------*/
/* FUNCTION DEFINITIONS                                                                           */
/*------------------------------------------------------------------------------------------------*/

void led_init() {
    led.begin();
    led.setPixelColor(0, led.Color(60, 0, 0));  // dim red, safe current
    led.show();
    delay(1500);                                 // hold it so you can see it (remove later)
    led.clear();
    led.show();
}

void led_write(uint8_t red, uint8_t green, uint8_t blue) {
    led.setPixelColor(0, led.Color(red, green, blue));
    led.show();
}

void led_render(Moods mood) {
    const MoodDefinition *mood_def = get_mood_definition(mood);
    uint8_t red, green, blue;
    mood_frame(*mood_def, millis(), red, green, blue);

    // Only latch the LED when the colour actually changed, so a SOLID mood (or any frame that
    // rounds to the same 8-bit value) doesn't re-transmit an identical frame every tick.
    static bool    first = true;
    static uint8_t last_red, last_green, last_blue;
    if (first || red != last_red || green != last_green || blue != last_blue) {
        led_write(red, green, blue);
        last_red = red; last_green = green; last_blue = blue;
        first = false;
    }
}
