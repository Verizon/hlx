//: ----------------------------------------------------------------------------
//: Copyright (C) 2015 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    host_info.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    11/20/2015
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
#ifndef _HOST_INFO_H
#define _HOST_INFO_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "ai_cache.h"
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: \details: Host info
//: ----------------------------------------------------------------------------
struct host_info {
        struct sockaddr_storage m_sa;
        int m_sa_len;
        int m_sock_family;
        int m_sock_type;
        int m_sock_protocol;
        uint64_t m_expires_s;

        host_info():
                m_sa(),
                m_sa_len(16),
                m_sock_family(AF_INET),
                m_sock_type(SOCK_STREAM),
                m_sock_protocol(IPPROTO_TCP),
                m_expires_s(0)
        {
                ((struct sockaddr_in *)(&m_sa))->sin_family = AF_INET;
        };

        void show(void)
        {
                printf("+-----------+\n");
                printf("| Host Info |\n");
                printf("+-----------+-------------------------\n");
                printf(": m_sock_family:   %d\n",  m_sock_family);
                printf(": m_sock_type:     %d\n",  m_sock_type);
                printf(": m_sock_protocol: %d\n",  m_sock_protocol);
                printf(": m_sa_len:        %d\n",  m_sa_len);
                printf(": m_expires:       %lu\n", m_expires_s);
                printf("+-------------------------------------\n");
        };
};

} //namespace ns_hlx {

#endif