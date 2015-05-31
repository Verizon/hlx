//: ----------------------------------------------------------------------------
//: Copyright (C) 2015 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    main.cc
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

// getrlimit
#include <sys/time.h>
#include <sys/resource.h>

// Shared pointer
//#include <tr1/memory>

// signal
#include <signal.h>

#include <list>

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
#include <string.h>

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

// Version
#define HSS_VERSION_MAJOR 0
#define HSS_VERSION_MINOR 0
#define HSS_VERSION_MACRO 1
#define HSS_VERSION_PATCH "alpha"

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Settings
//: ----------------------------------------------------------------------------
typedef struct settings_struct
{
        bool m_verbose;
        bool m_color;
        bool m_show_stats;
        ns_hlx::hlx_server *m_hlx_server;

        // ---------------------------------
        // Defaults...
        // ---------------------------------
        settings_struct() :
                m_verbose(false),
                m_color(false),
                m_show_stats(false),
                m_hlx_server(NULL)
        {}
private:
        HLX_SERVER_DISALLOW_COPY_AND_ASSIGN(settings_struct);

} settings_struct_t;

//: ----------------------------------------------------------------------------
//: \details: Signal handler
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
bool g_test_finished = false;
bool g_cancelled = false;
settings_struct_t *g_settings = NULL;
void sig_handler(int signo)
{
        if (signo == SIGINT)
        {
                // Kill program
                g_test_finished = true;
                g_cancelled = true;
                g_settings->m_hlx_server->stop();
        }
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
                                a_settings.m_hlx_server->stop();
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

                //if(a_thread_args.m_settings.m_show_stats)
                //{
                //        l_reqlet_repo->display_status_line(a_thread_args.m_settings.m_color);
                //}

                if (!a_settings.m_hlx_server->is_running())
                {
                        g_test_finished = true;
                }
        }

        // Send stop -if unsent
        if(!l_sent_stop)
        {
                a_settings.m_hlx_server->stop();
                l_sent_stop = true;
        }

        nonblock(NB_DISABLE);

}

//: ----------------------------------------------------------------------------
//: \details: Print the version.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void print_version(FILE* a_stream, int a_exit_code)
{

        // print out the version information
        fprintf(a_stream, "hss HLO Simpler Server.\n");
        fprintf(a_stream, "Copyright (C) 2015 Edgecast Networks.\n");
        fprintf(a_stream, "               Version: %d.%d.%d.%s\n",
                        HSS_VERSION_MAJOR,
                        HSS_VERSION_MINOR,
                        HSS_VERSION_MACRO,
                        HSS_VERSION_PATCH);
        exit(a_exit_code);

}


//: ----------------------------------------------------------------------------
//: \details: Print the command line help.
//: \return:  TODO
//: \param:   TODO
//: ----------------------------------------------------------------------------
void print_usage(FILE* a_stream, int a_exit_code)
{
        fprintf(a_stream, "Usage: hss [options]\n");
        fprintf(a_stream, "Options are:\n");
        fprintf(a_stream, "  -h, --help           Display this help and exit.\n");
        fprintf(a_stream, "  -v, --version        Display the version number and exit.\n");
        fprintf(a_stream, "  \n");

        fprintf(a_stream, "Server Options:\n");
        fprintf(a_stream, "  -p, --port           Server port -defaults to 23456.\n");
        fprintf(a_stream, "  -t, --threads        Number of server threads.\n");
        fprintf(a_stream, "  \n");

        fprintf(a_stream, "Print Options:\n");
        fprintf(a_stream, "  -r, --verbose        Verbose logging\n");
        fprintf(a_stream, "  -c, --color          Color\n");
        fprintf(a_stream, "  -s, --status         Status -show unhandled modsecurity info\n");
        fprintf(a_stream, "  \n");

        fprintf(a_stream, "Debug Options:\n");
        fprintf(a_stream, "  -G, --gprofile       Google profiler output file\n");

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
        ns_hlx::hlx_server *l_hlx_server = new ns_hlx::hlx_server();
        l_settings.m_hlx_server = l_hlx_server;

        // For sighandler
        g_settings = &l_settings;

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
                { "port",           1, 0, 'p' },
                { "threads",        1, 0, 't' },
                { "verbose",        0, 0, 'r' },
                { "color",          0, 0, 'c' },
                { "status",         0, 0, 's' },
                { "gprofile",       1, 0, 'G' },

                // list sentinel
                { 0, 0, 0, 0 }
        };

        std::string l_gprof_file;
        bool l_show_status = false;
        uint16_t l_server_port = 23456;

        // -------------------------------------------------
        // Args...
        // -------------------------------------------------
        char l_short_arg_list[] = "hvp:t:rcsG:";
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
                // Server port
                // ---------------------------------------
                case 'p':
                {
                        l_server_port = (uint16_t)strtoul(l_argument.c_str(), NULL, 10);
                        l_hlx_server->set_port(l_server_port);
                        break;
                }
                // ---------------------------------------
                // num threads
                // ---------------------------------------
                case 't':
                {
                        int l_num_threads = 1;
                        //NDBG_PRINT("arg: --threads: %s\n", l_argument.c_str());
                        l_num_threads = atoi(optarg);
                        if (l_num_threads < 1)
                        {
                                printf("num-threads must be at least 1\n");
                                print_usage(stdout, -1);
                        }
                        l_hlx_server->set_num_threads(l_num_threads);
                        break;
                }
                // ---------------------------------------
                // verbose
                // ---------------------------------------
                case 'r':
                {
                        l_settings.m_verbose = true;
                        l_hlx_server->set_verbose(true);
                        break;
                }

                // ---------------------------------------
                // color
                // ---------------------------------------
                case 'c':
                {
                        l_settings.m_color = true;
                        l_hlx_server->set_color(true);
                        break;
                }

                // ---------------------------------------
                // status
                // ---------------------------------------
                case 's':
                {
                        l_settings.m_show_stats = true;
                        l_show_status = true;
                        l_hlx_server->set_stats(true);
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

        // -------------------------------------------
        // Check for inputs
        // -------------------------------------------
        // ...

        // Start Profiler
        if (!l_gprof_file.empty())
        {
                ProfilerStart(l_gprof_file.c_str());
        }

        //uint64_t l_start_time_ms = get_time_ms();

        // Wait for completion
        if (!l_gprof_file.empty())
        {
                ProfilerStop();
        }

        //uint64_t l_end_time_ms = get_time_ms() - l_start_time_ms;

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
        // Run Server
        // -------------------------------------------
        // Run
        int32_t l_run_status = 0;
        l_run_status = l_hlx_server->run();
        if(0 != l_run_status)
        {
                printf("Error: performing hlx_server::run");
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
        l_hlx_server->wait_till_stopped();

        // -------------------------------------------
        // Status???
        // -------------------------------------------
        if(l_show_status)
        {
                // TODO
        }

        // -------------------------------------------
        // Cleanup...
        // -------------------------------------------
        // ...

        return 0;

}

