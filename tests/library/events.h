#ifndef CWDAEMON_TEST_LIB_EVENTS_H
#define CWDAEMON_TEST_LIB_EVENTS_H




#include <pthread.h>
#include <time.h>

#include "process.h"




typedef enum {
	event_type_none = 0,                  /**< Indicates empty/invalid event. */
	event_type_morse_receive,             /**< Something was received as Morse code by Morse receiver observing a cwdevice. */
	event_type_client_socket_receive,     /**< Something was received by cwdaemon client over network socket from cwdaemon server. */
	event_type_request_exit,              /**< EXIT request has been sent to cwdaemon. */
	event_type_sigchld,                   /**< SIGCHLD received from child process. */
} event_type_t;




typedef struct {
	char string[64];
} event_morse_receive_t;




typedef struct {
	char string[64];
} event_client_socket_receive_t;




typedef struct {
	int wstatus;
} event_sigchld_t;




typedef struct {
	event_type_t event_type;     /**< Type of the event. */
	struct timespec tstamp;      /**< Time stamp of the event. */

	union {
		event_morse_receive_t morse_receive;          /**< Morse code received by observer of cwdevice. */
		event_client_socket_receive_t socket_receive; /**< Data received over socket by cwdaemon client. */
		event_sigchld_t sigchld;                     /**< Data collected by waitpid() in signal handler for SIGCHLD. */
	} u;
} event_t;




typedef struct {
	event_t events[20];
	int event_idx;          /**< Indicates first non-occupied slot in events[]. */
	pthread_mutex_t mutex;
} events_t;




/**
   @brief Pretty-print events to stderr

   @param[in] events Events structure to print
*/
void events_print(const events_t * events);




/**
   @brief Clear events structure

   This function can be used to erase old events from @p events structure.

   @param[in/out] events Events structure to clear
*/
void events_clear(events_t * events);




int events_insert_morse_receive_event(events_t * events, const char * buffer, struct timespec * last_character_receive_tstamp);
int events_insert_socket_receive_event(events_t * events, const char * receive_buffer);
int events_insert_sigchld_event(events_t * events, const child_exit_info_t * exit_info);




#endif /* #ifndef CWDAEMON_TEST_LIB_EVENTS_H */

