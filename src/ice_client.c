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
#include "bp2p_ice_api.h"
#include "juice/juice.h"

#define BUFFER_SIZE 4096

static juice_agent_t *agent1;
static ice_cfg_t *g_ice_cfg = NULL;

static void on_state_changed1(juice_agent_t *agent, juice_state_t state, void *user_ptr);
//static void on_state_changed2(juice_agent_t *agent, juice_state_t state, void *user_ptr);

static void on_gathering_done1(juice_agent_t *agent, void *user_ptr);
//static void on_gathering_done2(juice_agent_t *agent, void *user_ptr);

static void on_recv1(juice_agent_t *agent, const char *data, size_t size, void *user_ptr);


// copy from umqtt
#define RECONNECT_INTERVAL  5
#define UMQTT_CLIENT_LOGIN_INTERVAL  1
#define UMQTT_CLIENT_SEND_INTERVAL  1


struct config {
    const char *host;
    int port;
    bool ssl;
    bool auto_reconnect;
    struct umqtt_connect_opts options;
};

static struct ev_timer reconnect_timer;
static struct ev_timer login_timer;
static struct ev_timer counter_timer;

static int login_succeed = 0;

static struct umqtt_client *cl;

static struct config cfg = {
    .host = MQTT_SERVER_HOST,
    .port = 1883,
    .options = {
        .keep_alive = 30,
        .clean_session = true,
        .username = "",
        .password = "",
        .will_topic = "will",
        .will_message = "will test"
    }
};

static void start_reconnect(struct ev_loop *loop)
{
    if (!cfg.auto_reconnect) {
        ev_break(loop, EVBREAK_ALL);
        return;
    }

    ev_timer_set(&reconnect_timer, RECONNECT_INTERVAL, 0.0);
    ev_timer_start(loop, &reconnect_timer);
}

static void start_login(struct ev_loop *loop)
{
    ev_timer_set(&login_timer, UMQTT_CLIENT_LOGIN_INTERVAL, 1.0);
    ev_timer_start(loop, &login_timer);
}

static void stop_login(struct ev_loop *loop)
{
    login_succeed = 0;
    ev_timer_stop(loop, &login_timer);
}

static void on_conack(struct umqtt_client *cl, bool sp, int code)
{
    struct umqtt_topic topics[] = {
        {
            .topic = g_ice_cfg->my_channel,
            .qos = UMQTT_QOS0
        }
    #if 0
    ,
        {
            .topic = "test2",
            .qos = UMQTT_QOS1
        },
        {
            .topic = "test3",
            .qos = UMQTT_QOS2
        }
    #endif
    };
    
    if (code != UMQTT_CONNECTION_ACCEPTED) {
        umqtt_log_err("Connect failed:%d\n", code);
        return;
    }

    umqtt_log_info("on_conack:  Session Present(%d)  code(%u)\n", sp, code);

    start_login(cl->loop);

    /* Session Present */
    if (!sp)
        cl->subscribe(cl, topics, ARRAY_SIZE(topics));

//    cl->publish(cl, "test1", "hello world", strlen("hello world"), UMQTT_QOS0, false);
//    cl->publish(cl, "test2", "hello world 2", strlen("hello world 2"), UMQTT_QOS1, false);
//    cl->publish(cl, "test3", "hello world 3", strlen("hello world 3"), UMQTT_QOS2, false);
    
}

static void on_suback(struct umqtt_client *cl, uint8_t *granted_qos, int qos_count)
{
    int i;

    printf("on_suback, qos(");
    for (i = 0; i < qos_count; i++)
        printf("%d ", granted_qos[i]);
    printf("\b)\n");
}

static void on_unsuback(struct umqtt_client *cl)
{
    umqtt_log_info("on_unsuback\n");
    umqtt_log_info("Normal quit\n");

    ev_break(cl->loop, EVBREAK_ALL);
}


static void on_publish(struct umqtt_client *cl, const char *topic, int topic_len,
    const void *payload, int payloadlen)
{
    umqtt_log_info("on_publish: topic:[%.*s] payload:[%.*s]\n", topic_len, topic,
        payloadlen, (char *)payload);

    int msg_type = -1;
    char *msg = NULL;
    char sdp[JUICE_MAX_SDP_STRING_LEN];

    int resp_msg_type = -1;
    char send_buf[JUICE_MQTT_MSG_MAX_SIZE];
    int send_len = 0;

    if (0 == strcmp (topic, g_ice_cfg->my_channel)) {
        msg_type = *((int*)payload);
        msg_type = ntohl(msg_type);

        msg = (char*)payload + sizeof(msg_type);

        printf("Received publish type:%d, msg:\n%s\n", msg_type, msg);

        switch (msg_type) {
            case JUICE_MQTT_MSG_TYPE_CONNECT_RESP:
                stop_login(cl->loop);

            	juice_get_local_description(agent1, sdp, sizeof (sdp));
	            printf("Local description :\n%s\n", sdp);

                
                resp_msg_type = JUICE_MQTT_MSG_TYPE_SDP;
                send_len = make_publish_msg(send_buf, sizeof(send_buf), resp_msg_type, sdp);
                cl->publish(cl, g_ice_cfg->peer_channel, send_buf, send_len, UMQTT_QOS0, false);
                break;
            case JUICE_MQTT_MSG_TYPE_SDP:
                juice_set_remote_description(agent1, msg);
                break;
            case JUICE_MQTT_MSG_TYPE_CANDIDATE:
                
                break;
            case JUICE_MQTT_MSG_TYPE_CANDIDATE_GATHER_DONE:
                break;
            default:
                break;
        }

    }else {
        printf ("error msg\n");
    }
}

static void on_pingresp(struct umqtt_client *cl)
{
}

static void on_error(struct umqtt_client *cl, int err, const char *msg)
{
    umqtt_log_err("on_error: %d: %s\n", err, msg);

    start_reconnect(cl->loop);
    free(cl);
}

static void on_close(struct umqtt_client *cl)
{
    umqtt_log_info("on_close\n");

    start_reconnect(cl->loop);
    free(cl);
}

static void on_net_connected(struct umqtt_client *cl)
{
    umqtt_log_info("on_net_connected\n");

    if (cl->connect(cl, &cfg.options) < 0) {
        umqtt_log_err("connect failed\n");

        start_reconnect(cl->loop);
        free(cl);
    }
}

static void do_connect(struct ev_loop *loop, struct ev_timer *w, int revents)
{
        umqtt_log_info("Start connect...s\n");

    cl = umqtt_new(loop, cfg.host, cfg.port, cfg.ssl);
    if (!cl) {
        start_reconnect(loop);
        return;
    }

    cl->on_net_connected = on_net_connected;
    cl->on_conack = on_conack;
    cl->on_suback = on_suback;
    cl->on_unsuback = on_unsuback;
    cl->on_publish = on_publish;
    cl->on_pingresp = on_pingresp;
    cl->on_error = on_error;
    cl->on_close = on_close;

    umqtt_log_info("Start connect...\n");
}


static void do_login(struct ev_loop *loop, struct ev_timer *w, int revents)
{
    if (NULL == cl) {
        umqtt_log_info("should connece mqtt server firstly\n");
        return ;
    }

    if (false == juice_is_gather_done(agent1)) {
        printf ("waitting for local gather done\n");
        return;
    }    

    int msg_type = JUICE_MQTT_MSG_TYPE_CONNECT_REQ;
    char send_buf[JUICE_MQTT_MSG_MAX_SIZE];
    int send_len = 0;
    send_len = make_publish_msg(send_buf, sizeof(send_buf), msg_type, g_ice_cfg->my_channel);
    cl->publish(cl, g_ice_cfg->peer_channel, send_buf, send_len, UMQTT_QOS0, false);
    umqtt_log_info("login ...\n");
}

static void signal_cb(struct ev_loop *loop, ev_signal *w, int revents)
{
    printf ("recv signal: %d\n", revents);
    ev_break(loop, EVBREAK_ALL);
}
// cpy end 

// Agent 1: on state changed
static void on_state_changed1(juice_agent_t *agent, juice_state_t state, void *user_ptr) {
	printf("State 1: %s\n", juice_state_to_string(state));

	if (state == JUICE_STATE_CONNECTED) {
		// Agent 1: on connected, send a message
		const char *message = "Hello from 1";
		juice_send(agent, message, strlen(message));
	} else if (state == JUICE_STATE_COMPLETED) {
	   printf ("ICE nego succeed\n");
	   if (g_ice_cfg && g_ice_cfg->cb_on_status_change) {
                g_ice_cfg->cb_on_status_change(ICE_STATUS_COMPLETE);
       }
	}
}



// Agent 1: on local candidates gathering done
static void on_gathering_done1(juice_agent_t *agent, void *user_ptr) {
	printf("Gathering done 1\n");

	// Agent 1: Generate local description
//	char sdp1[JUICE_MAX_SDP_STRING_LEN];
//	juice_get_local_description(agent1, sdp1, JUICE_MAX_SDP_STRING_LEN);
//	printf("Local description 1:\n%s\n", sdp1);

	// Agent 2: Receive description from agent 1
//	juice_set_remote_description(agent2, sdp1);

	// Agent 2: Gather candidates
//	juice_gather_candidates(agent2);
}



// Agent 1: on message received
static void on_recv1(juice_agent_t *agent, const char *data, size_t size, void *user_ptr)
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
    cfg.host = ice_cfg->signalling_srv;
    g_ice_cfg = ice_cfg;            
    struct ev_loop* loop = ice_cfg->loop;
    if (NULL == loop) {
        return 0;
    }

//    juice_set_log_level(JUICE_LOG_LEVEL_VERBOSE);
	juice_config_t config1;
	memset(&config1, 0, sizeof(config1));

	// TURN server
	// Please do not use outside of libjuice tests
	juice_turn_server_t turn_server;
	memset(&turn_server, 0, sizeof(turn_server));
	turn_server.host = ice_cfg->turn_srv;
	turn_server.port = 3478;
	turn_server.username = ice_cfg->turn_username;
	turn_server.password = ice_cfg->turn_password;
	config1.turn_servers = &turn_server;
	config1.turn_servers_count = 1;

	config1.cb_state_changed = on_state_changed1;
	config1.cb_gathering_done = on_gathering_done1;
	config1.cb_recv = on_recv1;
    config1.cb_on_idle_running = ice_cfg->cb_on_idle_running;
    
	config1.user_ptr = NULL;
	agent1 = juice_create(&config1);
	// Agent 1: Gather candidates
	juice_gather_candidates(agent1);
    
    ev_timer_init(&reconnect_timer, do_connect, 0.1, 0.0);
    ev_timer_start(loop, &reconnect_timer);

    ev_timer_init(&login_timer, do_login, 0.1, 0.0);
    umqtt_log_info("libumqttc version %s\n", UMQTT_VERSION_STRING);
    return 0;
}



int ice_client_send_data(void *data, int len)
{
    return juice_send(agent1, data, len);
}

int ice_client_get_valid_peer(struct sockaddr* dst)
{
#if 0
    struct sockaddr_in *pSockAddr = dst;
    const pj_ice_sess_check *valid_pair = pj_ice_strans_get_valid_pair(icedemo.icest, 1); 
    if (!valid_pair) {
        printf ("%s, warning! no valid candidate pair\n", __func__);
        return -1;
    }

    pSockAddr->sin_family = PJ_AF_INET;
    pSockAddr->sin_addr = *(struct in_addr*)pj_sockaddr_get_addr(&valid_pair->rcand->addr);
    pSockAddr->sin_port = htons((unsigned short)pj_sockaddr_get_port(&valid_pair->rcand->addr)); 
#endif    
    struct sockaddr *tmp = NULL;
    juice_get_selected_pair(agent1, NULL, &tmp);
    if (NULL == tmp) {
        return -1;
    }
    *dst = *tmp;
    return 0;
}

