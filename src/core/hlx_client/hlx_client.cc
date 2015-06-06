//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    hlx_client.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    03/11/2015
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
#include <stdint.h>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#include "hlx_client.h"
#include "reqlet.h"
#include "util.h"
#include "ssl_util.h"
#include "ndebug.h"
#include "resolver.h"
#include "reqlet.h"
#include "nconn_ssl.h"
#include "nconn_tcp.h"
#include "tinymt64.h"
#include "t_client.h"
#include "settings.h"

#include <string.h>

// getrlimit
#include <sys/time.h>
#include <sys/resource.h>

// signal
#include <signal.h>

// Shared pointer
//#include <tr1/memory>

#include <list>
#include <algorithm>
#include <unordered_set>

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h> // For getopt_long
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <math.h>

// json support
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::init_client_list(void)
{

        // -------------------------------------------
        // Bury the config into a settings struct
        // -------------------------------------------
        settings_struct_t l_settings;
        l_settings.m_verbose = m_verbose;
        l_settings.m_color = m_color;
        l_settings.m_quiet = m_quiet;
        l_settings.m_show_summary = m_show_summary;
        l_settings.m_url = m_url;
        l_settings.m_header_map = m_header_map;
        l_settings.m_verb = m_verb;
        l_settings.m_req_body = m_req_body;
        l_settings.m_req_body_len = m_req_body_len;
        l_settings.m_evr_loop_type = (evr_loop_type_t)m_evr_loop_type;
        l_settings.m_num_parallel = m_num_parallel;
        l_settings.m_timeout_s = m_timeout_s;
        l_settings.m_run_time_s = m_run_time_s;
        l_settings.m_request_mode = m_request_mode;
        l_settings.m_num_end_fetches = m_num_end_fetches;
        l_settings.m_connect_only = m_connect_only;
        l_settings.m_save_response = m_save_response;
        l_settings.m_collect_stats = m_collect_stats;
        l_settings.m_use_persistent_pool = m_use_persistent_pool;
        l_settings.m_num_reqs_per_conn = m_num_reqs_per_conn;
        l_settings.m_sock_opt_recv_buf_size = m_sock_opt_recv_buf_size;
        l_settings.m_sock_opt_send_buf_size = m_sock_opt_send_buf_size;
        l_settings.m_sock_opt_no_delay = m_sock_opt_no_delay;
        l_settings.m_ssl_ctx = m_ssl_ctx;
        l_settings.m_ssl_cipher_list = m_ssl_cipher_list;
        l_settings.m_ssl_options_str = m_ssl_options_str;
        l_settings.m_ssl_options = m_ssl_options;
        l_settings.m_ssl_verify = m_ssl_verify;
        l_settings.m_ssl_sni = m_ssl_sni;
        l_settings.m_ssl_ca_file = m_ssl_ca_file;
        l_settings.m_ssl_ca_path = m_ssl_ca_path;
        l_settings.m_resolver = m_resolver;

        if(m_rate > 0)
        {
                l_settings.m_rate = (int32_t)((double)m_rate / (double)m_num_threads);
                if(l_settings.m_rate == 0)
                {
                        l_settings.m_rate = 1;
                }
        }
        else
        {
                l_settings.m_rate = m_rate;
        }

        // -------------------------------------------
        // Create t_client list...
        // -------------------------------------------
        for(uint32_t i_client_idx = 0; i_client_idx < m_num_threads; ++i_client_idx)
        {
                t_client *l_t_client = NULL;
                if(m_split_requests_by_thread)
                {
                        reqlet_vector_t l_reqlet_vector;

                        // Calculate index
                        uint32_t l_idx = i_client_idx*(m_reqlet_vector.size()/m_num_threads);
                        uint32_t l_len = m_reqlet_vector.size()/m_num_threads;

                        if(i_client_idx + 1 == m_num_threads)
                        {
                                // Get remainder
                                l_len = m_reqlet_vector.size() - (i_client_idx * l_len);
                        }
                        for(uint32_t i_dx = 0; i_dx < l_len; ++i_dx)
                        {
                                l_reqlet_vector.push_back(m_reqlet_vector[l_idx + i_dx]);
                        }

                        l_t_client = new t_client(l_settings, l_reqlet_vector);
                }
                else
                {
                        l_t_client = new t_client(l_settings, m_reqlet_vector);
                }


                //if(a_settings.m_verbose)
                //{
                //        NDBG_PRINT("Creating...\n");
                //}

                // Construct with settings...
                for(header_map_t::iterator i_header = m_header_map.begin();
                    i_header != m_header_map.end();
                    ++i_header)
                {
                        l_t_client->set_header(i_header->first, i_header->second);
                }

                // Caculate num parallel per thread
                if(m_num_end_fetches != -1)
                {
                        uint32_t l_num_fetches_per_thread = m_num_end_fetches / m_num_threads;
                        uint32_t l_remainder_fetches = m_num_end_fetches % m_num_threads;
                        if (i_client_idx == (m_num_threads - 1))
                        {
                                l_num_fetches_per_thread += l_remainder_fetches;
                        }
                        l_t_client->set_end_fetches(l_num_fetches_per_thread);
                }

                m_t_client_list.push_back(l_t_client);
        }

        // Delete local copies
        for(size_t i_reqlet = 0;
            i_reqlet < m_reqlet_vector.size();
            ++i_reqlet)
        {
                reqlet *i_reqlet_ptr = m_reqlet_vector[i_reqlet];
                if(i_reqlet_ptr)
                {
                        delete i_reqlet_ptr;
                        i_reqlet_ptr = NULL;
                }
                m_reqlet_vector.clear();
        }

        return STATUS_OK;
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::run(void)
{
        int l_status = 0;
        if(!m_is_initd)
        {
                l_status = init();
                if(HLX_CLIENT_STATUS_OK != l_status)
                {
                        return HLX_CLIENT_STATUS_ERROR;
                }
        }
        // at this point m_resolver is a resolver instance

        if(m_t_client_list.empty())
        {
                l_status = init_client_list();
                if(STATUS_OK != l_status)
                {
                        return HLX_CLIENT_STATUS_ERROR;
                }
        }

        set_start_time_ms(get_time_ms());

        // -------------------------------------------
        // Run...
        // -------------------------------------------
        for(t_client_list_t::iterator i_t_client = m_t_client_list.begin();
                        i_t_client != m_t_client_list.end();
                        ++i_t_client)
        {
                (*i_t_client)->run();
        }

        return HLX_CLIENT_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::stop(void)
{
        int32_t l_retval = HLX_CLIENT_STATUS_OK;

        for (t_client_list_t::iterator i_t_client = m_t_client_list.begin();
                        i_t_client != m_t_client_list.end();
                        ++i_t_client)
        {
                (*i_t_client)->stop();
        }

        return l_retval;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::wait_till_stopped(void)
{
        //int32_t l_retval = HLX_CLIENT_STATUS_OK;

        // -------------------------------------------
        // Join all threads before exit
        // -------------------------------------------
        for(t_client_list_t::iterator i_client = m_t_client_list.begin();
            i_client != m_t_client_list.end();
            ++i_client)
        {
                //if(m_verbose)
                //{
                //      NDBG_PRINT("joining...\n");
                //}
                pthread_join(((*i_client)->m_t_run_thread), NULL);

        }
        //return l_retval;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool hlx_client::is_running(void)
{
        for (t_client_list_t::iterator i_t_client = m_t_client_list.begin();
                        i_t_client != m_t_client_list.end();
                        ++i_t_client)
        {
                if((*i_t_client)->is_running())
                        return true;
        }
        return false;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_quiet(bool a_val)
{
        m_quiet = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_verbose(bool a_val)
{
        m_verbose = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_color(bool a_val)
{
        m_color = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::set_url(const std::string &a_url)
{
        m_url = a_url;

        // If reqlets defined set path
        parsed_url l_parsed_url;
        int32_t l_status;
        l_status = l_parsed_url.parse(a_url);
        if(l_status != STATUS_OK)
        {
                //NDBG_PRINT("Error parsing url: %s\n", a_url.c_str());
                return HLX_CLIENT_STATUS_ERROR;
        }

        for(size_t i_reqlet = 0;
            i_reqlet < m_reqlet_vector.size();
            ++i_reqlet)
        {
                reqlet *i_reqlet_ptr = m_reqlet_vector[i_reqlet];
                if(i_reqlet_ptr)
                {
                        reqlet *l_reqlet = new reqlet(*i_reqlet_ptr);
                        l_reqlet->init_with_url(a_url, m_wildcarding);
                        l_reqlet->set_host(i_reqlet_ptr->m_url.m_host);

                        delete i_reqlet_ptr;
                        m_reqlet_vector[i_reqlet] = l_reqlet;
                }
        }

        return HLX_CLIENT_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_wildcarding(bool a_val)
{
        m_wildcarding = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::set_data(const char *a_data, uint32_t a_len)
{

        // If a_data starts with @ assume file
        if(a_data[0] == '@')
        {
                std::string l_file_str = a_data + 1;

                // ---------------------------------------
                // Check is a file
                // TODO
                // ---------------------------------------
                struct stat l_stat;
                int32_t l_status = STATUS_OK;
                l_status = stat(l_file_str.c_str(), &l_stat);
                if(l_status != 0)
                {
                        //NDBG_PRINT("Error performing stat on file: %s.  Reason: %s\n", a_ai_cache_file.c_str(), strerror(errno));
                        return HLX_CLIENT_STATUS_ERROR;
                }

                // Check if is regular file
                if(!(l_stat.st_mode & S_IFREG))
                {
                        //NDBG_PRINT("Error opening file: %s.  Reason: is NOT a regular file\n", a_ai_cache_file.c_str());
                        return HLX_CLIENT_STATUS_ERROR;
                }

                // ---------------------------------------
                // Open file...
                // ---------------------------------------
                FILE * l_file;
                l_file = fopen(l_file_str.c_str(),"r");
                if (NULL == l_file)
                {
                        //NDBG_PRINT("Error opening file: %s.  Reason: %s\n", a_ai_cache_file.c_str(), strerror(errno));
                        return HLX_CLIENT_STATUS_ERROR;
                }

                // ---------------------------------------
                // Read in file...
                // ---------------------------------------
                int32_t l_size = l_stat.st_size;

                // Bounds check -remove later
                if(l_size > 8*1024)
                {
                        return HLX_CLIENT_STATUS_ERROR;
                }

                m_req_body = (char *)malloc(sizeof(char)*l_size);
                m_req_body_len = l_size;

                int32_t l_read_size;
                l_read_size = fread(m_req_body, 1, l_size, l_file);
                if(l_read_size != l_size)
                {
                        //NDBG_PRINT("Error performing fread.  Reason: %s [%d:%d]\n",
                        //                strerror(errno), l_read_size, l_size);
                        return HLX_CLIENT_STATUS_ERROR;
                }

                // ---------------------------------------
                // Close file...
                // ---------------------------------------
                l_status = fclose(l_file);
                if (STATUS_OK != l_status)
                {
                        //NDBG_PRINT("Error performing fclose.  Reason: %s\n", strerror(errno));
                        return HLX_CLIENT_STATUS_ERROR;
                }
        }
        else
        {
                // Bounds check -remove later
                if(a_len > 8*1024)
                {
                        return HLX_CLIENT_STATUS_ERROR;
                }

                m_req_body = (char *)malloc(sizeof(char)*a_len);
                memcpy(m_req_body, a_data, a_len);
                m_req_body_len = a_len;

        }

        // Add content length
        char l_len_str[64];
        sprintf(l_len_str, "%u", m_req_body_len);
        set_header("Content-Length", l_len_str);

        return HLX_CLIENT_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::set_host_list(host_list_t &a_host_list)
{
        // Create the reqlet list
        uint32_t l_reqlet_num = 0;
        for(host_list_t::const_iterator i_host = a_host_list.begin();
                        i_host != a_host_list.end();
                        ++i_host, ++l_reqlet_num)
        {
                // Create a re
                reqlet *l_reqlet = new reqlet(l_reqlet_num, 1);
                l_reqlet->init_with_url(m_url);

                // Get host and port if exist
                parsed_url l_url;

                //TODO REMOVE!!!
                //NDBG_PRINT("HOST: %s\n", i_host->m_host.c_str());

                l_url.parse(i_host->m_host);

                if(strchr(i_host->m_host.c_str(), (int)':'))
                {
                        l_reqlet->set_host(l_url.m_host);
                        l_reqlet->set_port(l_url.m_port);
                }
                else
                {
                        // TODO make set host take const
                        l_reqlet->set_host(i_host->m_host);
                }

                if(!i_host->m_hostname.empty())
                {
                     l_reqlet->m_url.m_hostname = i_host->m_hostname;
                }
                if(!i_host->m_id.empty())
                {
                     l_reqlet->m_url.m_id = i_host->m_id;
                }
                if(!i_host->m_where.empty())
                {
                     l_reqlet->m_url.m_where = i_host->m_where;
                }

                // Add to list
                m_reqlet_vector.push_back(l_reqlet);
        }

        m_num_end_fetches = m_reqlet_vector.size();

        return HLX_CLIENT_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::set_server_list(server_list_t &a_server_list)
{
        // Create the reqlet list
        uint32_t l_reqlet_num = 0;
        for(server_list_t::const_iterator i_server = a_server_list.begin();
            i_server != a_server_list.end();
            ++i_server, ++l_reqlet_num)
        {
                // Create a re
                reqlet *l_reqlet = new reqlet(l_reqlet_num, 1);
                l_reqlet->init_with_url(m_url);

                // Get host and port if exist
                parsed_url l_url;
                l_url.parse(*i_server);

                if(strchr(i_server->c_str(), (int)':'))
                {
                        l_reqlet->set_host(l_url.m_host);
                        l_reqlet->set_port(l_url.m_port);
                }
                else
                {
                        // TODO make set host take const
                        l_reqlet->set_host(*i_server);
                }

                // Add to list
                m_reqlet_vector.push_back(l_reqlet);
        }

        m_num_end_fetches = m_reqlet_vector.size();

        return HLX_CLIENT_STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlx_client::add_url(std::string &a_url)
{

        // TODO
        // Make threadsafe...

        reqlet *l_reqlet = new reqlet((uint64_t)(m_reqlet_vector.size()));

        // Initialize
        int32_t l_status = HLX_CLIENT_STATUS_OK;
        l_status = l_reqlet->init_with_url(a_url, m_wildcarding);
        if(STATUS_OK != l_status)
        {
                NDBG_PRINT("Error performing init_with_url: %s\n", a_url.c_str());
                return HLX_CLIENT_STATUS_ERROR;
        }

        // Add to list
        m_reqlet_vector.push_back(l_reqlet);

        return HLX_CLIENT_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlx_client::add_url_file(std::string &a_url_file)
{

        FILE * l_file;
        int32_t l_status = HLX_CLIENT_STATUS_OK;

        l_file = fopen (a_url_file.c_str(),"r");
        if (NULL == l_file)
        {
                NDBG_PRINT("Error opening file.  Reason: %s\n", strerror(errno));
                return HLX_CLIENT_STATUS_ERROR;
        }

        //NDBG_PRINT("ADD_FILE: ADDING: %s\n", a_url_file.c_str());
        //uint32_t l_num_added = 0;

        ssize_t l_file_line_size = 0;
        char *l_file_line = NULL;
        size_t l_unused;
        while((l_file_line_size = getline(&l_file_line,&l_unused,l_file)) != -1)
        {

                //NDBG_PRINT("LINE: %s", l_file_line);

                std::string l_line(l_file_line);

                if(!l_line.empty())
                {
                        //NDBG_PRINT("Add url: %s\n", l_line.c_str());

                        l_line.erase( std::remove_if( l_line.begin(), l_line.end(), ::isspace ), l_line.end() );
                        if(!l_line.empty())
                        {
                                l_status = add_url(l_line);
                                if(STATUS_OK != l_status)
                                {
                                        NDBG_PRINT("Error performing addurl for url: %s\n", l_line.c_str());

                                        if(l_file_line)
                                        {
                                                free(l_file_line);
                                                l_file_line = NULL;
                                        }
                                        return HLX_CLIENT_STATUS_ERROR;
                                }
                        }
                }

                if(l_file_line)
                {
                        free(l_file_line);
                        l_file_line = NULL;
                }

        }

        //NDBG_PRINT("ADD_FILE: DONE: %s -- last line len: %d\n", a_url_file.c_str(), (int)l_file_line_size);
        //if(l_file_line_size == -1)
        //{
        //        NDBG_PRINT("Error: getline errno: %d Reason: %s\n", errno, strerror(errno));
        //}


        l_status = fclose(l_file);
        if (0 != l_status)
        {
                NDBG_PRINT("Error closing file.  Reason: %s\n", strerror(errno));
                return HLX_CLIENT_STATUS_ERROR;
        }

        return HLX_CLIENT_STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_split_requests_by_thread(bool a_val)
{
        m_split_requests_by_thread = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_connect_only(bool a_val)
{
        m_connect_only = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_use_ai_cache(bool a_val)
{
        m_use_ai_cache = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_ai_cache(const std::string &a_ai_cache)
{
        m_ai_cache = a_ai_cache;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_timeout_s(uint32_t a_timeout_s)
{
        m_timeout_s = a_timeout_s;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_num_threads(uint32_t a_num_threads)
{
        m_num_threads = a_num_threads;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_num_parallel(uint32_t a_num_parallel)
{
        m_num_parallel = a_num_parallel;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_show_summary(bool a_val)
{
        m_show_summary = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_run_time_s(int32_t a_val)
{
        m_run_time_s = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_end_fetches(int32_t a_val)
{
        m_num_end_fetches = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_num_reqs_per_conn(int32_t a_val)
{
        m_num_reqs_per_conn = a_val;
        if((m_num_reqs_per_conn > 1) ||
           (m_num_reqs_per_conn < 0))
        {
                set_header("Connection", "keep-alive");
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_rate(int32_t a_val)
{
        m_rate = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_request_mode(request_mode_t a_mode)
{
        m_request_mode = a_mode;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_save_response(bool a_val)
{
        m_save_response = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_collect_stats(bool a_val)
{
        m_collect_stats = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_use_persistent_pool(bool a_val)
{
        m_use_persistent_pool = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_sock_opt_no_delay(bool a_val)
{
        m_sock_opt_no_delay = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_sock_opt_send_buf_size(uint32_t a_send_buf_size)
{
        m_sock_opt_send_buf_size = a_send_buf_size;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_sock_opt_recv_buf_size(uint32_t a_recv_buf_size)
{
        m_sock_opt_recv_buf_size = a_recv_buf_size;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::set_header(const std::string &a_header)
{
        int32_t l_status;
        std::string l_header_key;
        std::string l_header_val;
        l_status = break_header_string(a_header, l_header_key, l_header_val);
        if(l_status != 0)
        {
                // If verbose???
                //printf("Error header string[%s] is malformed\n", a_header.c_str());
                return HLX_CLIENT_STATUS_ERROR;
        }
        set_header(l_header_key, l_header_val);

        return HLX_CLIENT_STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::set_header(const std::string &a_key, const std::string &a_val)
{
        m_header_map[a_key] = a_val;
        return HLX_CLIENT_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::clear_headers(void)
{
        m_header_map.clear();
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_verb(const std::string &a_verb)
{
        m_verb = a_verb;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_ssl_cipher_list(const std::string &a_cipher_list)
{
        m_ssl_cipher_list = a_cipher_list;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::set_ssl_options(const std::string &a_ssl_options_str)
{
        int32_t l_status;
        l_status = get_ssl_options_str_val(a_ssl_options_str, m_ssl_options);
        if(l_status != HLX_CLIENT_STATUS_OK)
        {
                return HLX_CLIENT_STATUS_ERROR;
        }
        return HLX_CLIENT_STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::set_ssl_options(long a_ssl_options)
{
        m_ssl_options = a_ssl_options;
        return HLX_CLIENT_STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_ssl_ca_path(const std::string &a_ssl_ca_path)
{
        m_ssl_ca_path = a_ssl_ca_path;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_ssl_ca_file(const std::string &a_ssl_ca_file)
{
        m_ssl_ca_file = a_ssl_ca_file;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_ssl_sni_verify(bool a_val)
{
        m_ssl_sni = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::set_ssl_verify(bool a_val)
{
        m_ssl_verify = a_val;
}

//: ----------------------------------------------------------------------------
//: \details: Constructor
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hlx_client::hlx_client(void):

        // General
        m_verbose(false),
        m_color(false),
        m_quiet(false),

        // Run settings
        m_url(),
        m_url_file(),
        m_wildcarding(false),

        m_req_body(NULL),
        m_req_body_len(0),

        m_header_map(),

        // TODO Make define
        m_verb("GET"),

        m_use_ai_cache(true),
        m_ai_cache(),

        // TODO Make define
        m_num_parallel(128),

        // TODO Make define
        m_num_threads(4),

        // TODO Make define
        m_timeout_s(10),

        m_connect_only(false),
        m_show_summary(false),
        m_save_response(false),
        m_collect_stats(false),
        m_use_persistent_pool(false),

        m_rate(-1),
        m_num_end_fetches(-1),
        m_num_reqs_per_conn(1),
        m_run_time_s(-1),

        // TODO Make define
        m_request_mode(REQUEST_MODE_ROUND_ROBIN),
        m_split_requests_by_thread(true),

        // Socket options
        m_sock_opt_recv_buf_size(0),
        m_sock_opt_send_buf_size(0),
        m_sock_opt_no_delay(false),

        // SSL
        m_ssl_ctx(NULL),
        m_ssl_cipher_list(),
        m_ssl_options_str(),
        m_ssl_options(0),
        m_ssl_verify(false),
        m_ssl_sni(false),
        m_ssl_ca_file(),
        m_ssl_ca_path(),

        // t_client
        m_t_client_list(),

        // TODO Make define
        m_evr_loop_type(EVR_LOOP_EPOLL),

        m_reqlet_vector(),

        m_resolver(NULL),

        m_is_initd(false),
        m_start_time_ms(0)
{

};

//: ----------------------------------------------------------------------------
//: \details: Constructor
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
hlx_client::~hlx_client(void)
{
        // Delete reqlets
        for(size_t i_reqlet = 0;
            i_reqlet < m_reqlet_vector.size();
            ++i_reqlet)
        {
                reqlet *i_reqlet_ptr = m_reqlet_vector[i_reqlet];
                if(i_reqlet_ptr)
                {
                        delete i_reqlet_ptr;
                        i_reqlet_ptr = NULL;
                }
                m_reqlet_vector.clear();
        }

        // Delete t_client list...
        for(t_client_list_t::iterator i_client_hle = m_t_client_list.begin();
                        i_client_hle != m_t_client_list.end(); )
        {
                t_client *l_t_client_ptr = *i_client_hle;
                if(l_t_client_ptr)
                {
                        delete l_t_client_ptr;
                        m_t_client_list.erase(i_client_hle++);
                        l_t_client_ptr = NULL;
                }
        }

        // SSL Cleanup
        ssl_kill_locks();

        // TODO Deprecated???
        //EVP_cleanup();

        if(m_req_body)
        {
                free(m_req_body);
                m_req_body = NULL;
                m_req_body_len = 0;
        }

        delete m_resolver;
        m_resolver = NULL;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#define ARESP(_str) l_responses_str += _str
std::string hlx_client::dump_all_responses(bool a_color, bool a_pretty, output_type_t a_output_type, int a_part_map)
{
        std::string l_responses_str = "";
        switch(a_output_type)
        {
        case OUTPUT_LINE_DELIMITED:
        {
                l_responses_str = dump_all_responses_line_dl(a_color, a_pretty, a_part_map);
                break;
        }
        case OUTPUT_JSON:
        {
                l_responses_str = dump_all_responses_json(a_part_map);
                break;
        }
        default:
        {
                break;
        }
        }

        return l_responses_str;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
std::string hlx_client::dump_all_responses_line_dl(bool a_color,
                                                   bool a_pretty,
                                                   int a_part_map)
{

        std::string l_responses_str = "";
        std::string l_host_color = "";
        std::string l_server_color = "";
        std::string l_id_color = "";
        std::string l_status_color = "";
        std::string l_header_color = "";
        std::string l_body_color = "";
        std::string l_off_color = "";
        char l_buf[1024*1024];
        if(a_color)
        {
                l_host_color = ANSI_COLOR_FG_BLUE;
                l_server_color = ANSI_COLOR_FG_RED;
                l_id_color = ANSI_COLOR_FG_CYAN;
                l_status_color = ANSI_COLOR_FG_MAGENTA;
                l_header_color = ANSI_COLOR_FG_GREEN;
                l_body_color = ANSI_COLOR_FG_YELLOW;
                l_off_color = ANSI_COLOR_OFF;
        }

        for(t_client_list_t::const_iterator i_client = m_t_client_list.begin();
           i_client != m_t_client_list.end();)
        {
                const reqlet_vector_t &l_reqlet_vector = (*i_client)->get_reqlet_vector();

                int l_cur_reqlet = 0;
                for(reqlet_vector_t::const_iterator i_reqlet = l_reqlet_vector.begin();
                    i_reqlet != l_reqlet_vector.end();
                    ++i_reqlet, ++l_cur_reqlet)
                {


                        bool l_fbf = false;
                        // Host
                        if(a_part_map & PART_HOST)
                        {
                                sprintf(l_buf, "\"%shost%s\": \"%s\"",
                                                l_host_color.c_str(), l_off_color.c_str(),
                                                (*i_reqlet)->m_url.m_host.c_str());
                                ARESP(l_buf);
                                l_fbf = true;
                        }

                        // Server
                        if(a_part_map & PART_SERVER)
                        {

                                if(l_fbf) {ARESP(", "); l_fbf = false;}
                                sprintf(l_buf, "\"%sserver%s\": \"%s:%d\"",
                                                l_server_color.c_str(), l_server_color.c_str(),
                                                (*i_reqlet)->m_url.m_host.c_str(),
                                                (*i_reqlet)->m_url.m_port
                                                );
                                ARESP(l_buf);
                                l_fbf = true;

                                if(!(*i_reqlet)->m_url.m_id.empty())
                                {
                                        if(l_fbf) {ARESP(", "); l_fbf = false;}
                                        sprintf(l_buf, "\"%sid%s\": \"%s\"",
                                                        l_id_color.c_str(), l_id_color.c_str(),
                                                        (*i_reqlet)->m_url.m_id.c_str()
                                                        );
                                        ARESP(l_buf);
                                        l_fbf = true;
                                }

                                if(!(*i_reqlet)->m_url.m_where.empty())
                                {
                                        if(l_fbf) {ARESP(", "); l_fbf = false;}
                                        sprintf(l_buf, "\"%swhere%s\": \"%s\"",
                                                        l_id_color.c_str(), l_id_color.c_str(),
                                                        (*i_reqlet)->m_url.m_where.c_str()
                                                        );
                                        ARESP(l_buf);
                                        l_fbf = true;
                                }


                                l_fbf = true;
                        }

                        // Status Code
                        if(a_part_map & PART_STATUS_CODE)
                        {
                                if(l_fbf) {ARESP(", "); l_fbf = false;}
                                const char *l_status_val_color = "";
                                if(a_color)
                                {
                                        if((*i_reqlet)->m_status == 200) l_status_val_color = ANSI_COLOR_FG_GREEN;
                                        else l_status_val_color = ANSI_COLOR_FG_RED;
                                }
                                sprintf(l_buf, "\"%sstatus-code%s\": %s%d%s",
                                                l_status_color.c_str(), l_off_color.c_str(),
                                                l_status_val_color, (*i_reqlet)->m_status, l_off_color.c_str());
                                ARESP(l_buf);
                                l_fbf = true;
                        }

                        // Headers
                        // TODO -only in json mode for now
                        if(a_part_map & PART_HEADERS)
                        {
                                // nuthin
                        }

                        // Body
                        if(a_part_map & PART_BODY)
                        {
                                if(l_fbf) {ARESP(", "); l_fbf = false;}
                                //NDBG_PRINT("RESPONSE SIZE: %ld\n", (*i_reqlet)->m_response_body.length());
                                if(!(*i_reqlet)->m_body.empty())
                                {
                                        sprintf(l_buf, "\"%sbody%s\": %s",
                                                        l_body_color.c_str(), l_off_color.c_str(),
                                                        (*i_reqlet)->m_body.c_str());
                                }
                                else
                                {
                                        sprintf(l_buf, "\"%sbody%s\": \"NO_RESPONSE\"",
                                                        l_body_color.c_str(), l_off_color.c_str());
                                }
                                ARESP(l_buf);
                                l_fbf = true;
                        }
                        ARESP("\n");
                }
                ++i_client;
        }

        return l_responses_str;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
#define JS_ADD_MEMBER(_key, _val)\
l_obj.AddMember(_key,\
                rapidjson::Value(_val, l_js_allocator).Move(),\
                l_js_allocator)\

std::string hlx_client::dump_all_responses_json(int a_part_map)
{
        rapidjson::Document l_js_doc;
        l_js_doc.SetObject();
        rapidjson::Value l_js_array(rapidjson::kArrayType);
        rapidjson::Document::AllocatorType& l_js_allocator = l_js_doc.GetAllocator();

        for(t_client_list_t::const_iterator i_client = m_t_client_list.begin();
           i_client != m_t_client_list.end();
           ++i_client)
        {
                const reqlet_vector_t &l_reqlet_vector = (*i_client)->get_reqlet_vector();
                for(reqlet_vector_t::const_iterator i_reqlet = l_reqlet_vector.begin();
                    i_reqlet != l_reqlet_vector.end();
                    ++i_reqlet)
                {
                        rapidjson::Value l_obj;
                        l_obj.SetObject();
                        bool l_content_type_json = false;

                        // Search for json
                        header_map_t::const_iterator i_h = (*i_reqlet)->m_headers.find("Content-type");
                        if(i_h != (*i_reqlet)->m_headers.end() && i_h->second == "application/json")
                        {
                                l_content_type_json = true;
                        }

                        // Host
                        if(a_part_map & PART_HOST)
                        {
                                JS_ADD_MEMBER("host", (*i_reqlet)->m_url.m_host.c_str());
                        }

                        // Server
                        if(a_part_map & PART_SERVER)
                        {
                                char l_server_buf[1024];
                                sprintf(l_server_buf, "%s:%d",
                                                (*i_reqlet)->m_url.m_host.c_str(),
                                                (*i_reqlet)->m_url.m_port);
                                JS_ADD_MEMBER("server", l_server_buf);

                                if(!(*i_reqlet)->m_url.m_id.empty())
                                {
                                        JS_ADD_MEMBER("id", (*i_reqlet)->m_url.m_id.c_str());
                                }

                                if(!(*i_reqlet)->m_url.m_where.empty())
                                {
                                        JS_ADD_MEMBER("where", (*i_reqlet)->m_url.m_where.c_str());
                                }
                        }

                        // Status Code
                        if(a_part_map & PART_STATUS_CODE)
                        {
                                l_obj.AddMember("status-code", (*i_reqlet)->m_status, l_js_allocator);
                        }

                        // Headers
                        if(a_part_map & PART_HEADERS)
                        {
                                for(header_map_t::iterator i_header = (*i_reqlet)->m_headers.begin();
                                                i_header != (*i_reqlet)->m_headers.end();
                                    ++i_header)
                                {
                                        l_obj.AddMember(rapidjson::Value(i_header->first.c_str(), l_js_allocator).Move(),
                                                        rapidjson::Value(i_header->second.c_str(), l_js_allocator).Move(),
                                                        l_js_allocator);
                                }
                        }

                        // Connection info
                        //if(a_part_map & PART_HEADERS)
                        for(header_map_t::iterator i_header = (*i_reqlet)->m_conn_info.begin();
                                        i_header != (*i_reqlet)->m_conn_info.end();
                            ++i_header)
                        {
                                l_obj.AddMember(rapidjson::Value(i_header->first.c_str(), l_js_allocator).Move(),
                                                rapidjson::Value(i_header->second.c_str(), l_js_allocator).Move(),
                                                l_js_allocator);
                        }

                        // Body
                        if(a_part_map & PART_BODY)
                        {

                                //NDBG_PRINT("RESPONSE SIZE: %ld\n", (*i_reqlet)->m_response_body.length());
                                if(!(*i_reqlet)->m_body.empty())
                                {
                                        // Append json
                                        if(l_content_type_json)
                                        {
                                                rapidjson::Document l_doc_body;
                                                l_doc_body.Parse((*i_reqlet)->m_body.c_str());
                                                l_obj.AddMember("body",
                                                                rapidjson::Value(l_doc_body, l_js_allocator).Move(),
                                                                l_js_allocator);

                                        }
                                        else
                                        {
                                                JS_ADD_MEMBER("body", (*i_reqlet)->m_body.c_str());
                                        }
                                }
                                else
                                {
                                        JS_ADD_MEMBER("body", "NO_RESPONSE");
                                }
                        }

                        l_js_array.PushBack(l_obj, l_js_allocator);

                }
        }

        // TODO -Can I just create an array -do I have to stick in a document?
        l_js_doc.AddMember("array", l_js_array, l_js_allocator);
        rapidjson::StringBuffer l_strbuf;
        rapidjson::Writer<rapidjson::StringBuffer> l_js_writer(l_strbuf);
        l_js_doc["array"].Accept(l_js_writer);

        //NDBG_PRINT("Document: \n%s\n", l_strbuf.GetString());
        std::string l_responses_str = l_strbuf.GetString();
        return l_responses_str;
}


//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  client status indicating success or failure
//: \param:   TODO
//: ----------------------------------------------------------------------------
int hlx_client::init(void)
{

        if(true == m_is_initd)
                return HLX_CLIENT_STATUS_OK;
        // not initialized yet

        m_resolver = new resolver();

        // -------------------------------------------
        // Init resolver with cache
        // -------------------------------------------
        int32_t l_ldb_init_status;
        l_ldb_init_status = m_resolver->init(m_ai_cache, m_use_ai_cache);
        if(STATUS_OK != l_ldb_init_status)
        {
                return HLX_CLIENT_STATUS_ERROR;
        }

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
                return HLX_CLIENT_STATUS_ERROR;
        }

        m_is_initd = true;
        return HLX_CLIENT_STATUS_OK;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void hlx_client::get_stats(t_stat_t &ao_all_stats,
                           bool a_get_breakdown,
                           tag_stat_map_t &ao_breakdown_stats) const
{
        // -------------------------------------------
        // Aggregate
        // -------------------------------------------
        tag_stat_map_t l_copy;
        for(t_client_list_t::const_iterator i_client = m_t_client_list.begin();
           i_client != m_t_client_list.end();
           ++i_client)
        {
                const reqlet_vector_t &l_reqlet_vector = (*i_client)->get_reqlet_vector();
                for(reqlet_vector_t::const_iterator i_reqlet = l_reqlet_vector.begin(); i_reqlet != l_reqlet_vector.end(); ++i_reqlet)
                {
                        std::string l_label = (*i_reqlet)->get_label();
                        tag_stat_map_t::iterator i_copy = l_copy.find(l_label);
                        if(i_copy != l_copy.end())
                        {
                                add_to_total_stat_agg(i_copy->second, (*i_reqlet)->m_stat_agg);
                        }
                        else
                        {
                                l_copy[(*i_reqlet)->get_label()] = (*i_reqlet)->m_stat_agg;
                        }
                }
        }

        for(tag_stat_map_t::iterator i_reqlet = l_copy.begin(); i_reqlet != l_copy.end(); ++i_reqlet)
        {
                if(a_get_breakdown)
                {
                        std::string l_tag = i_reqlet->first;
                        tag_stat_map_t::iterator i_stat;
                        if((i_stat = ao_breakdown_stats.find(l_tag)) == ao_breakdown_stats.end())
                        {
                                ao_breakdown_stats[l_tag] = i_reqlet->second;
                        }
                        else
                        {
                                // Add to existing
                                add_to_total_stat_agg(i_stat->second, i_reqlet->second);
                        }
                }

                // Add to total
                add_to_total_stat_agg(ao_all_stats, i_reqlet->second);
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t hlx_client::get_stats_json(char *l_json_buf, uint32_t l_json_buf_max_len)
{

        tag_stat_map_t l_tag_stat_map;
        t_stat_t l_total;

        uint64_t l_time_ms = get_time_ms();
        // Get stats
        get_stats(l_total, true, l_tag_stat_map);

        int l_cur_offset = 0;
        l_cur_offset += snprintf(l_json_buf + l_cur_offset, l_json_buf_max_len - l_cur_offset,"{\"data\": [");
        bool l_first_stat = true;
        for(tag_stat_map_t::iterator i_agg_stat = l_tag_stat_map.begin();
                        i_agg_stat != l_tag_stat_map.end();
                        ++i_agg_stat)
        {

                if(l_first_stat) l_first_stat = false;
                else
                        l_cur_offset += snprintf(l_json_buf + l_cur_offset, l_json_buf_max_len - l_cur_offset,",");

                l_cur_offset += snprintf(l_json_buf + l_cur_offset, l_json_buf_max_len - l_cur_offset,
                                "{\"key\": \"%s\", \"value\": ",
                                i_agg_stat->first.c_str());

                l_cur_offset += snprintf(l_json_buf + l_cur_offset, l_json_buf_max_len - l_cur_offset,
                                "{\"%s\": %" PRIu64 ", \"%s\": %" PRIu64 "}",
                                "count", (uint64_t)(i_agg_stat->second.m_total_reqs),
                                "time", (uint64_t)(l_time_ms));

                l_cur_offset += snprintf(l_json_buf + l_cur_offset, l_json_buf_max_len - l_cur_offset,"}");
        }

        l_cur_offset += snprintf(l_json_buf + l_cur_offset, l_json_buf_max_len - l_cur_offset,"]}");


        return l_cur_offset;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void t_stat_struct::clear(void)
{
        // Stats
        m_stat_us_connect.clear();
        m_stat_us_connect.clear();
        m_stat_us_ssl_connect.clear();
        m_stat_us_first_response.clear();
        m_stat_us_download.clear();
        m_stat_us_end_to_end.clear();

        // Totals
        m_total_bytes = 0;
        m_total_reqs = 0;

        // Client stats
        m_num_resolved = 0;
        m_num_conn_started = 0;
        m_num_conn_completed = 0;
        m_num_idle_killed = 0;
        m_num_errors = 0;
        m_num_bytes_read = 0;

        m_status_code_count_map.clear();
}

} //namespace ns_hlx {
