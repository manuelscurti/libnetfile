/*
 * ===============================
 * AUTHOR       : Manuel Scurti
 * STUDENT ID   : 251175
 *
 * File transfer mechanism
 *      Client -> GET filename\r\n
 *      Server -> +OK B1 B2 B3 B4 T1 T2 T3 T4 file content.
 *              or -ERR
 *
 * SERVER SIDE
 * ===============================
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/stat.h>
#include "./../errlib.h"
#include "./../sockwrap.h"
#include "./../libnetfile.h"

#define BUFSIZE 4096
//#define TRANSFERBUFFER 256 /* bytes */
#define FILENAMELEN 256
#define ERRSTR "-ERR\r\n"
#define ERRSTRLEN 6
#define BACKLOG 4
#define _DEBUG

char *prog_name;

int setup_server(char* port_num);
void handle_file_req(int socket);
int send_file(int skt, char *filename);
void signal_handler(int sig);

int main (int argc, char *argv[])
{
    int answer_socket,s;
    struct sockaddr_in caddr;
    socklen_t addrlen;

    prog_name = argv[0];
    if(argc!=2)
    {
        printf("Usage: %s <port num>\n",prog_name);
        exit(1);
    }

    answer_socket = setup_server(argv[1]); //returns initialized socket
    printf("Server is running.\n");

    //registering the signal handler
    struct sigaction sa;
    sa.sa_handler = &signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    //If SA_RESTART is set, returning from a handler resumes the library function. If the flag is clear, returning from a handler makes the function fail
    if (sigaction(SIGPIPE, &sa, 0) == -1) { 
        printf("Error: cannot handle SIGPIPE handler.\n");
        exit(1);
    }


    for(;;)
    {
        addrlen = sizeof(struct sockaddr_in);
        s = Accept(answer_socket, (struct sockaddr *) &caddr, &addrlen);
        
        handle_file_req(s);
    }
}

int setup_server(char* port_num)
{
    int s; //socket container
    uint16_t port_n,port_h;
    struct sockaddr_in saddr;

    if (sscanf(port_num, "%" SCNu16, &port_h)!=1)
        err_sys("Invalid port number");
    port_n = htons(port_h);

    s = Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    bzero(&saddr, sizeof(saddr));
    saddr.sin_family      = AF_INET;
    saddr.sin_port        = port_n;
    saddr.sin_addr.s_addr = INADDR_ANY;
    Bind(s, (struct sockaddr *) &saddr, sizeof(saddr));

    /* listen */
    Listen(s, BACKLOG);

    return s;
}

void handle_file_req(int skt)
{
    char filename[FILENAMELEN];

    netcomm_t inbox = netfile_inbox_init(FILENAMELEN);
    char *client_cmd;

    for(;;)
    {
        printf("HANDLE LOOP\n");

        client_cmd = netfile_recv_msg(skt,&inbox);
        if(client_cmd == NULL)
            break;

        if(sscanf(client_cmd,"GET %s\r\n",filename)==1){
            //received a file request
            if(send_file(skt,filename)==0)
                printf("%s sent!\n", filename);
            else{
                netfile_send_msg(skt,ERR_MSG,NULL);
                printf("err_msg sent.\n");
            }
        } else if(strcmp(client_cmd,QUIT_MSG)==0){
            printf("Done with client.\n");
            break;
        }
    }
    close(skt);
    netfile_inbox_close(inbox);
}

int send_file(int skt, char *filename)
{
    FILE *fp;

    fp=fopen(filename,"r");
    if(fp==NULL){
        printf("send_file(): cannot open file.\n");
        return -1;
    }

    netfile_t userfile = netfile_init(skt,fp);

    if(netfile_send_msg(skt,OK_MSG,NULL)==-1)
        printf("send_file(): cannot send ok msg.\n");


    if(netfile_send(&userfile,BUFSIZE)!=0){
        printf("send_file(): %s\n", netfile_error_info(userfile));
        return -1;
    }

    fclose(fp);

    printf("err_state: %s\n", netfile_error_info(userfile));
    netfile_close(userfile);

    return 0;
}


void signal_handler(int sig) {

    switch(sig){

    case SIGPIPE:
        printf("SIGPIPE HANDLED\n");
        break;

    }
}