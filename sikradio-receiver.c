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
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <inttypes.h>
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



#include "err.h"


#define BUFFER_SIZE 20
#define DATA_PORT 20000 //TODO
#define PSIZE 512   
#define BSIZE 65536
#define NAZWA "Nienazwany Nadajnik"

char *host;
//sem_t *mutex;
char *buffer;
bool *received_package;
bool set_byte0;
uint64_t  reading_data ;
uint64_t  writing_data ;
uint64_t last_received_package;
pthread_mutex_t  sem;
pthread_mutex_t  change_value;
pthread_cond_t  cond;
uint16_t port = DATA_PORT;
uint16_t psize = PSIZE;
uint32_t bsize = (uint32_t) BSIZE;
uint64_t max_package;
char* name;


uint16_t read_port(char *string) {
    errno = 0;
    unsigned long port = strtoul(string, NULL, 10);
    PRINT_ERRNO();
    if (port > UINT16_MAX) {
        fatal("%ul is not a valid port number", port);
    }

    return (uint16_t) port;
}


struct sockaddr_in get_send_address() {
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

int bind_socket(uint16_t port) {
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0); // creating IPv4 UDP socket
    ENSURE(socket_fd > 0);
    // after socket() call; we should close(sock) on any execution path;

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET; // IPv4
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); // listening on all interfaces
    server_address.sin_port = htons(port);

    // bind the socket to a concrete address
    CHECK_ERRNO(bind(socket_fd, (struct sockaddr *) &server_address,
                        (socklen_t) sizeof(server_address)));

    return socket_fd;
}

ssize_t read_audio(int socket_fd, struct sockaddr_in *client_address, char *buffer, size_t max_length) {
    //sprawdz czy chcesz się w ogóle połaczyć
    socklen_t address_length = (socklen_t) sizeof(*client_address);
    int flags = 0; // we do not request anything special
    errno = 0;
    ssize_t len = recvfrom(socket_fd, buffer, max_length, flags,
                           (struct sockaddr *) client_address, &address_length);


  //  if(client_address.sin_addr.s_addr != actual_connect) {
  //      return -1;
  //  }
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

void setValue(int argc, char *argv[]){
    host = argv[2];
    name = argv[4];
    //uint16_t port = read_port(argv[2]);
}
uint64_t convert_to_uint_64(char* buffer, int beginning){
    uint64_t answer;
    for ( int i = beginning + 7 ; i >= beginning; i-- ){
        answer = answer + (int)buffer[i];
        answer = answer * 256;
    }
    return answer;
}
uint64_t setsession(char * buffer){
    //printf("siema");
    uint64_t byte0 = be64toh(*((uint64_t*)buffer));
    //uint64_t *session_id = (uint64_t *) audio_package;
    //uint64_t byte0 = convert_to_uint_64(buffer, 8);
    return byte0;
    //return 17;
}
void reset_data(){
    pthread_mutex_lock(&change_value);
    set_byte0 = false;
    for( int i = 0 ; i<bsize; i ++){
        buffer[i] = 0;
    }
    writing_data = 0;
    reading_data = 0;
    last_received_package = 0;
    pthread_mutex_unlock(&change_value);

}
int interested(uint64_t *actual_session, char*  buffer, uint64_t *byte0){
    //printf("debug");
    //pierwszy raz jaki przyjdzie mi paczka o moim numerze
    uint64_t *session_id = (uint64_t *) buffer;
    //uint64_t session_id = convert_to_uint_64(buffer, 0);
    //printf("%ld", *session_id);
    if(*actual_session == 0) {
        *byte0 = be64toh(*(((uint64_t *) buffer ) + 1));
        *actual_session= *session_id;
        return 1;
    }
    if(*session_id < *actual_session){
        return 0;
    }
    if(*session_id > *actual_session){
        //wyczyść cały bufor
        reset_data();
        *actual_session = 0;
       // pthread_mutex_lock(&change_value);
       // writing_data = 0;
       // reading_data = 0;
       // last_received_package = 0;
       // pthread_mutex_unlock(&change_value);
        return -1;
    }
    return 1;
}
void set_zero_buffer(uint64_t beginning, uint64_t end){
    pthread_mutex_lock(&change_value);
    for (uint64_t i = beginning * psize; i < end * psize; i ++){
        buffer[i %bsize] = 0;
    }
    pthread_mutex_unlock(&change_value);
//od beginning do end jezeli end > beginning
}
void missingpackage(uint64_t actual_byte){
    received_package[actual_byte %(bsize/psize)] = 1;
    if(actual_byte > 30) {
        return 0;
    }
    fprintf(stderr, "%ld %ld \n", last_received_package, actual_byte);
    // na actual_byte mam ten co przyszedł
    // na last_received mam to co był ostatnio
    for( uint64_t i = last_received_package +1; i < actual_byte; i ++){
        received_package[i %(bsize/psize)] = 0;
        set_zero_buffer(last_received_package +1, actual_byte);
        //fprintf(stderr, "siema");
        //zeruj
    }
    int start = 0; 
    if( actual_byte > max_package){
        start = actual_byte + 1 - max_package;
    }
   // fprintf(stderr, "siema");
    for( uint64_t i = start; i < actual_byte; i ++){
        if(!received_package[i%(bsize/psize)]){
            fprintf(stderr,"MISSING: BEFORE %ld EXPECTED %ld \n", actual_byte, i);
        }
    }

    // obsluguje wypisywanie stderr
    // odpowiedio się wpisuje
    // nie zejsc ponizej numerem paczki do zera oraz nie przekroczyć byyte0
}
uint64_t setBuffer( char * temporary_buffer, char* buffer, uint64_t index, uint64_t byte0){
    //ustaw numer na wpisywanie
   // pthread_mutex_lock(sem);
//    missingpackage();
   // pthread_cond_wait(cond, sem);
    //wziac monitor
    //fprintf(stderr, "%ld", byte0);
    uint64_t first_num_byte = be64toh(*(((uint64_t *) temporary_buffer ) + 1));
    missingpackage((first_num_byte-byte0)/ psize);
    memcpy(buffer + index, temporary_buffer + 16, psize);
    pthread_mutex_lock(&change_value);
    (writing_data) = ((writing_data) + psize ) %bsize;
    pthread_mutex_unlock(&change_value);
   // *writing_data = *writing_data + psize;
    //pthread_cond_signal(cond);
    //for(int i = 0; i< 8; i ++){
    //    printf("%c",buffer[i]);
   // }

   // uint64_t first_num_byte = be64toh(*(((uint64_t *) temporary_buffer ) + 1));
    //return first_num_byte;
    //uint64_t byte = convert_to_uint_64(buffer, 8);
    //printf("%d", byte);
    //pthread_mutex_unlock(sem);
    //return *session_id;
    //printf("%ld ", first_num_byte);
    last_received_package = (first_num_byte - byte0)/psize;
    //received_package[last_received_package % (bsize/psize)] = 1;
    return first_num_byte;
   // return *session_id;
}
void* writing(void* data)
{
   // while(true){
   //     printf("olej");
   // }
    // zrob wait na scond
    //pthread_mutex_lock(sem);
    //printf("siema");
    //pthread_cond_wait(cond, sem);
    //while(true){
    //printf("elooo");
    //}
    while(true){
        pthread_cond_wait(&cond, &sem);
        while(writing_data !=reading_data){
    //while(true){
           // printf("cycki");
            //printf("%c", buffer[(*reading_data)]);
            fwrite(buffer + (reading_data), sizeof(char), psize, stdout);
            pthread_mutex_lock(&change_value);
            (reading_data) =( ( reading_data) + psize )% bsize;
            pthread_mutex_unlock(&change_value);
            //if(*reading_data >= bsize){
            //    (*reading_data) = (*reading_data) - bsize;
            //}
        }
    }
    //pthread_cond_signal(cond);
    //pthread_mutex_unlock(sem);
}
bool check(struct sockaddr_in send_address, struct sockaddr_in excpeted_address){
    return send_address.sin_addr.s_addr == excpeted_address.sin_addr.s_addr;
}

int main(int argc, char *argv[]) {
  //  *reading_data = 0;
  //  *writing_data = 0;
    //printf("%d", bsize );
    //printf("chuj");
   // printf("%" PRIu16 "\n",bsize);
    setValue(argc, argv);
    last_received_package = 0;
    struct sockaddr_in actual_connect = get_send_address();
    buffer = malloc (bsize);
    max_package = bsize/psize;
    received_package = malloc ( max_package);
    pthread_cond_init(&cond, NULL);
    pthread_mutex_init(&sem, NULL);
    pthread_mutex_init(&change_value, NULL);
    //sem_init(mutex, 1, 0);
    char temporary_buffer[psize + 16];
    uint64_t actually_session = 0;
    uint64_t BYTE0 = 0;
    uint64_t first_byte;
    set_byte0 = false;
    uint64_t counter = 0;
    bool started = 0;
    //printf("%d", sizeof(t));
    memset(buffer, 0, bsize);
    //return 0;
    memset(temporary_buffer, 0,psize + 16);
    //return 0;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_t writing_data;
    pthread_create(&writing_data, &attr, writing, NULL);
    struct sockaddr_in send_address = get_send_address(host, port);

    int socket_fd = bind_socket(port);

    struct sockaddr_in client_address;
    size_t audio_length;
    //int i = 0;
    do {
        //printf("siema");
        audio_length = read_audio(socket_fd, &client_address, temporary_buffer, psize + 16);
       // fwrite(temporary_buffer+16, 1, psize, stdout);
       if(!check(send_address,client_address)){
        fprintf(stderr,"zly nadawca\n");
        continue;
       }
        //session_id
        int decision = interested(&actually_session, temporary_buffer, &BYTE0);
        //return 0;
        //printf("%d", decision);
        if(decision == 1){
            if(!set_byte0){
            last_received_package = BYTE0;
            set_byte0 = 1;
            }
            first_byte = setBuffer(temporary_buffer, buffer, counter, BYTE0);
           // fprintf(stderr, "%ld ", first_byte);
            counter = (counter + psize ) % bsize;
            if(started){
                pthread_cond_signal(&cond);
            }
             if(BYTE0 + bsize*3/4 < first_byte + psize &&
                BYTE0 + bsize*3/4 >= first_byte){
                pthread_cond_signal(&cond);
                started = 1;
             }
        }
      //  i++;
    } while (true);
    
    //printf("finished exchange\n");

    CHECK_ERRNO(close(socket_fd));

    return 0;
    // naprawić wypisywanie missin errory.
    // valgrinda
    // Jeśli rozmiar odczytanych danych nie jest podzielny przez PSIZE,
    }