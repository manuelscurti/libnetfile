/***********************************************************************
* LIBNETFILE
*   a NETFILE is a file descriptor associated with a socket. with this library you define a NETFILE to declare
*   where you will get that file and where it has to be saved in the local file system.
*   a NETCOMM is a structure to manage protocol messages before and after the start of the file transfer.
*   This library implements the following protocol for file transfer.
* DESCRIPTION OF THE PROTOCOL:
*
*		client --> GET filename\r\n
*		server --> +OK file_size file_timestamp file_contents
*					otherwise
*				   -ERR\r\n
*		client --> GET another_filename\r\n
*					or
*				   QUIT\r\n (to close the communication)
*
*
* AUTHOR: MANUEL SCURTI 251175
***********************************************************************/

#ifndef _LIBNETFILE
#define _LIBNETFILE

/* STANDARD PROTOCOL MESSAGES */
#define FILE_MSG "GET"
#define ERR_MSG "-ERR\r\n"
#define OK_MSG "+OK\r\n"
#define QUIT_MSG "QUIT\r\n"

#define BUFSIZE 4096 /* used both for send_buffer and receive_buffer size as a DEFAULT value */

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
	also in this structure there is a timer duration to manage timeout errors
*/
typedef struct netcomm * netcomm_t;

/* in a standard client/server program that uses the protocol descripted before
you will need both an inbox to manage protocol standard messages 
and a netfile descriptor to specify where to store/read the file to be recvd/sent */



/* 
 * NETFILE DESCRIPTOR METHODS
 */

/**
 * @brief  Start the netfile descriptor and bind a socket and a file descriptor to it
 *
 * @param[in]  socket: set socket in which the file transfer will take place
 * @param[in]  fp: set the file pointer where the data will be stored(client)/read(server)
 *
 * @return netfile_t
 *         NULL
 *         
 */
netfile_t netfile_init(int socket, FILE *fp);


/**
 * @brief  Free resources associated with the netfile descriptor. by the way, it is up to the user to
 *			close the socket and close the file descriptor
 *
 * @param[in]  userfile: a netfile descriptor
 *         
 */
void netfile_close(netfile_t userfile); /* releases resources used by a netfile descriptor */

/**
 * @brief  Send a file.
 *			to be used after netfile_init, so that you can set a file descriptor and a socket.
 *
 * @param[in]  userfile: initialized by netfile_init
 * @param[in]  buffer_size: how many bytes at a time. if no valid size is specified, the default buffer size will be used
 *
 *
 * @return ERRVAL -> for detailed error reporting, use netfile_error_info
 *         SUCCESSVAL 
 *         
 */
int netfile_send(netfile_t *userfile, size_t buffer_size); /* send in order: file size, file timestamp and file content */


/**
 * @brief  Receive a file.
 *			to be used after netfile_init, so that you can set a file descriptor and a socket.
 *
 * @param[in]  userfile: initialized by netfile_init
 * @param[in]  buffer_size: how many bytes at a time. if no valid size is specified, the default buffer size will be used
 *
 * @return ERRVAL -> for detailed error reporting, use netfile_error_info
 *         SUCCESSVAL 
 *         
 */
int netfile_recv(netfile_t *userfile, size_t buffer_size); /* receive in order: file size, file timestamp and file content */

/* methods to retrieve file stats AFTER file transfer is completed */
uint32_t netfile_get_size(netfile_t userfile); 
uint32_t netfile_get_timestamp(netfile_t userfile); 

///// end NETFILE DESCRIPTOR METHODS //////




/* 
 * INBOX METHODS
 */ 

/**
 * @brief  Start the netcomm descriptor with a buffer of dim max size
 *
 * @param[in]  dim: set socket in which the file transfer will take place
 *
 * @return netcomm_t
 *         NULL
 *         
 */
netcomm_t netfile_inbox_init(size_t dim); 

/**
 * @brief  Free resources associated with the netcomm descriptor.
 *
 * @param[in]  inbox: a netcomm descriptor
 *         
 */
void netfile_inbox_close(netcomm_t inbox);


/**
 * @brief  Send a protocol message.
 *			PROTOCOL MESSAGES:  FILE_MSG
 *								ERR_MSG
 *								OK_MSG
 *								QUIT_MSG
 *
 * @param[in]  socket: where to send the message
 * @param[in]  msg: message to be sent
 * @optional_param[in]  filename: OPTIONAL PARAMETER. to be used only if the message is a FILE_MSG otherwise set to NULL
 *
 *
 * @return ERRVAL
 *         SUCCESSVAL 
 *         
 */
int netfile_send_msg(int socket, char *msg, char *filename);

/**
 * @brief  Send a protocol message.
 *
 * @param[in]  socket: where to send the message
 * @param[in]  msg: message to be sent
 * @optional_param[in]  filename: OPTIONAL PARAMETER. to be used only if the message is a FILE_MSG otherwise set to NULL
 *
 *
 * @return message string
 *         "" (empty string)
 */
char *netfile_recv_msg(int socket, netcomm_t *inbox);


///// end INBOX METHODS ////// 

/*
 * ERROR HANDLING METHODS
 */

/**
 * @brief  Retrieve info on error/success state. to be used AFTER netfile_recv/netfile_send 
 *			TODO: actually error handling on protocol messages are not supported. 
 *
 * @param[in]  userfile: netfile descriptor
 *
 * @return error string
 *         "" (empty string)
 */
char *netfile_error_info(netfile_t userfile); /* status informations about file transfer. not real time! */

///// end ERROR HANDLING METHODS /////




/* TIMERS 
 *	timers can be used to set a max wait time for a response message.
 *	they can be used when calling functions to receive data (netfile or netcomm messages).
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

/* timer methods enabler/disabler to be used before netfile_recv */
void netfile_enable_timer(netfile_t *userfile, time_t timerlength); //seconds
void netfile_disable_timer(netfile_t *userfile);

/* timer methods enabler/disabler to be used before netfile_recv_msg */
void netfile_inbox_enable_timer(netcomm_t *inbox, time_t timerlength);
void netfile_inbox_disable_timer(netcomm_t *inbox);

////// end TIMER METHODS //////

#endif
