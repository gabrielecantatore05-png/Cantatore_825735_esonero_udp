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
#include <ctype.h>

#define MAX_CITY_LEN 64
#define DEFAULT_PORT 56700
#define DEFAULT_SERVER "localhost"

void clearwinsock() {
#if defined _WIN32
    WSACleanup();
#endif
}

int main(int argc, char *argv[]) {
#if defined _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        printf("Error at WSAStartup()\n");
        return -1;
    }
#endif

    char *server_name = DEFAULT_SERVER;
    int port = DEFAULT_PORT;
    char *req_arg = NULL;

    // 1. Parsing argomenti a riga di comando
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) server_name = argv[++i];
        else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) port = atoi(argv[++i]);
        else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) req_arg = argv[++i];
    }

    if (req_arg == NULL) {
        fprintf(stderr, "Errore: specificare una richiesta con -r \"tipo citta'\"\n");
        clearwinsock();
        return -1;
    }

    // 2. Parsing della stringa di richiesta (-r)
    if (strchr(req_arg, '\t') != NULL) {
        fprintf(stderr, "Errore: tabulazioni non ammesse.\n");
        clearwinsock();
        return -1;
    }

    char type = req_arg[0];
    if (req_arg[1] != ' ' && req_arg[1] != '\0') {
        fprintf(stderr, "Errore: il primo token deve essere un singolo carattere.\n");
        clearwinsock();
        return -1;
    }

    char *city = strchr(req_arg, ' ');
    if (city == NULL) {
        fprintf(stderr, "Errore: formato richiesta non valido.\n");
        clearwinsock();
        return -1;
    }
    while (*city == ' ') city++; // Salta spazi extra

    if (strlen(city) >= MAX_CITY_LEN) {
        fprintf(stderr, "Errore: nome città troppo lungo.\n");
        clearwinsock();
        return -1;
    }

    // 3. Risoluzione DNS e Reverse Lookup
    struct hostent *host = gethostbyname(server_name);
    if (host == NULL) {
        fprintf(stderr, "Errore risoluzione host.\n");
        clearwinsock();
        return -1;
    }

    struct in_addr addr = *(struct in_addr *)host->h_addr;
    char ip_str[INET_ADDRSTRLEN];
    strcpy(ip_str, inet_ntoa(addr));

    struct hostent *rev_host = gethostbyaddr((const char*)&addr, sizeof(addr), AF_INET);
    char *canonical_name = (rev_host) ? rev_host->h_name : server_name;

    // 4. Creazione Socket UDP
    SOCKET my_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (my_socket == INVALID_SOCKET) {
        fprintf(stderr, "Errore creazione socket\n");
        clearwinsock();
        return -1;
    }

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr = addr;

    // 5. Serializzazione Manuale Richiesta
    // Formato buffer: [char type] [64 bytes city inclusi \0]
    char send_buf[1 + MAX_CITY_LEN];
    memset(send_buf, 0, sizeof(send_buf));
    send_buf[0] = type;
    strncpy(send_buf + 1, city, MAX_CITY_LEN - 1);

    if (sendto(my_socket, send_buf, sizeof(send_buf), 0, 
               (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        fprintf(stderr, "Errore invio\n");
        closesocket(my_socket);
        clearwinsock();
        return -1;
    }

    // 6. Ricezione e Deserializzazione Risposta
    // Formato atteso: [uint32 status] [char type] [uint32 value_bits]
    char recv_buf[9];
    struct sockaddr_in from_addr;
    socklen_t from_size = sizeof(from_addr);

    int n = recvfrom(my_socket, recv_buf, sizeof(recv_buf), 0, (struct sockaddr *)&from_addr, &from_size);

    if (n >= 9) {
        uint32_t net_status, net_val;
        memcpy(&net_status, recv_buf, 4);
        uint32_t status = ntohl(net_status);
        
        char r_type = recv_buf[4];
        
        memcpy(&net_val, recv_buf + 5, 4);
        net_val = ntohl(net_val);
        float val;
        memcpy(&val, &net_val, 4);

        printf("Ricevuto risultato dal server %s (ip %s). ", canonical_name, ip_str);

        if (status == 1) {
            printf("Città non disponibile\n");
        } else if (status == 2) {
            printf("Richiesta non valida\n");
        } else {
            // Formattazione output come richiesto
            city[0] = toupper(city[0]); 
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