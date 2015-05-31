//: ----------------------------------------------------------------------------
//: Copyright (C) 2015 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    nconn_pool.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    05/27/2015
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
#include "nconn_pool.h"

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define T_CLIENT_SET_NCONN_OPT(_conn, _opt, _buf, _len) \
        do { \
                int _status = 0; \
                _status = _conn.set_opt((_opt), (_buf), (_len)); \
                if (_status != STATUS_OK) { \
                        NDBG_PRINT("STATUS_ERROR: Failed to set_opt %d.  Status: %d.\n", _opt, _status); \
                        return NULL;\
                } \
        } while(0)

#define HLX_CLIENT_GET_NCONN_OPT(_conn, _opt, _buf, _len) \
        do { \
                int _status = 0; \
                _status = _conn.get_opt((_opt), (_buf), (_len)); \
                if (_status != STATUS_OK) { \
                        NDBG_PRINT("STATUS_ERROR: Failed to get_opt %d.  Status: %d.\n", _opt, _status); \
                        return NULL;\
                } \
        } while(0)

namespace ns_hlx {
//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nconn *nconn_pool::get(reqlet *a_reqlet,
                       const settings_struct_t &a_settings)
{

        if(!m_initd)
        {
                init();
        }

        if(!a_reqlet)
        {
                NDBG_PRINT("Error a_reqlet == NULL\n");
                return NULL;
        }

        // ---------------------------------------
        // Try to grab from conn cache
        // ---------------------------------------
        if(m_idle_conn_ncache.size())
        {
                uint64_t l_hash = a_reqlet->get_label_hash();

                // ---------------------------------------
                // Lookup label
                // ---------------------------------------
                nconn* const*l_nconn_lkp = m_idle_conn_ncache.get(l_hash);
                if(l_nconn_lkp)
                {
                        nconn *l_nconn = NULL;
                        l_nconn = const_cast<nconn *>(*l_nconn_lkp);
                        return l_nconn;
                }

                // -----------------------------------------
                // Try create new from free-list
                // -----------------------------------------
                if(m_conn_free_list.empty())
                {
                        // Evict from idle_conn_ncache
                        m_idle_conn_ncache.evict();
                }

        }

        if(m_conn_free_list.empty())
        {
                //NDBG_PRINT("Error no free connections\n");
                return NULL;
        }

        // -----------------------------------------
        // Get connection for reqlet
        // Either return existing connection or
        // null for new connection
        // -----------------------------------------
        nconn *l_nconn = NULL;
        conn_id_list_t::iterator i_conn = m_conn_free_list.begin();

        // Start client for this reqlet
        //NDBG_PRINT("i_conn: %d\n", *i_conn);
        l_nconn = m_nconn_vector[*i_conn];
        // TODO Check for NULL

        if(l_nconn &&
           (l_nconn->m_scheme != a_reqlet->m_url.m_scheme))
        {
                // Destroy nconn and recreate
                //NDBG_PRINT("Destroy nconn and recreate: %u -- l_nconn->m_scheme: %d -- l_reqlet->m_url.m_scheme: %d\n",
                //                *i_conn,
                //                l_nconn->m_scheme,
                //                l_reqlet->m_url.m_scheme);
                delete l_nconn;
                l_nconn = NULL;
        }


        if(!l_nconn)
        {
                //-----------------------------------------------------
                // Create nconn BEGIN
                //-----------------------------------------------------
                // TODO Make function
                //-----------------------------------------------------

                //NDBG_PRINT("CREATING NEW CONNECTION: id: %u\n", a_id);
                if(a_reqlet->m_url.m_scheme == nconn::SCHEME_TCP)
                {
                        l_nconn = new nconn_tcp(a_settings.m_verbose,
                                        a_settings.m_color,
                                        a_settings.m_num_reqs_per_conn,
                                        a_settings.m_save_response,
                                        a_settings.m_collect_stats,
                                        a_settings.m_connect_only);
                }
                else if(a_reqlet->m_url.m_scheme == nconn::SCHEME_SSL)
                {
                        l_nconn = new nconn_ssl(a_settings.m_verbose,
                                        a_settings.m_color,
                                        a_settings.m_num_reqs_per_conn,
                                        a_settings.m_save_response,
                                        a_settings.m_collect_stats,
                                        a_settings.m_connect_only);
                }

                l_nconn->set_id(*i_conn);

                // -------------------------------------------
                // Set options
                // -------------------------------------------
                // Set generic options
                T_CLIENT_SET_NCONN_OPT((*l_nconn), nconn_tcp::OPT_TCP_RECV_BUF_SIZE, NULL, a_settings.m_sock_opt_recv_buf_size);
                T_CLIENT_SET_NCONN_OPT((*l_nconn), nconn_tcp::OPT_TCP_SEND_BUF_SIZE, NULL, a_settings.m_sock_opt_send_buf_size);
                T_CLIENT_SET_NCONN_OPT((*l_nconn), nconn_tcp::OPT_TCP_NO_DELAY, NULL, a_settings.m_sock_opt_no_delay);

                // Set ssl options
                if(a_reqlet->m_url.m_scheme == nconn::SCHEME_SSL)
                {
                        T_CLIENT_SET_NCONN_OPT((*l_nconn),
                                               nconn_ssl::OPT_SSL_CIPHER_STR,
                                               a_settings.m_ssl_cipher_list.c_str(),
                                               a_settings.m_ssl_cipher_list.length());

                        T_CLIENT_SET_NCONN_OPT((*l_nconn),
                                               nconn_ssl::OPT_SSL_CTX,
                                               a_settings.m_ssl_ctx,
                                               sizeof(a_settings.m_ssl_ctx));

                        T_CLIENT_SET_NCONN_OPT((*l_nconn),
                                               nconn_ssl::OPT_SSL_VERIFY,
                                               &(a_settings.m_ssl_verify),
                                               sizeof(a_settings.m_ssl_verify));

                        //T_CLIENT_SET_NCONN_OPT((*l_nconn), nconn_ssl::OPT_SSL_OPTIONS,
                        //                              &(m_settings.m_ssl_options),
                        //                              sizeof(m_settings.m_ssl_options));

                }
                //-----------------------------------------------------
                // Create nconn END
                //-----------------------------------------------------

                m_nconn_vector[*i_conn] = l_nconn;
        }

        // Assign the reqlet for this client
        l_nconn->set_data1(a_reqlet);

        m_conn_used_set.insert(*i_conn);
        m_conn_free_list.erase(i_conn++);

        l_nconn->set_host(a_reqlet->m_url.m_host);

        return l_nconn;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_pool::add_idle(nconn *a_nconn)
{

        if(!m_initd)
        {
                init();
        }

        if(!a_nconn)
        {
                NDBG_PRINT("Error a_nconn == NULL\n");
                return STATUS_ERROR;
        }


        reqlet *l_reqlet = reinterpret_cast<reqlet *>(a_nconn->m_data1);
        if(!l_reqlet)
        {
                NDBG_PRINT("Error l_reqlet == NULL\n");
                return STATUS_ERROR;
        }

        uint64_t l_hash = l_reqlet->get_label_hash();
        if(!m_idle_conn_ncache.get(l_hash))
        {
                // Insert
                //NDBG_PRINT(" ::NCACHE::%sINSERT%s: HASH[%24lu] %p\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, l_hash, a_nconn);
                m_idle_conn_ncache.insert(l_hash, a_nconn);
        }

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int nconn_pool::delete_cb(void* o_1, void *a_2)
{

        //NDBG_PRINT("::NCACHE::%sDELETE%s: %p %p\n", ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF, o_1, a_2);

        nconn_pool *l_nconn_pool = reinterpret_cast<nconn_pool *>(o_1);
        nconn *l_nconn = *(reinterpret_cast<nconn **>(a_2));
        l_nconn_pool->release(l_nconn);
        return 0;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t nconn_pool::release(nconn *a_nconn)
{
        if(!m_initd)
        {
                init();
        }

        if(!a_nconn)
        {
                NDBG_PRINT("Error a_nconn == NULL\n");
                return STATUS_ERROR;
        }

        if(STATUS_OK != a_nconn->cleanup())
        {
                return STATUS_ERROR;
        }

        uint64_t l_conn_id = a_nconn->get_id();
        a_nconn->reset_stats();
        m_conn_free_list.push_back(l_conn_id);
        m_conn_used_set.erase(l_conn_id);

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void nconn_pool::init(void)
{
        m_idle_conn_ncache.set_delete_cb(delete_cb, this);
};

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nconn_pool::nconn_pool(uint32_t a_size):
                       m_nconn_vector(a_size),
                       m_conn_free_list(),
                       m_conn_used_set(),
                       m_idle_conn_ncache(a_size, NCACHE_LRU),
                       m_initd(false)
{
        for(uint32_t i_conn = 0; i_conn < a_size; ++i_conn)
        {
                m_nconn_vector[i_conn] = NULL;
                //NDBG_PRINT("ADDING i_conn: %u\n", i_conn);
                m_conn_free_list.push_back(i_conn);
        }

};

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
nconn_pool::~nconn_pool(void)
{
        for(uint32_t i_conn = 0; i_conn < m_nconn_vector.size(); ++i_conn)
        {
                if(m_nconn_vector[i_conn])
                {
                        delete m_nconn_vector[i_conn];
                        m_nconn_vector[i_conn] = NULL;
                }
        }

}

}
