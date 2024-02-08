/*
 * This file is a part of cwdaemon project.
 *
 * Copyright (C) 2002 - 2005 Joop Stakenborg <pg4i@amsat.org>
 *		        and many authors, see the AUTHORS file.
 * Copyright (C) 2012 - 2024 Kamil Ignacak <acerion@wp.pl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Library General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */




/**
   Test of special cases for "-p"/"--port" command line option.

   In general there are several areas that can be tested when it comes to
   specifying network port for cwdaemon process. Only the last one of them is
   tested here.

    - using short option ("-p") vs. long option ("--port"). This is already
      covered by test library's get_option_port() function that selects one
      of the two forms at random.

    - specifying any valid port number (from valid range). This is already
      done by other functional tests: cwdaemon process is started with a port
      number that is randomly selected from the valid range.

    - not passing any command line option for port, allowing cwdaemon run
      with its default port. This is already done also by test library's
      get_option_port() function (see "explicit port argument" in the
      function).

    - trying to start cwdaemon with unusual port numbers, e.g. 0, 1, 1023.
      This is done in this test.
*/




#define _DEFAULT_SOURCE




#include "config.h"

/* For kill() on FreeBSD 13.2 */
#include <signal.h>
//#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
//#include <time.h>
//#include <unistd.h>

#include <errno.h>

#include "src/cwdaemon.h"
#include "tests/library/client.h"
//#include "tests/library/cwdevice_observer.h"
//#include "tests/library/cwdevice_observer_serial.h"
#include "tests/library/events.h"
#include "tests/library/log.h"
#include "tests/library/misc.h"
#include "tests/library/morse_receiver.h"
#include "tests/library/morse_receiver_utils.h"
#include "tests/library/random.h"
#include "tests/library/server.h"
//#include "tests/library/sleep.h"
#include "tests/library/socket.h"
#include "tests/library/test_env.h"
#include "tests/library/time_utils.h"




static child_exit_info_t g_child_exit_info;




typedef struct test_case_t {
	const char * description;  /** Human-readable description of the test case. */
	const char * full_message; /** Full text of message to be played by cwdaemon. */
	bool expected_fail;        /** Whether we expect cwdaemon to fail to start correctly due to invalid port number. */
	int port;                  /** Value of port passed to cwdaemon. */
} test_case_t;




static test_case_t g_test_cases[] = {
	{ .description = "failure case: port 0",        .full_message = "paris",  .expected_fail = true,   .port = -1 }, /* port == -1 will be interpreted by code in server.c as "pass port 0 to cwdaemon". */
	{ .description = "failure case: port 1",        .full_message = "paris",  .expected_fail = true,   .port = 1  },
	{ .description = "failure case: port MIN - 2",  .full_message = "paris",  .expected_fail = true,   .port = CWDAEMON_NETWORK_PORT_MIN - 2 },
	{ .description = "failure case: port MIN - 1",  .full_message = "paris",  .expected_fail = true,   .port = CWDAEMON_NETWORK_PORT_MIN - 1 },

	/* All valid ports between MIN and MAX are indirectly tested by other
	   functional tests that use random valid port. Here we just explicitly
	   test the MIN and MAX itself */
	{ .description = "success case: port MIN",      .full_message = "paris",  .expected_fail = false,  .port = CWDAEMON_NETWORK_PORT_MIN     },
	{ .description = "success case: port MAX",      .full_message = "paris",  .expected_fail = false,  .port = CWDAEMON_NETWORK_PORT_MAX     },

	{ .description = "failure case: port MAX + 1",  .full_message = "paris",  .expected_fail = true,   .port = CWDAEMON_NETWORK_PORT_MAX + 1 },
	{ .description = "failure case: port MAX + 2",  .full_message = "paris",  .expected_fail = true,   .port = CWDAEMON_NETWORK_PORT_MAX + 2 },
};




static int server_setup(server_t * server, const test_case_t * test_case, int * wpm);
static int testcase_setup(const server_t * server, client_t * client, morse_receiver_t * morse_receiver, int wpm);
static int testcase_run(const test_case_t * test_case, server_t * server, client_t * client, morse_receiver_t * morse_receiver, events_t * events);
static int testcase_teardown(client_t * client, morse_receiver_t * morse_receiver);
static int evaluate_events(const events_t * events, const test_case_t * test_case);




/*
  Since this test is starting a child process, we want to handle SIGCHLD
  signal.
*/
static void sighandler(int sig)
{
	if (SIGCHLD == sig) {
		g_child_exit_info.waitpid_retv = waitpid(g_child_exit_info.pid, &g_child_exit_info.wstatus, 0);
		clock_gettime(CLOCK_MONOTONIC, &g_child_exit_info.sigchld_timestamp);
	}
}




int main(void)
{
#if 0
	if (!test_env_is_usable(test_env_libcw_without_signals)) {
		test_log_err("Test: preconditions for test env are not met, exiting\n");
		exit(EXIT_FAILURE);
	}
#endif

	const uint32_t seed = cwdaemon_srandom(0);
	test_log_debug("Test: random seed: 0x%08x (%u)\n", seed, seed);

	signal(SIGCHLD, sighandler);


	const size_t n_test_cases = sizeof (g_test_cases) / sizeof (g_test_cases[0]);


	for (size_t i = 0; i < n_test_cases; i++) {
		const test_case_t * test_case = &g_test_cases[i];

		test_log_newline(); /* Visual separator. */
		test_log_info("Test: starting test case %zu / %zu: [%s]\n", i + 1, n_test_cases, test_case->description);

		bool failure = false;
		events_t events = { .mutex = PTHREAD_MUTEX_INITIALIZER };
		server_t server = { .events = &events };
		client_t client = { .events = &events };
		morse_receiver_t morse_receiver = { .events = &events };


		int wpm = 0;
		if (0 != server_setup(&server, test_case, &wpm)) {
			test_log_err("Test: failed at setting up of server for test case %zu / %zu\n", i + 1, n_test_cases);
			failure = true;
			goto cleanup;
		}

		if (test_case->expected_fail) {
			/* We are expecting cwdaemon server to fail to start in
			   server_setup(). Without a server, calling testcase_setup() and
			   testcase_run() doesn't make sense. But still evaluate
			   collected events. */
			goto evaluate;
		}

		if (0 != testcase_setup(&server, &client, &morse_receiver, wpm)) {
			test_log_err("Test: failed at setting up of test case %zu / %zu\n", i + 1, n_test_cases);
			failure = true;
			goto cleanup;
		}

		if (0 != testcase_run(test_case, &server, &client, &morse_receiver, &events)) {
			test_log_err("Test: running test case %zu / %zu has failed\n", i + 1, n_test_cases);
			failure = true;
			goto cleanup;
		}

	evaluate:
		events_sort(&events);
		events_print(&events);
		if (0 != evaluate_events(&events, test_case)) {
			test_log_err("Test: evaluation of events has failed %s\n", "");
			failure = true;
			goto cleanup;
		}
		test_log_info("Test: evaluation of events was successful %s\n", "");

	cleanup:
		/* Clear stuff before running next test case. */
		memset(&g_child_exit_info, 0, sizeof (g_child_exit_info));

		if (0 != testcase_teardown(&client, &morse_receiver)) {
			test_log_err("Test: failed at tear-down for test case %zu / %zu\n", i + 1, n_test_cases);
			failure = true;
		}

		if (failure) {
			test_log_err("Test: test case #%zu/%zu failed, terminating\n", i + 1, n_test_cases);
			exit(EXIT_FAILURE);
		}
		test_log_info("Test: test case #%zu/%zu succeeded\n\n", i + 1, n_test_cases);
	}

	exit(EXIT_SUCCESS);
}




static int save_child_exit_to_events(const child_exit_info_t * child_exit_info, events_t * events)
{
	if (0 != child_exit_info->sigchld_timestamp.tv_sec) {
		/*
		  SIGCHLD was received by test program at some point in time.
		  Record this in array of events.

		  Signal handler can record a timestamp, but can't add the event
		  to array of events itself. Let's do this here.

		  My tests show that there is no need to sort (by timestamp) the
		  array afterwards.
		*/
		events_insert_sigchld_event(events, child_exit_info);
	}

	return 0;
}




/**
   @brief Prepare cwdaemon server used to execute single test case

   Server is being prepared outside of testcase_setup() because in some cases
   we expect the server to fail. To properly handle "successful failure" in a
   given test case run, we need to separate setup of server (in this
   function) and setup of other resources.

   Again: it may be expected and desired that server fails to start, see
   test_case_t::expected_fail.

   @param[out] server cwdaemon server to set up
   @param[in] test_case Test case according to which to set up a server
   @param[out] wpm Morse code speed used by server
   @param[in/out] events Events containter

   @return 0 if starting of a server ended as expected
   @return -1 if starting of server ended not as expected
*/
static int server_setup(server_t * server, const test_case_t * test_case, int * wpm)
{
	/* Remember that some receive timeouts in tests were selected when the
	   wpm was hardcoded to 10 wpm. Picking values lower than 10 may lead to
	   overrunning the timeouts. */
	cwdaemon_random_uint(10, 15, (unsigned int *) wpm);


	const cwdaemon_opts_t cwdaemon_opts = {
		.tone           = 640,
		.sound_system   = CW_AUDIO_SOUNDCARD,
		.nofork         = true,
		.cwdevice_name  = TEST_TTY_CWDEVICE_NAME,
		.wpm            = *wpm,
		.l4_port        = test_case->port,
	};
	const int retv = server_start(&cwdaemon_opts, server);
	if (0 != retv) {
		save_child_exit_to_events(&g_child_exit_info, server->events);
		if (test_case->expected_fail) {
			return 0; /* Setting up of server has failed, as expected. */
		} else {
			test_log_err("Test: unexpected failure to start cwdaemon with valid port %d\n", test_case->port);
			return -1;
		}
	} else {
		if (test_case->expected_fail) {
			test_log_err("Test: unexpected success in starting cwdaemon with invalid port %d\n", test_case->port);
			return -1;
		} else {
			return 0; /* Setting up of server has succeeded, as expected. */
		}
	}
}




/**
   @brief Prepare resources used to execute single test case
*/
static int testcase_setup(const server_t * server, client_t * client, morse_receiver_t * morse_receiver, int wpm)
{
	bool failure = false;


	if (0 != client_connect_to_server(client, server->ip_address, (in_port_t) server->l4_port)) { /* TODO acerion 2024.01.29: remove casting. */
		test_log_err("Test: can't connect cwdaemon client to cwdaemon server at [%s:%d]\n", server->ip_address, server->l4_port);
		failure = true;
	}


	const morse_receiver_config_t morse_config = { .wpm = wpm };
	if (0 != morse_receiver_ctor(&morse_config, morse_receiver)) {
		test_log_err("Test: failed to create Morse receiver %s\n", "");
		failure = true;
	}


	return failure ? -1 : 0;
}




static int testcase_run(const test_case_t * test_case, server_t * server, client_t * client, morse_receiver_t * morse_receiver, events_t * events)
{
	if (0 != morse_receiver_start(morse_receiver)) {
		test_log_err("Test: failed to start Morse receiver %s\n", "");
		return -1;
	}

	/* Send the message to be played to double-check that a cwdaemon server
	   is running, and that it's listening on a network socket on a port
	   specified in test case.. */
	client_send_request(client, CWDAEMON_REQUEST_MESSAGE, test_case->full_message);

	morse_receiver_wait(morse_receiver);


	/* Terminate local test instance of cwdaemon. Being able to send EXIT
	   request on specific, valid port is a part of test case. */
	if (0 != local_server_stop(server, client)) {
		test_log_err("Test: failed to correctly stop local test instance of cwdaemon at end of test case %s\n", "");
		return -1;
	}
	save_child_exit_to_events(&g_child_exit_info, events);


	return 0;
}



static int testcase_teardown(client_t * client, morse_receiver_t * morse_receiver)
{
	/* cwdaemon server is being stopped as a part of test case, in
	   testcase_run(). */

	morse_receiver_dtor(morse_receiver);

	/* Close our socket to cwdaemon server. */
	client_disconnect(client);
	client_dtor(client);

	return 0;
}




static int evaluate_events(const events_t * events, const test_case_t * test_case)
{
	/* Expectation 1: count of events. */
	if (test_case->expected_fail) {
		/* We only expect "exit" event to be recorded. */
		if (1 != events->event_idx) {
			test_log_err("Expectation 1: failure case: incorrect count of events: %d (expected 1)\n", events->event_idx);
			return -1;
		}
	} else {
		/* We expect two events: first a Morse receive event (because
		   cwdaemon was running and listening on valid port), and then a
		   clean "exit" event. */
		if (2 != events->event_idx) {
			test_log_err("Expectation 1: success case: incorrect count of events: %d (expected 2)\n", events->event_idx);
			return -1;
		}
	}
	test_log_info("Expectation 1: found expected count of events: %d\n", events->event_idx);




	/* Expectation 2: correct event types. */
	const event_t * morse_event = NULL;
	const event_t * sigchld_event = NULL;

	if (test_case->expected_fail) {
		int i = 0;
		if (events->events[i].event_type != event_type_sigchld) {
			test_log_err("Expectation 2: failure case: event #%d has unexpected type: %d\n", i, events->events[i].event_type);
			return -1;
		}
		sigchld_event = &events->events[i];
	} else {
		int i = 0;
		if (events->events[i].event_type != event_type_morse_receive) {
			test_log_err("Expectation 2: success case: event #%d has unexpected type: %d\n", i, events->events[i].event_type);
			return -1;
		}
		morse_event = &events->events[i];

		i++;

		if (events->events[i].event_type != event_type_sigchld) {
			test_log_err("Expectation 2: success case: event #%d has unexpected type: %d\n", i, events->events[i].event_type);
			return -1;
		}
		sigchld_event = &events->events[i];
	}
	test_log_info("Expectation 2: found expected types of events %s\n", "");




	/* Expectation 3: cwdaemon terminated in expected way. */
	const int wstatus = sigchld_event->u.sigchld.wstatus;
	if (test_case->expected_fail) {
		/* cwdaemon should have exited when it detected invalid value of port
		   option. */
		if (!WIFEXITED(wstatus)) {
			test_log_err("Expectation 3: failure case: cwdaemon did not exit, wstatus = 0x%04x\n", wstatus);
			return -1;
		}
		if (EXIT_FAILURE != WEXITSTATUS(wstatus)) {
			test_log_err("Expectation 3: failure case: incorrect exit status (expected %04x/EXIT_FAILURE): 0x%04x\n", EXIT_FAILURE, WEXITSTATUS(wstatus));
			return -1;
		}
		test_log_info("Expectation 3: failure case: exit status is as expected (0x%04x)\n", wstatus);
	} else {
		/* cwdaemon should have exited when asked nicely (through request) at
		   the end of testcase. */
		if (!WIFEXITED(wstatus)) {
			test_log_err("Expectation 3: success case: cwdaemon did not exit, wstatus = 0x%04x\n", wstatus);
			return -1;
		}
		if (EXIT_SUCCESS != WEXITSTATUS(wstatus)) {
			test_log_err("Expectation 3: success case: incorrect exit status (expected 0/EXIT_SUCCESS): 0x%04x\n", WEXITSTATUS(wstatus));
			return -1;
		}
		test_log_info("Expectation 3: success case: exit status is as expected (0x%04x)\n", wstatus);
	}




	/* Expectation 4: the Morse event contains correct received text. */
	if (!test_case->expected_fail) {
		if (!morse_receive_text_is_correct(morse_event->u.morse_receive.string, test_case->full_message)) {
			test_log_err("Expectation 4: success case: received Morse message [%s] doesn't match text from message request [%s]\n",
			             morse_event->u.morse_receive.string, test_case->full_message);
			return -1;
		}
		test_log_info("Expectation 4: received Morse message [%s] matches test from message request [%s] (ignoring the first character)\n",
		              morse_event->u.morse_receive.string, test_case->full_message);
	}




	test_log_info("Test: Evaluation of test events was successful %s\n", "");

	return 0;
}

