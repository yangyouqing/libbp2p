/* $Id$ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ev.h>
#include "umqtt/umqtt.h"
#include "ice_common.h"
#include "juice/juice.h"
#define BUFFER_SIZE 4096


ice_cfg_t *icepeer_cfg = NULL;
juice_agent_t *agent = NULL;
static struct ev_timer counter_timer;    // calc eclipse time
static ev_async async_watcher;
static juice_state_t local_status = JUICE_STATE_DISCONNECTED; 

// Agent 2: on state changed
static void on_state_changed(juice_agent_t *agent, juice_state_t state, void *user_ptr) {
	printf("State 2: %s\n", juice_state_to_string(state));
    local_status = state;
	if (state == JUICE_STATE_CONNECTED) {
		// Agent 2: on connected, send a message
		//const char *message = "peer state connected";
		//juice_send(agent, message, strlen(message));
	} else if (state == JUICE_STATE_COMPLETED) {
	    printf ("ICE peer nego succeed\n");
        const char *message = "peer state completed";
		juice_send(agent, message, strlen(message));

        ice_peer_get_valid_localaddr(icepeer_cfg->local_ip, &icepeer_cfg->lport);
        ice_peer_get_valid_peeraddr(icepeer_cfg->remote_ip, &icepeer_cfg->rport);


	   if (icepeer_cfg && icepeer_cfg->cb_on_status_change) {
            icepeer_cfg->cb_on_status_change(ICE_STATUS_COMPLETE);
       }
       ev_async_send(icepeer_cfg->loop, &async_watcher);
	}
}



// Agent 2: on local candidates gathering done
static void on_gathering_done(juice_agent_t *agent, void *user_ptr) {
	printf("Gathering done \n");

	// Agent 2: Generate local description
//	char sdp2[JUICE_MAX_SDP_STRING_LEN];
//	juice_get_local_description(agent2, sdp2, JUICE_MAX_SDP_STRING_LEN);
//	printf("Local description 2:\n%s\n", sdp2);

	// Agent 1: Receive description from agent 2
//	juice_set_remote_description(agent1, sdp2);
}

// Agent 2: on message received
static void on_recv(juice_agent_t *agent, const char *data, size_t size, void *user_ptr) {
    struct sockaddr *src = NULL;
    struct sockaddr *dest = NULL;
    juice_get_selected_pair(agent, &src, &dest);
    if (icepeer_cfg && icepeer_cfg->cb_on_rx_pkt && NULL != src && NULL != dest) {
        icepeer_cfg->cb_on_rx_pkt(data, (int)size, (struct sockaddr*)src, (struct sockaddr*)dest);
    }
}

static struct ev_timer reconnect_timer;
static struct ev_timer send_timer;


static void do_send(struct ev_loop *loop, struct ev_timer *w, int revents)
{
     const char *msg = "[from peer]......\n";
    if (JUICE_STATE_COMPLETED == juice_get_state(agent)) {
        juice_send(agent, msg, strlen(msg)+1);
        printf ("juice-peer sent\n");
    }
}

static void do_overtime(struct ev_loop *loop, struct ev_timer *w, int revents)
{
    printf ("do_overtime\n");

    ev_break(loop, EVBREAK_ALL);
    ice_peer_clear_signaling_info();

    if (NULL != agent) {
        juice_destroy(agent);
        agent = NULL;
    }
}

void ice_peer_start_count_timer(struct ev_loop *loop)
{
    ev_timer_start(loop, &counter_timer);
}

static void signal_cb(struct ev_loop *loop, ev_signal *w, int revents)
{
    ev_break(loop, EVBREAK_ALL);
}

static void async_cb(EV_P_ ev_async *w, int revents)
{
    printf("quit\n");
    ev_break(icepeer_cfg->loop, EVBREAK_ALL);
}

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [option]\n"
        "      -h host      # Default is 'localhost'\n"
        "      -p port      # Default is 1883\n"
        "      -i ClientId  # Default is 'libumqtt-Test\n"
        "      -s           # Use ssl\n"
        "      -u           # Username\n"
        "      -P           # Password\n"
        "      -a           # Auto reconnect to the server\n"
        "      -d           # enable debug messages\n"
        , prog);
    exit(1);
}
// cpy end ...


int ice_peer_init(ice_cfg_t *ice_cfg)
{
    if (NULL == ice_cfg) {
        return -1;
    }

    icepeer_cfg = ice_cfg;            
    struct ev_loop* loop = ice_cfg->loop;
    if (NULL == loop) {
        return 0;
    }

//    if (!cfg.options.client_id)
//        cfg.options.client_id = "libumqtt-Test";
    juice_turn_server_t turn_server;
	memset(&turn_server, 0, sizeof(turn_server));
	turn_server.host = ice_cfg->turn_srv;
	turn_server.port = 3478;
	turn_server.username = ice_cfg->turn_username;
	turn_server.password = ice_cfg->turn_password;
	

	// Agent 2: Create agent
	juice_config_t config;
	memset(&config, 0, sizeof(config));
	config.cb_state_changed = on_state_changed;
	config.cb_gathering_done = on_gathering_done;
	config.cb_recv = on_recv;

	config.user_ptr = NULL;

	// Use the same TURN server
	config.turn_servers = &turn_server;
	config.turn_servers_count = 1;

	// Port range example
	config.local_port_range_begin = 60000;
	config.local_port_range_end = 61000;
    juice_set_log_level(JUICE_LOG_LEVEL_INFO);
    config.user_ptr = (void *)1;
	agent = juice_create(&config);

	// Agent 1: Gather candidates
	juice_gather_candidates(agent);

    ice_peer_conn_signalling_srv(loop, ice_cfg->signalling_srv, 0);

     if (ice_cfg->overtime >= 0) {
        float overtime = ice_cfg->overtime / 1000;
        ev_timer_init(&counter_timer, do_overtime, overtime, 0.0);
    }
    
    ev_async_init(&async_watcher, async_cb);
    ev_async_start(loop, &async_watcher);

    umqtt_log_info("libumqttc version %s\n", UMQTT_VERSION_STRING);
    ev_run(ice_cfg->loop, 0);

    if (local_status == JUICE_STATE_COMPLETED) {
        return 1;
    } else if (local_status == JUICE_STATE_FAILED) {
        return 0;
    }
    return -1;
}


int ice_peer_send_data(void *data, int len)
{
    if (JUICE_STATE_COMPLETED !=  juice_get_state(agent)) {
        return -1;
    }
    return juice_send(agent, data, len);
}

int ice_peer_get_valid_peer(struct sockaddr* dst)
{
    return 0;
}

int ice_peer_get_valid_localaddr(char *ip, unsigned int *port)
{
    if (NULL == ip || NULL == port) {
        return -1;
    }

    if (JUICE_STATE_COMPLETED != juice_get_state(agent)) {
        return -1;
    }

    struct sockaddr *tmp = NULL;
    juice_get_selected_pair(agent, &tmp, NULL);
    if (NULL == tmp) {
        return -1;
    }

    unsigned short src_port = -1;
    char *src_addr = NULL;    
    
    struct sockaddr_in *sin_src = (struct sockaddr_in *)tmp;
    src_port = ntohs(sin_src->sin_port);
    src_addr = inet_ntoa(sin_src->sin_addr);

    *port = src_port;
    strcpy(ip, src_addr);
    return 0;
}

int ice_peer_get_valid_peeraddr(char *ip, unsigned int *port)
{
    if (NULL == ip || NULL == port) {
        return -1;
    }

    if (JUICE_STATE_COMPLETED != juice_get_state(agent)) {
        return -1;
    }

    struct sockaddr *tmp = NULL;
    juice_get_selected_pair(agent, NULL, &tmp);
    if (NULL == tmp) {
        return -1;
    }

    unsigned short dst_port = -1;
    char *dst_addr = NULL;

    struct sockaddr_in *sin = (struct sockaddr_in *)tmp;
    dst_port = ntohs(sin->sin_port); 
    dst_addr = inet_ntoa(sin->sin_addr);
    
    *port = dst_port;
    strcpy(ip, dst_addr);
    return 0;
}

int ice_peer_get_status()
{
    return juice_get_state(agent);
}

