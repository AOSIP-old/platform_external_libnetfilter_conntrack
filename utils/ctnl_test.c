/*
 * (C) 2005 by Pablo Neira Ayuso <pablo@eurodev.net>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * libnetfilter_conntrack test file: yet incomplete
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <libnetfilter_conntrack/libnetfilter_conntrack.h>

static int event_counter(void *arg, unsigned int flags, int type)
{
	static int counter = 0;

	fprintf(stdout, "Event number %d\n", ++counter);
	if (counter >= 10)
		return -1;
	
	return 0;
}

static struct nfct_conntrack *ct;
static struct nfct_handle *cth;

static void event_sighandler(int s)
{
	nfct_conntrack_free(ct);
	nfct_close(cth);
}

/* I know, better with fork() as Rusty does in nfsim ;), later */
int main(int argc, char **argv)
{
	struct nfct_tuple orig = {
		.src = { .v4 = inet_addr("1.1.1.1") },
		.dst = { .v4 = inet_addr("2.2.2.2") },
		.protonum = IPPROTO_TCP,
		.l4src = { .tcp = { .port = 10 } },
		.l4dst = { .tcp = { .port = 20 } }
	};
	struct nfct_tuple reply = {
		.src = { .v4 = inet_addr("2.2.2.2") },
		.dst = { .v4 = inet_addr("1.1.1.1") },
		.protonum = IPPROTO_TCP,
		.l4src = { .tcp = { .port = 20 } },
		.l4dst = { .tcp = { .port = 10 } }
	};
	union nfct_protoinfo proto = {
		.tcp = { .state = 1 },
	};
	unsigned long status = IPS_ASSURED | IPS_CONFIRMED;
	unsigned long timeout = 100;
	unsigned long mark = 0;
	unsigned long id = NFCT_ANY_ID;
	int ret = 0, errors = 0;

	/* Here we go... */
	fprintf(stdout, "Test for libnetfilter_conntrack\n\n");

	ct = nfct_conntrack_alloc(&orig, &reply, timeout, &proto, status,
				  mark, id, NULL);
	if (!ct) {
		fprintf(stderr, "Not enough memory");
		errors++;
		ret = -ENOMEM;
		goto end;
	}

	cth = nfct_open(CONNTRACK, NFCT_ANY_GROUP);
	if (!cth) {
		fprintf(stderr, "Can't open handler\n");
		errors++;
		ret = -ENOENT;
		nfct_conntrack_free(ct);
		goto end;
	}

	ret = nfct_create_conntrack(cth, ct);
	fprintf(stdout, "TEST 1: create conntrack (%d)\n", ret);
	
	/* Skip EEXIST error, in case that the test has been called
	 * twice this spot a bogus error */
	if (ret < 0 && ret != -EEXIST)
		errors++;

	nfct_register_callback(cth, nfct_default_conntrack_display);
	ret = nfct_dump_conntrack_table_reset_counters(cth);
	fprintf(stdout, "TEST 2: dump conntrack table and reset (%d)\n", ret);
	if (ret < 0)
		errors++;

	ret = nfct_dump_conntrack_table(cth);
	fprintf(stdout, "TEST 3: dump conntrack table (%d)\n", ret);
	if (ret < 0)
		errors++;

	ret = nfct_get_conntrack(cth, &orig, NFCT_DIR_ORIGINAL, NFCT_ANY_ID);
	fprintf(stdout, "TEST 4: get conntrack (%d)\n", ret);
	if (ret < 0)
		errors++;

	ct->status |= IPS_SEEN_REPLY;
	ct->timeout = 1000;
	ret = nfct_update_conntrack(cth, ct);
	fprintf(stdout, "TEST 5: update conntrack (%d)\n", ret);
	if (ret < 0)
		errors++;

	ret = nfct_delete_conntrack(cth, &orig, NFCT_DIR_ORIGINAL, NFCT_ANY_ID);
	fprintf(stdout, "TEST 6: delete conntrack (%d)\n", ret);
	if (ret < 0)
		errors++;

	fprintf(stdout, "TEST 7: Waiting for 10 conntrack events\n");
	signal(SIGINT, event_sighandler);
	nfct_register_callback(cth, event_counter);
	ret = nfct_event_conntrack(cth);
	fprintf(stdout, "TEST 7: Received 10 conntrack events (%d)\n", ret);

	nfct_close(cth);
	nfct_conntrack_free(ct);

end:
	if (errors)
		fprintf(stdout, "Test failed with error %d. Errors=%d\n", 
			ret, errors);
	else
		fprintf(stdout, "Test OK\n");
}