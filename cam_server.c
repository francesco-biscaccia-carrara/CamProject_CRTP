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

#define TRUE 1
#define BUFFER_SIZE 1024    // Buffer size for receiving data
#define MAX_FILE_LEN 512    // Maximum length for filename
#define QUEUE_LEN 2         // Max length of connection queue for listen()
// Macro to clear struct memory
#define CLEAR(x) memset(&(x), 0, sizeof(x))

// Function to handle errors and exit
static void errno_exit(const char* s){
    fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
    exit(EXIT_FAILURE);
}
#pragma endregion

#pragma region UTILS

// Function to clean a string by removing non-printable characters
void clean_string(char *str) {
    for (int i = 0; i < strlen(str); i++) if (!isprint(str[i])) str[i] = '\0';
}

// Function to change the file extension from .mjpeg to .mp4
void change_extension(const char *input, char *output) {
    sprintf(output,"%s", input);
    strcpy(strrchr(output, '.'), ".mp4");
}

// Function to count frames by detecting MJPEG start marker (0xFF 0xD8)
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
    char convert=0;

    if(argc < 2){
        printf("Usage: ./Cserver <port> [-c]\n");
        exit(0);
    }
    sscanf(argv[1], "%d", &port);
    if(argc>2 && !strcmp(argv[2],"-c")) convert=1;
    
    // Create socket
    int socket_ds=-1;
    if ((socket_ds = socket(AF_INET, SOCK_STREAM, 0)) == -1) errno_exit("Socket");

    // Enable address reuse
    int reuse = 1;
    if (setsockopt(socket_ds, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0) errno_exit("Setsockopt(SO_REUSEADDR)");

    // Bind socket to localhost:<port> 
    struct  sockaddr_in sin;
    CLEAR(sin);
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(port);

    if(bind(socket_ds, (struct sockaddr *) &sin, sizeof(sin)) == -1) errno_exit("Bind");

    // Start listening for incoming connections 
    if(listen(socket_ds, QUEUE_LEN) == -1) errno_exit("Listen");
    printf("Listening to port %d...\n",port);

    while(TRUE){
        int client_ds=-1;
        struct  sockaddr_in sClient;
        CLEAR(sClient);
        int sAddrLen = sizeof(sClient);

        if ((client_ds = accept(socket_ds, (struct sockaddr *) &sClient, &sAddrLen)) == -1) errno_exit("Accept");
        printf("Connection received from %s\n", inet_ntoa(sClient.sin_addr));

        // Read the filename from the client
        char filename[MAX_FILE_LEN];
        CLEAR(filename);
        if(read(client_ds, filename, MAX_FILE_LEN)==-1) errno_exit("Read");
        clean_string(filename);
        printf("Filename: %s\n", filename);

        // Open file for writing received data
        int file_ds = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        char *buffer = calloc(BUFFER_SIZE, sizeof(char));
        int frame_count=0;
        int rec_bytes; 

        // Receive frames from client and write them to file
        while ((rec_bytes = recv(client_ds, buffer, BUFFER_SIZE-1, 0)) > 0) {
            if(write(file_ds, buffer, rec_bytes)==-1) errno_exit("Write");
            
            // Count frames by detecting MJPEG start marker (0xFF 0xD8)
            count_frame(&frame_count,buffer,rec_bytes);
        }
        free(buffer);
        printf("File saved successfully. Frames received: %d\n",frame_count);

       // Convert MJPEG to MP4 if <-c> flag is set
        if(convert){
            char output_filename[MAX_FILE_LEN];
            CLEAR(output_filename);
            change_extension(filename, output_filename);
            
            char command[4*MAX_FILE_LEN];
            CLEAR(command);
            sprintf(command, "ffmpeg -i %s -c:v libx264 -preset fast -crf 23 %s > /dev/null 2>&1", filename, output_filename);
            if(system(command)==-1) errno_exit("System_command");
            printf("Conversion to MP4 complete: %s\n", output_filename);
        }
        
        // Close file
        close(file_ds);
        // Close client connection
        close(client_ds);
    }


    return EXIT_SUCCESS;
}
