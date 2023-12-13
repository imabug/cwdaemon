#ifndef H_CWDAEMON_SOCKET
#define H_CWDAEMON_SOCKET




#include <stdbool.h>
#include <stdint.h>




#include "cwdaemon.h"




bool    cwdaemon_initialize_socket(cwdaemon_t * cwdaemon, uint16_t network_port);
void    cwdaemon_close_socket(cwdaemon_t * cwdaemon);
ssize_t cwdaemon_sendto(cwdaemon_t * cwdaemon, const char * reply);
ssize_t cwdaemon_recvfrom(cwdaemon_t * cwdaemon, char * request, int size);




#endif /* #ifndef H_CWDAEMON_SOCKET */

