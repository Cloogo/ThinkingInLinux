#ifndef _PACKET_H
#define _PACKET_H
#include "type.h"
extern int pack(uchar* buf,const char* fmt,...);
extern int unpack(uchar* buf,const char* fmt,...);
#endif
