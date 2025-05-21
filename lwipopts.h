#ifndef LWIPOPTS_H
#define LWIPOPTS_H

// Configuração otimizada para páginas HTML maiores com suporte a DHCP
#define NO_SYS 1
#define LWIP_SOCKET 0
#define LWIP_NETCONN 0

// Protocolos
#define LWIP_TCP 1
#define LWIP_UDP 1 // Necessário para DHCP
#define LWIP_IPV4 1
#define LWIP_ICMP 1
#define LWIP_RAW 1

// Gerenciamento de Memória
#define MEM_ALIGNMENT 4
#define MEM_SIZE (12 * 1024) // 12KB para buffers maiores
#define MEMP_NUM_PBUF 24
#define PBUF_POOL_SIZE 24      // Aumentado para 24 buffers
#define PBUF_POOL_BUFSIZE 1024 // 1KB por buffer

// Conexões TCP
#define MEMP_NUM_TCP_PCB 6        // Aumentado para 6 conexões
#define MEMP_NUM_TCP_SEG 32       // Mais segmentos para dados grandes
#define TCP_WND (4 * TCP_MSS)     // Janela TCP maior
#define TCP_MSS 1460              // MTU padrão Ethernet
#define TCP_SND_BUF (4 * TCP_MSS) // Buffer de envio maior

// Conexões UDP (necessárias para DHCP)
#define MEMP_NUM_UDP_PCB 4

// DHCP/DNS
#define LWIP_DHCP 1
#define LWIP_AUTOIP 1 // Pode ser desligado se não usar
#define LWIP_DNS 1

// Configurações HTTPD
#define LWIP_HTTPD 1
#define LWIP_HTTPD_SSI 1
#define LWIP_HTTPD_SUPPORT_POST 1 // Mantido habilitado para flexibilidade
#define LWIP_HTTPD_DYNAMIC_HEADERS 1
#define LWIP_HTTPD_MAX_RESPONSE_DATA_LEN (3 * 1024 + 150) // Suporte a 3KB
#define HTTPD_USE_CUSTOM_FSDATA 0
#define LWIP_HTTPD_CGI 0 // Mantém desativado

// Outros
#define LWIP_NETIF_HOSTNAME 1

#endif /* LWIPOPTS_H */