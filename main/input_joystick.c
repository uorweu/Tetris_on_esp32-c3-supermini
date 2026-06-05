/* input_joystick.c - analog joystick + three buttons + joystick click.
 *
 *   Joystick VRx GPIO0 (ADC1_CH0)   VRy GPIO1 (ADC1_CH1)   +5V -> 3V3
 *   Joystick SW (click) GPIO9       -> hard drop (same as button 2)
 *   Button 1 GPIO2  = new game       (pull-up, pressed = 0)
 *   Button 2 GPIO20 = hard drop
 *   Button 3 GPIO21 = benchmark screen
 *
 * The stick axes are remapped to match the physical orientation of the
 * module (rdx = -dy, rdy = dx) - the mapping confirmed with the diagnostic.
 *
 * Note: GPIO9 is the boot strapping pin. Pressing the joystick during normal
 * play is fine; just don't hold it down while the board powers on.
 */
#include "input_joystick.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PIN_NEWGAME 2          /* button 1 */
#define PIN_DROP    20         /* button 2 */
#define PIN_MENU    21         /* button 3 */
#define PIN_JOYSW   9          /* joystick click -> hard drop */
#define ADC_X_CH    ADC_CHANNEL_0   /* GPIO0 */
#define ADC_Y_CH    ADC_CHANNEL_1   /* GPIO1 */
#define DEADZONE    600

static adc_oneshot_unit_handle_t s_adc;
static int s_cx = 2048, s_cy = 2048;

static int read_x(void) { int v=0; adc_oneshot_read(s_adc, ADC_X_CH, &v); return v; }
static int read_y(void) { int v=0; adc_oneshot_read(s_adc, ADC_Y_CH, &v); return v; }

void joystick_init(void)
{
    adc_oneshot_unit_init_cfg_t ucfg = { .unit_id = ADC_UNIT_1 };
    adc_oneshot_new_unit(&ucfg, &s_adc);

    adc_oneshot_chan_cfg_t ccfg = {
        .atten = ADC_ATTEN_DB_12,          /* full ~0..3.3V range */
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    adc_oneshot_config_channel(s_adc, ADC_X_CH, &ccfg);
    adc_oneshot_config_channel(s_adc, ADC_Y_CH, &ccfg);

    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PIN_NEWGAME) | (1ULL << PIN_DROP) |
                        (1ULL << PIN_MENU)    | (1ULL << PIN_JOYSW),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);

    /* calibrate resting centre */
    vTaskDelay(pdMS_TO_TICKS(150));
    long sx = 0, sy = 0;
    for (int i = 0; i < 16; i++) { sx += read_x(); sy += read_y(); vTaskDelay(pdMS_TO_TICKS(3)); }
    s_cx = (int)(sx / 16);
    s_cy = (int)(sy / 16);
}

void joystick_read(input_state_t *in)
{
    int dx = read_x() - s_cx;
    int dy = read_y() - s_cy;

    /* orientation correction confirmed on the hardware */
    int rdx = -dy;
    int rdy =  dx;

    in->up    = (rdy < -DEADZONE);                 /* rotate (joystick only) */
    in->down  = (rdy >  DEADZONE);                 /* soft drop              */
    in->left  = (rdx < -DEADZONE);
    in->right = (rdx >  DEADZONE);

    /* hard drop: button 2 OR the joystick click */
    in->action  = (gpio_get_level(PIN_DROP) == 0) || (gpio_get_level(PIN_JOYSW) == 0);
    in->menu    = (gpio_get_level(PIN_MENU) == 0);     /* benchmark screen */
    in->newgame = (gpio_get_level(PIN_NEWGAME) == 0);  /* start a new game */
}
