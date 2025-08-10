#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/dns.h"
#include "lwip/init.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "pico/time.h"
#include "inc/ssd1306.h"
#include "ws2818b.pio.h"

// Quantidade de LEDs e pinagem
#define LED_COUNT 25
#define LED_PIN 7
// Pinos e definições do I2C
#define I2C_SDA 14
#define I2C_SCL 15

// Pinos dos LEDs e botões
#define LED_G 11
#define LED_B 12
#define LED_R 13
#define BTN_A_PIN 5
#define BTN_B_PIN 6

// Configuração do ThingSpeak e rede
#define WIFI_SSID "NomeDaSuaRede"
#define WIFI_PASS "SenhaDaSuaRede"

#define THINGSPEAK_HOST "api.thingspeak.com"
#define THINGSPEAK_PORT 80
#define API_KEY "87I2DNOEL7RUZHHB" // KEY do canal: https://thingspeak.mathworks.com/channels/2840385

// Estrutura para os pixels dos LEDs
typedef struct
{
    uint8_t G, R, B;
} npLED_t;
npLED_t leds[LED_COUNT];

PIO np_pio;
uint sm;

// Variável global para armazenar o IP resolvido do ThingSpeak
ip_addr_t server_ip;

// Funções para controle  da Matriz de LEDs (WS2818B via PIO)
void npInit(uint pin)
{

    // Cria o PIO e toma posse da máquina
    uint offset = pio_add_program(pio0, &ws2818b_program);
    np_pio = pio0;
    sm = pio_claim_unused_sm(np_pio, false);
    if (sm < 0)
    {
        np_pio = pio1;
        sm = pio_claim_unused_sm(np_pio, true);
    }
    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);

    // Limpa o buffer
    for (uint i = 0; i < LED_COUNT; ++i)
    {
        leds[i].R = 0;
        leds[i].G = 0;
        leds[i].B = 0;
    }
}

// Seta as cores RGB para um LED da matriz
void npSetLED(uint index, uint8_t r, uint8_t g, uint8_t b)
{
    leds[index].R = r;
    leds[index].G = g;
    leds[index].B = b;
}

// Escreve os dados no buffer, ligando os LEDs respectivamente na cor e ordem certa
// Usa temporizador para o sinal de RESET

void npWrite()
{
    for (uint i = 0; i < LED_COUNT; ++i)
    {
        pio_sm_put_blocking(np_pio, sm, leds[i].G);
        pio_sm_put_blocking(np_pio, sm, leds[i].R);
        pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
    // Aguarda 100 µs usando get_absolute_time() e delayed_by_us()
    absolute_time_t next_wake_time = delayed_by_us(get_absolute_time(), 100);
    while (!time_reached(next_wake_time))
    {
    } // Busy-wait até que 100 µs se passem
}

// Cria as predefinições para a comunicação I2C
void setup_i2c()
{
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
}

// Inicializa os pinos dos botões
void inileds()
{
    gpio_init(BTN_A_PIN);
    gpio_set_dir(BTN_A_PIN, GPIO_IN);
    gpio_pull_up(BTN_A_PIN);

    gpio_init(BTN_B_PIN);
    gpio_set_dir(BTN_B_PIN, GPIO_IN);
    gpio_pull_up(BTN_B_PIN);
}

// Atualiza o display e a matriz de LEDs de acordo com o estado dos botões
void display_vagas(int btnA, int btnB)
{

    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);

    struct render_area frame_area = {
        .start_column = 0,
        .end_column = ssd1306_width - 1,
        .start_page = 0,
        .end_page = ssd1306_n_pages - 1};

    // Calcula o buffer para a área de renderização
    calculate_render_area_buffer_length(&frame_area);

    // Analisa em qual ocasião está o botão e define quais informações transmitirá o display e matriz
    char *textos[2];

    if (btnA == 0 && btnB == 0)
    {
        textos[0] = "Nenhuma";
        textos[1] = "vaga";
        npSetLED(24, 255, 0, 0);
        npSetLED(23, 255, 0, 0);
    }
    else if (btnA == 1 && btnB == 1)
    {
        textos[0] = "Vagas livres";
        textos[1] = "A1   A2";
        npSetLED(24, 0, 255, 0);
        npSetLED(23, 0, 255, 0);
    }
    else if (btnB == 0)
    {
        textos[0] = "Vagas livres";
        textos[1] = "A1";
        npSetLED(24, 0, 255, 0);
        npSetLED(23, 255, 0, 0);
    }
    else if (btnA == 0)
    {
        textos[0] = "Vagas livres";
        textos[1] = "     A2";
        npSetLED(24, 255, 0, 0);
        npSetLED(23, 0, 255, 0);
    }

    // Escreve o texto no display nas coordenadas definidas
    // Posteriormente pula uma quantidade y de linhas para o próximo texto
    int y = 16;
    for (uint i = 0; i < (sizeof(textos) / sizeof(textos[0])); i++)
    {
        ssd1306_draw_string(ssd, 5, y, textos[i]);
        y += 32;
    }
    render_on_display(ssd, &frame_area);
    npWrite();
}

// Funções de Rede e ThingSpeak

// Callback para tratar a resposta HTTP
static err_t http_recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (p == NULL)
    {
        tcp_close(tpcb);
        return ERR_OK;
    }
    printf("Resposta recebida do ThingSpeak: %.*s\n", p->len, (char *)p->payload);
    pbuf_free(p);
    return ERR_OK;
}

// callback para quando a conexão for estabelecida
static err_t http_connected_callback(void *arg, struct tcp_pcb *tpcb, err_t err)
{
    if (err != ERR_OK)
    {
        printf("Falha na conexão TCP\n");
        return err;
    }

    printf("Conexão com o ThingSpeak estabelecida\n");

    // Ler sinal dos botões
    int16_t sinalpin1 = gpio_get(BTN_A_PIN);
    int16_t sinalpin2 = gpio_get(BTN_B_PIN);

    // Monta a requisição HTTP GET com os dados dos botões e a chave de API
    char request[256];
    snprintf(request, sizeof(request),
             "GET /update?api_key=%s&field1=%d&field2=%d HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n"
             "\r\n",
             API_KEY, sinalpin1, sinalpin2, THINGSPEAK_HOST);

    // Envia a requisição para o servidor
    tcp_write(tpcb, request, strlen(request), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);
    tcp_recv(tpcb, http_recv_callback);

    return ERR_OK;
}

// Callback para quando a solução de DNS for resolvida

static void dns_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{
    if (ipaddr)
    {
        printf("IP resolvido para ThingSpeak: %s\n", ipaddr_ntoa(ipaddr));
        server_ip = *ipaddr;                        // Armazena o IP resolvido
        struct tcp_pcb *tcp_client_pcb = tcp_new(); // cria uma nova conexao tcp

        // Inicia a conexão TCP
        tcp_connect(tcp_client_pcb, ipaddr, THINGSPEAK_PORT, http_connected_callback);
    }
    else
    {
        printf("Erro ao resolver DNS\n");
    }
}

int main()
{
    stdio_init_all();
    // Inicializa a matriz de LEDs (WS2818B)
    npInit(LED_PIN);
    // Configura I2C e o display SSD1306
    setup_i2c();
    ssd1306_init();
    // Inicializa os botões
    inileds();
    // Inicialização de Wi-Fi
    if (cyw43_arch_init())
    {
        printf("Erro ao configurar o Wi-Fin");
        return 1; // Termina o programa se houver erro de conexão
    }
    cyw43_arch_enable_sta_mode(); // Habilita o modo estação

    // Observa se ao conectar o Wi-Fi com as credenciais definidas houve erro
    if (cyw43_arch_wifi_connect_blocking(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_MIXED_PSK))
    {
        printf("Erro ao estabelecer conexão Wi-Fi\n");
        return 1;
    }
    printf("Conexão Wi-Fi estabelecida\n");
    int btnA = 1;
    int btnB = 1;
    display_vagas(btnA, btnB);
    // Variáveis para detectar mudanças no estado dos botões
    int last_btnA = 1, last_btnB = 1;
    while (true)
    {
        btnA = gpio_get(BTN_A_PIN);
        btnB = gpio_get(BTN_B_PIN);
        // Atualiza display e LEDs se houver alteração no estado dos botões
        if (btnA != last_btnA || btnB != last_btnB)
        {
            printf("Atualizando display: BTN_A=%d, BTN_B=%d\n", btnA, btnB); // printf para debug
            display_vagas(btnA, btnB);
            last_btnA = btnA;
            last_btnB = btnB;
        }
        // Resolve o domínio do ThingSpeak para obter o endereço IP
        dns_gethostbyname(THINGSPEAK_HOST, &server_ip, dns_callback, NULL);

        sleep_ms(500); // Realiza uma pausa de 0,5 segundos na placa para menos estresse da placa e economia de energia
    }

    // O código não chega a essa parte pelo loop, apenas uma boa prática de programação
    return 0;
}
