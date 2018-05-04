/***********************************************************************
* LIBNETFILE - TCP library for rapid prototyping of a file exchange system
*
* DESCRIPTION :
*       Simple library that implements the following protocol for file transfer.
*		client --> GET filename\r\n
*		server --> +OK file_size file_timestamp file_contents
*					otherwise
*				   -ERR\r\n
*		client --> GET another_filename\r\n
*					or
*				   QUIT\r\n (to close the communication)
*	
*
* AUTHOR: 	MANUEL SCURTI 251175
* EMAIL: 	manuel9scurti (at) gmail (dot) com
***********************************************************************/

#ifndef _LIBNETFILE
#define _LIBNETFILE

/* STANDARD PROTOCOL MESSAGES */
#define FILE_MSG "GET"
#define ERR_MSG "-ERR\r\n"
#define OK_MSG "+OK\r\n"
#define QUIT_MSG "QUIT\r\n"

#define BUFSIZE 4096 /* used both for send_buffer and receive_buffer size */

/* netfile_t is an enhanced file handler 
	which includes: 
		file descriptor, (open and close phase is leaved to the end user)
		connected socket,
		timer duration
		and other stuff useful to check errors or to retrieve info from file 
*/
typedef struct netfile * netfile_t;

/* netcomm_t is essentially a message inbox to collect protocol messages like +OK, -ERR, QUIT and GET 
	by using this structure you access to a more controlled way to receive and send messages.
	also in this structure there is a timer duration
*/
typedef struct netcomm * netcomm_t;

/* in a standard client/server program that uses this protocol 
you will need both an inbox to manage protocol standard messages 
and a netfile descriptor to specify where to store/read the file to be recvd/sent */

/* 
 * NETFILE DESCRIPTOR METHODS
 */
netfile_t netfile_init(int socket, FILE *fp);
void netfile_close(netfile_t userfile); /* releases resources used by a netfile descriptor */

int netfile_send(netfile_t *userfile, size_t buffer_size); /* send in order: file size, file timestamp and file content */
int netfile_recv(netfile_t *userfile, size_t buffer_size); /* receive in order: file size, file timestamp and file content */

/* methods to retrieve file stats AFTER file transfer is completed */
uint32_t netfile_get_size(netfile_t userfile); 
uint32_t netfile_get_timestamp(netfile_t userfile); 

char *netfile_error_info(netfile_t userfile); /* status informations about file transfer. not real time! */
/* end NETFILE DESCRIPTOR METHODS */


/* 
 * INBOX METHODS
 */ 
netcomm_t netfile_inbox_init(size_t dim); 
void netfile_inbox_close(netcomm_t inbox);

int netfile_send_msg(int socket, char *msg, char *filename);
char *netfile_recv_msg(int socket, netcomm_t *inbox);
/* end INBOX METHODS */ 



/* TIMERS 
 *	timers can be used to set a max wait time for a response message.
 *	they can be used when calling receiving data functions.
 * 	to do this, before netfile_recv_msg or netfile_recv 
 *	call for a function that enables a timer.
 *	if timer is set to zero, timer will still be disabled.
 *  NOTE THAT if you don't disable a timer and you call again a netfile_recv_msg
 *  or a netfile_recv you will again use a timer with the same value as specified 
 *	before while enabling the timer for the first time.
 *	to change the value of a timer just call again enable_timer. 
 *	
 *	example usage:
 *		netfile_enable_timer(&userfile,5); //set a timer of 5 seconds
 *		netfile_recv_msg(&userfile, BUF_SIZE); //if no byte is received in 5 seconds a timeout_err will occur
 *		
 */

/* timer methods to be set before netfile_recv */
void netfile_enable_timer(netfile_t *userfile, time_t timerlength); //seconds
void netfile_disable_timer(netfile_t *userfile);

/* timer methods to be set before netfile_recv_msg */
void netfile_inbox_enable_timer(netcomm_t *inbox, time_t timerlength);
void netfile_inbox_disable_timer(netcomm_t *inbox);


#endif
