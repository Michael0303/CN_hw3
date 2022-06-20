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
#define MAXSEQ 64

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

int expectseqnum = 1;
segment segseq[MAXSEQ];
int cantake = 0;
struct sockaddr_in receiver, agent, tmp_addr;
int receiversocket;
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

void recvinfo(int num, int fin){
    if (fin == 1){
        cout << "recv    fin" << endl;
    } else{
        cout << "recv    data    #" << num << endl;
    }
    return;
}

void sendinfo(int num, int fin){
    if (fin == 1){
        cout << "send    finack" << endl;
    } else{
        cout << "send    ack     #" << num << endl;
    }
    return;
}

void dropinfo(int num){
    cout << "drop    data    #" << num << endl;
    return;
}

void sendack(int num, int fin){
    segment s_tmp;
    //head
    s_tmp.head.ack = 1;
    if (fin == 1)
        s_tmp.head.fin = 1;
    else
        s_tmp.head.fin = 0;
    s_tmp.head.ackNumber = num;
    sendto(receiversocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, agent_size);
    return;
}

int segrecv(){
    int fin = 0;
    while (1){
        segment s_tmp;
        int segment_size = recvfrom(receiversocket, &s_tmp, sizeof(segment), 0, (struct sockaddr *)&tmp_addr, &tmp_size);
        if (segment_size <= 0){
            continue;
        } else{
            if (s_tmp.head.fin == 1){
                recvinfo(0, 1);
                fin = 1;
                break;
            }
            if (s_tmp.head.seqNumber == expectseqnum){
                if (cantake == MAXSEQ){
                    dropinfo(s_tmp.head.seqNumber);
                    sendinfo(expectseqnum - 1, 0);
                    sendack(expectseqnum - 1, 0);
                    cout << "flush" << endl;
                    cantake = 0;
                } else{
                    recvinfo(expectseqnum, 0);
                    sendinfo(expectseqnum, 0);
                    sendack(expectseqnum, 0);
                    expectseqnum++;
                    //buffer segment
                    //cout << "recv size = " << segment_size << endl;
                    //cout << "recv length = " << s_tmp.head.length << endl;
                    segseq[cantake] = s_tmp;
                    cantake++;
                    break;
                } 
            } else{
                //drop occur
                dropinfo(s_tmp.head.seqNumber);
                sendinfo(expectseqnum - 1, 0);
                sendack(expectseqnum - 1, 0);
            }
        }
    }
    if (fin == 1){
        cout << "flush" << endl;
        cantake = 0;
        return -1;
    } 
    return 0;
}

int main(int argc, char* argv[]){
    //argument
    char ip[2][50];
    int port[2];
    char srcfile[BUFF_SIZE];
    if(argc != 4){
        fprintf(stderr,"Usage: %s <agent IP> <recv port> <agent port> \n", argv[0]);
        fprintf(stderr, "Example: ./receiver local 8889 8888\n");
        exit(1);
    }else{
        setIP(ip[0], "local");
        setIP(ip[1], argv[1]); 
        sscanf(argv[2], "%d", &port[0]);
        sscanf(argv[3], "%d", &port[1]);
        cout << "receiver IP = " << ip[0] << " port = " << port[0] << endl;
        cout << "to agent IP = " << ip[1] << " port = " << port[1] << endl;
    }
    
    /* Create UDP socket */
    receiversocket = socket(PF_INET, SOCK_DGRAM, 0);
 
    /* Configure settings in agent struct */
    agent.sin_family = AF_INET;
    agent.sin_port = htons(port[1]);
    agent.sin_addr.s_addr = inet_addr(ip[1]);
    memset(agent.sin_zero, '\0', sizeof(agent.sin_zero));  

    /* Configure settings in receiver struct */
    receiver.sin_family = AF_INET;
    receiver.sin_port = htons(port[0]);
    receiver.sin_addr.s_addr = inet_addr(ip[0]);
    memset(receiver.sin_zero, '\0', sizeof(receiver.sin_zero)); 

    /* bind socket */
    bind(receiversocket,(struct sockaddr *)&receiver,sizeof(receiver));
    //set timeout
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;
    setsockopt(receiversocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    agent_size = sizeof(agent);
    tmp_size = sizeof(tmp_addr);

    char buf[BUFF_SIZE];
    Mat client_img;
    memset(buf, 0, BUFF_SIZE);
    segrecv();
    strncpy(buf, segseq[cantake - 1].data, segseq[cantake - 1].head.length); 
    int width = atoi(buf);
    memset(buf, 0, BUFF_SIZE);
    segrecv(); 
    strncpy(buf, segseq[cantake - 1].data, segseq[cantake - 1].head.length); 
    int height = atoi(buf);
    //cout << "width = " << width << " height = " << height << endl;
    // Allocate container to load frames    
    client_img = Mat::zeros(height, width, CV_8UC3);
    
    // Ensure the memory is continuous (for efficiency issue.)
    if(!client_img.isContinuous()){
         client_img = client_img.clone();
    }
    //cout << "playing the video..." << endl;
    
    while(1){
        // get imgSize
        memset(buf, 0, BUFF_SIZE);
        segrecv();
        strncpy(buf, segseq[cantake - 1].data, segseq[cantake - 1].head.length); 
        int imgSize = atoi(buf);
        //cout << "size = " << imgSize << endl;
        if (imgSize == 0){
            //cout << "error" << endl;
            break;
        }
    
        // Allocate a buffer to load the frame (there would be 2 buffers in the world of the Internet)

        int count = 0;
        uchar *iptr = client_img.data;
        while (count < imgSize){
            segrecv();
            int size = segseq[cantake - 1].head.length;
            memcpy(iptr, segseq[cantake - 1].data, size); 
            iptr += size;
            count += size;
        }
        // show the frame 
        imshow("Video", client_img);
        char c = (char)waitKey(11.1111);  
    }
    segrecv();
    sendinfo(0, 1);
    sendack(0, 1);
	destroyAllWindows();
	return 0;
}