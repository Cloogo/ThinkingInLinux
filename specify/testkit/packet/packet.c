#include <stdio.h>
#include <stdarg.h>
#include "packet.h"

/*
 *Pack data according to fmt.
 *characters in fmt:
 *"c" stands for unsigned char,
 *"s" stands for unsigned short int,
 *"l" stands for unsigned long.
 *return val:the number of bytes packed into buf.
 */
int
pack(uchar* buf,const char* fmt,...){
    uchar* pbuf=buf;
    ushort s=0;
    ulong l=0;
    va_list args;
    va_start(args,fmt);
    for(const char* pfmt=fmt;*pfmt!='\0';pfmt++){
        switch(*pfmt){
        case 'c':
            *pbuf++=va_arg(args,int);
            break;
        case 's':
            s=va_arg(args,int);
            *pbuf++=s>>8;
            *pbuf++=s;
            break;
        case 'l':
            l=va_arg(args,ulong);
            *pbuf++=l>>24;
            *pbuf++=l>>16;
            *pbuf++=l>>8;
            *pbuf++=l;
            break;
        default:
            va_end(args);
            return -1;
        }
    }
    va_end(args);
    return pbuf-buf;
}

/*unpacked buf into ... according to fmt.
 * 
 * for example:
 * char packet[1024];
 * pack(packet,"csl",'s',10,512);
 * unsigned char op;
 * unsigned short argsnum;
 * unsigned long cntlen;
 * unpack(packet,"csl",&op,&argsnum,&cntlen);
 * 
 * return val:
 * the number of bytes unpacked from buf.
 */
int
unpack(uchar* buf,const char* fmt,...){
    uchar* pbuf=buf;
    uchar* c;
    ushort* sp;
    ulong* ulp;
    va_list args;
    va_start(args,fmt);
    for(const char* pfmt=fmt;*pfmt!='\0';pfmt++){
        switch(*pfmt){
        case 'c':
            c=va_arg(args,uchar*);
            *c=*pbuf++;
            break;
        case 's':
            sp=va_arg(args,ushort*);
            *sp=*pbuf++<<8;
            *sp|=*pbuf++;
            break;
        case 'l':
            ulp=va_arg(args,ulong*);
            *ulp=*pbuf++<<24;
            *ulp|=*pbuf++<<16;
            *ulp|=*pbuf++<<8;
            *ulp|=*pbuf++;
            break;
        default:
            va_end(args);
            return -1;
        }
    }
    va_end(args);
    return pbuf-buf;
}
