#pragma once

#include <utils/linux.hpp>

struct msghdr {
	void *msg_name;
	unsigned int msg_namelen;
	iovec *msg_iov;
#if __INTPTR_WIDTH__ == 64 && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	int __pad0;
#endif
	int msg_iovlen;
#if __INTPTR_WIDTH__ == 64 && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	int __pad0;
#endif
	void *msg_control;
#if __INTPTR_WIDTH__ == 64 && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	int __pad1;
#endif
	uint32_t msg_controllen;
#if __INTPTR_WIDTH__ == 64 && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	int __pad1;
#endif
	int msg_flags;
};

struct mmsghdr {
	struct msghdr msg_hdr;
	unsigned int  msg_len;
};

struct cmsghdr {
#if __INTPTR_WIDTH__ == 64 && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	int __pad;
#endif
	unsigned int cmsg_len;
#if __INTPTR_WIDTH__ == 64 && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	int __pad;
#endif
	int cmsg_level;
	int cmsg_type;
};

#define SCM_RIGHTS 1

#define SCM_CREDENTIALS 2

#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2

#ifndef SOCK_STREAM
#define SOCK_STREAM    1
#define SOCK_DGRAM     2
#endif

#define SOCK_RAW       3
#define SOCK_SEQPACKET 5

#define SOCK_RDM       4
#define SOCK_DCCP      6
#define SOCK_PACKET    10

#ifndef SOCK_CLOEXEC
#define SOCK_CLOEXEC   02000000
#define SOCK_NONBLOCK  04000
#endif

#define AF_UNSPEC       0
#define AF_UNIX         1
#define AF_INET         2
#define AF_INET6        10

#define PF_UNSPEC       0
#define PF_LOCAL        1
#define PF_UNIX         PF_LOCAL
#define PF_FILE         PF_LOCAL
#define PF_INET         2
#define PF_AX25         3
#define PF_IPX          4
#define PF_APPLETALK    5
#define PF_NETROM       6
#define PF_BRIDGE       7
#define PF_ATMPVC       8
#define PF_X25          9
#define PF_INET6        10
#define PF_ROSE         11
#define PF_DECnet       12
#define PF_NETBEUI      13
#define PF_SECURITY     14
#define PF_KEY          15
#define PF_NETLINK      16
#define PF_ROUTE        PF_NETLINK
#define PF_PACKET       17
#define PF_ASH          18
#define PF_ECONET       19
#define PF_ATMSVC       20
#define PF_RDS          21
#define PF_SNA          22
#define PF_IRDA         23
#define PF_PPPOX        24
#define PF_WANPIPE      25
#define PF_LLC          26
#define PF_IB           27
#define PF_MPLS         28
#define PF_CAN          29
#define PF_TIPC         30
#define PF_BLUETOOTH    31
#define PF_IUCV         32
#define PF_RXRPC        33
#define PF_ISDN         34
#define PF_PHONET       35
#define PF_IEEE802154   36
#define PF_CAIF         37
#define PF_ALG          38
#define PF_NFC          39
#define PF_VSOCK        40
#define PF_KCM          41
#define PF_QIPCRTR      42
#define PF_SMC          43
#define PF_XDP          44
#define PF_MAX          45

#define MSG_PROXY     0x0010
#define MSG_DONTWAIT  0x0040
#define MSG_FIN       0x0200
#define MSG_SYN       0x0400
#define MSG_CONFIRM   0x0800
#define MSG_RST       0x1000
#define MSG_ERRQUEUE  0x2000
#define MSG_MORE      0x8000
#define MSG_WAITFORONE 0x10000
#define MSG_BATCH     0x40000
#define MSG_ZEROCOPY  0x4000000
#define MSG_FASTOPEN  0x20000000
#define SOMAXCONN       128

#define MSG_OOB       0x0001
#define MSG_PEEK      0x0002
#define MSG_DONTROUTE 0x0004
#define MSG_CTRUNC    0x0008
#define MSG_TRUNC     0x0020
#define MSG_EOR       0x0080
#define MSG_WAITALL   0x0100
#define MSG_NOSIGNAL  0x4000
#define MSG_CMSG_CLOEXEC 0x40000000

#define SOL_SOCKET      1
#define SOL_IP          0
#define SOL_IPV6        41
#define SOL_ICMPV6      58

#define SOL_RAW         255
#define SOL_DECNET      261
#define SOL_X25         262
#define SOL_PACKET      263
#define SOL_ATM         264
#define SOL_AAL         265
#define SOL_IRDA        266
#define SOL_NETBEUI     267
#define SOL_LLC         268
#define SOL_DCCP        269
#define SOL_NETLINK     270
#define SOL_TIPC        271
#define SOL_RXRPC       272
#define SOL_PPPOL2TP    273
#define SOL_BLUETOOTH   274
#define SOL_PNPIPE      275
#define SOL_RDS         276
#define SOL_IUCV        277
#define SOL_CAIF        278
#define SOL_ALG         279
#define SOL_NFC         280
#define SOL_KCM         281
#define SOL_TLS         282
#define SOL_XDP         283