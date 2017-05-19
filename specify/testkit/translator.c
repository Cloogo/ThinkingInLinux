#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include "packet/packet.h"

#define DEFAULT_MAXSEQID 64
#define DEFAULT_PACKETSIZE 1024
#define NSUBVARS 10
#define VARLEN 32

char subs[NSUBVARS][VARLEN]={{'\0'}};

void initsubs();
void rorder(uchar* pkts,int mseqid,int mpktsz);
void repwrt(uchar* pkts,int mseqid,int mpktsz);
void fatal(const char* msg);


//reads from pack.pk and
//replaces variants in form of $? with strings read from stdin,
//places packets in order,
//writes them to stdout.
int
main(){
    initsubs();

    int mseqid,mpktsz;
    char* p;
    if((p=getenv("DEFAULT_MAXSEQID"))==NULL){
        mseqid=DEFAULT_MAXSEQID;
    }else{
        mseqid=atoi(p);
    }
    p=NULL;
    if((p=getenv("DEFAULT_PACKETSIZE"))==NULL){
        mpktsz=DEFAULT_PACKETSIZE;
    }else{
        mpktsz=atoi(p);
    }
    uchar pkts[mseqid][mpktsz];
    memset(pkts,'\0',mseqid*mpktsz);
    rorder((uchar*)pkts,mseqid,mpktsz);
    repwrt((uchar*)pkts,mseqid,mpktsz);
    return 0;
}

void initsubs(){
    int n=0;
    while(1){
        if(fgets(subs[n],VARLEN,stdin)==NULL){
            break;
        }
        subs[n][strlen(subs[n])-1]='\0';
//        printf("[DEBUG]:initsubs:subs[%d]=%s\n",n,subs[n]);
        n++;
        if(n>=NSUBVARS){
            break;
        }
    }
}

//reads packets and places them in order.
//note:
//1.truncates packet if its size larger than mpktsz.
//2.throws away packet if seqid>=mseqid or seqid <0
void rorder(uchar* pkts_,int mseqid,int mpktsz){
    uchar (*pkts)[mpktsz];
    uchar*** ppp=(uchar***)&pkts;
    *ppp=(uchar**)pkts_;
    int in=open("pack.pk",O_RDONLY);
    if(in==-1){
        fatal("while open pack.pk");
    }
    while(1){
        uchar hdr[8];
        int n=read(in,hdr,8);
        if(n==0){
            break;
        }else if(n==-1){
            fatal("while reading hdr in pack.pk");
        }
        ulong seqid,len;
        unpack(hdr,"ll",&seqid,&len);
        if(seqid>=mseqid||seqid<0){
            fprintf(stderr,"[WARNING]:packet seqid [%d] is invalid,required seqid>=0&& seqid<%d\n",seqid,mseqid);
            uchar tmp[len];
            read(in,tmp,len);
            continue;
        }
        if(len>mpktsz){
            fprintf(stderr,"[WARNING]:length of content of packet [%d] is too large,required len>0&&len<=%d\n",seqid,mpktsz);
            int ov=len-mpktsz;
            uchar tmp[ov];
            read(in,pkts[seqid],mpktsz);
            read(in,tmp,ov);
            continue;
        }
        n=read(in,pkts[seqid],len);
        if(n==0){
            break;
        }else if(n==-1){
            fatal("while reading body in pack.pk");
        }
//        printf("[DEBUG]:main:seqid=%lu\tlen=%lu\tbody=%s\n",seqid,len,pkts[seqid]);
    }
    close(in);
}

//replaces $i with subs[i],
//writes the packet to stdout.
void repwrt(uchar* pkts_,int mseqid,int mpktsz){
    uchar (*pkts)[mpktsz];
    uchar*** ppp=(uchar***)&pkts;
    *ppp=(uchar**)pkts_;
    for(int i=0;i<mseqid;i++){
        if(pkts[i][0]!='\0'){
            printf("%d",i);
            for(uchar* p=&pkts[i][0];*p!='\0';p++){
                if(*p=='$'){
                    p++;
                    if(*p!='\0'){
                        switch(*p){
                        case '$':
                            fputc(*p,stdout);
                            break;
                        case '0':
                        case '1':
                        case '2':
                        case '3':
                        case '4':
                        case '5':
                        case '6':
                        case '7':
                        case '8':
                        case '9':
                            fputs(subs[*p-'0'],stdout);
                            break;
                        default:
                            fputc(*p,stdout);
                            break;
                        }
                    }//end if(*p!='\0')
                }//end if(*p=='$')
                else{
                    fputc(*p,stdout);
                }
            }//end for(p)
            fputc('\n',stdout);
        }//end if(pkts[i][0]!='\0')
    }//end for(i)
}

void fatal(const char* msg){
    fprintf(stderr,"[FATAL]!!!:%s:%s\n",msg,strerror(errno));
    exit(1);
}
