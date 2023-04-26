#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <time.h>
#include "err.h"

#define BUFFER_SIZE 20
#define DATA_PORT 20000 //TODO
#define PSIZE 512   
#define BSIZE 65536
#define NAZWA "Nienazwany Nadajnik"

char *host;
uint16_t port = DATA_PORT;
uint16_t psize = PSIZE;
uint16_t bsize = (uint16_t) BSIZE;
char* name;

char shared_buffer[BUFFER_SIZE];

struct package {
    uint64_t session_id;

    uint64_t first_byte_num;

    uint8_t* audio_data;
} ;

uint16_t read_port(char *string) {
    errno = 0;
    unsigned long port = strtoul(string, NULL, 10);
    PRINT_ERRNO();
    if (port > UINT16_MAX) {
        fatal("%ul is not a valid port number", port);
    }

    return (uint16_t) port;
}

void setValue(int argc, char *argv[]){
    host = argv[2];
    name = argv[4];
    //uint16_t port = read_port(argv[2]);
}

struct sockaddr_in get_send_address(char *host, uint16_t port) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    struct addrinfo *address_result;
    CHECK(getaddrinfo(host, NULL, &hints, &address_result));

    struct sockaddr_in send_address;
    send_address.sin_family = AF_INET; // IPv4
    send_address.sin_addr.s_addr =
            ((struct sockaddr_in *) (address_result->ai_addr))->sin_addr.s_addr; // IP address
    send_address.sin_port = htons(port); // port from the command line

    freeaddrinfo(address_result);

    return send_address;
}

void send_audio(int socket_fd, const struct sockaddr_in *send_address, const char *message) {
   // size_t audio_length = sizeof(message);
   size_t audio_length = psize + 16;
    //dowiedzieć się jak to zrobić

   // if (message_length == BUFFER_SIZE) {
   //     fatal("parameters must be less than %d characters long", BUFFER_SIZE);
   // }
   // jak to wysłać sensownie, sklejone dane czy struktura
    int send_flags = 0;
    socklen_t address_length = (socklen_t) sizeof(*send_address);
    errno = 0;
    ssize_t sent_length = sendto(socket_fd, message, audio_length, send_flags,
                                 (struct sockaddr *) send_address, address_length);
    if (sent_length < 0) {
        PRINT_ERRNO();
    }
    ENSURE(sent_length == (ssize_t) audio_length);
}
void convert_uint_64(uint64_t value, int beginning, char* audio){
    for ( int i = beginning + 7 ; i >= beginning; i-- ){
        audio[i] = value %256;
        value = value / 256;
    }
}
bool correct_input(char *audio, int counter){
    convert_uint_64(counter * psize, 8, audio);
    for( int i = 16; i < psize + 16; i++) {
        int a = getchar();
        if(a != EOF) {
            return false;
        }
        audio[i] = (char) a;
    }
    return true;
}
int main(int argc, char *argv[]) {
    time_t start = time(NULL);
    setValue(argc,argv);

    struct sockaddr_in send_address = get_send_address(host, port);

    int socket_fd = socket(PF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        PRINT_ERRNO();
    }
    //jak dostać dane ?


    // char *client_ip = inet_ntoa(send_address.sin_addr);
    // uint16_t client_port = ntohs(send_address.sin_port);

    //for (int i = 3; i < argc; i++) {
   // printf("%d \n", psize);
    char audio_package[psize + 16];
    //int number;
    //char *audio = malloc(sizeof(char) * (psize + 16));
    //struct package audio_package;
    //audio_package.session_id = start;
    //audio_package.audio_data = audio;
    //audio uzupełnić 
    convert_uint_64(start, 0, audio_package);
    int counter = 0;
    while(correct_input(audio_package, counter)){
      //  audio_package.first_byte_num = 0;
        //audio_package.audio_data = audio;
        send_audio(socket_fd, &send_address, audio_package);
        counter++;
    }
    //struct package audio_package;
    //audio_package.session_id = start;
    //audio_package.first_byte_num = 0;
    //audio_package.audio_data = audio;
        //jedna paczka
    //    while( check_data(audio)){
        //    send_audio(socket_fd, &send_address, audio_package);
    //    }
    //    printf("sent to %s:%u: '%s'\n", client_ip, client_port, argv[i]);
     //   memset(shared_buffer, 0, sizeof(shared_buffer)); // clean the buffer
    //    size_t max_length = sizeof(shared_buffer) - 1; // leave space for the null terminator
    //    size_t received_length = receive_message(socket_fd, &send_address, shared_buffer, max_length);
     //   printf("received %zd bytes from %s:%u: '%s'\n", received_length, client_ip, client_port, shared_buffer);
    //}

    CHECK_ERRNO(close(socket_fd));

    return 0;
}
