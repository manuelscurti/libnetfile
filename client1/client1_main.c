/*
 * ===============================
 * AUTHOR		: Manuel Scurti
 * STUDENT ID 	: 251175
 *
 * File transfer mechanism
 * 		Client -> GET filename\r\n
 * 		Server -> +OK B1 B2 B3 B4 T1 T2 T3 T4 file content.
 * 				or -ERR
 *
 * CLIENT SIDE
 * ===============================
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/stat.h>
#include "../sockwrap.h"
#include "../errlib.h"
#include "../libnetfile.h"

#define BUFSIZE 4096
#define REQMSG_LEN 6
#define FILENAMELEN 256
#define CLOSESTRING "QUIT\r\n"
#define TIMER_VAL 2


char *prog_name;

int connect_to(char *ip, char *portnum);
int receive_file(int skt, char *filename);
int make_request(int skt, char *filename);
void close_link(int skt);

int main (int argc, char *argv[])
{

	int link; //socket descriptor
	int i=3, outcome, completed=0 /* completed downloads */, to_download = argc-i;


	printf("*** USER CLIENT ***\n");
	prog_name = argv[0];
	if(argc < 4)
	{
		printf("Usage: %s <ip address> <port num> <filename> [...]\n",prog_name);
		exit(1);
	}

	/* CONNECTION PHASE */
	printf("Connecting to %s:%s\n",argv[1],argv[2]);
	link = connect_to(argv[1],argv[2]); //error messages and behaviours are handled by errlib
	printf("Connected.\n");


	/* REQUEST PHASE */
	while(i < argc)
	{
		outcome = receive_file(link, argv[i]);
		if(outcome != -1)
			completed++;
		else printf("File \"%s\" skipped.\n",argv[i]);
		i++;
	}


	printf("Completed %d/%d file requests.\n", completed, to_download);
	printf("Shutting down connection...\n");
	close_link(link);

	printf("Bye!\n");
	return 0;
}

int connect_to(char *ip, char *portnum)
{
    uint16_t port_n, port_h;	/* server port number (net/host ord) */

	int s; //returning socket
	struct sockaddr_in saddr;		/* server address structure */
    struct in_addr server_ip; 	/* server IP addr. structure */

	Inet_aton(ip, &server_ip);


	if (sscanf(portnum, "%" SCNu16, &port_h)!=1)
		err_quit("Invalid port number");
	port_n = htons(port_h);

	s = Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	bzero(&saddr, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_port = port_n;
	saddr.sin_addr = server_ip;

	Connect(s, (struct sockaddr *) &saddr, sizeof(saddr));

	return s;
}

int receive_file(int skt, char *filename)
{
	FILE *fp;
    netcomm_t inbox = netfile_inbox_init(FILENAMELEN); /* create an inbox for protocol messages */

	if((fp = fopen(filename,"w")) == NULL){
		printf("Unable to create file.\n");
		return -1;
	}
	//file is ready to be written

	//sending request
	if(netfile_send_msg(skt,FILE_MSG,filename)!=0){
        printf("Unable to send request.\n");
        return -1;
	}

	netfile_inbox_enable_timer(&inbox, TIMER_VAL);
	if(strcmp(netfile_recv_msg(skt,&inbox),OK_MSG)!=0){
		printf("Error message received.\n");
		return -1;
	}
	netfile_inbox_disable_timer(&inbox);

	netfile_t userfile = netfile_init(skt,fp); /* create a container for the file */

	netfile_enable_timer(&userfile, TIMER_VAL);
	if(netfile_recv(&userfile,BUFSIZE) == -1){
		printf("Error while receiving file.\n");
		return -1;
	}
	netfile_disable_timer(&userfile); //not strictly needed

    printf("err status: %s\n",netfile_error_info(userfile));
    printf("FILE SIZE: %"PRIu32"\n",netfile_get_size(userfile));
    printf("FILE TIMESTAMP: %"PRIu32"\n",netfile_get_timestamp(userfile));

    fclose(fp);
    netfile_close(userfile);
    netfile_inbox_close(inbox);

    return 0;
}

void close_link(int skt)
{
	netfile_send_msg(skt,QUIT_MSG,NULL);
	close(skt);
}
