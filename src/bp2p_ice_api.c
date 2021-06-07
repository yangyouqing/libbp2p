#include <stdio.h>
#include <unistd.h>
#include "bp2p_ice_api.h"
#include "ice_common.h"
#include "ev.h"
static ice_role_t ice_role = ICE_ROLE_UNKNOWN;
int bp2p_ice_init(ice_cfg_t *cfg)
{
    ice_role = cfg->role;

    if (0 == strlen(cfg->my_channel)) {
        printf("ice channel is needed\n");
        return 0;
    }
    if (ICE_ROLE_CLIENT == cfg->role) {
        return ice_client_init(cfg);
    } else if (ICE_ROLE_PEER == cfg->role) {
        return ice_peer_init(cfg);
    }
    return -1;
}


int bp2p_ice_send(void *data, int len)
{
    if (ICE_ROLE_CLIENT == ice_role) {
        ice_client_send_data(data, len);
    } else if (ICE_ROLE_PEER == ice_role) {
        ice_peer_send_data(data, len);
    }
    return -1;
}

int bp2p_ice_get_valid_peer(struct sockaddr* peer)
{
    if (NULL == peer) {
        return -1;
    }
    
    if (ICE_ROLE_CLIENT == ice_role) {
        return ice_client_get_valid_peer(peer);
    } else if (ICE_ROLE_PEER == ice_role) {
        return ice_peer_get_valid_peer(peer);
    }
    return -1;
}


