#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#pragma region DEF_CONST 

#define BUFFER_SIZE 4096
#define MAX_FILE_LEN 1024
#define QUEUE_LEN 2
#define CLEAR(x) memset(&(x), 0, sizeof(x))

static void errno_exit(const char* s){
    fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
    exit(EXIT_FAILURE);
}
#pragma endregion

#pragma region UTILS

void clean_string(char *str) {
    for (int i = 0; i < strlen(str); i++) {
        if (!isprint(str[i])) str[i] = '\0';  // Remove non-printable chars
    }
}

static void count_frame(int* frame_count, const char* buffer,int rec_bytes){
    char prev_byte=0;
    for (int i = 0; i < rec_bytes; i++) {
        if (prev_byte == (char)0xFF && buffer[i] == (char)0xD8) {
            (*frame_count)++;
        }
        prev_byte = buffer[i];
    }
}
#pragma endregion

int main(int argc, char *argv[]){
    int port;

    if(argc < 2){
        printf("Usage: ./Cserver <port>\n");
        exit(0);
    }
    sscanf(argv[1], "%d", &port);

    //Creating socket
    int socket_ds=-1;
    if ((socket_ds = socket(AF_INET, SOCK_STREAM, 0)) == -1) errno_exit("socket");

    int reuse = 1;
    if (setsockopt(socket_ds, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0) errno_exit("setsockopt(SO_REUSEADDR) failed");

    //Binding process to localhost:<port> 
    struct  sockaddr_in sin;
    CLEAR(sin);
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(port);

    if(bind(socket_ds, (struct sockaddr *) &sin, sizeof(sin)) == -1) errno_exit("bind");

    //Listening to incoming connection 
    if(listen(socket_ds, QUEUE_LEN) == -1) errno_exit("listen");
    printf("Listening to port %d...\n",port);

    for(;;){
        int client_ds=-1;
        struct  sockaddr_in sClient;
        int sAddrLen = sizeof(sClient);
        if ((client_ds = accept(socket_ds, (struct sockaddr *) &sClient, &sAddrLen)) == -1) errno_exit("accept");
        printf("Connection received from %s\n", inet_ntoa(sClient.sin_addr));

        //Reading the filename received from the client
        char filename[MAX_FILE_LEN];
        read(client_ds, filename, MAX_FILE_LEN);
        printf("Filename: %s\n", filename);
        clean_string(filename);
        int file_ds = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);

        char *buffer = calloc(BUFFER_SIZE, sizeof(char));
        int frame_count=0;
        int rec_bytes;  
        //Receiving frames from client and writing them on file <filename>
        while ((rec_bytes = recv(client_ds, buffer, BUFFER_SIZE-1, 0)) > 0) {
            write(file_ds, buffer, rec_bytes);
            
            // Count frames by detecting MJPEG start marker (0xFF 0xD8)
            count_frame(&frame_count,buffer,rec_bytes);
        }
        free(buffer);

        printf("File saved successfully. Frames received: %d\n",frame_count);
        //Command to convert MJPEG to MP4
        //ffmpeg -i filename -c:v libx264 -preset fast -crf 23 output.mp4
        
        //Closing file
        close(file_ds);
        //Closing client connection (server-side)
        close(client_ds);
    }


    return EXIT_SUCCESS;
}
