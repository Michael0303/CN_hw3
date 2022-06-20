#include <iostream>
#include <string>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include "opencv2/opencv.hpp"

#define BUFF_SIZE 5000
#define MAXSEQ 50000

using namespace std;
using namespace cv;

typedef struct {
	int length;
	int seqNumber;
	int ackNumber;
	int fin;
	int syn;
	int ack;
} header;

typedef struct{
	header head;
	char data[5000];
} segment;

int seqnum = 1;
int cwnd = 1;
int thrshd = 16;
segment segseq[MAXSEQ];
int resnd[MAXSEQ] = {0};
int cansend = 0;
struct sockaddr_in sender, agent, tmp_addr;
int sendersocket;
socklen_t agent_size, tmp_size;


void setIP(char *dst, char *src){
    if(strcmp(src, "0.0.0.0") == 0 || strcmp(src, "local") == 0 || strcmp(src, "localhost")){
        sscanf("127.0.0.1", "%s", dst);
    }
    else{
        sscanf(src, "%s", dst);
    }
    return;
}

void sendinfo(int num, int fin){
    if (fin == 1){
        cout << "send    fin" << endl;
    } else{
        cout << "send    data    #" << num << ",    windSize = " << cwnd << endl;
    }
    return;
}

void resndinfo(int num){
    cout << "resnd   data    #" << num << ",    windSize = " << cwnd << endl;
    return;
}

int getack(void){
    segment s_tmp;
    int segment_size = recvfrom(sendersocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&tmp_addr, &tmp_size);
    if (segment_size <= 0){
        return -1;
    } else{
        if (s_tmp.head.fin == 0){
            int num = s_tmp.head.ackNumber;
            cout << "recv    ack     #" << num << endl;
            return num;
        } else{
            cout << "recv    finack" << endl;
            return 0;
        }
    }
}

void changecwnd(int lost){
    if (lost == 0){
        if (cwnd < thrshd){
            cwnd *= 2;
        } else{
            cwnd += 1;
        }
    } else{
        thrshd = cwnd / 2;
        if (thrshd < 1)
            thrshd = 1;
        cout << "time    out,            threshold = " << thrshd << endl;
        cwnd = 1;
    }
    return;
}

void Go_back_N(int sendsize){
    for (int i = 0; i < sendsize; i++){
        int segment_size = sizeof(segment);
        if (resnd[i] == 1)
            resndinfo(segseq[i].head.seqNumber);
        else
            sendinfo(segseq[i].head.seqNumber, 0);
        sendto(sendersocket, &segseq[i], segment_size, 0, (struct sockaddr *)&agent, agent_size);
        resnd[i] = 1;
    }
    //check #ack
    int expectacknum = segseq[0].head.seqNumber;
    int dupack = 0;
    int ackcount = 0;
    for (int i = 0; i < sendsize; i++){
        int num = getack();
        if (num == expectacknum && dupack == 0){
            ackcount++;
            expectacknum++;
        } else if (num > expectacknum && dupack == 0){
            int d = num - expectacknum;
            i += d;
            ackcount += (d + 1);
            expectacknum = num + 1;
        } else{
            //get dup ack
            dupack = 1;
        }
    }
    //move unack segments forward to 0
    int count = 0;
    for (int i = ackcount; i < cansend; i++, count++){
        segseq[count] = segseq[i];
        resnd[count] = resnd[i];
    }
    cansend = count;
    if (dupack == 1)
        changecwnd(1);
    else{
        changecwnd(0);
    }
    return;
}

void seqlineup(char buf[], int length, int fin){
    //make new segment
    segment s_tmp;
    memset(&s_tmp, 0, sizeof(s_tmp));
    memcpy(s_tmp.data, buf, length);
    //head
    s_tmp.head.seqNumber = seqnum;
    s_tmp.head.length = length;
    s_tmp.head.fin = fin;
    seqnum++;

    if (fin == 0){
        //Go back N
        if (cansend >= cwnd){
            Go_back_N(cwnd);

            //insert new segment
            segseq[cansend] = s_tmp;
            resnd[cansend] = 0;
            cansend++;
        }else{
            //insert new segment
            segseq[cansend] = s_tmp;
            resnd[cansend] = 0;
            cansend++;     
        }
    } else {
        //insert fin segment
        segseq[cansend] = s_tmp;
        resnd[cansend] = 0;
        int finseqnum = cansend;
        while (cansend > 0){
            int sendsize = cwnd;
            if (cansend < sendsize)
                sendsize = cansend;
            Go_back_N(sendsize);
        }
        //send fin
        int segment_size = sizeof(segment);
        sendinfo(segseq[finseqnum].head.seqNumber, 1);
        sendto(sendersocket, &segseq[finseqnum], segment_size, 0, (struct sockaddr *)&agent, agent_size);
        getack();
    }
    return;
}

int main(int argc, char* argv[]){
    //argument
    char ip[2][50];
    int port[2];
    char srcfile[BUFF_SIZE];
    if(argc != 5){
        fprintf(stderr,"Usage: %s <agent IP> <sender port> <agent port> <srcfile>\n", argv[0]);
        fprintf(stderr, "Example: ./sender local 8887 8888 video.mpg\n");
        exit(1);
    }else{
        setIP(ip[0], "local");
        setIP(ip[1], argv[1]); 
        sscanf(argv[2], "%d", &port[0]);
        sscanf(argv[3], "%d", &port[1]);
        sscanf(argv[4], "%s", srcfile);
        cout << "sender IP = " << ip[0] << " port = " << port[0] << " videosrc = " << srcfile << endl;
        cout << "to agent IP = " << ip[1] << " port = " << port[1] << endl;
    }
    
    /* Create UDP socket */
    sendersocket = socket(PF_INET, SOCK_DGRAM, 0);

    /* Configure settings in sender struct */
    sender.sin_family = AF_INET;
    sender.sin_port = htons(port[0]);
    sender.sin_addr.s_addr = inet_addr(ip[0]);
    memset(sender.sin_zero, '\0', sizeof(sender.sin_zero));  

    /* Configure settings in agent struct */
    agent.sin_family = AF_INET;
    agent.sin_port = htons(port[1]);
    agent.sin_addr.s_addr = inet_addr(ip[1]);
    memset(agent.sin_zero, '\0', sizeof(agent.sin_zero));  

    /* bind socket */
    bind(sendersocket,(struct sockaddr *)&sender,sizeof(sender));
    //set timeout
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;
    setsockopt(sendersocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    agent_size = sizeof(agent);
    tmp_size = sizeof(tmp_addr);

    //start play  
    segment s_tmp;
    int segment_size;
    string tmp;
    char buf[BUFF_SIZE];
    Mat server_img;
    VideoCapture cap(srcfile);
    // Get the resolution of the video
    int width = cap.get(CV_CAP_PROP_FRAME_WIDTH);
    int height = cap.get(CV_CAP_PROP_FRAME_HEIGHT);
    tmp = to_string(width);
    strncpy(buf, tmp.c_str(), tmp.length());
    seqlineup(buf, strlen(buf), 0);
    tmp = to_string(height);
    strncpy(buf, tmp.c_str(), tmp.length());
    seqlineup(buf, strlen(buf), 0);

    // Allocate container to load frames 
    server_img = Mat::zeros(height, width, CV_8UC3);    
    // Ensure the memory is continuous (for efficiency issue.)
    if(!server_img.isContinuous()){
        server_img = server_img.clone();
    }
    while(1){
        // Get a frame from the video to the container of the server.
        cap >> server_img;   

        // Get the size of a frame in bytes 
        int imgSize = server_img.total() * server_img.elemSize();
        tmp = to_string(imgSize);
        memset(buf, 0, BUFF_SIZE);
        strncpy(buf, tmp.c_str(), tmp.length());
        seqlineup(buf, strlen(buf), 0);
        if (imgSize == 0){
            seqlineup(buf, 0, 1);
            break;
        }
        // Allocate a buffer to load the frame (there would be 2 buffers in the world of the Internet)
        uchar buffer[imgSize];
        // Copy a frame to the buffer
        memcpy(buffer, server_img.data, imgSize);
        uchar *iptr = buffer;
        int count = 0;
        while (count < imgSize){
            int copysize = BUFF_SIZE;
            if (imgSize - count < copysize)
                copysize = imgSize - count;
            memset(buf, 0, BUFF_SIZE);
            memcpy(buf, iptr, copysize);
            //cout << "copy size = " << copysize << endl;
            seqlineup(buf, copysize, 0);
            iptr += copysize;
            count += copysize;
        }
    }
    cap.release();  
    
    return 0;
}