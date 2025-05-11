/**
 * AULA IoT - Embarcatech - Ricardo Prates - 004 - Webserver Raspberry Pi Pico w - wlan
 *
 * Material de suporte
 *
 * https://www.raspberrypi.com/documentation/pico-sdk/networking.html#group_pico_cyw43_arch_1ga33cca1c95fc0d7512e7fef4a59fd7475
 */

#include <stdio.h>  // Biblioteca padrão para entrada e saída
#include <string.h> // Biblioteca manipular strings
#include <stdlib.h> // funções para realizar várias operações, incluindo alocação de memória dinâmica (malloc)

#include "pico/stdlib.h"     // Biblioteca da Raspberry Pi Pico para funções padrão (GPIO, temporização, etc.)
#include "hardware/adc.h"    // Biblioteca da Raspberry Pi Pico para manipulação do conversor ADC
#include "pico/cyw43_arch.h" // Biblioteca para arquitetura Wi-Fi da Pico com CYW43

#include "lwip/pbuf.h"  // Lightweight IP stack - manipulação de buffers de pacotes de rede
#include "lwip/tcp.h"   // Lightweight IP stack - fornece funções e estruturas para trabalhar com o protocolo TCP
#include "lwip/netif.h" // Lightweight IP stack - fornece funções e estruturas para trabalhar com interfaces de rede (netif)

#include "lib/np_led.h"  // Biblioteca para controle de LEDs Neopixel
#include "lib/ssd1306.h" // Biblioteca para controle do display OLED SSD1306
#include "lib/font.h"

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
ssd1306_t ssd; // Inicializa a estrutura do display

// Credenciais WIFI - Tome cuidado se publicar no github!
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// Definição dos pinos dos LEDs
#define LED_PIN CYW43_WL_GPIO_LED_PIN // GPIO do CI CYW43
#define LED_BLUE_PIN 12               // GPIO12 - LED azul
#define LED_GREEN_PIN 11              // GPIO11 - LED verde
#define LED_RED_PIN 13                // GPIO13 - LED vermelho
#define MATRIX_LED_PIN 7              // Pino do LED neopixel

bool luz_da_varanda = false; // Estado da luz da varanda

// Inicializar os Pinos GPIO para acionamento dos LEDs da BitDogLab
void gpio_led_bitdog(void);

// Função de callback ao aceitar conexões TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err);

// Função de callback para processar requisições HTTP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);

// Leitura da temperatura interna
float temp_read(void);

// Tratamento do request do usuário
void user_request(char **request);

// Função principal
int main()
{
    // Inicializa todos os tipos de bibliotecas stdio padrão presentes que estão ligados ao binário.
    stdio_init_all();

    // Inicializar os Pinos GPIO para acionamento dos LEDs da BitDogLab
    gpio_led_bitdog();

    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400 * 1000);
    bool cor = true;
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_pull_up(I2C_SDA);                                        // Pull up the data line
    gpio_pull_up(I2C_SCL);                                        // Pull up the clock line
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd);                                         // Configura o display
    ssd1306_send_data(&ssd);                                      // Envia os dados para o display
    ssd1306_fill(&ssd, false);                                    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, !cor);                                     // Limpa o display
    ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor);                 // Desenha um retângulo
    ssd1306_line(&ssd, 3, 16, 123, 16, cor);                      // Desenha uma linha
    ssd1306_draw_string(&ssd, "SmartHouse", 20, 6);               // Desenha uma string
    ssd1306_send_data(&ssd);

    // Inicializa a arquitetura do cyw43
    while (cyw43_arch_init())
    {
        printf("Falha ao inicializar Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }

    // GPIO do CI CYW43 em nível baixo
    cyw43_arch_gpio_put(LED_PIN, 0);

    // Ativa o Wi-Fi no modo Station, de modo a que possam ser feitas ligações a outros pontos de acesso Wi-Fi.
    cyw43_arch_enable_sta_mode();

    // Conectar à rede WiFI - fazer um loop até que esteja conectado
    printf("Conectando ao Wi-Fi...\n");
    ssd1306_draw_string(&ssd, "Conectando...", 10, 29);
    ssd1306_send_data(&ssd);
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 20000))
    {
        printf("Falha ao conectar ao Wi-Fi\n");
        ssd1306_draw_string(&ssd, "Falha ao conectar", 10, 29);
        ssd1306_send_data(&ssd);
        sleep_ms(100);
        return -1;
    }
    printf("Conectado ao Wi-Fi\n");

    // Caso seja a interface de rede padrão - imprimir o IP do dispositivo.
    if (netif_default)
    {
        printf("IP do dispositivo: %s\n", ipaddr_ntoa(&netif_default->ip_addr));
    }

    // Configura o servidor TCP - cria novos PCBs TCP. É o primeiro passo para estabelecer uma conexão TCP.
    struct tcp_pcb *server = tcp_new();
    if (!server)
    {
        printf("Falha ao criar servidor TCP\n");
        return -1;
    }

    // vincula um PCB (Protocol Control Block) TCP a um endereço IP e porta específicos.
    if (tcp_bind(server, IP_ADDR_ANY, 80) != ERR_OK)
    {
        printf("Falha ao associar servidor TCP à porta 80\n");
        return -1;
    }

    // Coloca um PCB (Protocol Control Block) TCP em modo de escuta, permitindo que ele aceite conexões de entrada.
    server = tcp_listen(server);

    // Define uma função de callback para aceitar conexões TCP de entrada. É um passo importante na configuração de servidores TCP.
    tcp_accept(server, tcp_server_accept);
    printf("Servidor ouvindo na porta 80\n");

    // Inicializa o conversor ADC
    adc_init();
    adc_set_temp_sensor_enabled(true);

    while (true)
    {
        /*
         * Efetuar o processamento exigido pelo cyw43_driver ou pela stack TCP/IP.
         * Este método deve ser chamado periodicamente a partir do ciclo principal
         * quando se utiliza um estilo de sondagem pico_cyw43_arch
         */
        cyw43_arch_poll(); // Necessário para manter o Wi-Fi ativo

        if (gpio_get(LED_BLUE_PIN))
        {
            ssd1306_draw_string(&ssd, "Sala: On ", 10, 19);
        }
        else
        {
            ssd1306_draw_string(&ssd, "Sala: Off ", 10, 19);
        }

        if (gpio_get(LED_GREEN_PIN))
        {
            ssd1306_draw_string(&ssd, "Quarto: On   ", 10, 29);
        }
        else
        {
            ssd1306_draw_string(&ssd, "Quarto: Off  ", 10, 29);
        }

        if (gpio_get(LED_RED_PIN))
        {
            ssd1306_draw_string(&ssd, "Cozinha: On ", 10, 39);
        }
        else
        {
            ssd1306_draw_string(&ssd, "Cozinha: Off", 10, 39);
        }

        if (luz_da_varanda)
        {
            ssd1306_draw_string(&ssd, "Varanda: On ", 10, 49);
        }
        else
        {
            ssd1306_draw_string(&ssd, "Varanda: Off", 10, 49);
        }

        ssd1306_send_data(&ssd); // Atualiza o display

        sleep_ms(100); // Reduz o uso da CPU
    }

    // Desligar a arquitetura CYW43.
    cyw43_arch_deinit();
    return 0;
}

// -------------------------------------- Funções ---------------------------------

// Inicializar os Pinos GPIO para acionamento dos LEDs da BitDogLab
void gpio_led_bitdog(void)
{
    // Configuração dos LEDs como saída
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    gpio_put(LED_BLUE_PIN, false);

    gpio_init(LED_GREEN_PIN);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_put(LED_GREEN_PIN, false);

    gpio_init(LED_RED_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_put(LED_RED_PIN, false);

    npInit(MATRIX_LED_PIN); // Inicializa o LED neopixel
    npClear();              // Limpa o buffer do LED neopixel
}

// Inicializa o display OLED SSD1306

// Função de callback ao aceitar conexões TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, tcp_server_recv);
    return ERR_OK;
}

void luzDaVaranda(void)
{

    luz_da_varanda = !luz_da_varanda; // Alterna o estado da luz da varanda

    if (luz_da_varanda)
    {
        // Liga a luz da varanda
        npOn();
        npWrite();
    }
    else
    {
        // Desliga a luz da varanda
        npClear();
        npWrite();
    }
}

// Tratamento do request do usuário - digite aqui
void user_request(char **request)
{
    if (strstr(*request, "GET /sala") != NULL)
    {
        gpio_put(LED_BLUE_PIN, gpio_get(LED_BLUE_PIN) == 0 ? 1 : 0);
    }
    else if (strstr(*request, "GET /quarto") != NULL)
    {
        gpio_put(LED_GREEN_PIN, gpio_get(LED_GREEN_PIN) == 0 ? 1 : 0);
    }
    else if (strstr(*request, "GET /cozinha") != NULL)
    {
        gpio_put(LED_RED_PIN, gpio_get(LED_RED_PIN) == 0 ? 1 : 0);
    }
    else if (strstr(*request, "GET /varanda") != NULL)
    {
        luzDaVaranda();
    }
    else if (strstr(*request, "GET /on") != NULL)
    {
        cyw43_arch_gpio_put(LED_PIN, 1);
    }
    else if (strstr(*request, "GET /off") != NULL)
    {
        cyw43_arch_gpio_put(LED_PIN, 0);
    }
};

// Leitura da temperatura interna
float temp_read(void)
{
    adc_select_input(4);
    uint16_t raw_value = adc_read();
    const float conversion_factor = 3.3f / (1 << 12);
    float temperature = 27.0f - ((raw_value * conversion_factor) - 0.706f) / 0.001721f;
    return temperature;
}

// Função de callback para processar requisições HTTP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (!p)
    {
        tcp_close(tpcb);
        tcp_recv(tpcb, NULL);
        return ERR_OK;
    }

    // Alocação do request na memória dinámica
    char *request = (char *)malloc(p->len + 1);
    memcpy(request, p->payload, p->len);
    request[p->len] = '\0';

    printf("Request: %s\n", request);

    // Tratamento de request - Controle dos LEDs
    user_request(&request);

    // Leitura da temperatura interna
    float temperature = temp_read();

    // Variáveis de estado dos LEDs (0 = desligado, 1 = ligado)
    int blue_status = 0;  // Estado inicial do LED Azul
    int green_status = 0; // Estado inicial do LED Verde
    int red_status = 0;   // Estado inicial do LED Vermelho

    // Cria a resposta HTML
    char html[1024];

    // Instruções html do webserver
    snprintf(html, sizeof(html),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "\r\n"
             "<!DOCTYPE html>\n"
             "<html>\n"
             "<head>\n"
             "<title>Embarcatech - SmartHouse Control</title>\n"
             "<style>\n"
             "body { font-family: sans-serif; max-width: 600px; margin: 0 auto; padding: 2rem; line-height: 1.5; }\n"
             "h1 { text-align: center; margin-bottom: 2rem; }\n"
             "form { margin: 0.5rem 0; text-align: center; }\n"
             "button { padding: 0.75rem 1.5rem; font-size: 1rem; background: #f0f0f0; border: 1px solid #ddd; cursor: pointer; }\n"
             "button:hover { background: #e0e0e0; }\n"
             ".temperature { text-align: center; margin-top: 2rem; font-size: 1.25rem; }\n"
             "</style>\n"
             "</head>\n"
             "<body>\n"
             "<h1>SmartHouse Control</h1>\n"
             "<form action=\"./sala\"><button>Ligar/Desligar Luz da Sala</button></form>\n"
             "<form action=\"./quarto\"><button>Ligar/Desligar Luz do Quarto</button></form>\n"
             "<form action=\"./cozinha\"><button>Ligar/Desligar Luz da Cozinha</button></form>\n"
             "<form action=\"./varanda\"><button>Ligar/Desligar Luz da Varanda</button></form>\n"
             "<p class=\"temperature\">Temperatura da casa: %.2f C</p>\n"
             "</body>\n"
             "</html>\n",
             temperature);

    // Escreve dados para envio (mas não os envia imediatamente).
    tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY);

    // Envia a mensagem
    tcp_output(tpcb);

    // libera memória alocada dinamicamente
    free(request);

    // libera um buffer de pacote (pbuf) que foi alocado anteriormente
    pbuf_free(p);

    return ERR_OK;
}
