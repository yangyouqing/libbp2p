//add by yangyouqing@20210513
/*
    boring p2p ice api
*/
#ifndef BP2P_ICE_API_H
#define BP2P_ICE_API_H
#include <ev.h>
#include <arpa/inet.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ice_role{
    ICE_ROLE_UNKNOWN,
    ICE_ROLE_CLIENT,
    ICE_ROLE_PEER
}ice_role_t;

typedef enum ice_status{
    ICE_STATUS_INIT,
    ICE_STATUS_CONNECT_SIGNALLING_SRV,
    ICE_STATUS_CONNECT_PEER,         // client only status
    ICE_STATUS_CONNECT_ICE_SRV,
    ICE_STATUS_CONNECTIVITY_CHECK,
    ICE_STATUS_COMPLETE,
    ICE_STATUS_FAILED
}ice_status_t;

typedef struct ice_cfg{
    struct ev_loop *loop;
    ice_role_t role;
    int     overtime;                // in micro second
    char    my_channel[64];          // must be a random val
    char    peer_channel[64];        //used by mqtt topic
    char*    signalling_srv;
	char*    stun_srv;
	char*    turn_srv;
	char*    turn_username;
	char*    turn_password;
	int      turn_fingerprint;
    char     local_ip[24];          //out param
    unsigned short lport;           //out param
    char     remote_ip[24];         //out param
    unsigned short rport;           //out param
    void (*cb_on_rx_pkt)(void * pkt, int size, struct sockaddr* src, struct sockaddr* dest);
    void (*cb_on_idle_running)();  // just for app level use, maybe send pkt should be reside in same thread with recv thread
    void (*cb_on_status_change)(ice_status_t s);
}ice_cfg_t;

int p2p_start(ice_cfg_t *cfg);

// maybe not use
//int p2p_get_status();
//int p2p_get_valid_localaddr(char *ip, unsigned int *port);
//int p2p_get_valid_peeraddr(char *ip, unsigned int *port);
//int p2p_get_valid_peer(struct sockaddr* peer);
//int p2p_send(void *data, int len);
//int p2p_get_elapsed_time(int *microsec);


#ifdef __cplusplus
}
#endif

#endif

