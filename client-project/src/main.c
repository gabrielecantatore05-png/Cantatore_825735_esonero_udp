#if defined _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define closesocket close
#else
    #include <string.h>
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #define closesocket close
    #define SOCKET int
    #define INVALID_SOCKET -1
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "protocol.h"


#define BUFFMAX 255
#define MAX_CITY_LEN 64

// Struttura risposta per deserializzazione
/*struct response {
    unsigned int status;
    char type;
    float value;
};*/

void clearwinsock() {
#if defined _WIN32
    WSACleanup();
#endif
}

int main(void) {
#if defined _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        printf("Error at WSAStartup()\n");
        return 0;
    }
#endif

    // 1. INPUT SERVER E PORTA (Struttura richiesta)
    char input_buffer[BUFFMAX];
    memset(input_buffer, 0, BUFFMAX);
    printf("Inserire server e porta (formato servername:port): ");
    fgets(input_buffer, BUFFMAX, stdin);

    char *name = strtok(input_buffer, ":\n");
    char *port_str = strtok(NULL, ":\n");
    if (name == NULL || port_str == NULL) {
        printf("Formato non valido.\n");
        clearwinsock();
        return -1;
    }
    int port = atoi(port_str);

    struct hostent *host = gethostbyname(name);
    if (host == NULL) {
        printf("Errore risoluzione host.\n");
        clearwinsock();
        return -1;
    }

    // Risoluzione IP e Reverse DNS per output richiesto
    struct in_addr addr = *(struct in_addr *)host->h_addr;
    char *ip_str = inet_ntoa(addr);
    struct hostent *rev_host = gethostbyaddr((const char*)&addr, sizeof(addr), AF_INET);
    char *canonical_name = (rev_host) ? rev_host->h_name : name;

    // 2. INPUT RICHIESTA METEO (formato "t bari")
    printf("Inserire la richiesta meteo (formato \"tipo citta'\", es: t bari): ");
    char req_buffer[BUFFMAX];
    fgets(req_buffer, BUFFMAX, stdin);
    req_buffer[strcspn(req_buffer, "\n")] = 0; // rimuove newline

    char type = req_buffer[0];
    char *city_part = strchr(req_buffer, ' ');
    if (city_part == NULL || (city_part - req_buffer) != 1) {
        printf("Errore: il primo token deve essere un singolo carattere.\n");
        clearwinsock();
        return -1;
    }
    char *city = city_part + 1;
    while(isspace(*city)) city++; // salta spazi multipli

    if (strlen(city) >= MAX_CITY_LEN) {
        printf("Errore: nome città troppo lungo.\n");
        clearwinsock();
        return -1;
    }

    // 3. CREAZIONE SOCKET E INVIO
    int my_socket;
    if ((my_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        printf("Errore creazione socket\n");
        clearwinsock();
        return -1;
    }

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr = addr;

    // SERIALIZZAZIONE MANUALE (Richiesta)
    char send_buf[1 + MAX_CITY_LEN];
    memset(send_buf, 0, sizeof(send_buf));
    send_buf[0] = type;
    strncpy(send_buf + 1, city, MAX_CITY_LEN - 1);

    sendto(my_socket, send_buf, 1 + strlen(city) + 1, 0, 
           (struct sockaddr *)&server_address, sizeof(server_address));

    // 4. RICEZIONE E DESERIALIZZAZIONE
    char recv_buf[512];
    unsigned int srv_size = sizeof(server_address);
    int n = recvfrom(my_socket, recv_buf, 512, 0, (struct sockaddr *)&server_address, &srv_size);

    if (n >= 9) {
        // Deserializzazione manuale (Response)
        uint32_t net_status, net_val;
        memcpy(&net_status, recv_buf, 4);
        uint32_t status = ntohl(net_status);
        
        char r_type = recv_buf[4];
        
        memcpy(&net_val, recv_buf + 5, 4);
        net_val = ntohl(net_val);
        float val;
        memcpy(&val, &net_val, 4);

        // Formattazione output richiesta
        printf("Ricevuto risultato dal server %s (ip %s). ", canonical_name, ip_str);

        if (status == 1) printf("Città non disponibile\n");
        else if (status == 2) printf("Richiesta non valida\n");
        else {
            city[0] = toupper(city[0]); // Capitalizzazione prima lettera
            if (r_type == 't') printf("%s: Temperatura = %.1f°C\n", city, val);
            else if (r_type == 'h') printf("%s: Umidità = %.1f%%\n", city, val);
            else if (r_type == 'w') printf("%s: Vento = %.1f km/h\n", city, val);
            else if (r_type == 'p') printf("%s: Pressione = %.1f hPa\n", city, val);
        }
    }

    closesocket(my_socket);
    clearwinsock();
    return 0;
}