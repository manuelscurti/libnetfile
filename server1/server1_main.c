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
 * SEQUENTIAL SERVER IMPLEMENTATION
 * ===============================
 */
#include "./../server_core.h"

char *prog_name;


void signal_handler(int sig) {

    switch(sig){

    case SIGPIPE:
        break;

    }
}


int main (int argc, char *argv[])
{
    int s, answer_socket;
    struct sockaddr_in caddr;
    socklen_t addrlen;

    prog_name = argv[0];
    if(argc!=2)
    {
        printf("Usage: %s <port num>\n",prog_name);
        exit(1);
    }

    answer_socket = setup_server(argv[1]); //returns initialized socket
    printf("*** SERVER ***\n");

    //registering the signal handler
    struct sigaction sa;
    sa.sa_handler = &signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    //If SA_RESTART is set, returning from a handler resumes the library function. If the flag is clear, returning from a handler makes the function fail
    if (sigaction(SIGPIPE, &sa, 0) == -1) { 
        printf("Error: cannot handle SIGPIPE.\n");
        exit(1);
    }
    

    for(;;)
    {
        addrlen = sizeof(struct sockaddr_in);
        s = Accept(answer_socket, (struct sockaddr *) &caddr, &addrlen);

        showAddr("Processing CLIENT at",&caddr);
        handle_file_req(s);
        showAddr("Done with CLIENT at",&caddr);
    }
}