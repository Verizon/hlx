//: ----------------------------------------------------------------------------
//: Copyright (C) 2014 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    main.cc
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
//: Includes
//: ----------------------------------------------------------------------------
#include "util.h"
#include "reqlet_repo.h"
#include "ndebug.h"
#include "resolver.h"
#include "t_client.h"
#include "reqlet.h"
#include "ssl_util.h"

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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h> // For getopt_long
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>


// Profiler
#define ENABLE_PROFILER 1
#ifdef ENABLE_PROFILER
#include <google/profiler.h>
#endif

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define NB_ENABLE  1
#define NB_DISABLE 0

#define MAX_READLINE_SIZE 1024


// Version
#define HLE_VERSION_MAJOR 0
#define HLE_VERSION_MINOR 0
#define HLE_VERSION_MACRO 1
#define HLE_VERSION_PATCH "alpha"

#define HLE_DEFAULT_CONN_TIMEOUT_S 10

//: ----------------------------------------------------------------------------
//: Macros
//: ----------------------------------------------------------------------------
#define SET_HEADER(_key, _val) l_settings.m_header_map[_key] =_val

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
typedef std::list <std::string> host_list_t;
typedef std::list <t_client *> t_client_list_t;
typedef std::map <std::string, std::string> header_map_t;

//: ----------------------------------------------------------------------------
//: Settings
//: ----------------------------------------------------------------------------
typedef struct settings_struct
{
        bool m_verbose;
        bool m_color;
        bool m_quiet;
        bool m_show_stats;

        // request options
        std::string m_url;
        header_map_t m_header_map;

        // run options
        t_client_list_t m_t_client_list;
        evr_loop_type_t m_evr_loop_type;
        int32_t m_start_parallel;
        uint32_t m_num_threads;
        uint32_t m_timeout_s;

        // tcp options
        uint32_t m_sock_opt_recv_buf_size;
        uint32_t m_sock_opt_send_buf_size;
        bool m_sock_opt_no_delay;

        // SSL options
        SSL_CTX* m_ssl_ctx;
        std::string m_cipher_list_str;

        // ---------------------------------
        // Defaults...
        // ---------------------------------
        settings_struct(void) :
                m_verbose(false),
                m_color(false),
                m_quiet(false),
                m_show_stats(false),
                m_url(),
                m_header_map(),
                m_t_client_list(),
                m_evr_loop_type(EVR_LOOP_EPOLL),
                m_start_parallel(100),
                m_num_threads(4),
                m_timeout_s(HLE_DEFAULT_CONN_TIMEOUT_S),
                m_sock_opt_recv_buf_size(0),
                m_sock_opt_send_buf_size(0),
                m_sock_opt_no_delay(false),
                m_ssl_ctx(NULL),
                m_cipher_list_str("")
        {}

private:
        DISALLOW_COPY_AND_ASSIGN(settings_struct);

} settings_struct_t;

//: ----------------------------------------------------------------------------
//: Forward Decls
//: ----------------------------------------------------------------------------
struct ssl_ctx_st;
typedef ssl_ctx_st SSL_CTX;

//: ----------------------------------------------------------------------------
//: Prototypes
//: ----------------------------------------------------------------------------
void command_exec(settings_struct_t &a_settings);
int32_t add_line(FILE *a_file_ptr, host_list_t &a_host_list);
int32_t set_header(const std::string &a_header_key, const std::string &a_header_val);

int32_t run(settings_struct_t &a_settings, host_list_t &a_host_list);
bool is_running(settings_struct_t &a_settings);
int32_t stop(settings_struct_t &a_settings);
int32_t wait_till_stopped(settings_struct_t &a_settings);

//: ----------------------------------------------------------------------------
//: \details: Signal handler
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool g_test_finished = false;
bool g_cancelled = false;
settings_struct *g_settings = NULL;

void sig_handler(int signo)
{
        if (signo == SIGINT)
        {
                // Kill program
                //NDBG_PRINT("SIGINT\n");
                g_test_finished = true;
                g_cancelled = true;
                stop(*g_settings);
        }
}

//: ----------------------------------------------------------------------------
//: \details: Print the version.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void print_version(FILE* a_stream, int a_exit_code)
{

        // print out the version information
        fprintf(a_stream, "hle HTTP Load Runner.\n");
        fprintf(a_stream, "Copyright (C) 2014 Edgecast Networks.\n");
        fprintf(a_stream, "               Version: %d.%d.%d.%s\n",
                        HLE_VERSION_MAJOR,
                        HLE_VERSION_MINOR,
                        HLE_VERSION_MACRO,
                        HLE_VERSION_PATCH);
        exit(a_exit_code);

}


//: ----------------------------------------------------------------------------
//: \details: Print the command line help.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void print_usage(FILE* a_stream, int a_exit_code)
{
        fprintf(a_stream, "Usage: hle [http[s]://]hostname[:port]/path [options]\n");
        fprintf(a_stream, "Options are:\n");
        fprintf(a_stream, "  -h, --help           Display this help and exit.\n");
        fprintf(a_stream, "  -v, --version        Display the version number and exit.\n");
        fprintf(a_stream, "  \n");

        fprintf(a_stream, "URL Options -or without parameter\n");
        fprintf(a_stream, "  -u, --url            URL -REQUIRED.\n");
        fprintf(a_stream, "  \n");

        fprintf(a_stream, "Hostname Input Options -also STDIN:\n");
        fprintf(a_stream, "  -f, --host_file      Host name file.\n");
        fprintf(a_stream, "  -x, --execute        Script to execute to get host names.\n");
        fprintf(a_stream, "  \n");

        fprintf(a_stream, "Settings:\n");
        fprintf(a_stream, "  -p, --parallel       Num parallel.\n");
        fprintf(a_stream, "  -t, --threads        Number of parallel threads.\n");
        fprintf(a_stream, "  -H, --header         Request headers -can add multiple ie -H<> -H<>...\n");
        fprintf(a_stream, "  -T, --timeout        Timeout (seconds).\n");
        fprintf(a_stream, "  -R, --recv_buffer    Socket receive buffer size.\n");
        fprintf(a_stream, "  -S, --send_buffer    Socket send buffer size.\n");
        fprintf(a_stream, "  -D, --no_delay       Socket TCP no-delay.\n");
        fprintf(a_stream, "  -A, --ai_cache       Path to Address Info Cache (DNS lookup cache).\n");
        fprintf(a_stream, "  \n");

        fprintf(a_stream, "SSL Settings:\n");
        fprintf(a_stream, "  -y, --cipher         Cipher --see \"openssl ciphers\" for list.\n");
        fprintf(a_stream, "  \n");

        fprintf(a_stream, "Print Options:\n");
        fprintf(a_stream, "  -r, --verbose        Verbose logging\n");
        fprintf(a_stream, "  -c, --color          Color\n");
        fprintf(a_stream, "  -q, --quiet          Suppress output\n");
        fprintf(a_stream, "  -s, --show_progress  Show progress\n");
        fprintf(a_stream, "  \n");

        fprintf(a_stream, "Output Options: -defaults to line delimited\n");
        fprintf(a_stream, "  -o, --output         File to write output to. Defaults to stdout\n");
        fprintf(a_stream, "  -l, --line_delimited Output <HOST> <RESPONSE BODY> per line\n");
        fprintf(a_stream, "  -j, --json           JSON { <HOST>: \"body\": <RESPONSE> ...\n");
        fprintf(a_stream, "  -P, --pretty         Pretty output\n");
        fprintf(a_stream, "  \n");

        fprintf(a_stream, "Debug Options:\n");
        fprintf(a_stream, "  -G, --gprofile       Google profiler output file\n");

        fprintf(a_stream, "  \n");
        fprintf(a_stream, "Note: If running large jobs consider enabling tcp_tw_reuse -eg:\n");
        fprintf(a_stream, "echo 1 > /proc/sys/net/ipv4/tcp_tw_reuse\n");

        fprintf(a_stream, "\n");

        exit(a_exit_code);

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int main(int argc, char** argv)
{

        settings_struct_t l_settings;

        // For sighandler
        g_settings = &l_settings;

        // -------------------------------------------
        // Setup default headers before the user
        // -------------------------------------------
        SET_HEADER("User-Agent", "EdgeCast Parallel Curl hle ");
        //SET_HEADER("User-Agent", "ONGA_BONGA (╯°□°）╯︵ ┻━┻)");
        //SET_HEADER("User-Agent", "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/33.0.1750.117 Safari/537.36");
        //SET_HEADER("x-select-backend", "self");
        SET_HEADER("Accept", "*/*");
        //SET_HEADER("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8");
        //SET_HEADER("Accept-Encoding", "gzip,deflate");
        //SET_HEADER("Connection", "keep-alive");

        // -------------------------------------------
        // Get args...
        // -------------------------------------------
        char l_opt;
        std::string l_argument;
        int l_option_index = 0;
        struct option l_long_options[] =
                {
                { "help",           0, 0, 'h' },
                { "version",        0, 0, 'v' },
                { "url",            1, 0, 'u' },
                { "host_file",      1, 0, 'f' },
                { "execute",        1, 0, 'x' },
                { "cipher",         1, 0, 'y' },
                { "parallel",       1, 0, 'p' },
                { "threads",        1, 0, 't' },
                { "header",         1, 0, 'H' },
                { "timeout",        1, 0, 'T' },
                { "recv_buffer",    1, 0, 'R' },
                { "send_buffer",    1, 0, 'S' },
                { "no_delay",       1, 0, 'D' },
                { "ai_cache",       1, 0, 'A' },
                { "verbose",        0, 0, 'r' },
                { "color",          0, 0, 'c' },
                { "quiet",          0, 0, 'q' },
                { "show_progress",  0, 0, 's' },
                { "output",         1, 0, 'o' },
                { "line_delimited", 0, 0, 'l' },
                { "json",           0, 0, 'j' },
                { "pretty",         0, 0, 'P' },
                { "gprofile",       1, 0, 'G' },

                // list sentinel
                { 0, 0, 0, 0 }
        };

        std::string l_gprof_file;
        std::string l_execute_line;
        std::string l_host_file_str;
        std::string l_url;
        std::string l_ai_cache;
        std::string l_output_file = "";

        // Defaults
        reqlet_repo::output_type_t l_output_mode = reqlet_repo::OUTPUT_JSON;
        //bool l_output_part_user_specd = false;
        int l_output_part =   reqlet_repo::PART_HOST
                            | reqlet_repo::PART_STATUS_CODE
                            | reqlet_repo::PART_HEADERS
                            //| reqlet_repo::PART_BODY
                            ;
        bool l_output_pretty = false;

        // -------------------------------------------
        // Assume unspecified arg url...
        // TODO Unsure if good way to allow unspecified
        // arg...
        // -------------------------------------------
        bool is_opt = false;
        for(int i_arg = 1; i_arg < argc; ++i_arg) {

                if(argv[i_arg][0] == '-') {
                        // next arg is for the option
                        is_opt = true;
                }
                else if(argv[i_arg][0] != '-' && is_opt == false) {
                        // Stuff in url field...
                        l_url = std::string(argv[i_arg]);
                        //if(l_settings.m_verbose)
                        //{
                        //      NDBG_PRINT("Found unspecified argument: %s --assuming url...\n", l_url.c_str());
                        //}
                        break;
                } else {
                        // reset option flag
                        is_opt = false;
                }

        }

        // -------------------------------------------------
        // Args...
        // -------------------------------------------------
        char l_short_arg_list[] = "hvu:f:x:y:p:t:H:T:R:S:DA:rcqso:ljPG:";
        while ((l_opt = getopt_long_only(argc, argv, l_short_arg_list, l_long_options, &l_option_index)) != -1)
        {

                if (optarg)
                        l_argument = std::string(optarg);
                else
                        l_argument.clear();
                //NDBG_PRINT("arg[%c=%d]: %s\n", l_opt, l_option_index, l_argument.c_str());

                switch (l_opt)
                {

                // ---------------------------------------
                // Help
                // ---------------------------------------
                case 'h':
                {
                        print_usage(stdout, 0);
                        break;
                }

                // ---------------------------------------
                // Version
                // ---------------------------------------
                case 'v':
                {
                        print_version(stdout, 0);
                        break;
                }

                // ---------------------------------------
                // URL
                // ---------------------------------------
                case 'u':
                {
                        l_url = l_argument;
                        break;
                }

                // ---------------------------------------
                // Host file
                // ---------------------------------------
                case 'f':
                {
                        l_host_file_str = l_argument;
                        break;
                }

                // ---------------------------------------
                // Execute line
                // ---------------------------------------
                case 'x':
                {
                        l_execute_line = l_argument;
                        break;
                }

                // ---------------------------------------
                // cipher
                // ---------------------------------------
                case 'y':
                {
                        std::string l_cipher_str = l_argument;
                        if (strcasecmp(l_cipher_str.c_str(), "fastsec") == 0)
                                l_cipher_str = "RC4-MD5";
                        else if (strcasecmp(l_cipher_str.c_str(), "highsec") == 0)
                                l_cipher_str = "DES-CBC3-SHA";
                        else if (strcasecmp(l_cipher_str.c_str(), "paranoid") == 0)
                                l_cipher_str = "AES256-SHA";

                        l_settings.m_cipher_list_str = l_cipher_str;
                        break;
                }

                // ---------------------------------------
                // parallel
                // ---------------------------------------
                case 'p':
                {
                        int l_start_parallel = 1;
                        //NDBG_PRINT("arg: --parallel: %s\n", optarg);
                        //l_settings.m_start_type = START_PARALLEL;
                        l_start_parallel = atoi(optarg);
                        if (l_start_parallel < 1)
                        {
                                printf("parallel must be at least 1\n");
                                print_usage(stdout, -1);
                        }
                        l_settings.m_start_parallel = l_start_parallel;
                        break;
                }

                // ---------------------------------------
                // num threads
                // ---------------------------------------
                case 't':
                {
                        int l_max_threads = 1;
                        //NDBG_PRINT("arg: --threads: %s\n", l_argument.c_str());
                        l_max_threads = atoi(optarg);
                        if (l_max_threads < 1)
                        {
                                printf("num-threads must be at least 1\n");
                                print_usage(stdout, -1);
                        }
                        l_settings.m_num_threads = l_max_threads;
                        break;
                }

                // ---------------------------------------
                // Header
                // ---------------------------------------
                case 'H':
                {
                        int32_t l_status;
                        std::string l_header_key;
                        std::string l_header_val;
                        l_status = break_header_string(l_argument, l_header_key, l_header_val);
                        if(l_status != 0)
                        {
                                printf("Error header string[%s] is malformed\n", l_argument.c_str());
                                print_usage(stdout, -1);
                        }

                        // Add to reqlet_repo map
                        SET_HEADER(l_header_key, l_header_val);
                        // TODO Check status???
                        break;
                }

                // ---------------------------------------
                // Timeout
                // ---------------------------------------
                case 'T':
                {
                        int l_timeout_s = -1;
                        //NDBG_PRINT("arg: --threads: %s\n", l_argument.c_str());
                        l_timeout_s = atoi(optarg);
                        if (l_timeout_s < 1)
                        {
                                printf("connection timeout must be at least 1\n");
                                print_usage(stdout, -1);
                        }
                        l_settings.m_timeout_s = l_timeout_s;
                        break;
                }

                // ---------------------------------------
                // sock_opt_recv_buf_size
                // ---------------------------------------
                case 'R':
                {
                        int l_sock_opt_recv_buf_size = atoi(optarg);
                        // TODO Check value...
                        l_settings.m_sock_opt_recv_buf_size = l_sock_opt_recv_buf_size;
                        break;
                }

                // ---------------------------------------
                // sock_opt_send_buf_size
                // ---------------------------------------
                case 'S':
                {
                        int l_sock_opt_send_buf_size = atoi(optarg);
                        // TODO Check value...
                        l_settings.m_sock_opt_send_buf_size = l_sock_opt_send_buf_size;
                        break;
                }

                // ---------------------------------------
                // No delay
                // ---------------------------------------
                case 'D':
                {
                        l_settings.m_sock_opt_no_delay = true;
                        break;
                }

                // ---------------------------------------
                // Address Info cache
                // ---------------------------------------
                case 'A':
                {
                        l_ai_cache = l_argument;
                        break;
                }

                // ---------------------------------------
                // verbose
                // ---------------------------------------
                case 'r':
                {
                        l_settings.m_verbose = true;
                        break;
                }

                // ---------------------------------------
                // color
                // ---------------------------------------
                case 'c':
                {
                        l_settings.m_color = true;
                        break;
                }

                // ---------------------------------------
                // quiet
                // ---------------------------------------
                case 'q':
                {
                        l_settings.m_quiet = true;
                        break;
                }

                // ---------------------------------------
                // show progress
                // ---------------------------------------
                case 's':
                {
                        l_settings.m_show_stats = true;
                        break;
                }

                // ---------------------------------------
                // output file
                // ---------------------------------------
                case 'o':
                {
                        l_output_file = l_argument;
                        break;
                }

                // ---------------------------------------
                // line delimited
                // ---------------------------------------
                case 'l':
                {
                        l_output_mode = reqlet_repo::OUTPUT_LINE_DELIMITED;
                        break;
                }

                // ---------------------------------------
                // json output
                // ---------------------------------------
                case 'j':
                {
                        l_output_mode = reqlet_repo::OUTPUT_JSON;
                        break;
                }

                // ---------------------------------------
                // pretty output
                // ---------------------------------------
                case 'P':
                {
                        l_output_pretty = true;
                        break;
                }

                // ---------------------------------------
                // Google Profiler Output File
                // ---------------------------------------
                case 'G':
                {
                        l_gprof_file = l_argument;
                        break;
                }

                // What???
                case '?':
                {
                        // Required argument was missing
                        // '?' is provided when the 3rd arg to getopt_long does not begin with a ':', and is preceeded
                        // by an automatic error message.
                        fprintf(stdout, "  Exiting.\n");
                        print_usage(stdout, -1);
                        break;
                }

                // Huh???
                default:
                {
                        fprintf(stdout, "Unrecognized option.\n");
                        print_usage(stdout, -1);
                        break;
                }
                }
        }

        // Check for required url argument
        if(l_url.empty())
        {
                fprintf(stdout, "No URL specified.\n");
                print_usage(stdout, -1);
        }
        // else set url
        l_settings.m_url = l_url;


        host_list_t l_host_list;
        // -------------------------------------------------
        // Host list processing
        // -------------------------------------------------
        // Read from command
        if(!l_execute_line.empty())
        {
                FILE *fp;
                int32_t l_status = STATUS_OK;

                fp = popen(l_execute_line.c_str(), "r");
                // Error executing...
                if (fp == NULL)
                {
                }

                l_status = add_line(fp, l_host_list);
                if(STATUS_OK != l_status)
                {
                        return STATUS_ERROR;
                }

                l_status = pclose(fp);
                // Error reported by pclose()
                if (l_status == -1)
                {
                        printf("Error: performing pclose\n");
                        return STATUS_ERROR;
                }
                // Use macros described under wait() to inspect `status' in order
                // to determine success/failure of command executed by popen()
                else
                {
                }
        }
        // Read from file
        else if(!l_host_file_str.empty())
        {
                FILE * l_file;
                int32_t l_status = STATUS_OK;

                l_file = fopen(l_host_file_str.c_str(),"r");
                if (NULL == l_file)
                {
                        printf("Error opening file: %s.  Reason: %s\n", l_host_file_str.c_str(), strerror(errno));
                        return STATUS_ERROR;
                }

                l_status = add_line(l_file, l_host_list);
                if(STATUS_OK != l_status)
                {
                        return STATUS_ERROR;
                }

                //NDBG_PRINT("ADD_FILE: DONE: %s\n", a_url_file.c_str());

                l_status = fclose(l_file);
                if (0 != l_status)
                {
                        NDBG_PRINT("Error performing fclose.  Reason: %s\n", strerror(errno));
                        return STATUS_ERROR;
                }
        }
        // Read from stdin
        else
        {
                int32_t l_status = STATUS_OK;
                l_status = add_line(stdin, l_host_list);
                if(STATUS_OK != l_status)
                {
                        return STATUS_ERROR;
                }
        }

        if(l_settings.m_verbose)
        {
                NDBG_PRINT("Showing hostname list:\n");
                //for(host_list_t::iterator i_host = l_host_list.begin(); i_host != l_host_list.end(); ++i_host)
                //{
                //        NDBG_OUTPUT("%s\n", i_host->c_str());
                //}
        }

        // -------------------------------------------
        // Sigint handler
        // -------------------------------------------
        if (signal(SIGINT, sig_handler) == SIG_ERR)
        {
                printf("Error: can't catch SIGINT\n");
                return -1;
        }
        // TODO???
        //signal(SIGPIPE, SIG_IGN);


        // -------------------------------------------
        // Init resolver with cache
        // -------------------------------------------
        int32_t l_ldb_init_status;
        l_ldb_init_status = resolver::get()->init(l_ai_cache, true);
        if(STATUS_OK != l_ldb_init_status)
        {
                return -1;
        }

        // -------------------------------------------
        // SSL init...
        // -------------------------------------------
        l_settings.m_ssl_ctx = ssl_init(l_settings.m_cipher_list_str);
        if(NULL == l_settings.m_ssl_ctx) {
                NDBG_PRINT("Error: performing ssl_init with cipher_list: %s\n", l_settings.m_cipher_list_str.c_str());
                return STATUS_ERROR;
        }

        // Start Profiler
        if (!l_gprof_file.empty())
        {
                ProfilerStart(l_gprof_file.c_str());
        }

        // Run
        int32_t l_run_status = 0;
        l_run_status = run(l_settings, l_host_list);
        if(0 != l_run_status)
        {
                printf("Error: performing hle::run");
                return -1;
        }

        //uint64_t l_start_time_ms = get_time_ms();

        // -------------------------------------------
        // Run command exec
        // -------------------------------------------
        // Copy in settings
        command_exec(l_settings);

        if(l_settings.m_verbose)
        {
                printf("Finished -joining all threads\n");
        }

        // Wait for completion
        wait_till_stopped(l_settings);

        // One more status for the lovers
        reqlet_repo::get()->display_status_line(l_settings.m_color);

        if (!l_gprof_file.empty())
        {
                ProfilerStop();
        }

        //uint64_t l_end_time_ms = get_time_ms() - l_start_time_ms;

        // -------------------------------------------
        // Results...
        // -------------------------------------------
        if(!g_cancelled && !l_settings.m_quiet)
        {
                bool l_use_color = l_settings.m_color;
                if(!l_output_file.empty()) l_use_color = false;
                std::string l_responses_str;
                l_responses_str = reqlet_repo::get()->dump_all_responses(l_use_color, l_output_pretty, l_output_mode, l_output_part);
                if(l_output_file.empty())
                {
                        NDBG_OUTPUT("%s\n", l_responses_str.c_str());
                }
                else
                {
                        int32_t l_num_bytes_written = 0;
                        int32_t l_status = 0;
                        // Open
                        FILE *l_file_ptr = fopen(l_output_file.c_str(), "w+");
                        if(l_file_ptr == NULL)
                        {
                                NDBG_PRINT("Error performing fopen. Reason: %s\n", strerror(errno));
                                return STATUS_ERROR;
                        }

                        // Write
                        l_num_bytes_written = fwrite(l_responses_str.c_str(), 1, l_responses_str.length(), l_file_ptr);
                        if(l_num_bytes_written != (int32_t)l_responses_str.length())
                        {
                                NDBG_PRINT("Error performing fwrite. Reason: %s\n", strerror(errno));
                                fclose(l_file_ptr);
                                return STATUS_ERROR;
                        }

                        // Close
                        l_status = fclose(l_file_ptr);
                        if(l_status != 0)
                        {
                                NDBG_PRINT("Error performing fclose. Reason: %s\n", strerror(errno));
                                return STATUS_ERROR;
                        }

                }

        }

        // -------------------------------------------
        // Cleanup...
        // -------------------------------------------
        // -------------------------------------------
        // Delete t_client list...
        // -------------------------------------------
        for(t_client_list_t::iterator i_client_hle = l_settings.m_t_client_list.begin();
                        i_client_hle != l_settings.m_t_client_list.end(); )
        {
                t_client *l_t_client_ptr = *i_client_hle;
                delete l_t_client_ptr;
                l_settings.m_t_client_list.erase(i_client_hle++);
        }

        // SSL Cleanup
        ssl_kill_locks();

        // TODO Deprecated???
        //EVP_cleanup();

        //if(l_settings.m_verbose)
        //{
        //      NDBG_PRINT("Cleanup\n");
        //}

        return 0;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int kbhit()
{
        struct timeval tv;
        fd_set fds;
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        //STDIN_FILENO is 0
        select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
        return FD_ISSET(STDIN_FILENO, &fds);
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void nonblock(int state)
{
        struct termios ttystate;

        //get the terminal state
        tcgetattr(STDIN_FILENO, &ttystate);

        if (state == NB_ENABLE)
        {
                //turn off canonical mode
                ttystate.c_lflag &= ~ICANON;
                //minimum of number input read.
                ttystate.c_cc[VMIN] = 1;
        } else if (state == NB_DISABLE)
        {
                //turn on canonical mode
                ttystate.c_lflag |= ICANON;
        }
        //set the terminal attributes.
        tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void command_exec(settings_struct_t &a_settings)
{
        int i = 0;
        char l_cmd = ' ';
        bool l_sent_stop = false;
        //bool l_first_time = true;

        nonblock(NB_ENABLE);

        reqlet_repo *l_reqlet_repo = reqlet_repo::get();

        //: ------------------------------------
        //:   Loop forever until user quits
        //: ------------------------------------
        while (!g_test_finished)
        {
                i = kbhit();
                if (i != 0)
                {

                        l_cmd = fgetc(stdin);

                        switch (l_cmd)
                        {

                        // Quit -only works when not reading from stdin
                        case 'q':
                        {
                                g_test_finished = true;
                                g_cancelled = true;
                                stop(a_settings);
                                l_sent_stop = true;
                                break;
                        }

                        // Default
                        default:
                        {
                                break;
                        }
                        }
                }

                // TODO add define...
                usleep(200000);

                if(a_settings.m_show_stats)
                {
                        l_reqlet_repo->display_status_line(a_settings.m_color);
                }

                if (!is_running(a_settings))
                {
                        //NDBG_PRINT("IS NOT RUNNING.\n");
                        g_test_finished = true;
                }

        }

        // Send stop -if unsent
        if(!l_sent_stop)
        {
                stop(a_settings);
                l_sent_stop = true;
        }

        nonblock(NB_DISABLE);

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t add_line(FILE *a_file_ptr, host_list_t &a_host_list)
{

        char l_readline[MAX_READLINE_SIZE];
        while(fgets(l_readline, sizeof(l_readline), a_file_ptr))
        {
                size_t l_readline_len = strnlen(l_readline, MAX_READLINE_SIZE);
                if(MAX_READLINE_SIZE == l_readline_len)
                {
                        // line was truncated
                        // Bail out -reject lines longer than limit
                        // -host names ought not be too long
                        printf("Error: hostnames must be shorter than %d chars\n", MAX_READLINE_SIZE);
                        return STATUS_ERROR;
                }
                // read full line
                // Nuke endline
                l_readline[l_readline_len - 1] = '\0';
                std::string l_string(l_readline);
                l_string.erase( std::remove_if( l_string.begin(), l_string.end(), ::isspace ), l_string.end() );
                if(!l_string.empty())
                        a_host_list.push_back(l_string);
                //NDBG_PRINT("READLINE: %s\n", l_readline);
        }

        return STATUS_OK;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t run(settings_struct_t &a_settings, host_list_t &a_host_list)
{
        int32_t l_retval = STATUS_OK;
        reqlet_repo *l_reqlet_repo = NULL;

        // Create the reqlet list
        l_reqlet_repo = reqlet_repo::get();
        uint32_t l_reqlet_num = 0;
        for(host_list_t::iterator i_host = a_host_list.begin();
                        i_host != a_host_list.end();
                        ++i_host, ++l_reqlet_num)
        {
                // Create a re
                reqlet *l_reqlet = new reqlet(l_reqlet_num, 1);
                l_reqlet->init_with_url(a_settings.m_url);

                // Get host and port if exist
                parsed_url l_url;
                l_url.parse(*i_host);

                if(strchr(i_host->c_str(), (int)':'))
                {
                        l_reqlet->set_host(l_url.m_host);
                        l_reqlet->set_port(l_url.m_port);
                }
                else
                {
                        l_reqlet->set_host(*i_host);
                }

                // Add to list
                l_reqlet_repo->add_reqlet(l_reqlet);

        }

        // -------------------------------------------
        // Create t_client list...
        // -------------------------------------------
        for(uint32_t i_client_idx = 0; i_client_idx < a_settings.m_num_threads; ++i_client_idx)
        {

                if(a_settings.m_verbose)
                {
                        NDBG_PRINT("Creating...\n");
                }

                // Construct with settings...
                t_client *l_t_client = new t_client(
                                a_settings.m_verbose,
                                a_settings.m_color,
                                a_settings.m_sock_opt_recv_buf_size,
                                a_settings.m_sock_opt_send_buf_size,
                                a_settings.m_sock_opt_no_delay,
                                a_settings.m_timeout_s,
                                a_settings.m_cipher_list_str,
                                a_settings.m_ssl_ctx,
                                a_settings.m_evr_loop_type,
                                a_settings.m_start_parallel
                );

                for(header_map_t::iterator i_header = a_settings.m_header_map.begin();
                    i_header != a_settings.m_header_map.end();
                    ++i_header)
                {
                        l_t_client->set_header(i_header->first, i_header->second);
                }

                a_settings.m_t_client_list.push_back(l_t_client);
        }

        // -------------------------------------------
        // Run...
        // -------------------------------------------
        for(t_client_list_t::iterator i_t_client = a_settings.m_t_client_list.begin();
                        i_t_client != a_settings.m_t_client_list.end();
                        ++i_t_client)
        {
                if(a_settings.m_verbose)
                {
                        NDBG_PRINT("Running...\n");
                }
                (*i_t_client)->run();
        }

        return l_retval;

}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
int32_t stop(settings_struct_t &a_settings)
{
        int32_t l_retval = STATUS_OK;

        for (t_client_list_t::iterator i_t_client = a_settings.m_t_client_list.begin();
                        i_t_client != a_settings.m_t_client_list.end();
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
int32_t wait_till_stopped(settings_struct_t &a_settings)
{
        int32_t l_retval = STATUS_OK;

        // -------------------------------------------
        // Join all threads before exit
        // -------------------------------------------
        for(t_client_list_t::iterator i_client = a_settings.m_t_client_list.begin();
            i_client != a_settings.m_t_client_list.end();
            ++i_client)
        {

                //if(m_verbose)
                //{
                //      NDBG_PRINT("joining...\n");
                //}
                pthread_join(((*i_client)->m_t_run_thread), NULL);

        }
        return l_retval;
}

//: ----------------------------------------------------------------------------
//: \details: TODO
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool is_running(settings_struct_t &a_settings)
{
        for (t_client_list_t::iterator i_client_hle = a_settings.m_t_client_list.begin();
             i_client_hle != a_settings.m_t_client_list.end();
             ++i_client_hle)
        {
                if((*i_client_hle)->is_running())
                        return true;
        }

        return false;
}
