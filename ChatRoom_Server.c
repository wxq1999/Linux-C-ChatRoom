#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "Protocol.h"

#define COUNT 5
#define SERVER_PORT 12552
#define SERVER_ADDR "172.21.125.185"


int socket_fd[COUNT]={-1,-1,-1,-1,-1};
int remain=5;
int LoginSuccess = 0;
pthread_mutex_t remainmutex;


struct Account account[COUNT];
struct Packet pack_recv;
struct Packet pack_send;

void Register_Process(int fd);
void Login_Process(int fd);
void SendFile_Process(int fd);
void Chat_Process(int fd);


//每来一个客户就开一个线程
void pthread_function(void* clientfd){

    InitPack(&pack_recv);
    InitPack(&pack_send);
    int i;
    int client_fd=*(int*)clientfd;


    pthread_mutex_lock(&remainmutex);
    if(remain>0)
        remain--;
    else pthread_exit(NULL);
    pthread_mutex_unlock(&remainmutex);



    if(recv(client_fd,&pack_recv, sizeof(pack_recv),0) == -1)
    {
        perror("recv error");
        pthread_exit(NULL);
    }
    switch(pack_recv.PacketType) {
        case REGISTER:
            Register_Process(client_fd);
            break;
        case LOGIN:
            Login_Process(client_fd);
            break;
        default:
            break;
    }



    //close socket and reset the fd -1
    close(client_fd);

    pthread_mutex_lock(&remainmutex);
    if(remain<5)
        remain++;
    else pthread_exit(NULL);
    pthread_mutex_unlock(&remainmutex);
    for (i = 0; i < COUNT; ++i)
    {
        if(socket_fd[i]==client_fd)
            socket_fd[i]=-1;
        if(account[i].accntfd == client_fd)
            account[i].accntfd = -1;
    }
    pthread_exit(NULL);
}


int main(){

    int i;
    for(i = 0; i < COUNT; i++)
        account[i].accntfd = -1;
    pthread_mutex_init(&remainmutex,NULL);

    pthread_t tid;
    int sockfd,client_fd;
    socklen_t sin_size;
    struct sockaddr_in my_addr;
    struct sockaddr_in remote_addr;
    if((sockfd=socket(AF_INET,SOCK_STREAM,0))==-1){
        perror("socket");
        exit(1);
    }

    my_addr.sin_family=AF_INET;
    my_addr.sin_port=htons(SERVER_PORT);
    my_addr.sin_addr.s_addr=inet_addr(SERVER_ADDR);
    bzero(&(my_addr.sin_zero), sizeof(my_addr.sin_addr));


    if(bind(sockfd,(struct sockaddr*)&my_addr,sizeof(struct sockaddr))==-1){
        perror("bind");
        exit(1);
    }

    if(listen(sockfd,10)==-1){
        perror("listen");
        exit(1);
    }

    i=0;

    //主线程不断接受来自客户的连接
    while(1){
        sin_size=sizeof(struct sockaddr_in);
        if((client_fd=accept(sockfd,(struct sockaddr*)&remote_addr,&sin_size))==-1){
            perror("accept");
            exit(1);
        }
        if(remain>0){
            while(socket_fd[i]==-1)
                socket_fd[i]=client_fd;
            i=(i+1)%COUNT;
            pthread_create(&tid,NULL,(void*)pthread_function,(void*)&client_fd);
        }
        else break;
    }

    pthread_mutex_destroy(&remainmutex);
    return 0;
}

void Register_Process(int fd)
{
    struct Account account_tmp;
    recv(fd, &account_tmp, sizeof(account_tmp),0);
    for(int i = 0; i < COUNT; i++)
    {
        if(strcmp(account_tmp.UserID, account[i].UserID) == 0)
        {
            char* tmp = "Sorry, this username has been taken! Please try another one.";
            setPack(&pack_send, REGISTER, strlen(tmp),"admin" ,tmp);
            send(fd, &pack_send, sizeof(pack_send), 0);
            return;
        }
    }
    account_tmp.accntfd = fd;
    account[4 - remain] = account_tmp;

    char* tmp = "Registered!";
    setPack(&pack_send, REGISTER, strlen(tmp), "admin", tmp);
    send(fd, &pack_send, sizeof(pack_send), 0);
    LoginSuccess = 1;
    if(recv(fd,&pack_recv, sizeof(pack_recv),0) == -1)
    {
        perror("recv error");
        exit(1);
    }
    sleep(1);
    switch(pack_recv.PacketType)
    {
        case CHAT:
            Chat_Process(fd);
            break;
        case SENDFILE:
            SendFile_Process(fd);
            break;
        case EXIT:
            break;
        default:
            break;
    }
}

void Login_Process(int fd) {
    int i;
    struct Account account_tmp;
    char *tmp;
    recv(fd, &account_tmp, sizeof(account_tmp), 0);

    for (i = 0; i < COUNT; i++)
    {
        if ((strcmp(account_tmp.UserID, account[i].UserID) == 0) && (account_tmp.Passwd == account[i].Passwd)) {
            if (account[i].accntfd == -1) {
                tmp = "Login Successfully!";
                LoginSuccess = 1;
                account[i].accntfd = fd;
                break;
            } else{
                tmp = "Someone has already logged in with your account before!";
                LoginSuccess = 0;
                break;
            }
        }
    }
    if (i == COUNT) {
        LoginSuccess = 0;
        tmp = "Login Failed! Check your Username and Password!";
    }
    setPack(&pack_send, LOGIN, strlen(tmp),"admin", tmp);
    send(fd, &pack_send, sizeof(pack_send), 0);

    if (LoginSuccess) {
        recv(fd, &pack_recv, sizeof(pack_recv), 0);
        switch(pack_recv.PacketType)
        {
            case CHAT:
                Chat_Process(fd);
                break;
            case SENDFILE:
                SendFile_Process(fd);
                break;
            case EXIT:
                break;
            default:
                break;
        }
    }
}

void Chat_Process(int fd)
{
    long recvbytes;
    int i;
    int option;
    int choice;
    char userlist[140];
    char* plen = userlist;
    recv(fd, &option, sizeof(option), 0);
    if(option == 1) {
        for(i = 0; i < COUNT; i++)
        {
            memcpy(plen, &account[i], 28);
            plen += 28;
        }
        if(send(fd, userlist, 140, 0) == -1){
            perror("send");
        }
        recv(fd, &choice, sizeof(choice), 0);
    }
    while (1) {
        if ((recvbytes = recv(fd, &pack_recv, sizeof(pack_recv), 0)) == -1) {
            perror("recv");
            pthread_exit(NULL);
        }

        if (recvbytes == 0) {
            for(i = 0; i < COUNT; i++){
                if(account[i].accntfd == fd)
                    break;
            }
            printf("User %s has disconnected!\n",account[i].UserID);
            break;
        }
        if (pack_recv.PacketType == CHAT) {
            if (option == 1) {
                int tmpclient = account[choice-1].accntfd;
                if (tmpclient != -1) {
                    pack_send = pack_recv;
                    if (send(tmpclient, &pack_send, sizeof(pack_send), 0) == -1)
                        perror("send error");
                }
            }
            else if(option == 2){
                int tmpclient;
                for(i = 0; i < COUNT; i++) {
                    tmpclient = socket_fd[i];
                    if (tmpclient != -1) {
                        pack_send = pack_recv;
                        if (send(tmpclient, &pack_send, sizeof(pack_send), 0) == -1)
                            perror("send error");
                    }
                }
            }
        }
    }
}

void SendFile_Process(int fd)
{

}