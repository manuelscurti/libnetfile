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

#ifndef _SERVER_CORE
#define _SERVER_CORE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/stat.h>

#include "errlib.h"
#include "sockwrap.h"
#include "libnetfile.h"


#define FILENAMELEN 256
#define BACKLOG 4
#define MAX_REQ_WAIT_TIME 30 //seconds (amount of time in which we'll wait the client to send a GET request)

int setup_server(char* port_num);
void handle_file_req(int socket);

#endif