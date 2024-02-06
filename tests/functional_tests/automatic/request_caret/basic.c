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
   Basic tests of caret ('^') request.

   The tests are basic because a single test case just sends one caret
   requests and sees what happens.

   TODO acerion 2024.01.26: add "advanced" tests (in separate file) in which
   there is some client code that waits for server's response and interacts
   with it, perhaps by sending another caret request, and then another, and
   another. Come up with some good methods of testing of more advanced
   scenarios.
*/




#define _DEFAULT_SOURCE




#include "config.h"

/* For kill() on FreeBSD 13.2 */
#include <signal.h>
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "basic.h"

#include "tests/library/cwdevice_observer.h"
#include "tests/library/cwdevice_observer_serial.h"
#include "tests/library/events.h"
#include "tests/library/log.h"
#include "tests/library/misc.h"
#include "tests/library/morse_receiver.h"
#include "tests/library/morse_receiver_utils.h"
#include "tests/library/process.h"
#include "tests/library/random.h"
#include "tests/library/socket.h"
#include "tests/library/test_env.h"
#include "tests/library/thread.h"
#include "tests/library/time_utils.h"




events_t g_events = { .mutex = PTHREAD_MUTEX_INITIALIZER };




typedef struct test_case_t {
	const char * description;                 /**< Tester-friendly description of test case. */
	const char * full_message;                /**< Text to be sent to cwdaemon server in the MESSAGE request. Full message, so it SHOULD include caret. */
	const char * full_expected_socket_reply;  /**< What is expected to be received through socket from cwdaemon server. Full reply, so it SHOULD include terminating "\r\n". */
	const char * expected_morse_receive;      /**< What is expected to be received by Morse code receiver (without ending space). */
} test_case_t;




static test_case_t g_test_cases[] = {
	{ .description = "mixed characters",
	  .full_message               = "22 crows, 1 stork?^",
	  .full_expected_socket_reply = "22 crows, 1 stork?\r\n",
	  .expected_morse_receive     = "22 crows, 1 stork?",
	},



	/*
	  Handling of caret in cwdaemon indicates that once a first caret is
	  recognized, the caret and everything after it is ignored:

	      case '^':
	          *x = '\0';     // Remove '^' and possible trailing garbage.
	*/
	{ .description = "additional message after caret",
	  .full_message               = "Fun^Joy^",
	  .full_expected_socket_reply = "Fun\r\n",
	  .expected_morse_receive     = "Fun",
	},
	{ .description = "message with two carets",
	  .full_message               = "Monday^^",
	  .full_expected_socket_reply = "Monday\r\n",
	  .expected_morse_receive     = "Monday",
	},



	{ .description = "two words",
	  .full_message               = "Hello world!^",
	  .full_expected_socket_reply = "Hello world!\r\n",
	  .expected_morse_receive     = "Hello world!",
	},

	/* There should be no action from cwdaemon: neither keying nor socket
	   reply. */
	{ .description = "empty text",
	  .full_message               = "^",
	  .full_expected_socket_reply = "",
	  .expected_morse_receive     = "",
	},

	{ .description = "single character",
	  .full_message               = "f^",
	  .full_expected_socket_reply = "f\r\n",
	  .expected_morse_receive     = "f",
	},

	{ .description = "single word",
	  .full_message               = "Paris^",
	  .full_expected_socket_reply = "Paris\r\n",
	  .expected_morse_receive     = "Paris",
	},

	/* Notice how the leading space from message is preserved in socket reply. */
	{ .description = "single word with leading space",
	  .full_message               = " London^",
	  .full_expected_socket_reply = " London\r\n",
	  .expected_morse_receive     = "London",
	},

	/* Notice how the trailing space from message is preserved in socket reply. */
	{ .description = "mixed characters with trailing space",
	  .full_message               = "when, now = right: ^",
	  .full_expected_socket_reply = "when, now = right: \r\n",
	  .expected_morse_receive     = "when, now = right:",
	},
};




static int test_setup(cwdaemon_server_t * server, client_t * client, morse_receiver_t ** morse_receiver);
static int test_teardown(cwdaemon_server_t * server, client_t * client, morse_receiver_t ** morse_receiver);
static int test_run(test_case_t * test_cases, size_t n_test_cases, client_t * client, morse_receiver_t * morse_receiver);
static int evaluate_events(const events_t * events, const test_case_t * test_case);




int basic_caret_test(void)
{
	bool failure = false;
	const size_t n_test_cases = sizeof (g_test_cases) / sizeof (g_test_cases[0]);
	cwdaemon_server_t server = { 0 };
	client_t client = { 0 };
	morse_receiver_t * morse_receiver = NULL;

	if (0 != test_setup(&server, &client, &morse_receiver)) {
		test_log_err("Test: failed at test setup %s\n", "");
		failure = true;
		goto cleanup;
	}

	if (0 != test_run(g_test_cases, n_test_cases, &client, morse_receiver)) {
		test_log_err("Test: failed at running test cases %s\n", "");
		failure = true;
		goto cleanup;
	}

 cleanup:
	if (0 != test_teardown(&server, &client, &morse_receiver)) {
		test_log_err("Test: failed at test tear down %s\n", "");
		failure = true;
	}

	return failure ? -1 : 0;
}




/**
   @brief Evaluate events that were reported by objects used during execution
   of single test case

   Look at contents of @p events and check if order and types of events are
   as expected.

   The events may include
     - receiving Morse code
     - receiving reply from cwdaemon server,
     - changes of state of PTT pin,
     - exiting of local instance of cwdaemon server process,

   @return 0 if events are in proper order and of proper type
   @return -1 otherwise
*/
static int evaluate_events(const events_t * events, const test_case_t * test_case)
{
	/*
	  Expectation 1: in most cases there should be 2 events:
	   - Receiving some reply over socket from cwdaemon server,
	   - Receiving some Morse code on cwdevice.
	  In other cases there should be zero events.

	  The two events go hand in hand: if one is expected, the other is too.
	  If one is not expected to occur, the other is not expected either.
	*/
	const bool expecting_morse_event = 0 != strlen(test_case->expected_morse_receive);
	const bool expecting_socket_reply_event = 0 != strlen(test_case->full_expected_socket_reply);
	if (!expecting_morse_event && !expecting_socket_reply_event) {
		if (0 != events->event_idx) {
			test_log_err("Expectation 1: incorrect count of events recorded. Expected 0 events, got %d\n", events->event_idx);
			return -1;
		}
		test_log_info("Expectation 1: there are zero events (as expected), so evaluation of events is now completed %s\n", "");
		return 0;
	} else if (expecting_morse_event && expecting_socket_reply_event) {
		if (2 != events->event_idx) {
			test_log_err("Expectation 1: incorrect count of events recorded. Expected 2 events, got %d\n", events->event_idx);
			return -1;
		}
	} else {
		test_log_err("Expectation 1: Incorrect situation when checking 'expecting' flags: %d != %d\n", expecting_morse_event, expecting_socket_reply_event);
		return -1;
	}
	test_log_info("Expectation 1: count of events is correct: %d\n", events->event_idx);




	/*
	  Expectation 2: events are of correct type.
	*/
	const event_t * morse_event = NULL;
	const event_t * socket_event = NULL;
	int morse_idx = -1;
	int socket_idx = -1;

	if (events->events[0].event_type == events->events[1].event_type) {
		test_log_err("Expectation 2: both events have the same type %s\n", "");
		return -1;
	}

	if (events->events[0].event_type == event_type_morse_receive) {
		morse_event = &events->events[0];
		morse_idx = 0;
	} else if (events->events[1].event_type == event_type_morse_receive) {
		morse_event = &events->events[1];
		morse_idx = 1;
	} else {
		test_log_err("Expectation 2: can't find Morse receive event in events table %s\n", "");
		return -1;
	}

	if (events->events[0].event_type == event_type_client_socket_receive) {
		socket_event = &events->events[0];
		socket_idx = 0;
	} else if (events->events[1].event_type == event_type_client_socket_receive) {
		socket_event = &events->events[1];
		socket_idx = 1;
	} else {
		test_log_err("Expectation 2: can't find socket receive event in events table %s\n", "");
		return -1;
	}
	test_log_info("Expectation 2: types of events are correct %s\n", "");




	/*
	  Expectation 3: events in proper order.

	  I'm not 100% sure what the correct order should be in perfect
	  implementation of cwdaemon. In 0.12.0 it is "socket receive" first, and
	  then "morse receive" second, unless a message sent to server ends with
	  space.

	  TODO acerion 2024.01.28: check what SHOULD be the correct order of the
	  two events. Some comments in cwdaemon indicate that reply should be
	  sent after end of playing Morse.

	  TODO acerion 2024.01.26: Double check the corner case with space at the
	  end of message.
	*/
	if (morse_idx < socket_idx) {
		test_log_warn("Expectation 3: unexpected order of events: Morse first (idx = %d), socket second (idx = %d)\n", morse_idx, socket_idx);
		//return -1; /* TODO acerion 2024.01.26: uncomment after your are certain of correct order of events. */
	} else {
		test_log_info("Expectation 3: expected order of events: socket first (idx = %d), Morse second (idx = %d)\n", socket_idx, morse_idx);
	}




	/*
	  Expectation 4: cwdaemon client has received over socket a correct
	  reply.
	*/
	const char * full_expected = test_case->full_expected_socket_reply;
	char escaped_expected[64] = { 0 };
	escape_string(full_expected, escaped_expected, sizeof (escaped_expected));

	const char * full_received = socket_event->u.socket_receive.string;
	char escaped_received[64] = { 0 };
	escape_string(full_received, escaped_received, sizeof (escaped_received));

	if (0 != strcmp(full_received, full_expected)) {
		test_log_err("Expectation 4: received socket reply [%s] doesn't match expected socket reply [%s]\n", escaped_received, escaped_expected);
		return -1;
	}
	test_log_info("Expectation 4: received socket reply [%s] matches expected reply [%s]\n", escaped_received, escaped_expected);




	/*
	  Expectation 5: cwdaemon keyed a proper Morse message on cwdevice.

	  Receiving of message by Morse receiver should not be verified if the
	  expected message is too short (the problem with "warm-up" of receiver).
	  TOOO acerion 2024.01.28: remove "do_morse_test" flag after fixing
	  receiver.
	*/
	const bool do_morse_test = strlen(test_case->expected_morse_receive) > 1;
	if (do_morse_test) {
		if (!morse_receive_text_is_correct(morse_event->u.morse_receive.string, test_case->expected_morse_receive)) {
			test_log_err("Expectation 5: received Morse message [%s] doesn't match expected receive [%s]\n",
			             morse_event->u.morse_receive.string, test_case->expected_morse_receive);
			return -1;
		}
		test_log_info("Expectation 5: received Morse message [%s] matches expected receive [%s] (ignoring the first character)\n",
		              morse_event->u.morse_receive.string, test_case->expected_morse_receive);
	} else {
		test_log_notice("Expectation 5: skipping verification of message received by Morse receiver due to short expected string %s\n", "");
	}




	/*
	  Expectation 6: "Morse receive" event and "socket receive" event are
	  close to each other.

	  TODO acerion 2024.01.26: the threshold is still 0.5 seconds. That's a
	  lot. Try to bring it down. Notice that the time diff may depend on
	  Morse code speed (wpm).
	*/
	struct timespec diff = { 0 };
	if (morse_idx < socket_idx) {
		timespec_diff(&morse_event->tstamp, &socket_event->tstamp, &diff);
	} else {
		timespec_diff(&socket_event->tstamp, &morse_event->tstamp, &diff);
	}

	const int threshold = 500L * 1000 * 1000; /* [nanoseconds] */
	if (diff.tv_sec > 0 || diff.tv_nsec > threshold) {
		test_log_err("Expectation 6: time difference between end of 'Morse receive' and receiving socket reply is too large: %ld.%09ld seconds\n", diff.tv_sec, diff.tv_nsec);
		return -1;
	}
	test_log_info("Expectation 6: time difference between end of 'Morse receive' and receiving socket reply is ok: %ld.%09ld seconds\n", diff.tv_sec, diff.tv_nsec);




	test_log_info("Test: evaluation of test events was successful %s\n", "");

	return 0;
}




/**
   @brief Prepare resources used to execute set of test cases
*/
static int test_setup(cwdaemon_server_t * server, client_t * client, morse_receiver_t ** morse_receiver)
{
	bool failure = false;

	int wpm = 10;
	/* Remember that some receive timeouts in tests were selected when the
	   wpm was hardcoded to 10 wpm. Picking values lower than 10 may lead to
	   overrunning the timeouts. */
	cwdaemon_random_uint(10, 15, (unsigned int *) &wpm);


	/* Prepare local test instance of cwdaemon server. */
	const cwdaemon_opts_t cwdaemon_opts = {
		.tone           = 640,
		.sound_system   = CW_AUDIO_SOUNDCARD,
		.nofork         = true,
		.cwdevice_name  = TEST_TTY_CWDEVICE_NAME,
		.wpm            = wpm,
	};
	if (0 != server_start(&cwdaemon_opts, server)) {
		test_log_err("Test: failed to start cwdaemon server %s\n", "");
		failure = true;
	}


	if (0 != client_connect_to_server(client, server->ip_address, (in_port_t) server->l4_port)) { /* TODO acerion 2024.01.24: remove casting. */
		test_log_err("Test: can't connect cwdaemon client to cwdaemon server at [%s:%d]\n", server->ip_address, server->l4_port);
		failure = true;
	}
	client_socket_receive_enable(client);
	if (0 != client_socket_receive_start(client)) {
		test_log_err("Test: failed to start socket receiver %s\n", "");
		failure = true;
	}


	const morse_receiver_config_t morse_config = { .wpm = wpm };
	*morse_receiver = morse_receiver_ctor(&morse_config);
	if (NULL == *morse_receiver) {
		test_log_err("Test: failed to create Morse receiver %s\n", "");
		failure = true;
	}

	return failure ? -1 : 0;
}




/**
   @brief Clean up resources used to execute set of test cases
*/
static int test_teardown(cwdaemon_server_t * server, client_t * client, morse_receiver_t ** morse_receiver)
{
	bool failure = false;

	morse_receiver_dtor(morse_receiver);

	/* Terminate local test instance of cwdaemon server. */
	if (0 != local_server_stop(server, client)) {
		/*
		  Stopping a server is not a main part of a test, but if a
		  server can't be closed then it means that the main part of the
		  code has left server in bad condition. The bad condition is an
		  indication of an error in tested functionality. Therefore set
		  failure to true.
		*/
		test_log_err("Test: failed to correctly stop local test instance of cwdaemon %s\n", "");
		failure = true;
	}

	client_socket_receive_stop(client);
	client_disconnect(client);
	client_dtor(client);

	return failure ? -1 : 0;
}




/**
   @brief Run all test cases. Evaluate results (the events) of each test case.
*/
static int test_run(test_case_t * test_cases, size_t n_test_cases, client_t * client, morse_receiver_t * morse_receiver)
{
	bool failure = false;

	for (size_t i = 0; i < n_test_cases; i++) {
		const test_case_t * test_case = &test_cases[i];

		test_log_newline(); /* Visual separator. */
		test_log_info("Test: starting test case %zu / %zu: [%s]\n", i + 1, n_test_cases, test_case->description);

		if (0 != morse_receiver_start(morse_receiver)) {
			test_log_err("Test: failed to start Morse receiver %s\n", "");
			failure = true;
			break;
		}

		/* Send the message to be played. */
		client_send_request_va(client, CWDAEMON_REQUEST_MESSAGE, "%s", test_case->full_message);

		morse_receiver_wait(morse_receiver);

		events_sort(&g_events);
		events_print(&g_events); /* For debug only. */
		if (0 != evaluate_events(&g_events, test_case)) {
			test_log_err("Test: evaluation of events has failed for test case %zu / %zu\n", i + 1, n_test_cases);
			failure = true;
			break;
		}
		test_log_info("Test: evaluation of events was successful for test case %zu / %zu\n", i + 1, n_test_cases);

		/* Clear stuff before running next test case. */
		events_clear(&g_events);

		test_log_info("Test: test case %zu / %zu has succeeded\n\n", i + 1, n_test_cases);
	}

	return failure ? -1 : 0;
}




