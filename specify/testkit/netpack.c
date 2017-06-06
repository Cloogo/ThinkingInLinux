#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include "packet/packet.h"

void usage(const char* msg);
void fatal(const char* msg);
int lenofnum(int n);

//reads json string(s) from stdin,
//packages  them with the following format:
//size+json
//size  : 4 bytes
//order : BigEndian.
//writes packets into stdout.
int main(){
    int out=STDOUT_FILENO;
    while(1){
        int seqid; 
        int len;
        uchar* json;
        ssize_t total;
        int n=scanf("%u%m[^\n]%n", &seqid, &json,&len);
        if(n==2){
            len-=lenofnum(seqid);
            total=4+len;
            uchar buf[total];
            n=pack(buf,"l",len);
            memcpy(buf+n,json,len);
            n=write(out,buf,total);
            if(n==-1){
                fatal("while writing data to stdout");
            }
            free(json);
        }else if(n==EOF){
            break;
        }else{
            usage("Only the following format supported:\n"
                    "seqid+json\n"
                    "eg:\n"
                    "1{\"request\":10}\n"
                    "2{\"request\":0}\n"
                    "100{\"request\":1}"
                    );
        }
    }
    return 0;
}

void usage(const char* msg){
    fprintf(stderr,"%s\n",msg);
    exit(1);
}

void fatal(const char* msg){
    fprintf(stderr,"[FATAL]!!!:%s:%s\n",msg,strerror(errno));
    exit(1);
}

int lenofnum(int n){
    int c=1;
    while(n/10!=0){
        c++;
        n/=10;
    }
    return c;
}
