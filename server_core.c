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
 * SERVER CORE
 * ===============================
 */
#include "server_core.h"

int check_file_request(char *filename);
int send_file(int skt, char *filename, int *flag_done);


void handle_file_req(int skt)
{
    char filename[FILENAMELEN];

    netcomm_t inbox = netfile_inbox_init(FILENAMELEN);
    char *client_cmd = NULL;
    int flag_done = 0;

    for(;;)
    {
        //set a max waiting time for the request to be received from client
        netfile_inbox_enable_timer(&inbox,MAX_REQ_WAIT_TIME);
        client_cmd = netfile_recv_msg(skt,&inbox); 
        printf("PID %d -> COMMAND: %s", getpid(), client_cmd);

        if(client_cmd == NULL)
            break;
        else if(strcmp(client_cmd,"")==0)
        {
            //in case of timeout, close connection with the client
            //or in case of RST packet detected after the file transfer has been completed 
            if(flag_done == 1)
                printf("\nPID %d -> STATUS: BROKEN PIPE. files were not sent\n", getpid());

            //in case of timeout, close connection with the client
            printf("\nPID %d -> client disconnected.\n", getpid());
            break;
        }

        if(sscanf(client_cmd,"GET %s\r\n",filename)==1){
            //received a file request
            flag_done = 0;
            if(send_file(skt,filename,&flag_done)==0)
                printf("PID %d -> %s sent!\n", getpid(), filename);
            else{
                netfile_send_msg(skt,ERR_MSG,NULL);
                printf("PID %d -> err_msg sent.\n", getpid());
                break;
            }
        } else if(strcmp(client_cmd,QUIT_MSG)==0)
            break;
        
        strcpy(client_cmd,"");
    }
    close(skt);
    netfile_inbox_close(inbox);
}

int send_file(int skt, char *filename, int *flag_done)
{
    FILE *fp;
    if(check_file_request(filename)==-1){
    	printf("PID %d -> send_file(): access not granted.\n",getpid());
        return -1;
    }


    fp=fopen(filename,"r");
    if(fp==NULL){
        printf("PID %d -> send_file(): cannot open file.\n",getpid());
        return -1;
    }

    netfile_t userfile = netfile_init(skt,fp);

    if(netfile_send_msg(skt,OK_MSG,NULL)==-1){
        printf("PID %d -> send_file(): cannot send ok msg.\n",getpid());
        return -1;
    }

    if(netfile_send(&userfile,BUFSIZE)!=0){
        printf("PID %d -> send_file(): %s\n", getpid(), netfile_error_info(userfile));
        return -1;
    }

    fclose(fp);

    printf("PID %d -> STATUS: %s\n", getpid(), netfile_error_info(userfile));
    //note: DONE can be a false positive. because a RST packet from the client
    //could be detected only after the server has completed the file transfer 
    //(while instead client could have been aborted before it has received the file)
    //to check this condition we set the flag_done option and see if a QUIT is received.
    *flag_done = 1;

    netfile_close(userfile);
    return 0;
}

int check_file_request(char *filename){
	//check that the client is requesting a file in the 
	//same directory where the executable of the server is located
	int i;
	for(i=0;filename[i]!='\0' && filename[i]!='\r' && filename[i]!='\n';i++){
		if(filename[i] == '\\' || filename[i] == '/')
			return -1;
	}

	return 0;
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