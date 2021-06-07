#include <stdio.h>
#include <sys/types.h>
#if !defined(WIN32)
#include <sys/socket.h>
#include <netdb.h>
#endif
#if defined(__VMS)
#include <ioctl.h>
#endif
#include <fcntl.h>
#include "ice_common.h"
#include <string.h>

int make_publish_msg(char* buf, int max_buf_size, int msg_type, const char* msg)
{
    int new_msg_len = 0;
    int msg_type_net_order = htonl(msg_type);
    memcpy(buf, &msg_type_net_order, sizeof (msg_type));
    new_msg_len += sizeof (msg_type);
    if (NULL != msg) {
        strcpy (buf + new_msg_len, msg);
        new_msg_len += strlen(msg);
    }
    buf[new_msg_len] = 0;
    new_msg_len += 1;
    return new_msg_len;
}



/*
    A template for opening a non-blocking POSIX socket.
*/
int open_nb_socket(const char* addr, const char* port) {
    struct addrinfo hints = {0};

    hints.ai_family = AF_UNSPEC; /* IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* Must be TCP */
    int sockfd = -1;
    int rv;
    struct addrinfo *p, *servinfo;

    /* get address information */
    rv = getaddrinfo(addr, port, &hints, &servinfo);
    if(rv != 0) {
        fprintf(stderr, "Failed to open socket (getaddrinfo): %s\n", gai_strerror(rv));
        return -1;
    }

    /* open the first possible socket */
    for(p = servinfo; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) continue;

        /* connect to server */
        rv = connect(sockfd, p->ai_addr, p->ai_addrlen);
        if(rv == -1) {
          close(sockfd);
          sockfd = -1;
          continue;
        }
        break;
    }  

    /* free servinfo */
    freeaddrinfo(servinfo);

    /* make non-blocking */
#if !defined(WIN32)
    if (sockfd != -1) fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);
#else
    if (sockfd != INVALID_SOCKET) {
        int iMode = 1;
        ioctlsocket(sockfd, FIONBIO, &iMode);
    }
#endif
#if defined(__VMS)
    /* 
        OpenVMS only partially implements fcntl. It works on file descriptors
        but silently fails on socket descriptors. So we need to fall back on
        to the older ioctl system to set non-blocking IO
    */
    int on = 1;                 
    if (sockfd != -1) ioctl(sockfd, FIONBIO, &on);
#endif

    /* return the new socket fd */
    return sockfd;
}

