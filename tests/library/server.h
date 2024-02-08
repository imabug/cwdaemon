#ifndef CWDAEMON_TEST_LIB_SERVER_H
#define CWDAEMON_TEST_LIB_SERVER_H




#include <arpa/inet.h>
#include <stdbool.h>
#include <sys/types.h> /* pid_t */

#include <libcw.h>

#include "client.h"
#include "cwdevice_observer.h"




/* For now this structure doesn't allow for usage and tests of remote
   cwdaemon server. */
typedef struct server_t {
	pid_t pid;    /**< pid of local test instance of cwdaemon process. */
	int l4_port;  /**< Network port, on which cwdaemon server is available and listening. */
	int wstatus;  /**< Second argument to waitpid(). */
	char ip_address[INET6_ADDRSTRLEN]; /**< String representation of server's IP address. */
} server_t;




typedef struct {
	int tone;       /**< [Hz] Frequency of sound generated by cwdaemon server. */
	enum cw_audio_systems sound_system;
	bool nofork;             /* -n, --nofork; don't fork. */
	char cwdevice_name[16];  /* Name of a device in /dev/. The name does not include "/dev/" */
	int wpm;
	tty_pins_t tty_pins; /**< Configuration of pins of tty port to be used as cwdevice by cwdaemon. */

	/* IP address of machine where cwdaemon is available, If empty, local
	   IP will be used. */
	char l3_address[INET6_ADDRSTRLEN];

	/**
	   Layer 4 UDP port on which cwdaemon is listening. Passed to cwdaemon
	   through -p/--port command line option.

	   * -1: use zero as port number passed to cwdaemon (dirty special case
	        to test handling of invalid port number);
	   * 0: use random port when starting cwdaemon;
	   * positive value: use given port value when starting cwdaemon;

	   I'm using zero to signify random port, because this should be the
	   default testing method: to run a cwdaemon with any valid port number,
	   and zero is the easiest value to assign to this field.

	   I'm using 'int' type instead of 'in_port_t' type to allow testing
	   situations where an invalid port number (e.g. 65536) is passed to
	   cwdaemon. 'in_port_t' type would not allow that value.
	*/
	int l4_port;
} cwdaemon_opts_t;




/**
   @brief Start instance of cwdaemon server

   Currently the functional tests only deal with local test instance of
   cwdaemon server, so this function is starting a local cwdaemon process.

   @return 0 on success
   @return -1 on failure
*/
int server_start(const cwdaemon_opts_t * opts, server_t * server);




/**
   @brief Stop the local test instance of cwdaemon

   @param server Local server to be stopped
   @param client Client instance to use to communicate with the server

   @return 0 on success
   @return -1 on failure
*/
int local_server_stop(server_t * server, client_t * client);




#endif /* #ifndef CWDAEMON_TEST_LIB_SERVER_H */


