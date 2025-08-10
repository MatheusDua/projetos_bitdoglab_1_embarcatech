#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"
#include "inc/ssd1306.h"

// Definição dos pinos
#define VRX 26
#define VRY 27
#define SW 22
#define LED_B 12
#define LED_R 13
#define BUZZER_PIN 21
#define LED_G 11
const uint I2C_SDA = 14;
const uint I2C_SCL = 15;

// Definição do PWM
#define DIVIDER_PWM 16.0
#define PERIOD 4096
#define LED_STEP 1
uint slice_led_b, slice_led_r, slice_led_g;
uint16_t led_b_level = 0, led_r_level = 0;
uint16_t LED_G_level = 0;
uint up_down = 1;

// Variáveis para o botão
volatile bool button_pressed = false;
int opcao = 0;
uint16_t estagio_menu = 0;


// Notas "fictícias", pois aparentemente o buzzer é ativo
const uint jingle_bell_notes[] = {
    2637, 2637, 2637, 0, 0, 2637, 2637, 2637, 0, 0,
    2637, 3136, 2093, 2349, 2637, 0, 0, 0,
    2637, 2349, 2349, 2637, 2349, 0, 0, 3136, 0, 0,
    2637, 2637, 2637, 0, 0, 2637, 2637, 2637, 0, 0,
    2637, 3136, 2093, 2349, 2637, 0, 0, 0,
    3136, 3136, 2794, 2349, 2093, 0, 0, 0};

// Duração das notas em milissegundos
const uint note_duration[] = {
    350, 350, 350, 500, 500, 350, 350, 350, 500, 500,
    350, 350, 350, 350, 350, 500, 500, 500, 
    350, 350, 350, 350, 350, 500, 500, 350, 500, 500,
    350, 350, 350, 500, 500, 350, 350, 350, 500, 500,
    350, 350, 350, 350, 350, 500, 500, 500,
    350, 350, 350, 350, 350, 500, 500, 500,
};

// Iniciação do PWM do buzzer
void setup_pwm_buzzer(uint pin)
{
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 4.0f); 
    pwm_init(slice_num, &config, true);
    pwm_set_gpio_level(pin, 0); 
}

// Define como deve tocar as notas
void play_tone(uint pin, uint frequency, uint duration_ms)
{
    uint slice_num = pwm_gpio_to_slice_num(pin);
    uint32_t clock_freq = clock_get_hz(clk_sys);
    uint32_t top = clock_freq / frequency - 1;

    pwm_set_wrap(slice_num, top);
    pwm_set_gpio_level(pin, top / 2); 

    sleep_ms(duration_ms);

    pwm_set_gpio_level(pin, 0); 
    sleep_ms(50);               
}

// Toca a música de fato
void play_jingle_bell(uint pin)
{
    for (int i = 0; i < sizeof(jingle_bell_notes) / sizeof(jingle_bell_notes[0]); i++)
    {
        if (jingle_bell_notes[i] == 0)
        {
            sleep_ms(note_duration[i]);
        }
        else
        {
            play_tone(pin, jingle_bell_notes[i], note_duration[i]);
        }
    }
}

// Callback do botão SW
void gpio_callback(uint gpio, uint32_t events)
{
    if (gpio == SW)
    {
        button_pressed = true;
    }
}

// Inicia o I2C
void setup_i2c()
{
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

}

// Inicia pinos do joystick e faz a interrupção para o SW
void setup_joystick()
{
    adc_init();
    adc_gpio_init(VRX);
    adc_gpio_init(VRY);
    gpio_init(SW);
    gpio_set_dir(SW, GPIO_IN);
    gpio_pull_up(SW);
    gpio_set_irq_enabled_with_callback(SW, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
}

// Define os critérios para o sinal PWM
void setup_pwm_led(uint led, uint *slice, uint16_t level)
{
    gpio_set_function(led, GPIO_FUNC_PWM);
    *slice = pwm_gpio_to_slice_num(led);
    pwm_set_clkdiv(*slice, DIVIDER_PWM);
    pwm_set_wrap(*slice, PERIOD);
    pwm_set_gpio_level(led, level);
    pwm_set_enabled(*slice, true);
}

// Leitura e parâmetros do joystick
void joystick_read_axis(uint16_t *vry_value, uint16_t *vrx_value)
{
    adc_select_input(0);
    sleep_us(2);
    *vry_value = adc_read();
    adc_select_input(1);
    sleep_us(2);
    *vrx_value = adc_read();
}

// Parâmetros das cores do LED com base na posição dos eixos X/Y do joystick
void control_leds(uint16_t vry_value, uint16_t vrx_value)
{
    pwm_set_gpio_level(LED_B, vry_value);
    pwm_set_gpio_level(LED_R, vrx_value);
}

// PWM para o LED verde pulsar
void control_ledG_pulse()
{
    pwm_set_gpio_level(LED_G, LED_G_level);
    sleep_us(250);
    if (up_down)
    {
        LED_G_level += LED_STEP;
        if (LED_G_level >= PERIOD)
            up_down = 0;
    }
    else
    {
        LED_G_level -= LED_STEP;
        if (LED_G_level <= LED_STEP)
            up_down = 1;
    }

}

// Verifica o estado do botão para seleção do menu
void lerbotao()
{
    if (button_pressed == true)
    {
        sleep_ms(100);
        estagio_menu += 1;
        if (estagio_menu > 1)
        {
            estagio_menu = 0;
        }
    }
    button_pressed = false;
}

// Leitura dos estados do joystick para seleção do menu
void lerjoy()
{
    setup_joystick();

    uint16_t vry_value, vrx_value;

    joystick_read_axis(&vry_value, &vrx_value);

    if (vry_value > 2300)
    {
        opcao += 1;
        sleep_ms(300);
        if (opcao > 2)
        {
            opcao = 0;
        }
    }
    else if (vry_value < 1800)
    {
        opcao -= 1;
        sleep_ms(300);
        if (opcao < 0)
        {
            opcao = 2;
        }
    }
}

// Textos para a tela do menu
void displaymenu()
{

    ssd1306_init();
    struct render_area frame_area = {
        start_column : 0,
        end_column : ssd1306_width - 1,
        start_page : 0,
        end_page : ssd1306_n_pages - 1
    };

    calculate_render_area_buffer_length(&frame_area);

    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);

restart:

    char *apontador[3];

    if (opcao == 2)
    {
        apontador[0] = "0  Joystick ";
        apontador[1] = "   Jingle bell ";
        apontador[2] = "   LED Pulsante";
    }
    else if (opcao == 1)
    {
        apontador[0] = "   Joystick ";
        apontador[1] = "0  Jingle bell ";
        apontador[2] = "   LED Pulsante";
    }
    else if (opcao == 0)
    {
        apontador[0] = "   Joystick ";
        apontador[1] = "   Jingle bell ";
        apontador[2] = "0  LED Pulsante";
    }
    if (estagio_menu == 1 && opcao == 2)
    {
        apontador[0] = "  Joystick PWM";
        apontador[1] = "led R    Eixo X";
        apontador[2] = "led B    Eixo Y";
    }
    if (estagio_menu == 1 && opcao == 1)
    {
        apontador[0] = "Musica tocando";
        apontador[1] = "    Aguarde";
        apontador[2] = "  jingle bell";
    }

    if (estagio_menu == 1 && opcao == 0)
    {
        apontador[0] = "    LED PWM";
        apontador[1] = " ";
        apontador[2] = "  Pulsos PWM";
    }
    int y = 10;
    for (uint i = 0; i < count_of(apontador); i++) 
    {
        ssd1306_draw_string(ssd, 1, y, apontador[i]);
        y += 15; 
    }
    render_on_display(ssd, &frame_area);
}

// Função principal do programa
int main()
{
    
    stdio_init_all();
    setup_joystick();
    setup_pwm_buzzer(BUZZER_PIN);
    setup_i2c();
    lerbotao();
    displaymenu();
    uint16_t vry_value, vrx_value;

    
    while (1)
    {
        // Liga o display
        displaymenu();
        lerbotao();
        

        setup_pwm_led(LED_B, &slice_led_b, led_b_level);
        setup_pwm_led(LED_R, &slice_led_r, led_r_level);
        setup_pwm_led(LED_G, &slice_led_g, 0);
        // Correção dos números inteiros no eixo X, devido ao fato de ele nunca chegar definitivamente a 0
        int vrx_true = vrx_value - 19;
        if (vrx_true <= 0 || vrx_true > 4096)
        {
            vrx_true = 0;
        }

        // Default do menu
        if (estagio_menu == 0)
        {
            sleep_ms(200);
            displaymenu();
            lerjoy();
            
        }

        // Estados ao pressionar os botões

        // Joystick que varia o LED_PWM com a posição dos eixos
        if (estagio_menu == 1 && opcao == 2)
        {   
            joystick_read_axis(&vry_value, &vrx_value);
            control_leds(vry_value, vrx_true);
        }

        // Toca a música Jingle Bell
        if (estagio_menu == 1 && opcao == 1)
        {   
            displaymenu();
            play_jingle_bell(BUZZER_PIN);
            estagio_menu = 0;
        }

        // Deixa o PWM pulsar na cor verde
        
        if (estagio_menu == 1 && opcao == 0)
        {   
            displaymenu();
            while(estagio_menu ==1)
            {
             control_ledG_pulse();
             lerbotao();
            } 
            estagio_menu = 0;
        } 

        // Reescreve o estado de pressionamento do SW para o menu
    }

    
}
