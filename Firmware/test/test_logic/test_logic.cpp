/**************************************************************************************************/
/**
 * @file test_logic.cpp
 * @author  Ryan Jing
 * @brief  Host-side unit tests for the hardware-independent firmware logic:
 *         the generated mood table (moods.h) and the animation engine
 *         (led_effects.cpp / mood_frame). Runs on the CI host via `pio test -e native`.
 *
 * @version 0.1
 * @date 2026-07-06
 *
 * @copyright Copyright (c) 2026
 *
 */
/**************************************************************************************************/

/*------------------------------------------------------------------------------------------------*/
/* HEADERS                                                                                        */
/*------------------------------------------------------------------------------------------------*/

#include <unity.h>
#include "moods.h"
#include "hal/led.h"

/*------------------------------------------------------------------------------------------------*/
/* HELPERS                                                                                        */
/*------------------------------------------------------------------------------------------------*/

// Build a one-colour mood inline so tests don't depend on the generated table's values.
static MoodDefinition make_mood(uint8_t r, uint8_t g, uint8_t b, MoodPattern pattern, uint16_t period) {
    MoodDefinition m = { { {r, g, b}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0} }, 1, pattern, period };
    return m;
}

void setUp(void) {}
void tearDown(void) {}

/*------------------------------------------------------------------------------------------------*/
/* TESTS: moods.h                                                                                 */
/*------------------------------------------------------------------------------------------------*/

// A valid enum resolves to its own row; an out-of-range value falls back to row 0.
void test_get_mood_definition_bounds(void) {
    TEST_ASSERT_EQUAL_PTR(get_mood_definition((Moods)0), get_mood_definition((Moods)-1));
    TEST_ASSERT_EQUAL_PTR(get_mood_definition((Moods)0), get_mood_definition((Moods)(MOOD_COUNT + 5)));
    TEST_ASSERT_NOT_NULL(get_mood_definition((Moods)(MOOD_COUNT - 1)));
}

// Every generated mood must have a sane colour count within the fixed palette size.
void test_table_num_colours_in_range(void) {
    for (int i = 0; i < MOOD_COUNT; i++) {
        const MoodDefinition *d = get_mood_definition((Moods)i);
        TEST_ASSERT_TRUE(d->num_colours >= 1 && d->num_colours <= MAX_MOOD_COLOURS);
    }
}

/*------------------------------------------------------------------------------------------------*/
/* TESTS: mood_frame patterns                                                                     */
/*------------------------------------------------------------------------------------------------*/

void test_solid_returns_base_colour(void) {
    MoodDefinition m = make_mood(10, 20, 30, PATTERN_SOLID, 0);
    uint8_t r, g, b;
    mood_frame(m, 123456, r, g, b);       // time must not matter for solid
    TEST_ASSERT_EQUAL_UINT8(10, r);
    TEST_ASSERT_EQUAL_UINT8(20, g);
    TEST_ASSERT_EQUAL_UINT8(30, b);
}

void test_blink_on_first_half_off_second(void) {
    MoodDefinition m = make_mood(200, 200, 200, PATTERN_BLINK, 1000);
    uint8_t r, g, b;
    mood_frame(m, 0, r, g, b);            // first half -> on
    TEST_ASSERT_EQUAL_UINT8(200, r);
    mood_frame(m, 700, r, g, b);          // second half -> off
    TEST_ASSERT_EQUAL_UINT8(0, r);
}

void test_alternate_cycles_and_wraps(void) {
    MoodDefinition m = { { {1, 0, 0}, {2, 0, 0}, {0, 0, 0}, {0, 0, 0} }, 2, PATTERN_ALTERNATE, 100 };
    uint8_t r, g, b;
    mood_frame(m, 0,   r, g, b); TEST_ASSERT_EQUAL_UINT8(1, r);  // colour 0
    mood_frame(m, 100, r, g, b); TEST_ASSERT_EQUAL_UINT8(2, r);  // colour 1
    mood_frame(m, 200, r, g, b); TEST_ASSERT_EQUAL_UINT8(1, r);  // wrapped back to 0
}

// A mood with no colours must render as off, never read past the palette.
void test_zero_colours_is_off(void) {
    MoodDefinition m = make_mood(255, 255, 255, PATTERN_SOLID, 0);
    m.num_colours = 0;
    uint8_t r = 9, g = 9, b = 9;
    mood_frame(m, 0, r, g, b);
    TEST_ASSERT_EQUAL_UINT8(0, r);
    TEST_ASSERT_EQUAL_UINT8(0, g);
    TEST_ASSERT_EQUAL_UINT8(0, b);
}

// Breathe should hit ~0 brightness at phase 0 and full brightness at the half period.
void test_breath_min_at_zero_max_at_half(void) {
    MoodDefinition m = make_mood(100, 0, 0, PATTERN_BREATH, 1000);
    uint8_t r, g, b;
    mood_frame(m, 0, r, g, b);             // cos(0)=1 -> k=0 -> off
    TEST_ASSERT_EQUAL_UINT8(0, r);
    mood_frame(m, 500, r, g, b);           // cos(pi)=-1 -> k=1 -> full
    TEST_ASSERT_EQUAL_UINT8(100, r);
}

/*------------------------------------------------------------------------------------------------*/
/* RUNNER                                                                                         */
/*------------------------------------------------------------------------------------------------*/

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_get_mood_definition_bounds);
    RUN_TEST(test_table_num_colours_in_range);
    RUN_TEST(test_solid_returns_base_colour);
    RUN_TEST(test_blink_on_first_half_off_second);
    RUN_TEST(test_alternate_cycles_and_wraps);
    RUN_TEST(test_zero_colours_is_off);
    RUN_TEST(test_breath_min_at_zero_max_at_half);
    return UNITY_END();
}
