#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <linux/videodev2.h>
#include <sys/mman.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "ext_lib/render_sdl2.h"

#pragma region DEF_CONST

#define SDL_RENDER 0//[DEBUG PURPOSE] Enable rendering if set to 1

#define TRUE 1
#define MIN_BUFF 2  // Minimum number of buffers required
#define REQ_BUFF 4  // Requested number of buffers
#define FRAME_WIDTH 640
#define FRAME_HEIGHT 480
#define FILENAME_MAX_LEN 256

struct buffer{
    void *start;
    size_t length;
};

// Macro to clear struct memory
#define CLEAR(x) memset(&(x), 0, sizeof(x))

// Function to handle errors and exit
static void errno_exit(const char* s){
    fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
    exit(EXIT_FAILURE);
}

#pragma endregion

#pragma region FRAME_PROC_FUN

// Function to render a frame using SDL2 [DEBUG PURPOSE]
static void render_frame(const void* p, int size_bytes, const struct v4l2_format* v4l_sd2l){
    uint8_t *buf0 = malloc(sizeof(char) * (v4l_sd2l->fmt.pix.sizeimage) * 3);
    decode_sdl2_mjpeg_frame((uint8_t *)p, buf0, size_bytes);
    render_sdl2_frame(buf0, (v4l_sd2l->fmt.pix.width) * sizeof(char) * 3);
    free(buf0);
}

// Function to process a single video frame: dequeues, sends, optionally renders, then re-queues
static int process_frame(int webcam_ds, int socket_ds, struct buffer* buffers,const struct v4l2_format* v4l_sd2l){
    struct v4l2_buffer buf;
    CLEAR(buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    // Dequeue a frame from the buffer
    if (ioctl(webcam_ds, VIDIOC_DQBUF, &buf) == -1) errno_exit("VIDIOC_DQBUF");

    // Send the frame data to the server
    if(send(socket_ds, buffers[buf.index].start, buf.bytesused, 0) == -1) errno_exit("Frame_send");
    
    #if SDL_RENDER
        render_frame(buffers[buf.index].start, buf.bytesused,v4l_sd2l);
    #endif

    // Re-queue the buffer for further capturing
    if (ioctl(webcam_ds, VIDIOC_QBUF, &buf) == -1) errno_exit("VIDIOC_QBUF");
    return 1;
}

#pragma endregion

int main(int argc, char** argv){
    int port,num_frame;

    if(argc < 3){
        printf("Usage: ./CClient <port> <num_frame>\n");
        exit(EXIT_FAILURE);
    }
    sscanf(argv[1], "%d", &port);
    sscanf(argv[2], "%d", &num_frame);

    // Open the webcam device
    // REMINDER: Active webcam device on VirtualBox
    const char* dev_name = "/dev/video0";
    int webcam_ds = -1;
    if((webcam_ds = open(dev_name,O_RDWR | O_NONBLOCK, 0)) == -1) errno_exit(dev_name);

    // Create socket
    struct sockaddr_in sin;
    CLEAR(sin);
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(port);

    int socket_ds = -1;
    if ((socket_ds = socket(AF_INET, SOCK_STREAM, 0)) == -1) errno_exit("Socket");

    // Connect to server localhost:<port>
    if (connect(socket_ds,(struct sockaddr *)&sin, sizeof(sin)) == -1) errno_exit("Connect");
    printf("Connected to %s:%d\n", inet_ntoa(sin.sin_addr),port);

    // Send filename to server
    char filename[FILENAME_MAX_LEN];
    CLEAR(filename);
    sprintf(filename,"Webcam_%d_%d_%d.mjpeg",FRAME_WIDTH,FRAME_HEIGHT,num_frame);
    if (send(socket_ds, filename, strlen(filename), 0) == -1) errno_exit("Send_filename");
    printf("Filename %s sent to %s:%d\n",filename,inet_ntoa(sin.sin_addr),port);

    // Initialize the webcam device
    // 1. Query webcam device capabilities
    // NOTE: V4L2_CAP_READWRITE not work
    struct v4l2_capability cap;
    CLEAR(cap);

    if(ioctl(webcam_ds,VIDIOC_QUERYCAP,&cap)==-1) errno_exit("VIDIOC_QUERYCAP");

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) errno_exit("Not a video capture device");
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) errno_exit("Streaming I/O not supported");

    
    //2. Set video format (MJPEG for VirtualBox compatibility)
    struct v4l2_format my_fmt;
    CLEAR(my_fmt);

    my_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    my_fmt.fmt.pix.width = FRAME_WIDTH;
    my_fmt.fmt.pix.height = FRAME_HEIGHT;
    my_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;

    if (ioctl(webcam_ds, VIDIOC_S_FMT, &my_fmt) == -1) errno_exit("VIDIOC_S_FMT");
    
    // Initialize SDL2 [DEBUG PURPOSE]
    struct v4l2_format v4l_sd2l; 
    #if SDL_RENDER
        init_render_sdl2(FRAME_WIDTH, FRAME_HEIGHT, 0);
        v4l_sd2l = my_fmt;
    #endif

    // Initilzie mmap
    struct v4l2_requestbuffers req;
    CLEAR(req);

    req.count = REQ_BUFF; 
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(webcam_ds, VIDIOC_REQBUFS, &req) == -1) errno_exit("VIDIOC_REQBUFS");
    if (req.count < MIN_BUFF) errno_exit("Insufficient buffer memory");

    struct buffer* buffers = calloc(req.count, sizeof(*buffers));
    if (!buffers) errno_exit("Out of memory");

    // Map the webcam device registers to main memory addresses
    unsigned int num_buffer;
    for (num_buffer = 0; num_buffer < req.count; ++num_buffer){
        struct v4l2_buffer buf;
        CLEAR(buf);

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = num_buffer;

        if (ioctl(webcam_ds, VIDIOC_QUERYBUF, &buf)== -1) errno_exit("VIDIOC_QUERYBUF");

        buffers[num_buffer].length = buf.length;
        buffers[num_buffer].start =
            mmap(NULL /* start anywhere */,
                 buf.length,
                 PROT_READ | PROT_WRITE /* required */,
                 MAP_SHARED /* recommended */,
                 webcam_ds, buf.m.offset);

        if (MAP_FAILED == buffers[num_buffer].start) errno_exit("mmap");
    }


    // Start capturing the frames
    enum v4l2_buf_type type;
    for (size_t i = 0; i < num_buffer; ++i){
        
        struct v4l2_buffer buf;
        CLEAR(buf);

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(webcam_ds, VIDIOC_QBUF, &buf) == -1) errno_exit("VIDIOC_QBUF");
    }
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(webcam_ds, VIDIOC_STREAMON, &type) == -1) errno_exit("VIDIOC_STREAMON");

    
    // Catch the <num_frame> frames required and sending them to the server
    // if <num_frame>=-1 --> acquire frames until the client is stopped
    unsigned int count = num_frame; // = UINT_MAX = 4294967295
    for(int i = 0; i<count; i++){
        while(TRUE){
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(webcam_ds, &fds);

            // Time interval for selection
            struct timeval tv = {.tv_sec=5,.tv_usec=0};

            if(select(webcam_ds + 1, &fds, NULL, NULL, &tv)==-1) errno_exit("Select");
            #if SDL_RENDER
                render_sdl2_dispatch_events();
            #endif

            //Taking the frame and sending to the server/render by SD2L
            if (process_frame(webcam_ds,socket_ds,buffers,&v4l_sd2l)) break;
        }
        printf("Frame: %d CATCHED \t SENT to Cserver\n",i+1);
    }

    // Stop capturing the frames
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == ioctl(webcam_ds, VIDIOC_STREAMOFF, &type)) errno_exit("VIDIOC_STREAMOFF");
      
    // Clean-up the webcam device
    for (size_t i = 0; i < num_buffer; ++i)
        if (-1 == munmap(buffers[i].start, buffers[i].length)) errno_exit("munmap");
    free(buffers);
    #if SDL_RENDER
        render_sdl2_clean();
    #endif

    // Close the webcam device
    if(close(webcam_ds)==-1)  errno_exit(dev_name);
    // Close the socket
    if(close(socket_ds)==-1)  errno_exit("Socket_close");

    return EXIT_SUCCESS;
}
