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

#define SERVER_ADDR "172.21.125.185"
#define SERVER_PORT 12552

struct Packet pack_send;
struct Packet pack_recv;
struct Account account;
struct Account accntlist[5];
pthread_t tid;
int LoginSuccess = 0;

void pthread_function(void* sock_fd);  //线程主函数

void Register_request(int fd);  //注册
void Login_request(int fd);     //登录
void SendFile_request(int fd);  //发送文件
void Chat_request(int fd);      //聊天
void Chat_p2p(int fd);      //私聊


int main(void)
{
    int sockfd;
    struct sockaddr_in server_addr;

    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(SERVER_PORT);
    server_addr.sin_addr.s_addr=inet_addr(SERVER_ADDR);

    if((sockfd=socket(AF_INET,SOCK_STREAM,0))==-1){
        printf("socket error");
        exit(1);
    }
    if(connect(sockfd,(struct sockaddr*)&server_addr,sizeof(server_addr))==-1){
        printf("connect error");
        exit(1);
    }

    printf("Welcome to this chatroom! Select an option to continue:\n");
    printf("1.Register\n2.Login\n");
    int option;
    scanf("%d", &option);
    fflush(stdin);
    switch(option)
    {
        case REGISTER:
            Register_request(sockfd);
            break;
        case LOGIN:
            Login_request(sockfd);
            break;
        default:
            break;
    }

    if(LoginSuccess) {
        printf("Select an option to continue:\n3. Chat\n4. File Transfer\n5. Exit\n");
        scanf("%d", &option);
        fflush(stdin);
        switch(option){
            case CHAT:
                Chat_request(sockfd);
                break;
            case SENDFILE:
                SendFile_request(sockfd);
                break;
            case EXIT:
                close(sockfd);
                exit(1);
            default:
                break;
        }
    }
    else {
        close(sockfd);
        exit(1);
    }

    close(sockfd);
    pthread_cancel(tid);
    return 0;
}

void Register_request(int fd)
{
    setPack(&pack_send, REGISTER, 0, "", "");
    if(send(fd,&pack_send,sizeof(pack_send),0) == -1)
    {
        perror("send error");
        exit(1);
    }
    //sleep(1);
    printf("Username: ");
    scanf("%s",account.UserID);
    printf("Password: ");
    scanf("%d", &account.Passwd);
    if(send(fd,&account, sizeof(account),0) == -1)
    {
        perror("send error");
        exit(1);
    }
    //sleep(1);
    if(recv(fd, &pack_recv, sizeof(pack_recv), 0) == -1)
    {
        perror("recv error");
        exit(1);
    }
    printf("%s\n", pack_recv.Msg);
    if(strcmp(pack_recv.Msg,"Registered!") == 0) {
        printf("Successfully registered!\n");
        LoginSuccess = 1;
    }
}

void Login_request(int fd)
{
    printf("Username: ");
    scanf("%s",account.UserID);
    printf("Password: ");
    scanf("%d", &account.Passwd);
    setPack(&pack_send, 2, 0, account.UserID, "");
    if(send(fd,&pack_send, sizeof(pack_send), 0) == -1)
    {
        perror("send error");
        exit(1);
    }
    //sleep(1);
    if(send(fd,&account,sizeof(account),0) == -1)
    {
        perror("send error");
        exit(1);
    }
    //sleep(1);
    if(recv(fd, &pack_recv, sizeof(pack_recv),0) == -1)
    {
        perror("recv error");
        exit(1);
    }
    printf("%s\n", pack_recv.Msg);
    if(strcmp(pack_recv.Msg,"Login Successfully!") == 0)
        LoginSuccess = 1;
    else{
        LoginSuccess = 0;
    }
}

void Chat_request(int fd)
{
    int option;
    //sleep(1);
    setPack(&pack_send, CHAT, 0, "", "");
    if(send(fd, &pack_send, sizeof(pack_send), 0) == -1)
    {
        perror("send error");
        exit(1);
    }
    //sleep(1);
    printf("In which way do you like to chat?\n1. Chat with an online user.\n2. Chat in the group.\n");
    scanf("%d", &option);
    send(fd, &option, sizeof(option), 0);
    switch (option)
    {
        case 1:
            Chat_p2p(fd);
            break;
        case 2:
            break;
        default:
            printf("Wrong Option!\n");
            exit(1);
    }

    if (pthread_create(&tid, NULL, (void *) pthread_function, (void *) &fd) != 0)
        printf("create thread error\n");
    //不断发送聊天内容
    while(1){
        InitPack(&pack_send);
        char tmp[1500];
        fflush(stdout);
        fflush(stdin);
        fgets(tmp, 1500, stdin);
        tmp[1499] = '\0';
        if(tmp[0] == '\n')
            continue;
        setPack(&pack_send, CHAT, strlen(tmp),account.UserID,tmp);
        if(send(fd,&pack_send, sizeof(pack_send),0)==-1){
            printf("send error");
            exit(1);
        }
        //sleep(1);
    }

}

void SendFile_request(int fd)
{

}

void Chat_p2p(int fd)
{

    int j = 0, i = 0;
    int choice;
    char userlist[140];
    char* plen = userlist;
    recv(fd, userlist, 140, 0);
    for(i = 0; i < 5; i++)
    {
        memcpy(&accntlist[i], plen, 28);
        plen += 28;
    }

    printf("Choose one online user below to chat with:\n");

    for(i = 0; i < 5; i++)
    {
        if(accntlist[i].accntfd != -1)
            printf("%d. %s\n", i+1, accntlist[i].UserID);
    }
    scanf("%d",&choice);
    send(fd, &choice, sizeof(choice), 0);
}

//不断接收聊天内容
void pthread_function(void* sock_fd)
{
    int sockfd=*(int*)sock_fd;
    while(1) {
        if(recv(sockfd, &pack_recv, sizeof(pack_recv), 0)==-1)
        {
            printf("recv error");
            exit(1);
        }
        if(pack_recv.PacketType == CHAT)
        {
            if(strcmp(pack_recv.UserName,account.UserID) != 0)
            printf("%s: %s", pack_recv.UserName, pack_recv.Msg);
        }
    }
}
