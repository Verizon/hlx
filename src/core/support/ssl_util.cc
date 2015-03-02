//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    ssl_util.cc
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

//: ----------------------------------------------------------------------------
//:                          OpenSSL Support
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "ndebug.h"
#include "ssl_util.h"

#include <pthread.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>


//: ----------------------------------------------------------------------------
//: Globals
//: ----------------------------------------------------------------------------
static pthread_mutex_t *g_lock_cs;

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static void pthreads_locking_callback(int a_mode, int a_type, const char *a_file, int a_line)
{
#if 0
        fprintf(stdout,"thread=%4d mode=%s lock=%s %s:%d\n",
                        (int)CRYPTO_thread_id(),
                        (mode&CRYPTO_LOCK)?"l":"u",
                                        (type&CRYPTO_READ)?"r":"w",a_file,a_line);
#endif

#if 0
        if (CRYPTO_LOCK_SSL_CERT == type)
                fprintf(stdout,"(t,m,f,l) %ld %d %s %d\n",
                                CRYPTO_thread_id(),
                                a_mode,a_file,a_line);
#endif

        if (a_mode & CRYPTO_LOCK)
        {
                pthread_mutex_lock(&(g_lock_cs[a_type]));
        } else
        {
                pthread_mutex_unlock(&(g_lock_cs[a_type]));

        }

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static unsigned long pthreads_thread_id(void)
{
        unsigned long ret;

        ret=(unsigned long)pthread_self();
        return(ret);

}

//: ----------------------------------------------------------------------------
//:
//: ----------------------------------------------------------------------------
struct CRYPTO_dynlock_value
{
        pthread_mutex_t mutex;
};

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static struct CRYPTO_dynlock_value* dyn_create_function(const char* a_file, int a_line)
{
        struct CRYPTO_dynlock_value* value = new CRYPTO_dynlock_value;
        if (!value) return NULL;

        pthread_mutex_init(&value->mutex, NULL);
        return value;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static void dyn_lock_function(int a_mode,
                              struct CRYPTO_dynlock_value* a_l,
                              const char* a_file,
                              int a_line)
{
        if (a_mode & CRYPTO_LOCK)
        {
                pthread_mutex_lock(&a_l->mutex);
        }
        else
        {
                pthread_mutex_unlock(&a_l->mutex);
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static void dyn_destroy_function(struct CRYPTO_dynlock_value* a_l,
                                 const char* a_file,
                                 int a_line)
{
        if(a_l)
        {
                pthread_mutex_destroy(&a_l->mutex);
                free(a_l);
        }
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void ssl_kill_locks(void)
{
        CRYPTO_set_id_callback(NULL);
        CRYPTO_set_locking_callback(NULL);
        if(g_lock_cs)
        {
                for (int i=0; i<CRYPTO_num_locks(); ++i)
                {
                        pthread_mutex_destroy(&(g_lock_cs[i]));
                }
        }

        OPENSSL_free(g_lock_cs);
        g_lock_cs = NULL;
}

//: ----------------------------------------------------------------------------
//: \details: OpenSSL can safely be used in multi-threaded applications provided
//:           that at least two callback functions are set, locking_function and
//:           threadid_func this function sets those two callbacks.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
static void init_ssl_locking(void)
{
        int l_num_locks = CRYPTO_num_locks();
        g_lock_cs = (pthread_mutex_t *)OPENSSL_malloc(l_num_locks * sizeof(pthread_mutex_t));
        //g_lock_cs =(pthread_mutex_t*)malloc(        l_num_locks * sizeof(pthread_mutex_t));

        for (int i=0; i<l_num_locks; ++i)
        {
                pthread_mutex_init(&(g_lock_cs[i]),NULL);
        }

        CRYPTO_set_id_callback(pthreads_thread_id);
        CRYPTO_set_locking_callback(pthreads_locking_callback);
        CRYPTO_set_dynlock_create_callback(dyn_create_function);
        CRYPTO_set_dynlock_lock_callback(dyn_lock_function);
        CRYPTO_set_dynlock_destroy_callback(dyn_destroy_function);

}

//: ----------------------------------------------------------------------------
//: \details: Initialize OpenSSL
//: \return:  ctx on success, NULL on failure
//: \param:   TODO
//: ----------------------------------------------------------------------------
SSL_CTX* ssl_init(const std::string &a_cipher_list)
{
        SSL_CTX *server_ctx;

        // Initialize the OpenSSL library
        SSL_library_init();

        // Bring in and register error messages
        ERR_load_crypto_strings();
        SSL_load_error_strings();

        // TODO Deprecated???
        //SSLeay_add_ssl_algorithms();
        OpenSSL_add_all_algorithms();


        // Set up for thread safety
        init_ssl_locking();

        // We MUST have entropy, or else there's no point to crypto.
        if (!RAND_poll())
        {
                return NULL;
        }

        // TODO Old method???
#if 0
        // Random seed
        if (! RAND_status())
        {
                unsigned char bytes[1024];
                for (size_t i = 0; i < sizeof(bytes); ++i)
                        bytes[i] = random() % 0xff;
                RAND_seed(bytes, sizeof(bytes));
        }
#endif

        server_ctx = SSL_CTX_new(SSLv23_client_method()); /* Create new context */
        if (server_ctx == NULL)
        {
                ERR_print_errors_fp(stderr);
                NDBG_PRINT("SSL_CTX_new Error: %s\n", ERR_error_string(ERR_get_error(), NULL));
                return NULL;
        }

        if (a_cipher_list.size() > 0)
        {
                if (! SSL_CTX_set_cipher_list(server_ctx, a_cipher_list.c_str()))
                {
                        NDBG_PRINT("Error cannot set m_cipher list\n");
                        ERR_print_errors_fp(stderr);
                        //close_connection(con, nowP);
                        return NULL;
                }
        }


#if 0
        if (!SSL_CTX_load_verify_locations(ctx,
                        DEFAULT_PEM_FILE,
                        NULL))
        {
                fprintf(stderr, "Error loading trust store\n");
                ERR_print_errors_fp(stderr);
                SSL_CTX_free(ctx);
                ctx = NULL;
        }
#endif

        //NDBG_PRINT("SSL_CTX_new success\n");

        return server_ctx;

}

