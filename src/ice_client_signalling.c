#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ev.h>
#include "umqtt/umqtt.h"
#include "ice_common.h"
#include "juice/juice.h"
#include "ice_client.h"

// copy from umqtt
#define RECONNECT_INTERVAL  5
#define UMQTT_CLIENT_LOGIN_INTERVAL  1
#define UMQTT_CLIENT_SEND_INTERVAL  1

static struct umqtt_client *cl = NULL;
static struct ev_timer reconnect_timer;
static struct ev_timer login_timer;      // conn to peer
extern juice_agent_t *agent;
extern ice_cfg_t *g_ice_cfg;

static int login_succeed = 0;

static struct umqtt_client *cl;


struct config {
    const char *host;
    int port;
    bool ssl;
    bool auto_reconnect;
    struct umqtt_connect_opts options;
};


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


static void do_login(struct ev_loop *loop, struct ev_timer *w, int revents)
{
    umqtt_log_info("login ...\n");
    if (NULL == cl) {
        umqtt_log_info("should connece mqtt server firstly\n");
        return ;
    }

    if (false == juice_is_gather_done(agent)) {
        printf ("waitting for local gather done\n");
        return;
    }    

    int msg_type = JUICE_MQTT_MSG_TYPE_CONNECT_REQ;
    char send_buf[JUICE_MQTT_MSG_MAX_SIZE];
    int send_len = 0;
    send_len = make_publish_msg(send_buf, sizeof(send_buf), msg_type, g_ice_cfg->my_channel);
    cl->publish(cl, g_ice_cfg->peer_channel, send_buf, send_len, UMQTT_QOS0, false);
}



static void start_login(struct ev_loop *loop)
{
    ev_timer_init(&login_timer, do_login, 0.1, 0.0);
    ev_timer_set(&login_timer, UMQTT_CLIENT_LOGIN_INTERVAL, 1.0);
    ev_timer_start(loop, &login_timer);
}

static void stop_login(struct ev_loop *loop)
{
    login_succeed = 0;
    ev_timer_stop(loop, &login_timer);
}

void ice_client_clear_signaling_info()
{
    if (NULL != cl) {
        free(cl);
        cl = NULL;
    }
}

static void start_reconnect(struct ev_loop *loop)
{
    if (!cfg.auto_reconnect) {
        ev_break(loop, EVBREAK_ALL);
        return;
    }

    ev_timer_set(&reconnect_timer, RECONNECT_INTERVAL, 0.0);
    ev_timer_start(loop, &reconnect_timer);
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

            	juice_get_local_description(agent, sdp, sizeof (sdp));
	            printf("Local description :\n%s\n", sdp);

                
                resp_msg_type = JUICE_MQTT_MSG_TYPE_SDP;
                send_len = make_publish_msg(send_buf, sizeof(send_buf), resp_msg_type, sdp);
                cl->publish(cl, g_ice_cfg->peer_channel, send_buf, send_len, UMQTT_QOS0, false);
                break;
            case JUICE_MQTT_MSG_TYPE_SDP:
                juice_set_remote_description(agent, msg);
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


int ice_client_conn_signalling_srv(struct ev_loop *loop, char *ip, unsigned short port)
{
    if (NULL != ip) {
        cfg.host = ip;
    }

    if (0 != port) {
        cfg.port = port;
    }

    ev_timer_init(&reconnect_timer, do_connect, 0.1, 0.0);
    ev_timer_start(loop, &reconnect_timer);    
}

