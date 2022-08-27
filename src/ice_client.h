#ifndef JUICE_ICE_CLIENT_H
#define JUICE_ICE_CLIENT_H

#include "p2p_api.h"

int ice_client_init(ice_cfg_t *ice_cfg);
int ice_client_start_nego(ice_cfg_t *cfg);
int ice_client_send_data(void *data, int len);
int ice_client_get_valid_localaddr(char *ip, unsigned int *port);
int ice_client_get_valid_peeraddr(char *ip, unsigned int *port);

int ice_client_get_valid_peer(struct sockaddr* dst);

void ice_client_clear_signaling_info();
int ice_client_conn_signalling_srv(struct ev_loop *loop, char *ip, unsigned short port);
void ice_client_start_count_timer(struct ev_loop *loop);

#endif