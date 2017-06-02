#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/prctl.h>

struct report{
    int passed;
    int total_requests;
    double time_elapsed;
    double bench_time;
    unsigned long long bytes;
};

void usage(const char* progname);
void show_waiting_info();
void* cli(void* arg);
int dial_tcp();
int redirio_atfork(const char* subroutin,char* buf,int fd,int(*)(char*,int,int pfd0[2],int pfd1[2]));
int exec_netpack(char* buf,int fd,int pfd0[2],int pfd1[2]);
int exec_trans(char* buf,int fd,int pfd0[2],int pfd1[2]);
int setnonblock(int sfd);
int receive_from(int sfd);
void mkrpt();
void thread_err(pthread_t id,const char* msg);

struct report rpt={0,0,0.0,0.0,0};
char* ip=NULL;
char* port=NULL;
int ntask=0;
int ntimes=0;
int enable_recvall=0;

int main(int argc,char** argv){
    if(argc<6){
        usage(argv[0]);
    }
    srand(getpid());
    ip=argv[1];
    port=argv[2];
    ntask=atoi(argv[3]);
    ntimes=atoi(argv[4]);
    enable_recvall=atoi(argv[5]);
    show_waiting_info();
    pthread_t threads[ntask];
    for(int i=0;i<ntask;i++){
        pthread_create(&threads[i],NULL,cli,NULL);
    }
    struct report** cli_rpt;
    for(int i=0;i<ntask;i++){
        pthread_join(threads[i],(void**)cli_rpt);
        rpt.passed+=(*cli_rpt)->passed;
        rpt.time_elapsed+=(*cli_rpt)->time_elapsed;
        rpt.bytes+=(*cli_rpt)->bytes;
        free(*cli_rpt);
    }
    rpt.bench_time=rpt.time_elapsed/ntask;
    rpt.total_requests=ntask*ntimes;
    mkrpt();
    return 0;
}

void usage(const char* progname){
    fprintf(stderr,"usage:%s ip port ntask ntimes enable_recvall\n",progname);
    exit(1);
}

void show_waiting_info(){
    printf(".........................\n");
    printf("Name:\tlaunchpack\n");
    printf("Version:1.0\n");
    printf(".........................\n");
    printf("\n");
    printf("Running %d clients,totally sending %d packets to %s:%s...\n",ntask,ntask*ntimes,ip,port);
    printf("\n");
}

void* cli(void* arg){
    struct report* pocli=malloc(sizeof(struct report));
    pocli->passed=0;
    pocli->bytes=0;
    pocli->time_elapsed=0.0;
    for(int i=0;i<ntimes;i++){
        char buf[64*1024]={'\0'};
        redirio_atfork("trans",buf,-1,exec_trans);
        int n=redirio_atfork("netpack",buf,-1,exec_netpack);
        time_t start,end;
        time(&start);
        int sfd=dial_tcp();
        if(sfd==-1){
            thread_err(pthread_self(),"while dial_tcp");
            continue;
        }
        if(write(sfd,buf,n)!=n){
            thread_err(pthread_self(),"while write(sfd)");
            continue;
        }
        n=receive_from(sfd);
//        if(close(sfd)==-1){
//            thread_err(pthread_self(),"while close(sfd)");
//            continue;
//        }
        time(&end);
        if(n>0||enable_recvall==0){
            pocli->passed++;
            pocli->bytes+=n;
            pocli->time_elapsed+=difftime(end,start);
        }
        sleep(1);
    }
    pthread_exit(pocli);
}

int dial_tcp(){
    int sfd=socket(AF_INET,SOCK_STREAM,0);
    if(sfd==-1){
        return -1;
    }
    struct in_addr ipaddr;
    if(inet_aton(ip,&ipaddr)==0){
        return -1;
    }
    struct sockaddr_in s_addr;
    memset(&s_addr,0,sizeof(s_addr));
    s_addr.sin_family=AF_INET;
    s_addr.sin_addr=ipaddr;
    s_addr.sin_port=htons(atoi(port));
    if(connect(sfd,(struct sockaddr*)&s_addr,sizeof(s_addr))==-1){
        return -1;
    }
    return sfd;
}

int redirio_atfork(const char*subroutin,char* buf,int fd,int(*apply)(char* buf,int fd,int pfd0[2],int pfd1[2])){
    int pfd0[2],pfd1[2];
    int n;
    if(pipe2(pfd0,O_CLOEXEC)==-1){
        thread_err(pthread_self(),"while pipe2(pfd0)");
    }
    if(pipe2(pfd1,O_CLOEXEC)==-1){
        thread_err(pthread_self(),"while pipe2(pfd1)");
    }
    int cid=fork();
    if(cid==-1){
        thread_err(pthread_self(),"while fork() in trans");
    }else if(cid==0){
        close(pfd0[1]);
        close(pfd1[0]);
        if(pfd0[0]!=STDIN_FILENO){
            if(dup2(pfd0[0],STDIN_FILENO)!=STDIN_FILENO){
            }
            close(pfd0[0]);
        }
        if(pfd1[1]!=STDOUT_FILENO){
            if(dup2(pfd1[1],STDOUT_FILENO)!=STDOUT_FILENO){
            }
            close(pfd1[1]);
        }
        if(execv(subroutin,NULL)==-1){
            char ebuf[64];
            snprintf(ebuf,64,"while exec(\"./%s\")",subroutin);
            thread_err(pthread_self(),buf);
        }
    }else{
        //parent
        close(pfd0[0]);
        close(pfd1[1]);
        n=apply(buf,fd,pfd0,pfd1);
    }
    waitpid(cid,NULL,0);
    return n;
}

int exec_trans(char* buf,int fd,int pfd0[2],int pfd1[2]){
    char* pbuf=buf;
    char vars[64*10]={'\0'};
    char* pv=vars;
    for(int i=0;i<10;i++){
        pv+=sprintf(pv,"%d\n",rand());
    }
    write(pfd0[1],vars,pv-vars);
    close(pfd0[1]);
    char c;
    while(read(pfd1[0],&c,1)>0){
        *pbuf=c;
        pbuf++;
    }
    *pbuf='\0';
    close(pfd1[0]);
    return pbuf-buf;
}

int exec_netpack(char* buf,int fd,int pfd0[2],int pfd1[2]){
    write(pfd0[1],buf,strlen(buf));
    close(pfd0[1]);
    char* pbuf=buf;
    char c;
    while(read(pfd1[0],&c,1)>0){
        *pbuf++=c;
    }
    *pbuf='\0';
    close(pfd1[0]);
    return pbuf-buf;
}

int setnonblock(int sfd){
    int flags=fcntl(sfd,F_GETFD);
    if(flags==-1){
        return -1;
    }
    flags|=O_NONBLOCK;
    if(fcntl(sfd,F_SETFD,flags)==-1){
        return -1;
    }
    return 0;
}

int receive_from(int sfd){
    if(setnonblock(sfd)==-1){
        thread_err(pthread_self(),"while setnonblock");
        return 0;
    }
    char res[1024];
    int size_recv=0,total_size=0;
    while(1){
        memset(res,0,sizeof(res));
        if((size_recv=read(sfd,res,sizeof(res)))==-1){
            if(errno==EWOULDBLOCK||errno==EAGAIN){
//                 printf("Recv timeout.\n");
                break;
            }else if(errno==EINTR){
//                printf("Interrupt by signal.\n");
                continue;
            }else if(errno==ENOENT){
//                printf("Recv RST segement.\n");
                break;
            }else{
//                printf("Unknow error!\n");
                break;
            }
        }else if(size_recv==0){
//            printf("\nPeer close.\n");
            break;
        }else{
            if(!enable_recvall){
                return size_recv;
            }
            total_size+=size_recv;
            if(size_recv<sizeof(res)){
                break;
            }
        }
    }
//    printf("Reply received,total_size=%d bytes.\n",total_size);
    return total_size;
}

void mkrpt(){
    printf("Done.\n");
    printf("Total Time cost:%.2fs\n",rpt.bench_time);
    printf("Total Transferred:%dbytes\n",rpt.bytes);
    printf("\n");
    printf("%.2f requests/sec\n"
           "%.2f bytes/sec\n"
           "%.2f ms/request\n"
           "%.2f ms/request(across all concurrent requests)\n"
           "%d succeed\n"
           "%d failed\n",
           rpt.total_requests/rpt.bench_time,
           rpt.bytes/rpt.bench_time,
           rpt.bench_time*1000/rpt.total_requests/ntask,
           rpt.bench_time*1000/rpt.total_requests,
           rpt.passed,
           rpt.total_requests-rpt.passed);
    printf("\n");
}

void thread_err(pthread_t id,const char* msg){
    fprintf(stderr,"[FATAL]!!!thread:%d %s:%s\n",id,msg,strerror(errno));
//    pthread_exit(NULL);
}
