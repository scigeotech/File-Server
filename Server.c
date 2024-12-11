#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

#define server_port "8000"
char *input_path;

//Guide: A response will consist of: the string “HTTP/1.0 “
//followed by code (200 for OK or 404 for file not found), a new line,
//then “Content-Length: “ + the number of bytes of the response followed by 2 new lines.
char file_missing[64] = "HTTP/1.0 404 Not Found\r\nContent-Length: 13\r\n\r\n404 Not Found";

int server_setup() {
    int socket_fd; //socket/listen on socket_fd, accept on new_fd
    struct addrinfo hints, *serv_info, *current;

    memset(&hints, 0, sizeof(hints)); //hints for connection. clear to 0
    hints.ai_family = AF_UNSPEC; //for socket()
    hints.ai_socktype = SOCK_STREAM; //TCP
    hints.ai_flags = AI_PASSIVE; //my ip address

    //get destination data
    int addr_info = getaddrinfo(NULL, server_port, &hints, &serv_info); //self (NULL), port 8000, fill serv_info
    if (addr_info != 0) {//error
        fprintf(stderr, "getaddrinfo attempt error: %s\n", gai_strerror(addr_info));
        exit(1);
    }

    for (current = serv_info; current != NULL; current = current->ai_next) {
        //use TCP socket
        if ((socket_fd = socket(current->ai_family, current->ai_socktype, current->ai_protocol)) == -1) {
            printf("server socket attempt error, retrying\n");
            continue;
        }
        if (bind(socket_fd, current->ai_addr, current->ai_addrlen) == -1) {
            printf("server bind error, retrying\n");
            close(socket_fd);
            continue;
        }
        break; //success?
    }
    freeaddrinfo(serv_info); //done

    if (listen(socket_fd, 16) == -1) { //backlog 16
        printf("server listen error, exiting\n");
        exit(1);
    }

    printf("Server waiting to accept connections on port %s\n", server_port);
    return socket_fd;
}

void *request_mode(void *client_sock) {
    int client_socket = *(int*)client_sock;
    free(client_sock);

    char receive_buffer[1024];
    memset(receive_buffer, 0, sizeof(receive_buffer)); //clear buffer 0
    int bytes_read = read(client_socket, receive_buffer, 1023);
    if (bytes_read == 0) {
        close(client_socket);
        return NULL;
    }

    //GET /data.html HTTP/1.1
    //path processing
    char *strtok_r_save;
    char *strtok_buffer = strtok_r(receive_buffer, " ", &strtok_r_save);
    if (strncmp(strtok_buffer, "GET", strlen("GET")) != 0) {
        close(client_socket);
        return NULL;
    }
    char *processed_path = strtok_r(NULL, " ", &strtok_r_save);
    if (processed_path[0] == '/') processed_path++;
    char true_path[1024];
    snprintf(true_path, sizeof(true_path), "%s/%s", input_path, processed_path);


    struct stat file_status;
    //does the file exist? what is the size?
    if (stat(true_path, &file_status) < 0 || !S_ISREG(file_status.st_mode)) {
        write(client_socket, file_missing, strlen(file_missing));
        close(client_socket);
        return NULL;
    }

    //send back response
    char success_response[1024];
    snprintf(success_response, sizeof(success_response), "HTTP/1.0 200 OK\r\nContent-Length: %ld\r\n\r\n", file_status.st_size);
    write(client_socket, success_response, strlen(success_response));

    int file_open = open(true_path, O_RDONLY);
    char file_buffer[1024];
    int file_bytes_read;
    while ((file_bytes_read = read(file_open, file_buffer, 1024)) > 0) {
        write(client_socket, file_buffer, file_bytes_read);
    }
    close(file_open);

    close(client_socket);
    return NULL;
}

//IT WORKS!!!
int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("need just 2 arguments, do ./Server (filepath)\n");
        exit(1);
    }
    input_path = argv[1]; //hold path
    
    int server_socket = server_setup();
    if(server_socket == -1) {
        printf("server setup fail\n");
        exit(1);
    } else {
        printf("Server ready on port %s\n", server_port);
    }
    
    //send/receive
    while(1) {
        struct sockaddr_storage client_address; //address information

        //accept() section
        socklen_t sin_size = sizeof(client_address); //client, server address sizes
        int new_fd = accept(server_socket, (struct sockaddr *)&client_address, &sin_size); //new_fd is client address
        if (new_fd == -1) {
            printf("accept fail\n");
        }
        printf("Connection accepted.\n");
        
        int *client_socket_ptr = malloc(sizeof(int));
        *client_socket_ptr = new_fd;

        pthread_t thread_id;
        //request_mode is the function that this thread will run
        //client_socket_ptr is the argument that will be passed to the function
        int pthread = pthread_create(&thread_id, NULL, request_mode, client_socket_ptr);
        sleep(1); // otherwise, the thread doesn’t have time to start before exit…
        if(pthread != 0) {
            printf("pthread creation fail\n");
            close(new_fd);
            free(client_socket_ptr);
        }
        pthread_join(thread_id, NULL);
    }

    close(server_socket);
    return 0;
}