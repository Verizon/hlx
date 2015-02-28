//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    nconn.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    02/07/2014
//:
//:   Licensed under the Apache License, Version 2.0 (the "License");
//:   you may not use this file except in compliance with the License.
//:   You may obtain a copy of the License at
//:
//:       http://www.apache.org/licenses/LICENSE-2.0
//:
//:   Unless required by applicable law or agreed to in writing, software
//:   distributed under the License is distributed on an "AS IS" BASIS,
//:   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//:   See the License for the specific language governing permissions and
//:   limitations under the License.
//:
//: ----------------------------------------------------------------------------
#ifndef _NCONN_H
#define _NCONN_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "ndebug.h"
#include "req_stat.h"
#include "host_info.h"

#include <string>
#if 0
#include "http_cb.h"
#endif

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#if 0
#define MAX_READ_BUF (16*1024)
#define MAX_REQ_BUF (2048)
#endif

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
class evr_loop;
#if 0
class parsed_url;
#endif

//: ----------------------------------------------------------------------------
//: Enums
//: ----------------------------------------------------------------------------


//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class nconn
{
public:

        // ---------------------------------------
        // Protocol
        // ---------------------------------------
        typedef enum scheme_enum {

                SCHEME_TCP = 0,
                SCHEME_SSL,
                SCHEME_NONE

        } scheme_t;

        nconn(bool a_verbose,
              bool a_color
#if 0
              uint32_t a_sock_opt_recv_buf_size,
              uint32_t a_sock_opt_send_buf_size,
              bool a_sock_opt_no_delay,
              bool a_collect_stats,
              uint32_t a_timeout_s,
              int64_t a_max_reqs_per_conn = -1,
              void *a_rand_ptr = NULL
#endif

        ):
#if 0
                m_req_buf_len(0),
                m_fd(-1),

                // ssl
                m_ssl_ctx(NULL),
                m_ssl(NULL),

                m_state(CONN_STATE_FREE),
                m_stat(),
                m_http_parser_settings(),
                m_http_parser(),
#endif
                m_verbose(a_verbose),
                m_color(a_color),
                m_host(),
                m_stat(),
                m_save_response_in_reqlet(false),
                m_collect_stats_flag(false),
                m_data1(NULL),
                m_connect_start_time_us(0),
                m_request_start_time_us(0),
                m_last_connect_time_us(0),
                m_server_response_supports_keep_alives(false),
                m_timer_obj(NULL),
                m_id(0)

#if 0
                m_sock_opt_recv_buf_size(a_sock_opt_recv_buf_size),
                m_sock_opt_send_buf_size(a_sock_opt_send_buf_size),
                m_sock_opt_no_delay(a_sock_opt_no_delay),
                m_read_buf_idx(0),
                m_max_reqs_per_conn(a_max_reqs_per_conn),
                m_num_reqs(0),

                m_scheme(SCHEME_HTTP),
                m_host("NA"),
                m_collect_stats_flag(a_collect_stats),
                m_timeout_s(a_timeout_s)
#endif
        {
#if 0
                // Set up callbacks...
                m_http_parser_settings.on_message_begin = hp_on_message_begin;
                m_http_parser_settings.on_url = hp_on_url;
                m_http_parser_settings.on_status = hp_on_status;
                m_http_parser_settings.on_header_field = hp_on_header_field;
                m_http_parser_settings.on_header_value = hp_on_header_value;
                m_http_parser_settings.on_headers_complete = hp_on_headers_complete;
                m_http_parser_settings.on_body = hp_on_body;
                m_http_parser_settings.on_message_complete = hp_on_message_complete;

                // Set stats
                if(m_collect_stats_flag)
                {
                        stat_init(m_stat);
                }
#endif
        };

        // Destructor
        virtual ~nconn()
        {
        };

        void set_host(const std::string &a_host) {m_host = a_host;};
        void set_data1(void * a_data) {m_data1 = a_data;}
        void *get_data1(void) {return m_data1;}
        void reset_stats(void) { stat_init(m_stat); }
        const req_stat_t &get_stats(void) const { return m_stat;};
        uint64_t get_id(void) {return m_id;}
        void set_id(uint64_t a_id) {m_id = a_id;}

#if 0
        int32_t send_request(bool is_reuse = false);
        int32_t cleanup(evr_loop *a_evr_loop);
        bool can_reuse(void)
        {
                //NDBG_PRINT("CONN[%u] num / max %ld / %ld \n", m_connection_id, m_num_reqs, m_max_reqs_per_conn);
                if(m_server_response_supports_keep_alives &&
                   ((m_max_reqs_per_conn == -1) || (m_num_reqs < m_max_reqs_per_conn)))
                {
                        return true;
                }
                else
                {
                        return false;
                }
        }
        void set_ssl_ctx(SSL_CTX * a_ssl_ctx) { m_ssl_ctx = a_ssl_ctx;};

        void set_scheme(scheme_t a_scheme) {m_scheme = a_scheme;};
        bool is_done(void) { return (m_state == CONN_STATE_DONE);}

#endif

        // -------------------------------------------------
        // Virtual Methods
        // -------------------------------------------------
        virtual void set_state_done(void) = 0;
        virtual int32_t run_state_machine(evr_loop *a_evr_loop, const host_info_t &a_host_info) = 0;
        virtual bool is_done(void) = 0;
        virtual int32_t cleanup(evr_loop *a_evr_loop) = 0;
        virtual int32_t set_opt(uint32_t a_opt, void *a_buf, uint32_t a_len) = 0;
        virtual int32_t get_opt(uint32_t a_opt, void **a_buf, uint32_t *a_len) = 0;

        // -------------------------------------------------
        // Public static methods
        // -------------------------------------------------
        static const scheme_t m_scheme = SCHEME_NONE;

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        // TODO hide this!
        bool m_verbose;
        bool m_color;
        std::string m_host;
        req_stat_t m_stat;
        bool m_save_response_in_reqlet;
        bool m_collect_stats_flag;
        void *m_data1;
        uint64_t m_connect_start_time_us;
        uint64_t m_request_start_time_us;
        uint64_t m_last_connect_time_us;
        bool m_server_response_supports_keep_alives;

#if 0
        char m_req_buf[MAX_REQ_BUF];
        uint32_t m_req_buf_len;
#endif
        void *m_timer_obj;

private:

#if 0
        // ---------------------------------------
        // Connection state
        // ---------------------------------------
        typedef enum conn_state
        {
                CONN_STATE_FREE = 0,
                CONN_STATE_CONNECTING,

                // SSL
                CONN_STATE_SSL_CONNECTING,
                CONN_STATE_SSL_CONNECTING_WANT_READ,
                CONN_STATE_SSL_CONNECTING_WANT_WRITE,

                CONN_STATE_CONNECTED,
                CONN_STATE_READING,
                CONN_STATE_DONE
        } conn_state_t;
#endif
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(nconn)

#if 0
        int32_t setup_socket(const host_info_t &a_host_info);
        int32_t ssl_connect(const host_info_t &a_host_info);
        int32_t receive_response(void);
#endif

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        uint64_t m_id;
#if 0
        int m_fd;

        // ssl
        SSL_CTX * m_ssl_ctx;
        SSL *m_ssl;

        conn_state_t m_state;

        http_parser_settings m_http_parser_settings;
        http_parser m_http_parser;
#endif

#if 0
        // Socket options
        uint32_t m_sock_opt_recv_buf_size;
        uint32_t m_sock_opt_send_buf_size;
        bool m_sock_opt_no_delay;

        char m_read_buf[MAX_READ_BUF];
        uint32_t m_read_buf_idx;

        int64_t m_max_reqs_per_conn;
        int64_t m_num_reqs;


        scheme_t m_scheme;
        uint32_t m_timeout_s;
#endif

};



#endif



