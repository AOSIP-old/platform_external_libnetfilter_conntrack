/*
 * WARNING: Do *NOT* ever include this file, only for internal use!
 * 	    Use the set/get API in order to set/get the conntrack attributes
 */

#ifndef _NFCT_OBJECT_H_
#define _NFCT_OBJECT_H_

/*
 * nfct callback handler object
 */

struct nfct_handle {
	struct nfnl_handle		*nfnlh;
	struct nfnl_subsys_handle	*nfnlssh_ct;
	struct nfnl_subsys_handle	*nfnlssh_exp;

	/* deprecated old API */
	nfct_callback 			callback;
	void 				*callback_data;
	nfct_handler			handler;

	/* callback handler for the new API */
	struct nfnl_callback		nfnl_cb;

	int			(*cb)(enum nf_conntrack_msg_type type, 
				      struct nf_conntrack *ct,
				      void *data);

	int			(*expect_cb)(enum nf_conntrack_msg_type type, 
					     struct nf_expect *exp,
					     void *data);
};

/* container used to pass data to nfnl callbacks */
struct __data_container {
	struct nfct_handle *h;
	enum nf_conntrack_msg_type type;
	void *data;
};

/*
 * conntrack object
 */

union __nfct_l4_src {
	/* Add other protocols here. */
	u_int16_t 		all;
	struct {
		u_int16_t 	port;
	} tcp;
	struct {
		u_int16_t 	port;
	} udp;
	struct {
		u_int16_t 	id;
	} icmp;
	struct {
		u_int16_t 	port;
	} sctp;
};

union __nfct_l4_dst {
	/* Add other protocols here. */
	u_int16_t 		all;
	struct {
		u_int16_t 	port;
	} tcp;
	struct {
		u_int16_t 	port;
	} udp;
	struct {
		u_int8_t 	type, code;
	} icmp;
	struct {
		u_int16_t 	port;
	} sctp;
};

union __nfct_address {
	u_int32_t 		v4;
	struct in6_addr 	v6;
};

struct __nfct_tuple {
	union __nfct_address	src;
	union __nfct_address 	dst;

	u_int8_t		l3protonum;
	u_int8_t		protonum;
	union __nfct_l4_src	l4src;
	union __nfct_l4_dst	l4dst;

	struct {
		u_int32_t 	correction_pos;
		u_int32_t 	offset_before;
		u_int32_t 	offset_after;
	} natseq;
};

#define __DIR_ORIG 		0
#define __DIR_REPL 		1
#define __DIR_MASTER 		2
#define __DIR_MAX 		__DIR_MASTER+1

union __nfct_protoinfo {
	struct {
		u_int8_t 		state;
		struct {
			u_int8_t 	value;
			u_int8_t 	mask;
		} flags[__DIR_MAX];
	} tcp;
	struct {
		u_int8_t 		state;
		u_int32_t 		vtag[__DIR_MAX];
	} sctp;

};

struct __nfct_counters {
	u_int64_t 	packets;
	u_int64_t 	bytes;
};

struct __nfct_nat {
	u_int32_t 		min_ip, max_ip;
	union __nfct_l4_src 	l4min, l4max;
};

struct nf_conntrack {
	struct __nfct_tuple 	tuple[__DIR_MAX];
	
	u_int32_t 	timeout;
	u_int32_t	mark;
	u_int32_t	secmark;
	u_int32_t 	status;
	u_int32_t	use;
	u_int32_t	id;

	union __nfct_protoinfo 	protoinfo;
	struct __nfct_counters 	counters[__DIR_MAX];
	struct __nfct_nat 	snat;
	struct __nfct_nat 	dnat;

	u_int32_t 		set[2];
};

/*
 * conntrack filter object
 */

struct nfct_filter {
	/*
	 * As many other objects in this library, the attributes are
	 * private. This gives us the chance to modify the layout and
	 * object size.
	 *
	 * Another observation, although this object might seem too
	 * memory consuming, it is only needed to build the filter. Thus,
	 * once it is attached, you can release this object.
	 */

	/*
	 * filter logic: use positive or negative logic
	 */
	enum nfct_filter_logic 	logic[NFCT_FILTER_MAX];

	/*
	 * This the layer 4 protocol map for filtering.
	 */
	u_int32_t 		l4proto_map[IPPROTO_MAX/32];

	struct {
	/*
	 * No limitations in the protocol filtering. We use a map of 
	 * 16 bits per protocol. As for now, DCCP has 10 states, TCP has
	 * 10 states, SCTP has 8 state. Therefore, 16 bits is enough.
	 */
#define __FILTER_PROTO_MAX	16
		u_int16_t 	map;
	} l4proto_state[IPPROTO_MAX];

#define __FILTER_ADDR_SRC 0
#define __FILTER_ADDR_DST 1

	/*
	 * FIXME: For IPv4 filtering, up to 256 IPs or masks by now.
	 * This limitation is related to the existing autogenerated BSF code
	 * and the fact that the maximum jump offset if 2^8 = 256.
	 */
	u_int32_t 		l3proto_elems[2];
	struct {
#define __FILTER_ADDR_MAX	256
		u_int32_t 	addr;
		u_int32_t 	mask;
	} l3proto[2][__FILTER_ADDR_MAX];

	u_int32_t 		set[1];
};

/*
 * expectation object
 */

struct nf_expect {
	struct nf_conntrack 	master;
	struct nf_conntrack 	expected;
	struct nf_conntrack 	mask;
	u_int32_t 		timeout;
	u_int32_t 		id;
	u_int16_t 		expectfn_queue_id;

	u_int32_t 		set[1];
};

#endif