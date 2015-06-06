//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    resolver.h
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
#ifndef _RESOLVER_H
#define _RESOLVER_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "ndebug.h"
#include "host_info.h"

#include <pthread.h>

#include <string>
#include <map>

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define RESOLVER_DEFAULT_AI_CACHE_FILE "/tmp/addr_info_cache.json"


//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
namespace ns_hlx {

// TODO Create struct with TTL for storing ai cache
typedef std::map <std::string, std::string> ai_cache_map_t;

//: ----------------------------------------------------------------------------
//: Fwd Decl's
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Enums
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: \details: TODO
//: ----------------------------------------------------------------------------
class resolver
{
public:
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        int32_t init(std::string addr_info_cache_file = "", bool a_use_cache = false);

        resolver();
        ~resolver();

        // Settings...
        void set_verbose(bool a_val) { m_verbose = a_val;}
        void set_color(bool a_val) { m_color = a_val;}
        void set_timeout_s(int32_t a_val) {m_timeout_s = a_val;}
        int32_t cached_resolve(std::string &a_host,
                               uint16_t a_port,
                               host_info_t &a_host_info,
                               std::string &ao_error);
        int32_t sync_ai_cache(void);
        int32_t read_ai_cache(const std::string &a_ai_cache_file);

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------

private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        DISALLOW_COPY_AND_ASSIGN(resolver)

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        bool m_is_initd;

        // -------------------------------------------------
        // Settings
        // -------------------------------------------------
        bool m_verbose;
        bool m_color;
        uint32_t m_timeout_s;
        uint32_t m_use_cache;

        pthread_mutex_t m_cache_mutex;
        ai_cache_map_t m_ai_cache_map;
        std::string m_ai_cache_file;

};

} // ns_hlx

#endif
