# Networking

bedrock[RTOS] has a socket-style API on top of [lwIP](https://github.com/lwip-tcpip/lwip) 2.2.1. The stack is an optional port (`ports/lwip/`, see [ports](ports.md)): nothing links it unless the application does. It needs no dynamic allocation, lwIP uses its own static heap and pools.

## Layers

```
application          br_net_socket / bind / send / recv / ping
ports/lwip/          br_net.c, br_net_sock.c       sockets on lwIP netconn
                     sys_arch.c                    lwIP threads, mailboxes, mutexes on kernel primitives
                     tcpip thread, netrx task      the two tasks the stack runs in
br_netif_t           send, recv, link_up           network interface contract
drivers, boards      Stellaris Ethernet, virtio-net, mock
```

Two tasks run the stack: lwIP's `tcpip` thread at priority 2 and `netrx` at priority 3, each with a 3 KiB stack. `netrx` reads the received frames of every interface. An interface whose driver sets `irq` wakes it from the receive interrupt: the driver calls `rx_notify()`, which gives a semaphore, and `netrx` sleeps on that semaphore for at most `BR_NET_IRQ_FALLBACK_MS` (200 ms, a safety net). An interface without an interrupt, such as the mock, is polled every `BR_NET_POLL_MS` (5 ms) instead. `br_net_rx_wakeups()` counts the interrupt wakeups.

## Network interface: `include/bedrock/br_netif.h`

```c
typedef struct {
    br_err_t (*send)(struct br_netif *netif, const void *frame, size_t len);
    int      (*recv)(struct br_netif *netif, void *buf, size_t size);
    bool     (*link_up)(struct br_netif *netif);
} br_netif_ops_t;

typedef struct br_netif {
    const char            *name;
    uint8_t                mac[6];
    uint16_t               mtu;
    const br_netif_ops_t  *ops;
    void                  *priv;
    bool                   irq;
    void                 (*rx_notify)(struct br_netif *netif);
} br_netif_t;
```

`send` takes one Ethernet frame without the FCS. `recv` copies one received frame into `buf` and returns its length, `0` when there is none, or a negative `br_err_t`. A frame is at most `BR_NETIF_FRAME_MAX` (1536) bytes.

A driver with a receive interrupt sets `irq` and calls `rx_notify(netif)` from the interrupt handler when `rx_notify` is not `NULL`. The stack fills `rx_notify` in `br_net_netif_add`.

`br_hal_netif_default()` returns the board interface, or `NULL` if the board has none.

| Target | Interface |
|--------|-----------|
| QEMU LM3S6965 | Stellaris Ethernet MAC (`boards/qemu-cortex-m3/br_netif_enet.c`) |
| QEMU RISC-V `virt` | virtio-net on virtio-mmio (`boards/qemu-riscv32-virt/br_netif_virtio.c`) |
| STM32F103, RP2040, host | none |

`drivers/br_netif_mock.c` is an interface in software for tests. `br_netif_mock_inject` queues a frame for the stack to receive, `br_netif_mock_take` removes a frame the stack sent.

### Drivers

The Stellaris MAC driver reads the MAC address from the device and enables the receive interrupt (IRQ 42). The virtio-net driver takes the interrupt through the PLIC (source `1 + slot`) and supports the modern (version 2) transport only, with one 8-entry queue for each direction, so QEMU must run with `-global virtio-mmio.force-legacy=false`. Both drivers send synchronously.

## API: `ports/lwip/br_net.h`

```c
br_err_t br_net_init(void);
br_err_t br_net_netif_add(br_netif_t *netif, uint32_t ip, uint32_t mask, uint32_t gw);
br_err_t br_net_wait_ip(br_netif_t *netif, br_time_t timeout);
br_err_t br_net_ifinfo(uint32_t index, br_net_ifinfo_t *info);

int      br_net_socket(int type);
br_err_t br_net_bind(int fd, uint32_t ip, uint16_t port);
br_err_t br_net_listen(int fd, int backlog);
int      br_net_accept(int fd, br_time_t timeout, uint32_t *ip, uint16_t *port);
br_err_t br_net_connect(int fd, uint32_t ip, uint16_t port);
int      br_net_send(int fd, const void *buf, size_t len);
int      br_net_recv(int fd, void *buf, size_t len, br_time_t timeout);
int      br_net_sendto(int fd, const void *buf, size_t len, uint32_t ip, uint16_t port);
int      br_net_recvfrom(int fd, void *buf, size_t len, br_time_t timeout,
                         uint32_t *ip, uint16_t *port);
br_err_t br_net_close(int fd);

br_err_t br_net_ping(uint32_t ip, br_time_t timeout, uint32_t *rtt_us);

br_err_t br_net_resolve(const char *name, uint32_t *ip);
br_err_t br_net_set_dns(uint32_t ip);
uint32_t br_net_dns(void);

void     br_shell_register_net(void);
```

IPv4 addresses are `uint32_t` in host byte order. `BR_IP4(a, b, c, d)` builds one.

| Function | Behavior |
|----------|----------|
| `br_net_init` | Start the stack. Call once from a task, `BR_ERR_BUSY` the second time. The loopback interface `lo0` (127.0.0.1) exists after this. |
| `br_net_netif_add` | Add an interface. `ip` of `0` starts DHCP. The first interface added becomes the default route. At most `BR_NET_MAX_NETIFS` (2). |
| `br_net_wait_ip` | Block until the interface has an address, `BR_ERR_TIMEOUT` otherwise. |
| `br_net_ifinfo` | The `index`-th interface: name, MAC, address, mask, gateway, state. `BR_ERR_NOENT` past the last one. The order is the one lwIP keeps, the newest interface first. |
| `br_net_socket` | `type` is `BR_NET_DGRAM` (UDP) or `BR_NET_STREAM` (TCP). Returns a descriptor, `BR_ERR_NOMEM` when all `BR_NET_MAX_SOCKETS` (6) are in use. |
| `br_net_bind` | Address `0` means any. Port `0` picks one. `BR_ERR_BUSY` if the port is used. |
| `br_net_accept` | Returns a new descriptor for the connection and, if given, the peer address. |
| `br_net_send` | UDP: sends to the address set with `br_net_connect`. TCP: returns the bytes queued. |
| `br_net_recv`, `br_net_recvfrom` | Wait up to `timeout`, `BR_TIME_INFINITE` for no limit. UDP returns one datagram and drops what does not fit in `len`. TCP returns up to `len` bytes of the stream and `0` when the peer has closed. `BR_ERR_TIMEOUT` on timeout. |
| `br_net_close` | Close and free the descriptor. |
| `br_net_resolve` | Turn a name into an IPv4 address. A dotted address is returned as is, without a query. Blocks until the answer or until lwIP gives up, which takes up to about four seconds without a reply. `BR_ERR_NOENT` if the name does not exist, `BR_ERR_TIMEOUT` if the server does not answer, `BR_ERR_INVALID` for a name that is empty or 64 characters or longer. Answers are cached for their lifetime, in a table of 4 names. |
| `br_net_set_dns` | Set the DNS server, for an interface with a static address. DHCP sets it by itself. |
| `br_net_dns` | The DNS server in use, or `0` if there is none. |
| `br_net_ping` | Send an ICMP echo and wait for the reply. `BR_ERR_TIMEOUT` if there is none, `rtt_us` gets the round trip time. |

Timeouts are rounded up to whole milliseconds, and a timeout of `0` waits 1 ms. The calls must not be made from an ISR.

Descriptors are not safe to use from two tasks at once. Different descriptors can be used from different tasks.

## Configuration

`ports/lwip/lwipopts.h` holds the lwIP settings: IPv4 only, UDP, TCP, ICMP, DHCP, DNS and ARP, no IGMP, no sockets API, an 8 KiB heap, 12 packet buffers of 592 bytes and TCP segments of 536 bytes. The sizes are meant for a 64 KiB part and are the first thing to change for a larger one. The constants in `br_net.h` (`BR_NET_MAX_SOCKETS`, `BR_NET_MAX_NETIFS`, `BR_NET_POLL_MS`) can be overridden at build time.

`test_net` needs about 52 KiB of RAM and runs on the LM3S6965, so the stack fits in 64 KiB. The STM32F103C8 has 20 KiB and does not run it.

## Shell commands

`br_shell_register_net()` registers:

| Command | Behavior |
|---------|----------|
| `ifconfig` | Each interface with state, MAC, address, mask and gateway |
| `ping <a.b.c.d> [count]` | Ping 1 to 100 times, 1 second timeout, then print how many replies arrived |
| `nslookup <name>` | Resolve a name and print the address |

## Example: UDP echo server

`examples/net_udp_echo.c` gets an address by DHCP, runs an echo server on UDP port 7 and a shell with `ifconfig` and `ping`.

```
chorus run-udp-echo
chorus run-udp-echo-riscv32
```

Both start QEMU with user networking and forward host UDP port 7777 to the guest, so from another terminal:

```
echo hello | nc -u -w1 127.0.0.1 7777
```

## Tests

| Test | Targets | Covers |
|------|---------|--------|
| `test_net` | host, LM3S6965, RISC-V, RP2040 | Loopback UDP and TCP, socket limits, ARP and ICMP through the mock interface, ping, shell commands, DNS against a peer that answers queries |
| `test_net_qemu` | LM3S6965 and RISC-V in QEMU | The real drivers: DHCP lease, ping of the gateway, the DNS server from DHCP, the receive interrupt wakeups, a datagram sent through it. Prints `SKIP` on a target without an interface |

`test_net_qemu` needs the QEMU options above (`-nic user,model=stellaris` or the virtio-net options). `test_net_rp2040` is built but is not run in CI.

## Limits

- No IPv6, no TLS. DNS has one server at a time for `br_net_set_dns`, and only A records.
- Receive is interrupt driven on the two QEMU drivers. An interface without an interrupt is polled every 5 ms.
- One TCP connection at a time per descriptor, with `BR_NET_MAX_SOCKETS` in the whole system.
- The drivers were run on QEMU only. The Stellaris driver follows the QEMU model and the StellarisWare driver as far as the frame format goes, and has not been tried on real hardware.
- The virtio driver does not support the legacy transport.
