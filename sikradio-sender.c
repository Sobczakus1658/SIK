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

// #include "err.h"
#include "utils.h"

#define BUFFER_SIZE 20
#define DATA_PORT 20009 
#define PSIZE 512   
#define BSIZE 65536
#define NAZWA "Nienazwany Nadajnik"

char *host;
uint16_t port = DATA_PORT;
uint16_t psize = PSIZE;
uint16_t bsize = (uint16_t) BSIZE;
char* name = NAZWA;

// uint16_t read_port(char *string) {
//     errno = 0;
//     unsigned long port = strtoul(string, NULL, 10);
//     PRINT_ERRNO();
//     if (port > UINT16_MAX) {
//         fatal("%ul is not a valid port number", port);
//     }

//     return (uint16_t) port;
// }

void setValue(int argc, char *argv[]){
    for( int i = 1 ; i < argc; i++){
        if(!strcmp(argv[i],"-a")){
            host = argv[i +1];
        }
        else if(!strcmp(argv[i],"-P")){
            port = read_port(argv[i +1 ]);
        }
        else if(!strcmp(argv[i],"-p")){
            psize = strtoull(argv[i + 1], NULL, 10);
        }
        else if(!strcmp(argv[i],"-n")){
            name = argv[i+1];
         }
     }
}

// struct sockaddr_in get_send_address(char *host, uint16_t port) {
//     struct addrinfo hints;
//     memset(&hints, 0, sizeof(struct addrinfo));
//     hints.ai_family = AF_INET; // IPv4
//     hints.ai_socktype = SOCK_DGRAM;
//     hints.ai_protocol = IPPROTO_UDP;

//     struct addrinfo *address_result;
//     CHECK(getaddrinfo(host, NULL, &hints, &address_result));

//     struct sockaddr_in send_address;
//     send_address.sin_family = AF_INET; 
//     send_address.sin_addr.s_addr =
//             ((struct sockaddr_in *) (address_result->ai_addr))->sin_addr.s_addr; 
//     send_address.sin_port = htons(port); 

//     freeaddrinfo(address_result);

//     return send_address;
// }

void send_audio(int socket_fd, const struct sockaddr_in *send_address, const char *message) {
    size_t audio_length = psize +  2 * sizeof(uint64_t);
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

int main(int argc, char *argv[]) {
    time_t start = time(NULL);
    setValue(argc,argv);
    struct sockaddr_in send_address = get_send_address(host, port);

    int socket_fd = socket(PF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        PRINT_ERRNO();
    }
    char audio_package[psize + 2*sizeof(uint64_t)];
    int counter = 0;
    uint64_t *session_id = (uint64_t *) audio_package;

    uint64_t *converted_byte = session_id +1;
    uint64_t first_byte_num = 0;
    *session_id = htobe64(start);
    int i = 0;
    while( fread(audio_package + 2 * sizeof(uint64_t), sizeof(char), psize, stdin) ==psize ){
    //while(correct_input(audio_package, counter)){
        // if(i >2 ){
        // if(i %2 ==0){
        //     first_byte_num = (i+1)*psize;
        // }
        // else{
        //     first_byte_num = (i-1)*psize;
        // }
        // }
        // i++;
        //fprintf(stderr, "%ld \n", first_byte_num);
        // i++;
        // if( i %2 == 0) {
        //     first_byte_num += psize;
        //     continue;
        // }
        *converted_byte = htobe64(first_byte_num);
        send_audio(socket_fd, &send_address, audio_package);
        first_byte_num += psize;
    }

    CHECK_ERRNO(close(socket_fd));

    return 0;
}
