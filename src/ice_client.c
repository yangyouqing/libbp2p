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
#include <pthread.h>
#include <ev.h>
#include "umqtt/umqtt.h"
#include "ice_common.h"
#include "p2p_api.h"
#include "juice/juice.h"

#define BUFFER_SIZE 4096

juice_agent_t *agent;
ice_cfg_t *g_ice_cfg = NULL;

static struct ev_timer reconnect_timer;  // conn to signing server

static struct ev_timer counter_timer;    // calc eclipse time
static ev_async async_watcher;



static void do_overtime(struct ev_loop *loop, struct ev_timer *w, int revents)
{
    printf ("do_overtime\n");

    ev_break(loop, EVBREAK_ALL);
    ice_client_clear_signaling_info();

    if (NULL != agent) {
        juice_destroy(agent);
        agent = NULL;
    }
}

void ice_client_start_count_timer(struct ev_loop *loop)
{
    ev_timer_start(loop, &counter_timer);
}

static void signal_cb(struct ev_loop *loop, ev_signal *w, int revents)
{
    printf ("recv signal: %d\n", revents);
    ev_break(loop, EVBREAK_ALL);
}
// cpy end 


static void async_cb(EV_P_ ev_async *w, int revents)
{
    printf("async_cb\n");
    ev_break(g_ice_cfg->loop, EVBREAK_ALL);
}

// Agent 1: on state changed
static void on_state_changed(juice_agent_t *agent, juice_state_t state, void *user_ptr) 
{
	printf("State 1: %s\n", juice_state_to_string(state));

	if (state == JUICE_STATE_CONNECTED) {
		// Agent 1: on connected, send a message
		const char *message = "Hello from 1";
		juice_send(agent, message, strlen(message));
	} else if (state == JUICE_STATE_COMPLETED) {
	    printf ("ICE nego succeed\n");
        ice_client_get_valid_localaddr(g_ice_cfg->local_ip, &g_ice_cfg->lport);
        ice_client_get_valid_peeraddr(g_ice_cfg->remote_ip, &g_ice_cfg->rport);


	   if (g_ice_cfg && g_ice_cfg->cb_on_status_change) {
            g_ice_cfg->cb_on_status_change(ICE_STATUS_COMPLETE);
       }
       ev_async_send(g_ice_cfg->loop, &async_watcher);
	}
}



// Agent 1: on local candidates gathering done
static void on_gathering_done(juice_agent_t *agent, void *user_ptr) {
	printf("Gathering done\n");

	// Agent 1: Generate local description
//	char sdp1[JUICE_MAX_SDP_STRING_LEN];
//	juice_get_local_description(agent, sdp1, JUICE_MAX_SDP_STRING_LEN);
//	printf("Local description 1:\n%s\n", sdp1);

	// Agent 2: Receive description from agent 1
//	juice_set_remote_description(agent2, sdp1);

	// Agent 2: Gather candidates
//	juice_gather_candidates(agent2);
}



// Agent 1: on message received
static void on_recv(juice_agent_t *agent, const char *data, size_t size, void *user_ptr)
{
    struct sockaddr *src = NULL;
    struct sockaddr *dest = NULL;
    juice_get_selected_pair(agent, &src, &dest);
    if (g_ice_cfg && g_ice_cfg->cb_on_rx_pkt && NULL != src && NULL != dest) {
        g_ice_cfg->cb_on_rx_pkt(data, (int)size, (struct sockaddr*)src, (struct sockaddr*)dest);
    }
}

int ice_client_init(ice_cfg_t *ice_cfg)
{
    printf ("main thread id:%d\n", pthread_self());
    if (NULL == ice_cfg) {
        return -1;
    }
    g_ice_cfg = ice_cfg;            
    struct ev_loop* loop = ice_cfg->loop;
    if (NULL == loop) {
        return 0;
    }

//    juice_set_log_level(JUICE_LOG_LEVEL_VERBOSE);
	juice_config_t config;
	memset(&config, 0, sizeof(config));

	// TURN server
	// Please do not use outside of libjuice tests
	juice_turn_server_t turn_server;
	memset(&turn_server, 0, sizeof(turn_server));
	turn_server.host = ice_cfg->turn_srv;
	turn_server.port = 3478;
	turn_server.username = ice_cfg->turn_username;
	turn_server.password = ice_cfg->turn_password;
	config.turn_servers = &turn_server;
	config.turn_servers_count = 1;

	config.cb_state_changed = on_state_changed;
	config.cb_gathering_done = on_gathering_done;
	config.cb_recv = on_recv;
    
	config.user_ptr = NULL;
	agent = juice_create(&config);
	// Agent 1: Gather candidates
	juice_gather_candidates(agent);
    
    ice_client_conn_signalling_srv(loop, ice_cfg->signalling_srv, 0);

    if (ice_cfg->overtime >= 0) {
        float overtime = ice_cfg->overtime / 1000;
        ev_timer_init(&counter_timer, do_overtime, overtime, 0.0);
        ev_timer_start(loop, &counter_timer);
    }
    
    ev_async_init(&async_watcher, async_cb);
    ev_async_start(loop, &async_watcher);

    umqtt_log_info("libumqttc version %s\n", UMQTT_VERSION_STRING);
    return 0;
}

int ice_client_send_data(void *data, int len)
{
    if (JUICE_STATE_COMPLETED !=  juice_get_state(agent)) {
        return -1;
    }
    return juice_send(agent, data, len);
}

int ice_client_get_valid_peer(struct sockaddr* dst)
{
    struct sockaddr *tmp = NULL;
    juice_get_selected_pair(agent, NULL, &tmp);
    if (NULL == tmp) {
        return -1;
    }
    *dst = *tmp;
    return 0;
}

int ice_client_get_valid_localaddr(char *ip, unsigned int *port)
{
    if (NULL == ip || NULL == port) {
        return -1;
    }

    if (NULL == agent) {
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

int ice_client_get_valid_peeraddr(char *ip, unsigned int *port)
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

int ice_client_get_status()
{
    return juice_get_state(agent);
}
