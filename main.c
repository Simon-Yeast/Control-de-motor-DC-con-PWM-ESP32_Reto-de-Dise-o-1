#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"

#define PIN_SEG_A 13
#define PIN_SEG_B 12
#define PIN_SEG_C 14
#define PIN_SEG_D 27
#define PIN_SEG_E 26
#define PIN_SEG_F 25
#define PIN_SEG_G 33

#define DIG_CENT 21
#define DIG_DEC  22
#define DIG_UNIT 23

#define BTN_LEFT  4
#define BTN_RIGHT 5

#define LED_OK   18
#define LED_ERR  19

#define ADC_INPUT ADC_CHANNEL_6

#define H_L 32
#define L_L 16
#define H_R 17
#define L_R 15

#define PWM_MAX_VAL 4095
#define PWM_FREQ    5000
#define TIME_DEB    120
#define TIME_SWAP   300

typedef enum {
    SENTIDO_1 = 0,
    SENTIDO_2
} sentido_t;

static adc_oneshot_unit_handle_t adc_handle;

static sentido_t estado_actual = SENTIDO_1;

static int adc_val = 0;
static int porcentaje = 0;
static int duty_target = 0;

static const gpio_num_t seg_pins[] = {
    PIN_SEG_A, PIN_SEG_B, PIN_SEG_C, PIN_SEG_D,
    PIN_SEG_E, PIN_SEG_F, PIN_SEG_G
};

static const gpio_num_t dig_pins[] = {
    DIG_CENT, DIG_DEC, DIG_UNIT
};

static const uint8_t tabla_7seg[10][7] = {
    {0,0,0,0,0,0,1},
    {1,0,0,1,1,1,1},
    {0,0,1,0,0,1,0},
    {0,0,0,0,1,1,0},
    {1,0,0,1,1,0,0},
    {0,1,0,0,1,0,0},
    {0,1,0,0,0,0,0},
    {0,0,0,1,1,1,1},
    {0,0,0,0,0,0,0},
    {0,0,0,0,1,0,0}
};

static void set_pwm(ledc_channel_t ch, int duty)
{
    if (duty < 0) duty = 0;
    if (duty > PWM_MAX_VAL) duty = PWM_MAX_VAL;

    int real = PWM_MAX_VAL - duty;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, real);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, ch);
}

static void stop_left(void)  { set_pwm(LEDC_CHANNEL_0, 0); }
static void stop_right(void) { set_pwm(LEDC_CHANNEL_1, 0); }

static void apagar_puente(void)
{
    gpio_set_level(H_L, 0);
    gpio_set_level(H_R, 0);
    stop_left();
    stop_right();
}

static void cargar_digito(uint8_t n)
{
    for (int i = 0; i < 7; i++)
        gpio_set_level(seg_pins[i], tabla_7seg[n][i]);
}

static void apagar_digitos(void)
{
    for (int i = 0; i < 3; i++)
        gpio_set_level(dig_pins[i], 1);
}

static void multiplexar(int num)
{
    int cifras[3] = {
        num / 100,
        (num / 10) % 10,
        num % 10
    };

    for (int i = 0; i < 3; i++) {
        apagar_digitos();
        cargar_digito(cifras[i]);
        gpio_set_level(dig_pins[i], 0);
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    apagar_digitos();
}

static void mantener_display(int num, int ciclos)
{
    while (ciclos-- > 0)
        multiplexar(num);
}

static void actualizar_adc(void)
{
    adc_oneshot_read(adc_handle, ADC_INPUT, &adc_val);

    if (adc_val < 0) adc_val = 0;
    if (adc_val > 4095) adc_val = 4095;

    porcentaje = (adc_val * 100) / 4095;
    if (porcentaje > 100) porcentaje = 100;

    duty_target = adc_val;
    if (duty_target > PWM_MAX_VAL) duty_target = PWM_MAX_VAL;
}

static void mover_motor(void)
{
    if (estado_actual == SENTIDO_1) {
        gpio_set_level(H_R, 0);
        stop_left();

        gpio_set_level(H_L, 1);
        set_pwm(LEDC_CHANNEL_1, duty_target);
    } else {
        gpio_set_level(H_L, 0);
        stop_right();

        gpio_set_level(H_R, 1);
        set_pwm(LEDC_CHANNEL_0, duty_target);
    }
}

static void actualizar_leds(void)
{
    gpio_set_level(LED_OK,  estado_actual == SENTIDO_1);
    gpio_set_level(LED_ERR, estado_actual == SENTIDO_2);
}

static void delay_visual(int t_ms)
{
    for (int i = 0; i < t_ms / 3; i++)
        multiplexar(porcentaje);
}

static void cambiar_sentido(sentido_t nuevo)
{
    if (nuevo == estado_actual) return;

    apagar_puente();
    delay_visual(TIME_SWAP);

    estado_actual = nuevo;
    actualizar_leds();
    mover_motor();
}

static void esperar_boton(gpio_num_t btn)
{
    delay_visual(TIME_DEB);
    while (gpio_get_level(btn) == 0)
        multiplexar(porcentaje);
}

static void init_adc(void)
{
    adc_oneshot_unit_init_cfg_t cfg = { .unit_id = ADC_UNIT_1 };
    adc_oneshot_new_unit(&cfg, &adc_handle);

    adc_oneshot_chan_cfg_t ch = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12
    };
    adc_oneshot_config_channel(adc_handle, ADC_INPUT, &ch);
}

static void init_pwm(void)
{
    ledc_timer_config_t t = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_12_BIT,
        .freq_hz = PWM_FREQ
    };
    ledc_timer_config(&t);

    ledc_channel_config_t ch0 = {
        .gpio_num = L_L,
        .channel = LEDC_CHANNEL_0,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0,
        .duty = PWM_MAX_VAL
    };
    ledc_channel_config(&ch0);

    ledc_channel_config_t ch1 = {
        .gpio_num = L_R,
        .channel = LEDC_CHANNEL_1,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0,
        .duty = PWM_MAX_VAL
    };
    ledc_channel_config(&ch1);
}

static void config_pins(const gpio_num_t *pins, int n, gpio_mode_t mode)
{
    uint64_t mask = 0;
    for (int i = 0; i < n; i++)
        mask |= (1ULL << pins[i]);

    gpio_config_t cfg = {
        .pin_bit_mask = mask,
        .mode = mode,
        .pull_up_en = (mode == GPIO_MODE_INPUT),
        .pull_down_en = GPIO_PULLDOWN_DISABLE
    };
    gpio_config(&cfg);
}

static void setup(void)
{
    const gpio_num_t segs[] = {PIN_SEG_A, PIN_SEG_B, PIN_SEG_C, PIN_SEG_D, PIN_SEG_E, PIN_SEG_F, PIN_SEG_G};
    const gpio_num_t digs[] = {DIG_CENT, DIG_DEC, DIG_UNIT};
    const gpio_num_t btns[] = {BTN_LEFT, BTN_RIGHT};
    const gpio_num_t extra[] = {LED_OK, LED_ERR, H_L, H_R};

    config_pins(segs, 7, GPIO_MODE_OUTPUT);
    config_pins(digs, 3, GPIO_MODE_OUTPUT);
    config_pins(btns, 2, GPIO_MODE_INPUT);
    config_pins(extra, 4, GPIO_MODE_OUTPUT);

    init_adc();
    init_pwm();
}

void app_main(void)
{
    setup();

    estado_actual = SENTIDO_1;
    actualizar_leds();
    apagar_puente();

    while (1) {
        actualizar_adc();
        mantener_display(porcentaje, 20);

        if (!gpio_get_level(BTN_LEFT)) {
            cambiar_sentido(SENTIDO_1);
            esperar_boton(BTN_LEFT);
        }

        if (!gpio_get_level(BTN_RIGHT)) {
            cambiar_sentido(SENTIDO_2);
            esperar_boton(BTN_RIGHT);
        }

        mover_motor();
    }
}