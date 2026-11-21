/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef LWIPOPTS_H
#define LWIPOPTS_H

#define NO_SYS                          0
#define LWIP_SOCKET                     0
#define LWIP_NETCONN                    1
#define LWIP_TCPIP_CORE_LOCKING         1
#define SYS_LIGHTWEIGHT_PROT            1
#define LWIP_NETCONN_SEM_PER_THREAD     0

#define LWIP_IPV4                       1
#define LWIP_IPV6                       1
#define LWIP_IPV6_MLD                   0
#define LWIP_IPV6_FRAG                  0
#define LWIP_IPV6_REASS                 0
#define LWIP_IPV6_DUP_DETECT_ATTEMPTS   1
#define LWIP_ND6_NUM_NEIGHBORS          4
#define LWIP_ND6_NUM_DESTINATIONS       4
#define LWIP_ND6_NUM_PREFIXES           2
#define LWIP_ND6_NUM_ROUTERS            2
#define MEMP_NUM_ND6_QUEUE              4
#define LWIP_ARP                        1
#define LWIP_ETHERNET                   1
#define LWIP_ICMP                       1
#define LWIP_RAW                        1
#define LWIP_UDP                        1
#define LWIP_TCP                        1
#define LWIP_DHCP                       1
#define LWIP_DHCP_DOES_ACD_CHECK        0
#define LWIP_ACD                        0
#define LWIP_AUTOIP                     0
#define LWIP_DNS                        1
#define DNS_MAX_SERVERS                 2
#define DNS_TABLE_SIZE                  4
#define DNS_MAX_NAME_LENGTH             64
#define LWIP_DHCP_PROVIDE_DNS_SERVERS   2
#define LWIP_IGMP                       0
#define LWIP_HAVE_LOOPIF                1
#define LWIP_NETIF_LOOPBACK             1
#define LWIP_LOOPIF_MULTICAST           0
#define LWIP_SO_RCVTIMEO                1
#define LWIP_SO_SNDTIMEO                0
#define LWIP_NETIF_STATUS_CALLBACK      0
#define LWIP_NETIF_LINK_CALLBACK        0

#define LWIP_STATS                      0
#define LWIP_NOASSERT                   1
#define LWIP_DEBUG                      0
#define LWIP_PROVIDE_ERRNO              1
#define LWIP_NO_INTTYPES_H              1
#define LWIP_NO_CTYPE_H                 1

#define MEM_LIBC_MALLOC                 0
#define MEMP_MEM_MALLOC                 0
#define MEM_ALIGNMENT                   4
#define MEM_SIZE                        (8 * 1024)

#define MEMP_NUM_PBUF                   8
#define MEMP_NUM_UDP_PCB                7
#define MEMP_NUM_TCP_PCB                4
#define MEMP_NUM_TCP_PCB_LISTEN         2
#define MEMP_NUM_TCP_SEG                16
#define MEMP_NUM_NETBUF                 6
#define MEMP_NUM_NETCONN                8
#define MEMP_NUM_RAW_PCB                2
#define MEMP_NUM_SYS_TIMEOUT            (LWIP_NUM_SYS_TIMEOUT_INTERNAL + 2)
#define PBUF_POOL_SIZE                  12
#define PBUF_POOL_BUFSIZE               LWIP_MEM_ALIGN_SIZE(592)

#define TCP_MSS                         536
#define TCP_WND                         (4 * TCP_MSS)
#define TCP_SND_BUF                     (4 * TCP_MSS)
#define TCP_SND_QUEUELEN                ((2 * TCP_SND_BUF) / TCP_MSS)

#define TCPIP_THREAD_NAME               "tcpip"
#define TCPIP_THREAD_STACKSIZE          3072
#define TCPIP_THREAD_PRIO               2
#define TCPIP_MBOX_SIZE                 8
#define DEFAULT_RAW_RECVMBOX_SIZE       4
#define DEFAULT_UDP_RECVMBOX_SIZE       6
#define DEFAULT_TCP_RECVMBOX_SIZE       6
#define DEFAULT_ACCEPTMBOX_SIZE         4

#define LWIP_RAND()                     (br_lwip_rand())

#ifndef __ASSEMBLER__
#include <stdint.h>
uint32_t br_lwip_rand(void);
#endif

#endif
