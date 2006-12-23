/*
 * (C) 2006 by Pablo Neira Ayuso <pablo@netfilter.org>
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 */

#include "internal.h"

/*
 * XML output sample:
 *
 * <flow>
 * 	<meta direction="original">
 * 		<layer3 protonum="2" protoname="IPv4">
 * 			<src>192.168.0.1</src>
 * 			<dst>192.168.0.2</dst>
 * 		</layer3>
 * 		<layer4 protonum="16" protoname"udp">
 * 			<sport>80</sport>
 * 			<dport>56665</dport>
 * 		</layer4>
 * 		<counters>
 * 			<bytes>10</bytes>
 * 			<packets>1</packets>
 * 		</counters>
 * 	</meta>
 * 	<meta direction="reply">
 * 		<layer3 protonum="2" protoname="IPv4">
 * 			<src>192.168.0.2</src>
 * 			<dst>192.168.0.1</dst>
 * 		</layer3>
 * 		<layer4 protonum="16" protoname="udp">
 * 			<sport>80</sport>
 * 			<dport>56665</dport>
 * 		</layer4>
 * 		<counters>
 * 			<bytes>5029</bytes>
 * 			<packets>12</packets>
 *		</counters>
 * 	</meta>
 * 	<meta direction="independent">
 *	 	<layer4>
 * 			<state>ESTABLISHED</state>
 * 		</layer4>
 * 		<timeout>100</timeout>
 * 		<mark>1</mark>
 * 		<use>1</use>
 * 		<assured/>
 * 	</meta>
 * </flow>
 */

static char *proto2str[IPPROTO_MAX] = {
	[IPPROTO_TCP] = "tcp",
        [IPPROTO_UDP] = "udp",
        [IPPROTO_ICMP] = "icmp",
        [IPPROTO_SCTP] = "sctp"
};
static char *l3proto2str[AF_MAX] = {
	[AF_INET] = "ipv4",
	[AF_INET6] = "ipv6"
};

enum {
	__ADDR_SRC = 0,
	__ADDR_DST,
};

static void buffer_size(int ret, unsigned int *size, unsigned int *len)
{
	*size += ret;
	*len -= ret;
}

static char *__proto2str(u_int8_t protonum)
{
	return proto2str[protonum] ? proto2str[protonum] : "unknown";
}

static char *__l3proto2str(u_int8_t protonum)
{
	return l3proto2str[protonum] ? l3proto2str[protonum] : "unknown";
}

static int __snprintf_ipv4_xml(char *buf,
			       unsigned int len,
			       const struct __nfct_tuple *tuple,
			       unsigned int type)
{
	struct in_addr addr = { 
		.s_addr = (type == __ADDR_SRC) ? tuple->src.v4 : tuple->dst.v4,
	};

	return snprintf(buf, len, "%s", inet_ntoa(addr));
}

static int __snprintf_ipv6_xml(char *buf,
			       unsigned int len,
			       const struct __nfct_tuple *tuple,
			       unsigned int type)
{
	struct in6_addr addr;
	static char tmp[INET6_ADDRSTRLEN];
	const void *p = (type == __ADDR_SRC) ? &tuple->src.v6 : &tuple->dst.v6;

	memcpy(&addr.in6_u, p, sizeof(struct in6_addr));

	if (!inet_ntop(AF_INET6, &addr, tmp, sizeof(tmp)))
		return -1;

	return snprintf(buf, len, "%s", tmp);
}

static int __snprintf_addr_xml(char *buf,
			       unsigned int len,
			       const struct __nfct_tuple *tuple, 
			       unsigned int type)
{
	int ret;
	unsigned int size = 0;

	switch(type) {
	case __ADDR_SRC:
		ret = snprintf(buf, len, "<src>");
		if (ret == -1)
			return -1;
		buffer_size(ret, &size, &len);
		break;
	case __ADDR_DST:
		ret = snprintf(buf+size, len, "<dst>");
		if (ret == -1)
			return -1;
		buffer_size(ret, &size, &len);
		break;
	}

	switch (tuple->l3protonum) {
	case AF_INET:
		ret = __snprintf_ipv4_xml(buf+size, len, tuple, type);
		if (ret == -1)
			return -1;
		buffer_size(ret, &size, &len);
		break;
	case AF_INET6:
		ret = __snprintf_ipv6_xml(buf+size, len, tuple, type);
		if (ret == -1)
			return -1;
		buffer_size(ret, &size, &len);
		break;
	}

	switch(type) {
	case __ADDR_SRC:
		ret = snprintf(buf+size, len, "</src>");
		if (ret == -1)
			return -1;
		buffer_size(ret, &size, &len);
		break;
	case __ADDR_DST:
		ret = snprintf(buf+size, len, "</dst>");
		if (ret == -1)
			return -1;
		buffer_size(ret, &size, &len);
		break;
	}

	return size;
}

static int __snprintf_proto_xml(char *buf,
				unsigned int len,
				const struct __nfct_tuple *tuple, 
				unsigned int type)
{
	int ret = 0;
	unsigned int size = 0;

	switch(tuple->protonum) {
	case IPPROTO_TCP:
	case IPPROTO_UDP:
	case IPPROTO_SCTP:
		if (type == __ADDR_SRC) {
			ret = snprintf(buf, len, "<sport>%u</sport>", 
				       tuple->l4src.tcp.port);
			if (ret == -1)
				return -1;
			buffer_size(ret, &size, &len);
		} else {
			ret = snprintf(buf, len, "<dport>%u</dport>",
				       tuple->l4dst.tcp.port);
			if (ret == -1)
				return -1;
			buffer_size(ret, &size, &len);
		}
		break;
	}

	return ret;
}

static int __snprintf_counters_xml(char *buf,
				   unsigned int len,
				   const struct nf_conntrack *ct,
				   unsigned int type)
{
	int ret;
	unsigned int size = 0;

	ret = snprintf(buf, len, "<packets>%llu</packets>",
		       ct->counters[type].packets);
	if (ret == -1)
		return -1;
	buffer_size(ret, &size, &len);

	ret = snprintf(buf+size, len, "<bytes>%llu</bytes>",
		       ct->counters[type].bytes);
	if (ret == -1)
		return -1;
	buffer_size(ret, &size, &len);

	return size;
}

static int __snprintf_tuple_xml(char *buf,
				unsigned int len,
				const struct nf_conntrack *ct,
				unsigned int dir)
{
	int ret;
	unsigned int size = 0;
	const struct __nfct_tuple *tuple = &ct->tuple[dir];

	ret = snprintf(buf, len, "<meta direction=\"%s\">",
		       dir == __DIR_ORIG ? "original" : "reply");
	if (ret == -1)
		return -1;
	buffer_size(ret, &size, &len);

	ret = snprintf(buf+size, len, 
		       "<layer3 protonum=\"%d\" protoname=\"%s\">",
		       tuple->l3protonum, __l3proto2str(tuple->l3protonum));
	if (ret == -1)
		return -1;
	buffer_size(ret, &size, &len);

	ret = __snprintf_addr_xml(buf+size, len, tuple, __DIR_ORIG);
	if (ret == -1)
		return -1;
	buffer_size(ret, &size, &len);

	ret = __snprintf_addr_xml(buf+size, len, tuple, __DIR_REPL);
	if (ret == -1)
		return -1;
	buffer_size(ret, &size, &len);

	ret = snprintf(buf+size, len, "</layer3>");
	if (ret == -1)
		return -1;
	buffer_size(ret, &size, &len);

	ret = snprintf(buf+size, len, 
		       "<layer4 protonum=\"%d\" protoname=\"%s\">",
		       tuple->protonum, __proto2str(tuple->protonum));
	if (ret == -1)
		return -1;
	buffer_size(ret, &size, &len);

	ret = __snprintf_proto_xml(buf+size, len, tuple, __DIR_ORIG);
	if (ret == -1)
		return -1;
	buffer_size(ret, &size, &len);

	ret = __snprintf_proto_xml(buf+size, len, tuple, __DIR_REPL);
	if (ret == -1)
		return -1;
	buffer_size(ret, &size, &len);

	ret = snprintf(buf+size, len, "</layer4>");
	if (ret == -1)
		return -1;
	buffer_size(ret, &size, &len);

	if (test_bit(ATTR_ORIG_COUNTER_PACKETS, ct->set) &&
	    test_bit(ATTR_ORIG_COUNTER_BYTES, ct->set)) {
		ret = snprintf(buf+size, len, "<counters>");
		if (ret == -1)
			return -1;
		buffer_size(ret, &size, &len);

		ret = __snprintf_counters_xml(buf+size, len, ct, dir);
		if (ret == -1)
			return -1;
		buffer_size(ret, &size, &len);

		ret = snprintf(buf+size, len, "</counters>");
		if (ret == -1)
			return -1;
		buffer_size(ret, &size, &len);
	}

	ret = snprintf(buf+size, len, "</meta>");
	if (ret == -1)
		return -1;
	buffer_size(ret, &size, &len);

	return size;
}

int __snprintf_conntrack_xml(char *buf,
			     unsigned int len,
			     const struct nf_conntrack *ct,
			     const unsigned int msg_type,
			     const unsigned int flags) 
{
	int ret = 0;
	unsigned int size = 0;

	switch(msg_type) {
		case NFCT_T_NEW:
			ret = snprintf(buf, len, "<flow type=\"new\">");
			break;
		case NFCT_T_UPDATE:
			ret = snprintf(buf, len, "<flow type=\"update\"");
			break;
		case NFCT_T_DESTROY:
			ret = snprintf(buf, len, "<flow type=\"destroy\"");
			break;
		default:
			ret = snprintf(buf, len, "<flow>");
			break;
	}

	if (ret == -1)
		return -1;
	buffer_size(ret, &size, &len);

	ret = __snprintf_tuple_xml(buf+size, len, ct, __DIR_ORIG);
	if (ret == -1)
		return -1;
	buffer_size(ret, &size, &len);

	ret = __snprintf_tuple_xml(buf+size, len, ct, __DIR_REPL);
	if (ret == -1)
		return -1;
	buffer_size(ret, &size, &len);

	if (test_bit(ATTR_TIMEOUT, ct->set) ||
	    test_bit(ATTR_MARK, ct->set) ||
	    test_bit(ATTR_USE, ct->set) ||
	    test_bit(ATTR_STATUS, ct->set)) {
		ret = snprintf(buf+size, len, 
			       "<meta direction=\"independent\">");
		if (ret == -1)
			return -1;
		buffer_size(ret, &size, &len);
	}

	if (test_bit(ATTR_TIMEOUT, ct->set)) {
		ret = snprintf(buf+size, len,
				"<timeout>%u</timeout>", ct->timeout);
		if (ret == -1)
			return -1;
		buffer_size(ret, &size, &len);
	}

	if (test_bit(ATTR_MARK, ct->set)) {
		ret = snprintf(buf+size, len, "<mark>%u</mark>", ct->mark);
		if (ret == -1)
			return -1;
		buffer_size(ret, &size, &len);
	}

	if (test_bit(ATTR_USE, ct->set)) {
		ret = snprintf(buf+size, len, "<use>%u</use>", ct->use);
		if (ret == -1)
			return -1;
		buffer_size(ret, &size, &len);
	}

	if (test_bit(ATTR_STATUS, ct->set)
	    && ct->status & IPS_ASSURED) {
		ret = snprintf(buf+size, len, "<assured/>");
		if (ret == -1)
			return -1;
		buffer_size(ret, &size, &len);
	}

	if (test_bit(ATTR_STATUS, ct->set) 
	    && !(ct->status & IPS_SEEN_REPLY)) {
		ret = snprintf(buf+size, len, "<unreplied/>");
		if (ret == -1)
			return -1;
		buffer_size(ret, &size, &len);
	}

	if (test_bit(ATTR_TIMEOUT, ct->set) ||
	    test_bit(ATTR_MARK, ct->set) ||
	    test_bit(ATTR_USE, ct->set) ||
	    test_bit(ATTR_STATUS, ct->set)) {
	    	ret = snprintf(buf+size, len, "</meta>");
		if (ret == -1)
			return -1;
		buffer_size(ret, &size, &len);
	}

	ret = snprintf(buf+size, len, "</flow>");
	if (ret == -1)
		return -1;
	buffer_size(ret, &size, &len);

	return size;
}