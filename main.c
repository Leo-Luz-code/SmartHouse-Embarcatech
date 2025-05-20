#include "lwip/apps/httpd.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwipopts.h"
#include "ssi.h"
#include "cgi.h"

// Credenciais WiFi
#define SSID "XXX"
#define PASSWORD "XXX"

int main()
{
    stdio_init_all();
    cyw43_arch_init();
    cyw43_arch_enable_sta_mode();

    // Tentando conectar
    while (cyw43_arch_wifi_connect_timeout_ms(SSID, PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000))
    {
        printf("Tentando conectar...\n");
    }

    // Conectado com sucesso
    printf("Conectou com sucesso\n");

    // Inicializando servidor
    httpd_init();
    printf("Servidor HTTP inicializado\n");

    // Inicializando SSI e CGI
    ssi_init();
    printf("Handler SSI incializado\n");

    cgi_init();
    printf("Handler CGI incializado\n");

    // Loop infinito
    while (1)
    {
        /* code */
    }
}