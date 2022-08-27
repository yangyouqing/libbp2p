#ifndef JUICE_ICE_PEER_H
#define JUICE_ICE_PEER_H

#include "p2p_api.h"

int ice_peer_init(ice_cfg_t *ice_cfg);
int ice_peer_start_nego(ice_cfg_t *cfg);
int ice_peer_send_data(void *data, int len);

int ice_peer_get_valid_localaddr(char *ip, unsigned int *port);
int ice_peer_get_valid_peeraddr(char *ip, unsigned int *port);

int ice_peer_get_valid_peer(struct sockaddr* dst);

void ice_peer_clear_signaling_info();
int ice_peer_conn_signalling_srv(struct ev_loop *loop, char *ip, unsigned short port);
void ice_peer_start_count_timer(struct ev_loop *loop);
#endif