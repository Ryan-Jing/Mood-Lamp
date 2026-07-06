/**************************************************************************************************/
/**
 * @file led_effects.cpp
 * @author  Ryan Jing
 * @brief
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

#include "hal/led_effects.h"

#include <math.h>

/*------------------------------------------------------------------------------------------------*/
/* MACROS                                                                                         */
/*------------------------------------------------------------------------------------------------*/



/*------------------------------------------------------------------------------------------------*/
/* GLOBAL VARIABLES                                                                               */
/*------------------------------------------------------------------------------------------------*/



/*------------------------------------------------------------------------------------------------*/
/* FUNCTION PROTOTYPES                                                                            */
/*------------------------------------------------------------------------------------------------*/



/*------------------------------------------------------------------------------------------------*/
/* FUNCTION DEFINITIONS                                                                           */
/*------------------------------------------------------------------------------------------------*/

static uint8_t scale8(uint8_t c, float k) {
    if(k < 0) k = 0;
    if(k > 1) k = 1;
    return (uint8_t)(c * k);
}

static uint8_t lerp8(uint8_t a, uint8_t b, float t) {
    return (uint8_t)(a + (b - a) * t);
}

void mood_frame(const MoodDefinition &mood, uint32_t time_ms, uint8_t &red, uint8_t &green, uint8_t &blue) {
    const uint8_t (*C)[3] = mood.colours;

    if (mood.num_colours == 0) {
        red = 0;
        green = 0;
        blue = 0;
        return;
    }

    switch (mood.pattern) {
        case PATTERN_ALTERNATE: {                                  // hard switch each period
            uint8_t i = mood.period ? (time_ms / mood.period) % mood.num_colours : 0;
            red = C[i][0]; green = C[i][1]; blue = C[i][2];
            break;
        }
        case PATTERN_FADE: {                                       // smooth cross-fade
            if (mood.period && mood.num_colours > 1) {
                uint32_t seg = time_ms / mood.period;
                uint8_t  i0  = seg % mood.num_colours;
                uint8_t  i1  = (i0 + 1) % mood.num_colours;
                float    t   = (float)(time_ms % mood.period) / mood.period;
                red = lerp8(C[i0][0], C[i1][0], t);
                green = lerp8(C[i0][1], C[i1][1], t);
                blue = lerp8(C[i0][2], C[i1][2], t);
            } else { red = C[0][0]; green = C[0][1]; blue = C[0][2]; }
            break;
        }
        case PATTERN_BREATH: {
            float phase = mood.period ? (float)(time_ms % mood.period) / mood.period : 0;
            float k = (1.0f - cosf(2.0f * PI * phase)) * 0.5f;
            red = scale8(C[0][0], k); green = scale8(C[0][1], k); blue = scale8(C[0][2], k);
            break;
        }
        case PATTERN_BLINK: {
            float k = (mood.period && (time_ms % mood.period) < mood.period / 2) ? 1.0f : 0.0f;
            red = scale8(C[0][0], k); green = scale8(C[0][1], k); blue = scale8(C[0][2], k);
            break;
        }
        default:                                                   // PATTERN_SOLID
            red = C[0][0]; green = C[0][1]; blue = C[0][2];
    }
}

