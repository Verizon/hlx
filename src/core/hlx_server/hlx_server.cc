//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    hlx_server.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    05/28/2015
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

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "ndebug.h"
#include "hlx_server.h"
#include "t_server.h"
#include "util.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
// Set socket option macro...
#define SET_SOCK_OPT(_sock_fd, _sock_opt_level, _sock_opt_name, _sock_opt_val) \
        do { \
                int _l__sock_opt_val = _sock_opt_val; \
                int _l_status = 0; \
                _l_status = ::setsockopt(_sock_fd, \
                                _sock_opt_level, \
                                _sock_opt_name, \
                                &_l__sock_opt_val, \
                                sizeof(_l__sock_opt_val)); \
                                if (_l_status == -1) { \
                                        NDBG_PRINT("STATUS_ERROR: Failed to set %s count.  Reason: %s.\n", #_sock_opt_name, strerror(errno)); \
                                        return STATUS_ERROR;\
                                } \
                                \
        } while(0)

//: ----------------------------------------------------------------------------
//: http-parser callbacks
//: ----------------------------------------------------------------------------

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_server::set_stats(bool a_val)
{
        m_stats = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_server::set_verbose(bool a_val)
{
        m_verbose = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_server::set_color(bool a_val)
{
        m_color = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_server::set_port(uint16_t a_port)
{
        m_port = a_port;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_server::set_num_threads(uint32_t a_num_threads)
{
        m_num_threads = a_num_threads;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_server::set_num_parallel(uint32_t a_num_parallel)
{
        m_num_parallel = a_num_parallel;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlx_server::run(void)
{

        int l_status = 0;
        if(!m_is_initd)
        {
                l_status = init();
                if(HLX_SERVER_STATUS_OK != l_status)
                {
                        return HLX_SERVER_STATUS_ERROR;
                }
        }

        if(m_t_server_list.empty())
        {
                l_status = init_server_list();
                if(STATUS_OK != l_status)
                {
                        return HLX_SERVER_STATUS_ERROR;
                }
        }

        set_start_time_ms(get_time_ms());

        // -------------------------------------------
        // Run...
        // -------------------------------------------
        for(t_server_list_t::iterator i_t_server = m_t_server_list.begin();
                        i_t_server != m_t_server_list.end();
                        ++i_t_server)
        {
                (*i_t_server)->run();
        }

        return HLX_SERVER_STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#define MAX_PENDING_CONNECT_REQUESTS 5

static int32_t create_tcp_server_socket(uint16_t a_port)
{
        int l_sock_fd;
        sockaddr_in l_server_address;
        int32_t l_status;

        // -------------------------------------------
        // Create socket for incoming connections
        // -------------------------------------------
        l_sock_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (l_sock_fd < 0)
        {
                NDBG_PRINT("Error socket() failed. Reason[%d]: %s\n", errno, strerror(errno));
                return STATUS_ERROR;
        }

        // -------------------------------------------
        // Set socket options
        // -Enable Socket reuse.
        // -------------------------------------------
        SET_SOCK_OPT(l_sock_fd, SOL_SOCKET, SO_REUSEADDR, 1);

        // -------------------------------------------
        // Construct local address structure
        // -------------------------------------------
        memset(&l_server_address, 0, sizeof(l_server_address)); // Zero out structure

        l_server_address.sin_family      = AF_INET;             // Internet address family
        l_server_address.sin_addr.s_addr = htonl(INADDR_ANY);   // Any incoming interface
        l_server_address.sin_port        = htons(a_port);         // Local port

        // -------------------------------------------
        // Bind to the local address
        // -------------------------------------------
        l_status = bind(l_sock_fd, (struct sockaddr *) &l_server_address, sizeof(l_server_address));
        if (l_status < 0)
        {
                NDBG_PRINT("Error bind() failed (port: %d). Reason[%d]: %s\n", a_port, errno, strerror(errno));
                return STATUS_ERROR;
        }

        // -------------------------------------------
        // Mark the socket so it will listen for
        // incoming connections
        // -------------------------------------------
        l_status = listen(l_sock_fd, MAX_PENDING_CONNECT_REQUESTS);
        if (l_status < 0)
        {
                NDBG_PRINT("Error listen() failed. Reason[%d]: %s\n", errno, strerror(errno));
                return STATUS_ERROR;
        }

        return l_sock_fd;
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_server::stop(void)
{
        int32_t l_retval = HLX_SERVER_STATUS_OK;

        for (t_server_list_t::iterator i_t_server = m_t_server_list.begin();
                        i_t_server != m_t_server_list.end();
                        ++i_t_server)
        {
                (*i_t_server)->stop();
        }

        // shutdown
        shutdown(m_fd, SHUT_RD);

        return l_retval;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlx_server::wait_till_stopped(void)
{
        //int32_t l_retval = HLX_SERVER_STATUS_OK;

        // -------------------------------------------
        // Join all threads before exit
        // -------------------------------------------
        for(t_server_list_t::iterator i_server = m_t_server_list.begin();
           i_server != m_t_server_list.end();
            ++i_server)
        {
                //if(m_verbose)
                //{
                //      NDBG_PRINT("joining...\n");
                //}
                pthread_join(((*i_server)->m_t_run_thread), NULL);

        }
        //return l_retval;

        return HLX_SERVER_STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool hlx_server::is_running(void)
{
        for (t_server_list_t::iterator i_t_server = m_t_server_list.begin();
                        i_t_server != m_t_server_list.end();
                        ++i_t_server)
        {
                if((*i_t_server)->is_running())
                        return true;
        }
        return false;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  client status indicating success or failure
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_server::init(void)
{

        if(true == m_is_initd)
        {
                return HLX_SERVER_STATUS_OK;
        }
        // not initialized yet

        // Create listen socket
        m_fd = create_tcp_server_socket(m_port);
        if(m_fd == STATUS_ERROR)
        {
            NDBG_PRINT("Error performing create_tcp_server_socket with port number = %d", m_port);
            return HLX_SERVER_STATUS_ERROR;
        }

#if 0
        // -------------------------------------------
        // SSL init...
        // -------------------------------------------
        m_ssl_ctx = ssl_init(m_ssl_cipher_list, // ctx cipher list str
                             m_ssl_options,     // ctx options
                             m_ssl_ca_file,     // ctx ca file
                             m_ssl_ca_path);    // ctx ca path
        if(NULL == m_ssl_ctx)
        {
                NDBG_PRINT("Error: performing ssl_init with cipher_list: %s\n", m_ssl_cipher_list.c_str());
                return HLX_SERVER_STATUS_ERROR;
        }
#endif

        m_is_initd = true;
        return HLX_SERVER_STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_server::init_server_list(void)
{

        // -------------------------------------------
        // Bury the config into a settings struct
        // -------------------------------------------
        settings_struct_t l_settings;
        l_settings.m_verbose = m_verbose;
        l_settings.m_color = m_color;
        l_settings.m_evr_loop_type = (evr_loop_type_t)m_evr_loop_type;
        l_settings.m_num_parallel = m_num_parallel;
        l_settings.m_fd = m_fd;

        // -------------------------------------------
        // Create t_client list...
        // -------------------------------------------
        for(uint32_t i_server_idx = 0; i_server_idx < m_num_threads; ++i_server_idx)
        {
                t_server *l_t_server = new t_server(l_settings);
                m_t_server_list.push_back(l_t_server);
        }

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hlx_server::hlx_server(void):
        m_verbose(false),
        m_color(false),
        m_stats(false),
        m_port(23456),

        // TODO Make define
        m_num_threads(1),

        // TODO Make define
        m_num_parallel(128),

        m_start_time_ms(0),


        // t_client
        m_t_server_list(),

        // TODO Make define
        m_evr_loop_type(EVR_LOOP_EPOLL),

        m_fd(-1),
        m_is_initd(false)
{

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hlx_server::~hlx_server()
{
        // TODO
}

}

