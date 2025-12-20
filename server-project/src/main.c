#if defined _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
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
#include <time.h>
#include <ctype.h>

#define PORT 56700 
#define MAX_CITY_LEN 64
#define BUFFMAX 512


float get_temperature() { return (float)(rand() % 500) / 10.0f - 10.0f; } 
float get_humidity()    { return (float)(rand() % 800) / 10.0f + 20.0f; } 
float get_wind()        { return (float)(rand() % 1000) / 10.0f;}
float get_pressure()    { return (float)(rand() % 1000) / 10.0f + 950.0f; } 


int validate_city(const char* city) {
    const char* cities[] = {"Bari", "Roma", "Milano", "Napoli", "Torino", "Palermo", "Genova", "Bologna", "Firenze", "Venezia"};
    for(int i = 0; i < 10; i++) {
        if(strcasecmp(city, cities[i]) == 0) return 1;
    }
    return 0;
}

// Verifica caratteri non ammessi (tabulazioni o simboli speciali)
int has_invalid_chars(const char* city) {
    if (strchr(city, '\t') != NULL) return 1;
    if (strpbrk(city, "@#$%&") != NULL) return 1;
    return 0;
}

void clearwinsock() {
#if defined _WIN32
    WSACleanup();
#endif
}

int main(int argc, char *argv[]) {
    srand(time(NULL));
    int server_port = PORT;

    // Parsing porta da linea di comando
    if (argc == 3 && strcmp(argv[1], "-p") == 0) {
        server_port = atoi(argv[2]);
    }

#if defined _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        printf("Error at WSAStartup()\n");
        return -1;
    }
#endif

    SOCKET my_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (my_socket == INVALID_SOCKET) {
        perror("Errore creazione socket");
        clearwinsock();
        return -1;
    }

    struct sockaddr_in server_address, client_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);
    server_address.sin_addr.s_addr = INADDR_ANY; 

    if (bind(my_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Errore bind");
        closesocket(my_socket);
        clearwinsock();
        return -1;
    }

    printf("Server Meteo UDP in ascolto sulla porta %d...\n", server_port);

    char recv_buffer[1 + MAX_CITY_LEN];
    while (1) {
        socklen_t client_len = sizeof(client_address);
        memset(recv_buffer, 0, sizeof(recv_buffer));

        int bytes_received = recvfrom(my_socket, recv_buffer, sizeof(recv_buffer), 0, 
                                      (struct sockaddr *)&client_address, &client_len);
        
        if (bytes_received <= 0) continue;

        // 1. Deserializzazione richiesta
        char type = recv_buffer[0];
        char city[MAX_CITY_LEN];
        memcpy(city, recv_buffer + 1, MAX_CITY_LEN);
        city[MAX_CITY_LEN - 1] = '\0';

        // 2. DNS Reverse Lookup per il log
        struct hostent *client_host = gethostbyaddr((const char *)&client_address.sin_addr, 
                                                    sizeof(client_address.sin_addr), AF_INET);
        char *host_name = (client_host) ? client_host->h_name : "Unknown";
        
        // Log obbligatorio
        printf("Richiesta ricevuta da %s (ip %s): type='%c', city='%s'\n", 
               host_name, inet_ntoa(client_address.sin_addr), type, city);

        // 3. Logica di Business e Validazione
        unsigned int status = 0;
        float value = 0.0f;

        if (type != 't' && type != 'h' && type != 'w' && type != 'p') {
            status = 2; // Tipo non valido
        } else if (has_invalid_chars(city)) {
            status = 2; // Caratteri speciali/tab ammessi -> errore validazione status 2
        } else if (!validate_city(city)) {
            status = 1; // Città non in lista
        } else {
            if (type == 't') value = get_temperature();
            else if (type == 'h') value = get_humidity();
            else if (type == 'w') value = get_wind();
            else if (type == 'p') value = get_pressure();
        }

        // 4. Serializzazione Manuale Risposta (9 byte: 4 status, 1 type, 4 value)
        char send_buf[9];
        
        // Status (htonl)
        uint32_t net_status = htonl(status);
        memcpy(send_buf, &net_status, 4);

        // Type
        send_buf[4] = type;

        // Value (float -> uint32_t -> htonl)
        uint32_t net_val;
        memcpy(&net_val, &value, 4);
        net_val = htonl(net_val);
        memcpy(send_buf + 5, &net_val, 4);

        // 5. Invio risposta
        sendto(my_socket, send_buf, 9, 0, (struct sockaddr *)&client_address, client_len);
    }

    closesocket(my_socket);
    clearwinsock();
    return 0;
}
