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

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------
class evr_loop;

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

        // -------------------------------------------------
        // Const
        // -------------------------------------------------
        static const scheme_t m_scheme = SCHEME_NONE;
        static const int32_t  m_opt_unhandled = 12345;

        nconn(bool a_verbose,
              bool a_color,
              int64_t a_max_reqs_per_conn = -1,
              bool a_save_response_in_reqlet = false,
              bool a_collect_stats = false,
              void *a_rand_ptr = NULL):
                m_verbose(a_verbose),
                m_color(a_color),
                m_host(),
                m_stat(),
                m_save_response_in_reqlet(a_save_response_in_reqlet),
                m_collect_stats_flag(false),
                m_data1(NULL),
                m_connect_start_time_us(0),
                m_request_start_time_us(0),
                m_last_connect_time_us(0),
                m_server_response_supports_keep_alives(false),
                m_timer_obj(NULL),
                m_id(0),
                m_max_reqs_per_conn(a_max_reqs_per_conn),
                m_num_reqs(0)
        {
                // Set stats
                if(m_collect_stats_flag)
                {
                        stat_init(m_stat);
                }
        };

        // Destructor
        virtual ~nconn();

        void set_host(const std::string &a_host) {m_host = a_host;};
        void set_data1(void * a_data) {m_data1 = a_data;}
        void *get_data1(void) {return m_data1;}
        void reset_stats(void) { stat_init(m_stat); }
        const req_stat_t &get_stats(void) const { return m_stat;};
        uint64_t get_id(void) {return m_id;}
        void set_id(uint64_t a_id) {m_id = a_id;}

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

        // -------------------------------------------------
        // Virtual Methods
        // -------------------------------------------------
        virtual int32_t send_request(bool is_reuse = false) = 0;
        virtual int32_t run_state_machine(evr_loop *a_evr_loop, const host_info_t &a_host_info) = 0;
        virtual int32_t cleanup(evr_loop *a_evr_loop) = 0;
        virtual int32_t set_opt(uint32_t a_opt, void *a_buf, uint32_t a_len) = 0;
        virtual int32_t get_opt(uint32_t a_opt, void **a_buf, uint32_t *a_len) = 0;

        virtual void set_state_done(void) = 0;
        virtual bool is_done(void) = 0;

        // -------------------------------------------------
        // Public static methods
        // -------------------------------------------------

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
        void *m_timer_obj;

private:

        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(nconn)

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        uint64_t m_id;

protected:
        // -------------------------------------------------
        // Protected members
        // -------------------------------------------------
        int64_t m_max_reqs_per_conn;
        int64_t m_num_reqs;

};



#endif



