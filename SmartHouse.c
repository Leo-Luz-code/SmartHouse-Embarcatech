/**
 * SmartHouse - Embarcatech - Leonardo Rodrigues Luz - Webserver Raspberry Pi Pico w - wlan
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

#include "lib/ssd1306.h" // Biblioteca para controle do display OLED SSD1306
#include "lib/font.h"

#define I2C_PORT i2c1 // Porta I2C utilizada
#define I2C_SDA 14    // GPIO14 - SDA
#define I2C_SCL 15    // GPIO15 - SCL
#define endereco 0x3C // Endereço do display OLED SSD1306
ssd1306_t ssd;        // Inicializa a estrutura do display

// Credenciais WIFI
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// Definição dos pinos dos LEDs
#define LED_PIN CYW43_WL_GPIO_LED_PIN // GPIO do CI CYW43
#define LED_BLUE_PIN 12               // GPIO12 - LED azul
#define LED_GREEN_PIN 11              // GPIO11 - LED verde
#define LED_RED_PIN 13                // GPIO13 - LED vermelho

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

    i2c_init(I2C_PORT, 400 * 1000);                               // Inicialização I2C em 400Khz
    bool cor = true;                                              // Cor do display
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
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000))
    {
        printf("Falha ao conectar ao Wi-Fi\n");
        ssd1306_draw_string(&ssd, "Falha tentando", 10, 29);
        ssd1306_draw_string(&ssd, "conectar", 10, 39);
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

        if (cyw43_arch_gpio_get(LED_PIN))
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
}

// Função de callback ao aceitar conexões TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, tcp_server_recv);
    return ERR_OK;
}

// Tratamento do request do usuário
void user_request(char **request)
{
    if (strstr(*request, "GET /sala") != NULL)
    {
        gpio_put(LED_BLUE_PIN, !gpio_get(LED_BLUE_PIN));
    }
    else if (strstr(*request, "GET /quarto") != NULL)
    {
        gpio_put(LED_GREEN_PIN, !gpio_get(LED_GREEN_PIN));
    }
    else if (strstr(*request, "GET /cozinha") != NULL)
    {
        gpio_put(LED_RED_PIN, !gpio_get(LED_RED_PIN));
    }
    else if (strstr(*request, "GET /varanda") != NULL)
    {
        cyw43_arch_gpio_put(LED_PIN, !cyw43_arch_gpio_get(LED_PIN));
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

    // Tratamento de request - Controle dos LEDs / Luzes da casa
    user_request(&request);

    // Leitura da temperatura interna / Temperatura da casa
    float temperature = temp_read();

    // Cria a resposta HTML
    char html[3 * 1024];

    // Instruções html do webserver
    snprintf(html, sizeof(html),
             "<!DOCTYPE html>\n"
             "<html>\n"
             "<head>\n"
             "<title>SmartHouse Control</title>\n"
             "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
             "<style>\n"
             "  body {\n"
             "    font-family: 'Segoe UI', Roboto, sans-serif;\n"
             "    max-width: 800px;\n"
             "    margin: 0 auto;\n"
             "    padding: 2rem;\n"
             "    background-color: #f8f9fa;\n"
             "    color: #212529;\n"
             "  }\n"
             "  .container {\n"
             "    background: white;\n"
             "    border-radius: 12px;\n"
             "    padding: 2rem;\n"
             "    box-shadow: 0 4px 6px rgba(0,0,0,0.1);\n"
             "  }\n"
             "  h1 {\n"
             "    color: #0d6efd;\n"
             "    text-align: center;\n"
             "    margin-bottom: 2rem;\n"
             "  }\n"
             "  .btn-grid {\n"
             "    display: grid;\n"
             "    grid-template-columns: 1fr 1fr;\n"
             "    gap: 1rem;\n"
             "    margin-bottom: 2rem;\n"
             "  }\n"
             "  .btn {\n"
             "    display: block;\n"
             "    text-decoration: none;\n"
             "    text-align: center;\n"
             "    padding: 1rem;\n"
             "    border-radius: 8px;\n"
             "    font-weight: 500;\n"
             "    transition: all 0.2s;\n"
             "  }\n"
             "  .btn-primary {\n"
             "    background-color: #0d6efd;\n"
             "    color: white;\n"
             "    border: 1px solid #0d6efd;\n"
             "  }\n"
             "  .btn-primary:hover {\n"
             "    background-color: #0b5ed7;\n"
             "  }\n"
             "  .status {\n"
             "    padding: 1rem;\n"
             "    border-radius: 8px;\n"
             "    margin-bottom: 1rem;\n"
             "    background-color: #f8f9fa;\n"
             "  }\n"
             "  .status-on {\n"
             "    color: #198754;\n"
             "    font-weight: bold;\n"
             "  }\n"
             "  .status-off {\n"
             "    color: #dc3545;\n"
             "    font-weight: bold;\n"
             "  }\n"
             "  .temperature {\n"
             "    text-align: center;\n"
             "    font-size: 1.25rem;\n"
             "    margin-top: 2rem;\n"
             "    color: #6c757d;\n"
             "  }\n"
             "  @media (max-width: 600px) {\n"
             "    .btn-grid { grid-template-columns: 1fr; }\n"
             "  }\n"
             "</style>\n"
             "</head>\n"
             "<body>\n"
             "  <div class=\"container\">\n"
             "    <h1>SmartHouse Control</h1>\n"
             "    \n"
             "    <div class=\"status\">\n"
             "      <p>Sala: <span class=\"%s\">%s</span></p>\n"
             "      <p>Quarto: <span class=\"%s\">%s</span></p>\n"
             "      <p>Cozinha: <span class=\"%s\">%s</span></p>\n"
             "      <p>Varanda: <span class=\"%s\">%s</span></p>\n"
             "    </div>\n"
             "    \n"
             "    <div class=\"btn-grid\">\n"
             "      <a href=\"./sala\" class=\"btn btn-primary\">Luz da Sala</a>\n"
             "      <a href=\"./quarto\" class=\"btn btn-primary\">Luz do Quarto</a>\n"
             "      <a href=\"./cozinha\" class=\"btn btn-primary\">Luz da Cozinha</a>\n"
             "      <a href=\"./varanda\" class=\"btn btn-primary\">Luz da Varanda</a>\n"
             "    </div>\n"
             "    \n"
             "    <div class=\"temperature\">\n"
             "      Temperatura: %.2f C\n"
             "    </div>\n"
             "  </div>\n"
             "</body>\n"
             "</html>\n",
             // Status Sala
             gpio_get(LED_BLUE_PIN) ? "status-on" : "status-off",
             gpio_get(LED_BLUE_PIN) ? "Ligada" : "Desligada",
             // Status Quarto
             gpio_get(LED_GREEN_PIN) ? "status-on" : "status-off",
             gpio_get(LED_GREEN_PIN) ? "Ligada" : "Desligada",
             // Status Cozinha
             gpio_get(LED_RED_PIN) ? "status-on" : "status-off",
             gpio_get(LED_RED_PIN) ? "Ligada" : "Desligada",
             // Status Varanda
             cyw43_arch_gpio_get(LED_PIN) ? "status-on" : "status-off",
             cyw43_arch_gpio_get(LED_PIN) ? "Ligada" : "Desligada",
             // Temperatura
             temperature);

    char header[150];
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n"
             "\r\n",
             strlen(html));

    // Envio em 2 etapas (mais seguro)
    tcp_write(tpcb, header, strlen(header), TCP_WRITE_FLAG_COPY);
    tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    pbuf_free(p);
    return ERR_OK;
}
