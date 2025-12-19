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
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "protocol.h"

#define NO_ERROR 0
#define BUFFMAX 512
#define PORT 56700 // Porta di default come da specifica
#define MAX_CITY_LEN 64


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

int validate_request(request req){
	int status;
	if (strpbrk(req.city,"@#$%&")!= NULL){
		
		status=0;
	}
	return status;
	
	

}

void clearwinsock() {
#if defined _WIN32
    WSACleanup();
#endif
}

void errorhandler(char *error_message) {
    printf("%s\n", error_message);
}

int main(int argc, char *argv[]) {
    srand(time(NULL));
    int server_port = PORT;

   
    if (argc == 3 && strcmp(argv[1], "-p") == 0) {
        server_port = atoi(argv[2]);
    }

#if defined _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != NO_ERROR) {
        printf("Error at WSAStartup()\n");
        return 0;
    }
#endif

    int my_socket;
    if ((my_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        errorhandler("Error creating socket");
        clearwinsock();
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_address;
    struct sockaddr_in client_address;
    unsigned int client_address_length = sizeof(client_address);
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);
    server_address.sin_addr.s_addr = INADDR_ANY; 

    if ((bind(my_socket, (struct sockaddr *)&server_address, sizeof(server_address))) < 0) {
        errorhandler("bind() failed");
        closesocket(my_socket);
        clearwinsock();
        return EXIT_FAILURE;
    }

    printf("Server Meteo UDP in ascolto sulla porta %d...\n", server_port);

    char buffer[BUFFMAX];
    int rcv_msg_size;

    while (1) {
        memset(buffer, 0, BUFFMAX);
        
        
        rcv_msg_size = recvfrom(my_socket, buffer, BUFFMAX, 0, 
                                (struct sockaddr *)&client_address, &client_address_length);
        
        if (rcv_msg_size < 0) {
            errorhandler("recvfrom() failed");
            continue;
        }

       
        char type = buffer[0];
        char city[MAX_CITY_LEN];
        strncpy(city, buffer + 1, MAX_CITY_LEN - 1);
        city[MAX_CITY_LEN - 1] = '\0';
        
        request req;
        int offset=0;
        req.type = buffer[offset];
        offset = offset + 1;

        memcpy(req.city, &buffer[offset], MAX_CITY_LEN);
        req.city[MAX_CITY_LEN-1]='\0';

        struct hostent *client_host = gethostbyaddr((char *)&client_address.sin_addr.s_addr, 
                                                    sizeof(client_address.sin_addr.s_addr), AF_INET);
        char *host_name = (client_host) ? client_host->h_name : "Unknown";
        printf("Richiesta ricevuta da %s (ip %s): type='%c', city='%s'\n", 
               host_name, inet_ntoa(client_address.sin_addr), type, city);

        unsigned int status = 0;
        float value = 0.0f;

        if (type != 't' && type != 'h' && type != 'w' && type != 'p') {
            status = 2; 
        } else if (!validate_city(city)) {
            status = 1; 
        } else if (validate_request(req)){
			
			status=2;
		}
         else {
            if (type == 't') value = get_temperature();
            else if (type == 'h') value = get_humidity();
            else if (type == 'w') value = get_wind();
            else if (type == 'p') value = get_pressure();
        }

       
        char send_buf[9];
        offset = 0;

        uint32_t net_status = htonl(status);
        memcpy(send_buf + offset, &net_status, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        send_buf[offset] = type;
        offset += sizeof(char);

       
        uint32_t temp_val;
        memcpy(&temp_val, &value, sizeof(float));
        temp_val = htonl(temp_val);
        memcpy(send_buf + offset, &temp_val, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        if (sendto(my_socket, send_buf, offset, 0, 
                   (struct sockaddr *)&client_address, sizeof(client_address)) != offset) {
            errorhandler("sendto() failed");
        }
    }

    closesocket(my_socket);
    clearwinsock();
    return 0;

}
