/***********************************************************************
* LIBNETFILE
*
* AUTHOR: MANUEL SCURTI 251175
***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/stat.h>
#include "sockwrap.h"
#include "errlib.h"
#include "libnetfile.h"

#define MSG_MAX 256
#define ERRVAL -1
#define SUCCESSVAL 0

/*
 * ERROR CODES DEFINITIONS 
 *  used by error_handler function
 */
#define NO_CONNECTION_ERR 600
#define NEGATIVE_RESPONSE_ERR 601 /* -ERR received */
#define FILE_STAT_ERR 602
#define FILE_IO_ERR 603
#define FILE_TRANSFER_ERROR 606 
#define OPERATION_SUCCESS 200 /* not an error */
#define MEMORY_ERR 604 /* e.g. malloc fail */
#define TIMEOUT_ERR 605 /* timeout expired */

//#define _LIBNETFILE_DEBUG //comment this line for release version
#define READLINE_OPT 1 //this option tells to use readline instead read in recv methods

struct netfile
{
	FILE *fp; //file requested
	int socket; //connection handler
	char *err_status; //used by netfile_error_handler
	uint32_t fsize;      
	uint32_t ftimestamp;
    time_t timerlength; //if > 0, timer is enabled and every recv method will set a timer to this value  
};

struct netcomm
{
    char *msg; 
    size_t size;
    time_t timerlength; //if > 0, timer is enabled and every recv method will set a timer to this value 
};


///////////////////////////////////////////////
//////////// PRIVATE METHODS //////////////////
///////////////////////////////////////////////

/* set method to notify the user of possible errors occurred during file transfer */
void netfile_error_handler(netfile_t *userfile, int code); //error code is passed by the function where the error occured


/* netfile read methods are used to centralize the socket methods involved when receiving data
 * in this way we use readline_unbuffered when we want to receive a protocol message
 * or a standard unix read otherwise.
 * to choose among the two options we specify option parameter
 *      0 -> standard unix read
 *      READLINE_OPT -> readline_unbuffered (that is a function inside sockwrap.h)
 */
ssize_t netfile_std_read(int fd, void *buf, size_t count, int option); /* invoked when no timer is set */
ssize_t netfile_crono_read(int fd, void *buf, size_t count, int option, time_t timerlength);

/* 
 * wrapper method of the std and crono read methods. 
 * based on timerlength value the corresponding function is called 
 */
ssize_t netfile_read(int fd, void *buf, size_t count, int option, time_t timerlength){ 
    if(timerlength > 0)
        return netfile_crono_read(fd, buf, count, option, timerlength);
    
    return netfile_std_read(fd, buf, count, 0);
}

ssize_t netfile_crono_read(int fd, void *buf, size_t count, int option, time_t timerlength){
    fd_set cset;
    struct timeval tval;
    int n;
    ssize_t numRecv;

    if(timerlength <= 0)
        timerlength = 0;

    FD_ZERO(&cset);
    FD_SET(fd, &cset);
    tval.tv_sec = timerlength;
    tval.tv_usec = 0;
    n = select(FD_SETSIZE, &cset, NULL, NULL, &tval);
    if (n > 0)
        numRecv = netfile_std_read(fd,buf,count,option);
    else 
        numRecv = -1; //timeout

    return numRecv;
}

ssize_t netfile_std_read(int fd, void *buf, size_t count, int option){
    ssize_t numRecv;

    /* OPTIONS: { READLINE_OPT: used when we're reading a protocol message } */
    switch(option)
    {
        case READLINE_OPT:
            numRecv = readline_unbuffered(fd,buf,count);
            break;
        default:
            numRecv = read(fd, buf, count); 
    }

    return numRecv;
}

void netfile_enable_timer(netfile_t *userfile, time_t timerlength){
    if(timerlength > 0)
        (*userfile)->timerlength = timerlength;
}

void netfile_disable_timer(netfile_t *userfile){
    (*userfile)->timerlength = 0;
}

void netfile_inbox_enable_timer(netcomm_t *inbox, time_t timerlength){
    if(timerlength > 0)
        (*inbox)->timerlength = timerlength;
}

void netfile_inbox_disable_timer(netcomm_t *inbox){
    (*inbox)->timerlength = 0;
}


void netfile_error_handler(netfile_t *userfile, int code){
    netfile_t temp = *userfile;

    switch (code)
    {
    case 600:
        //server responded -ERR to a file request
        strcpy(temp->err_status,"NO_CONNECTION_ERR");
        break;

    case 601:
        strcpy(temp->err_status,"NEGATIVE_RESPONSE_ERR");
        break;

    case 602:
        strcpy(temp->err_status,"NO_FILE_STAT_ERR");
        break;

    case 603:
        strcpy(temp->err_status,"FILE_IO_ERR");
        break;

    case 200:
        strcpy(temp->err_status,"DONE");
        break;

    case 604:
        strcpy(temp->err_status,"MEMORY_ERR");
        break;

    case 605:
        strcpy(temp->err_status,"TIMEOUT_ERR");
        break;
    case 606:
        strcpy(temp->err_status,"FILE_TRANSFER_ERROR");
        break;
    default:
       sprintf(temp->err_status,"ERROR_NOT_HANDLED %d",code);
    }
}

///////////////////////////////////////////////
//////////// PUBLIC METHODS //////////////////
///////////////////////////////////////////////

/* 
 * NETFILE DESCRIPTOR METHODS
 */

netfile_t netfile_init(int socket, FILE *fp){

	netfile_t file = (netfile_t)malloc(sizeof(struct netfile)); 

	if((file->err_status = (char *)malloc(MSG_MAX * sizeof(char)))==NULL){
        #ifdef _LIBNETFILE_DEBUG
		printf("netfile_init(): memory error\n");
        #endif
		return NULL;
	}

    file->err_status[0] = '\0';
	file->socket = socket;
	file->fp = fp;
	file->fsize = -1;
	file->ftimestamp = -1; // will be replaced after file transfer is completed
    file->timerlength = 0; //default value = disabled

	return file;
}

void netfile_close(netfile_t userfile){
    char *temp = userfile->err_status;
    free(temp);
    userfile->err_status = NULL;

    free(userfile);
}

int netfile_send(netfile_t *userfile, size_t buffer_size){

    /* SENDING FILE SIZE AND LAST MODIFICATION TIMESTAMP */
    int fd = fileno((*userfile)->fp); //takes file descriptor from file pointer
    struct stat fileStat;
    if(fstat(fd,&fileStat) < 0){
        #ifdef _LIBNETFILE_DEBUG
        printf("netfile_send(): error while retreiving file stats.\n");
        #endif
        netfile_error_handler(userfile,FILE_STAT_ERR);
        return ERRVAL;
    }

    off_t fsize = fileStat.st_size; //retrieve filesize
    time_t ftimestamp = fileStat.st_mtime; //retrieve timestamp
    /* adjust for sending on the net */
    uint32_t fsize32 = htonl(fsize); 
    uint32_t ftimestamp32 = htonl(ftimestamp);

    /* SENDING FILESIZE */
    if(sendn((*userfile)->socket,&fsize32, sizeof(uint32_t), 0) < 0){
        #ifdef _LIBNETFILE_DEBUG
        printf("netfile_send(): error while sending file size.\n");
        #endif
        netfile_error_handler(userfile,FILE_IO_ERR);
        return ERRVAL;
    }

    /* SENDING LAST MODIFICATION TIMESTAMP */
    if(sendn((*userfile)->socket,&ftimestamp32, sizeof(uint32_t), 0) < 0){
        #ifdef _LIBNETFILE_DEBUG
        printf("netfile_send(): error while sending file timestamp.\n");
        #endif
        netfile_error_handler(userfile,FILE_IO_ERR);
        return ERRVAL;
    }

    /* BUFFER INITIALIZATION */
    char *s_buf; //send_buf

    if(buffer_size <= 0)
        buffer_size = BUFSIZE;

    if((s_buf = (char *)malloc(buffer_size * sizeof(char)))==NULL){
        #ifdef _LIBNETFILE_DEBUG
        printf("netfile_send(): memory error.\n");
        #endif
        netfile_error_handler(userfile, MEMORY_ERR);
        return ERRVAL;
    }

    /* SENDING FILE CONTENTS */
    off_t bytes_left = fsize, file_ptr = 0;
    size_t numRead;
    ssize_t numSent;
   
    while (bytes_left > 0)
    {
        fseek((*userfile)->fp, file_ptr, SEEK_SET); 
        numRead = fread(s_buf, sizeof(char), buffer_size, (*userfile)->fp);
        if(ferror((*userfile)->fp)){
            #ifdef _LIBNETFILE_DEBUG
            printf("netfile_send(): error while reading file.\n");
            #endif
            netfile_error_handler(userfile,FILE_IO_ERR); 
            free(s_buf);
            s_buf = NULL;
            return ERRVAL;
        }

        #ifdef _LIBNETFILE_DEBUG
        printf("bytes_left_sender: %zu\n",bytes_left);
        printf("numRead: %zu\n",numRead);
        #endif

        if(numRead > 0){
            numSent = writen((*userfile)->socket, s_buf, numRead);
            if(numSent != numRead){
                #ifdef _LIBNETFILE_DEBUG
                printf("netfile_send(): error while sending file contents.\n");
                #endif
                netfile_error_handler(userfile,FILE_IO_ERR);
                free(s_buf);
                s_buf = NULL;
                return ERRVAL;
            }
            #ifdef _LIBNETFILE_DEBUG
            else
                printf("data sent numSent == numRead\n");


                printf("numSent: %zu\n",numSent);
            #endif
        }

        bytes_left -= numSent;
        file_ptr += numSent;
    }

    /* RELEASE RESOURCES AND NOTIFY SUCCESS STATE */
    free(s_buf);
    s_buf = NULL;

    #ifdef _LIBNETFILE_DEBUG
    printf("Successfully sent file.\n");
    #endif
    netfile_error_handler(userfile, OPERATION_SUCCESS);

    return SUCCESSVAL;
}

int netfile_recv(netfile_t *userfile, size_t buffer_size){

    /* RECEIVE FILE SIZE */
    uint32_t file_size_n,fsize;
    
    if(netfile_read((*userfile)->socket, &file_size_n, sizeof(uint32_t),0,(*userfile)->timerlength) < 0)
    {
        #ifdef _LIBNETFILE_DEBUG
        printf("netfile_recv(): timeout while receiving file size.\n");
        #endif
        netfile_error_handler(userfile,TIMEOUT_ERR);
        return ERRVAL;
    }

    fsize = ntohl(file_size_n); //adjust format 
    (*userfile)->fsize = fsize; //and store to the netfile descriptor
    #ifdef _LIBNETFILE_DEBUG
    printf("FILE SIZE: %"PRIu32"\n",fsize);
    #endif

    /* RECEIVE FILE TIMESTAMP */
    uint32_t file_timestamp_n,file_timestamp; 
    
    if(netfile_read((*userfile)->socket, &file_timestamp_n, sizeof(uint32_t),0,(*userfile)->timerlength) < 0)
    {
        #ifdef _LIBNETFILE_DEBUG
        printf("netfile_recv(): timeout while receiving file timestamp.\n");
        #endif
        netfile_error_handler(userfile,TIMEOUT_ERR);
        return ERRVAL;
    }

    file_timestamp = ntohl(file_timestamp_n); //adjust format
    (*userfile)->ftimestamp = file_timestamp; //and store to the netfile descriptor
    #ifdef _LIBNETFILE_DEBUG
    printf("FILE TIMESTAMP: %"PRIu32"\n",file_timestamp);
    #endif

    /* BUFFER INITIALIZATION */
    char *r_buf; //receive buffer

    if(buffer_size <= 0)
        buffer_size = BUFSIZE;

    if((r_buf = (char *)malloc(buffer_size * sizeof(char)))==NULL){
        #ifdef _LIBNETFILE_DEBUG
        printf("netfile_recv(): memory error.\n");
        #endif
        netfile_error_handler(userfile, MEMORY_ERR);
        return ERRVAL;
    }

    /* RECEIVING FILE CONTENTS */
    off_t bytes_left = fsize;
    ssize_t numRecv;
    size_t numWritten;

    while (bytes_left > 0)
    {

        #ifdef _LIBNETFILE_DEBUG
        printf("bytes_left_receiver: %zu\n",bytes_left);
        #endif

        numRecv = netfile_read((*userfile)->socket, r_buf, buffer_size, 0, (*userfile)->timerlength);
        if(numRecv < 0)
        {
            #ifdef _LIBNETFILE_DEBUG
            printf("netfile_recv(): read error.\n");
            #endif
            netfile_error_handler(userfile,FILE_TRANSFER_ERROR);
            free(r_buf);
            r_buf = NULL;
            return ERRVAL;
        }
        
        #ifdef _LIBNETFILE_DEBUG
        printf("numRecv: %zu\n",numRecv);
        #endif

        if(numRecv > 0)
        {
            //fseek(fp,file_ptr,SEEK_SET);
            numWritten = fwrite(r_buf,sizeof(char),numRecv,(*userfile)->fp);
            if(numWritten != numRecv)
            {
                #ifdef _LIBNETFILE_DEBUG
                printf("netfile_recv(): error while writing.\n");
                #endif
                netfile_error_handler(userfile,FILE_TRANSFER_ERROR);
                free(r_buf);
                r_buf = NULL;
                return ERRVAL;
            }
            #ifdef _LIBNETFILE_DEBUG
            printf("numWritten: %zu\n",numWritten);
            #endif
        }

        bytes_left -= numWritten;
        if(numWritten == 0) // avoids situation in which the server stops working and sending data
        {
            //if this condition is true it means that 
            //  1) numRecv is not > 0 -> network error (e.g. server went down while sending data)
            //  or
            //  2) numWritten = 0 -> I/O error (e.g. memory problems, disk failure)
            #ifdef _LIBNETFILE_DEBUG
            printf("netfile_recv(): cannot complete file transfer.\n");
            #endif
            netfile_error_handler(userfile,FILE_TRANSFER_ERROR);
            free(r_buf);
            r_buf = NULL;
            return ERRVAL;
        }

        numWritten = 0;
    }

    /* RELEASE RESOURCES AND NOTIFY SUCCESS TO THE NETFILE DESCRIPTOR */
    free(r_buf);
    r_buf = NULL;

    #ifdef _LIBNETFILE_DEBUG
    printf("Successfully sent file.\n");
    #endif

    netfile_error_handler(userfile, OPERATION_SUCCESS);
    return SUCCESSVAL;
}

uint32_t netfile_get_size(netfile_t userfile){
    return userfile->fsize;
}

uint32_t netfile_get_timestamp(netfile_t userfile){
    return userfile->ftimestamp;
}

/* 
 * INBOX METHODS
 */ 

netcomm_t netfile_inbox_init(size_t dim){

    netcomm_t inbox = (netcomm_t)malloc(sizeof(struct netcomm));

    inbox->msg = (char *)malloc(dim*sizeof(char));
    if(inbox->msg == NULL){
        #ifdef _LIBNETFILE_DEBUG
        printf("netfile_setup_inbox(): Memory error.\n");
        #endif
        return NULL;
    }
    inbox->size = dim;
    inbox->timerlength = 0;

    return inbox;
}

void netfile_inbox_close(netcomm_t inbox){
    char *temp = inbox->msg;
    free(temp);
    inbox->msg = NULL;

    free(inbox);
}

int netfile_send_msg(int socket, char *msg, char *filename){
    char msg_wrapper[MSG_MAX];
    ssize_t numSent;
    size_t msg_len;

    if(filename!=NULL)
        sprintf(msg_wrapper,FILE_MSG" %s\r\n",filename); //compose request file message
    else
        strcpy(msg_wrapper,msg);

    msg_len = strlen(msg_wrapper)*sizeof(char);

    numSent = sendn(socket, msg_wrapper, msg_len, 0);

    if(numSent != msg_len)
        return ERRVAL;

    return SUCCESSVAL;
}

char *netfile_recv_msg(int socket, netcomm_t *inbox){
    size_t len;

    len = netfile_read(socket,(*inbox)->msg,(*inbox)->size,READLINE_OPT,(*inbox)->timerlength);
    if (len < 0)
    {
        #ifdef _LIBNETFILE_DEBUG
        printf("Read error\n");
        #endif
        return "";
    }
    else if (len == 0)
    {
        #ifdef _LIBNETFILE_DEBUG
        printf("Connection closed by party on socket %d\n",socket);
        #endif
        return "";
    } else if(len < (*inbox)->size)
        (*inbox)->msg[len] = '\0';
    else //length of received message exceeds buffer size
        (*inbox)->msg[(*inbox)->size-1] = '\0'; //in this case message is truncated

    #ifdef _LIBNETFILE_DEBUG
    printf("rcv: %s\n",(*inbox)->msg);
    #endif

    return (*inbox)->msg;
}


/*
 * ERROR HANDLING METHODS
 */

char *netfile_error_info(netfile_t userfile){
    return userfile->err_status;
}