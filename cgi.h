#include "lwip/apps/httpd.h"
#include "pico/cyw43_arch.h"

// Handler CGI para /led.cgi request
const char *cgi_led_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    // Verifica se foi feita uma requisição para o LED /led.cgi?led=x
    if (strcmp(pcParam[0], "led") == 0)
    {
        // Verifica o valor do parâmetro
        if (strcmp(pcValue[0], "0") == 0)
        {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1); // Liga o LED
        }
        else if (strcmp(pcValue[0], "1") == 0)
        {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0); // Desliga o LED
        }
    }

    // Envia o index HTML ao usuário
    return "/index.shtml";
}

// tCGI struct
static const tCGI cgi_handlers[] = {
    {
        "/led.cgi", cgi_led_handler // Handler CGI para /led.cgi
    },
};

// Inicializa o CGI
void cgi_init()
{
    // Inicializa o CGI
    http_set_cgi_handlers(cgi_handlers, 1);
}