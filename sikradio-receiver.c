#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <semaphore.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/mman.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <inttypes.h>
#include <netdb.h>
#include <time.h>

#include "utils.h"

#define BUFFER_SIZE 20
#define DATA_PORT 20009 
#define PSIZE 512   
#define BSIZE 65536
#define NAZWA "Nienazwany Nadajnik"

char *host;
bool started;
bool finished = 0;
uint64_t usefull_bytes; 
char *buffer;
bool *received_package;
bool set_byte0;
uint64_t  reading_data ;
uint64_t  writing_data ;
uint64_t max_received_package;
pthread_mutex_t  sem;
pthread_mutex_t  change_value;
pthread_cond_t  cond;
uint16_t port = DATA_PORT;
uint16_t psize = PSIZE;
uint32_t bsize = (uint32_t) BSIZE;
uint64_t max_package;
char* name;

int bind_socket(uint16_t port) {
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0); 
    ENSURE(socket_fd > 0);

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET; 
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); 
    server_address.sin_port = htons(port);

    CHECK_ERRNO(bind(socket_fd, (struct sockaddr *) &server_address,
                        (socklen_t) sizeof(server_address)));

    return socket_fd;
}

ssize_t read_audio(int socket_fd, struct sockaddr_in *client_address, char *buffer, size_t max_length) {
    socklen_t address_length = (socklen_t) sizeof(*client_address);
    int flags = 0; 
    errno = 0;
    ssize_t len = recvfrom(socket_fd, buffer, max_length, flags,
                           (struct sockaddr *) client_address, &address_length);

    if (len < 0) {
        PRINT_ERRNO();
    }
    return (ssize_t) len;
}

void send_message(int socket_fd, const struct sockaddr_in *client_address, const char *message, size_t length) {
    socklen_t address_length = (socklen_t) sizeof(*client_address);
    int flags = 0;
    ssize_t sent_length = sendto(socket_fd, message, length, flags,
                                 (struct sockaddr *) client_address, address_length);
    ENSURE(sent_length == (ssize_t) length);
}

void set_value(int argc, char *argv[]) {
    for (int i = 1; i < argc; i+=2) { 
        if (!strcmp(argv[i], "-a")) {
            host = argv[i +1];
        }
        else if(!strcmp(argv[i], "-P")) {
            port = read_port(argv[i +1 ]);
        }
        else if(!strcmp(argv[i], "-p")) {
            psize = strtoull(argv[i + 1], NULL, 10);
        }
        else if(!strcmp(argv[i], "-b")) {
            bsize = strtoull(argv[i +1], NULL, 10);
        }
        else{
            fprintf(stderr, "niepoprawne dane\n");
            exit(1);
        }
     }
}
void reset_data() {
    pthread_mutex_lock(&change_value);
    memset(buffer, 0, bsize);
    writing_data = 0;
    reading_data = 0;
    pthread_mutex_unlock(&change_value);
    max_received_package = 0;
    set_byte0 = false;
    memset(received_package, 0, bsize/psize);
    started = 0;
}
int interested (uint64_t *actual_session, char*  buffer, uint64_t *byte0) {
    uint64_t *session_id = (uint64_t *) buffer;
    if (*actual_session == 0) {
        *byte0 = be64toh(*(((uint64_t *) buffer ) + 1));
        *actual_session = *session_id;
        return 1;
    }
    if (*session_id < *actual_session) {
        return 0;
    }
    if (*session_id > *actual_session) {
        reset_data();
        *actual_session = 0;
        return -1;
    }
    return 1;
}
void set_zero_buffer(uint64_t beginning, uint64_t end) {
    pthread_mutex_lock(&change_value);
    for (uint64_t i = beginning * psize; i < end * psize; i ++) {
        buffer[i %usefull_bytes] = 0;
    }
    pthread_mutex_unlock(&change_value);
}
void missing_package(uint64_t actual_byte) {
    received_package[actual_byte %(bsize/psize)] = 1;
    for (uint64_t i = max_received_package +1; i < actual_byte; i ++) {
        received_package[i %(bsize/psize)] = 0;
        set_zero_buffer(max_received_package + 1, actual_byte);
    }
    int start = 0; 
    if (actual_byte > max_package) {
        start = actual_byte + 1 - max_package;
    }
    for (uint64_t i = start; i < actual_byte; i ++) {
        if (!received_package[i%(bsize/psize)]) {
            fprintf(stderr,"MISSING: BEFORE %ld EXPECTED %ld \n", actual_byte, i);
        }
    }
}
uint64_t set_buffer(char * temporary_buffer, char* buffer, uint64_t index, uint64_t byte0) {
    uint64_t first_num_byte = be64toh(*(((uint64_t *) temporary_buffer ) + 1));
    missing_package((first_num_byte-byte0)/ psize);
    memcpy(buffer + index, temporary_buffer + 16, psize);
    pthread_mutex_lock(&change_value);
    (writing_data) = ((writing_data) + psize) % usefull_bytes;
    pthread_mutex_unlock(&change_value);
    if ((first_num_byte - byte0) / psize > max_received_package) {
        max_received_package = (first_num_byte - byte0) / psize;
    }
    return first_num_byte;
}
void* writing(void* data) {
    (void) data;
    while (true) {
        pthread_cond_wait(&cond, &sem);
        while (writing_data !=reading_data) {
            fwrite(buffer + (reading_data), sizeof(char), psize, stdout);
            pthread_mutex_lock(&change_value);
            (reading_data) =((reading_data) + psize) % usefull_bytes;
            pthread_mutex_unlock(&change_value);
        }
    }
}
bool check(struct sockaddr_in send_address, struct sockaddr_in excpeted_address) {
    return send_address.sin_addr.s_addr == excpeted_address.sin_addr.s_addr;
}
void initial_data(){
    reading_data = 0;
    writing_data = 0;
    usefull_bytes = (bsize / psize) * psize; 
    max_received_package = 0;
    buffer = malloc (usefull_bytes);
    max_package = bsize / psize;
    received_package = malloc(max_package);
    pthread_cond_init(&cond, NULL);
    pthread_mutex_init(&sem, NULL);
    pthread_mutex_init(&change_value, NULL);
    set_byte0 = false;
    started = 0;
    memset(buffer, 0, usefull_bytes);
    pthread_t writing_data;
    pthread_create(&writing_data, NULL, writing, NULL);
}

int main(int argc, char *argv[]) {
    set_value(argc, argv);
    initial_data();
    char temporary_buffer[psize + 16];
    uint64_t actually_session = 0;
    uint64_t BYTE0 = 0;
    uint64_t first_byte;
    uint64_t counter = 0;
    struct sockaddr_in send_address = get_send_address(host, port);

    int socket_fd = bind_socket(port);

    struct sockaddr_in client_address;
    do {
        read_audio(socket_fd, &client_address, temporary_buffer, psize + 16);
        if(!check(send_address,client_address)){
            fprintf(stderr,"zly nadawca\n");
            exit(1);
            continue;
        }
        int decision = interested(&actually_session, temporary_buffer, &BYTE0);
        if (decision == 1) {
            if (!set_byte0) {
                max_received_package = BYTE0;
                set_byte0 = 1;
            }
            first_byte = set_buffer(temporary_buffer, buffer, counter, BYTE0);
            counter = (counter + psize ) % usefull_bytes;
            if (started) {
                pthread_cond_signal(&cond);
            }
            if(BYTE0 + bsize*3/4 < first_byte + psize &&
                BYTE0 + bsize*3/4 >= first_byte){
                pthread_cond_signal(&cond);
                started = 1;
            }
        }
    } while (true);
    
    pthread_cond_signal(&cond);
    CHECK_ERRNO(close(socket_fd));
    free(buffer);
    free(received_package);
    pthread_cond_init(&cond, NULL);
    pthread_mutex_destroy(&sem);
    pthread_mutex_destroy(&change_value);
    pthread_cond_destroy(&cond);
    pthread_join(writing_data, NULL);
    return 0;
}