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
 * CONCURRENT SERVER IMPLEMENTATION
 * ===============================
 */
#include "./../server_core.h"
#include <signal.h>
#include <sys/wait.h>

#define UI_MESSAGE_MAX_LEN 128

char *prog_name;


void signal_handler(int sig) {

    switch(sig){

    case SIGCHLD:
        //zombie killer
        while (waitpid((pid_t)(-1), 0, WNOHANG) > 0) {} 
        // the reason to use waitpid instead of wait is 
        // because we can exploit WNOHANG option to avoid 
        // blocking the master process
        break;
    case SIGPIPE:
        break;
        
    }


}

int main (int argc, char *argv[])
{
    int answer_socket,s;
    struct sockaddr_in caddr;
    socklen_t addrlen;
    int child_pid;
    char ui_message[UI_MESSAGE_MAX_LEN];


    prog_name = argv[0];
    if(argc!=2)
    {
        printf("Usage: %s <port num>\n",prog_name);
        exit(1);
    }

    answer_socket = setup_server(argv[1]); //returns initialized socket
    printf("*** CONCURRENT SERVER ***\n");

    //registering the signal handler
    struct sigaction sa;
    sa.sa_handler = &signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    //If SA_RESTART is set, returning from a handler resumes the library function. If the flag is clear, returning from a handler makes the function fail
    //If SA_NOCLDSTOP is set, the system delivers the signal for a terminated child process but not for one that is stopped.
    if (sigaction(SIGCHLD, &sa, 0) == -1) { //sigaction (used here instead of signal() ) can block other signals until the current handler returns
        printf("Error: cannot handle SIGCHLD.\n");
        exit(1);
    }

    if (sigaction(SIGPIPE, &sa, 0) == -1) { 
        printf("Error: cannot handle SIGPIPE.\n");
        exit(1);
    }

    for(;;)
    {
        addrlen = sizeof(struct sockaddr_in);
        s = Accept(answer_socket, (struct sockaddr *) &caddr, &addrlen);

        if((child_pid = fork()) < 0){
            printf("fork() error.\n");
            close(s);
        } else if (child_pid > 0){
            close(s);
        }else{
            //child process that will handle the request
            close(answer_socket);
            
            sprintf(ui_message,"PID %d -> processing CLIENT at", getpid());
            showAddr(ui_message,&caddr);

            handle_file_req(s);
            
            sprintf(ui_message,"PID %d -> done with CLIENT at", getpid());
            showAddr(ui_message,&caddr);

            exit(0);
        }
    }
}


