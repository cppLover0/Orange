
#include <cstdint>

#include <generic/vfs/vfs.hpp>
#include <generic/locks/spinlock.hpp>

#pragma once

typedef unsigned short sa_family_t;

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

#define AF_UNSPEC       PF_UNSPEC
#define AF_LOCAL        PF_LOCAL
#define AF_UNIX         AF_LOCAL
#define AF_FILE         AF_LOCAL
#define AF_INET         PF_INET
#define AF_AX25         PF_AX25
#define AF_IPX          PF_IPX
#define AF_APPLETALK    PF_APPLETALK
#define AF_NETROM       PF_NETROM
#define AF_BRIDGE       PF_BRIDGE
#define AF_ATMPVC       PF_ATMPVC
#define AF_X25          PF_X25
#define AF_INET6        PF_INET6
#define AF_ROSE         PF_ROSE
#define AF_DECnet       PF_DECnet
#define AF_NETBEUI      PF_NETBEUI
#define AF_SECURITY     PF_SECURITY
#define AF_KEY          PF_KEY
#define AF_NETLINK      PF_NETLINK
#define AF_ROUTE        PF_ROUTE
#define AF_PACKET       PF_PACKET
#define AF_ASH          PF_ASH
#define AF_ECONET       PF_ECONET
#define AF_ATMSVC       PF_ATMSVC
#define AF_RDS          PF_RDS
#define AF_SNA          PF_SNA
#define AF_IRDA         PF_IRDA
#define AF_PPPOX        PF_PPPOX
#define AF_WANPIPE      PF_WANPIPE
#define AF_LLC          PF_LLC
#define AF_IB           PF_IB
#define AF_MPLS         PF_MPLS
#define AF_CAN          PF_CAN
#define AF_TIPC         PF_TIPC
#define AF_BLUETOOTH    PF_BLUETOOTH
#define AF_IUCV         PF_IUCV
#define AF_RXRPC        PF_RXRPC
#define AF_ISDN         PF_ISDN
#define AF_PHONET       PF_PHONET
#define AF_IEEE802154   PF_IEEE802154
#define AF_CAIF         PF_CAIF
#define AF_ALG          PF_ALG
#define AF_NFC          PF_NFC
#define AF_VSOCK        PF_VSOCK
#define AF_KCM          PF_KCM
#define AF_QIPCRTR      PF_QIPCRTR
#define AF_SMC          PF_SMC
#define AF_XDP          PF_XDP
#define AF_MAX          PF_MAX

struct sockaddr {
	sa_family_t sa_family;
	char sa_data[14];
};

struct sockaddr_un {
	sa_family_t sun_family;
	char sun_path[108];
};

typedef struct socket_pending_obj {
	locks::spinlock is_accepted;
	userspace_fd_t* son;
	struct socket_pending_obj* next;
} socket_pending_obj_t;

typedef struct socket_node {
	char path[128];
	char is_used;
	socket_pending_obj_t* pending_list;
	struct socket_node* next;
} socket_node_t;

class sockets {
public:
	static char is_exists(char* path);
	static int bind(userspace_fd_t* fd, struct sockaddr_un* path);
	static int connect(userspace_fd_t* fd, struct sockaddr_un* path);
	static int accept(userspace_fd_t* fd, struct sockaddr_un* path);
	static void init();
};