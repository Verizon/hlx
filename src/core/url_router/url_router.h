//: ----------------------------------------------------------------------------
//: \file:    url_router.h
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    08/08/2015
//: ----------------------------------------------------------------------------
#ifndef _URL_ROUTER_H
#define _URL_ROUTER_H

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include <string>
#include <map>
#include <stdint.h>
#include <stddef.h>
#include <list>
#include <stack>

namespace ns_hlx {

//: ----------------------------------------------------------------------------
//: Types
//: ----------------------------------------------------------------------------
#ifndef URL_PARAM_MAP_T
#define URL_PARAM_MAP_T
typedef std::map <std::string, std::string> url_pmap_t;
#endif

class node;
class edge;

typedef std::list <edge *> edge_list_t;

//: ----------------------------------------------------------------------------
//: url_router
//: ----------------------------------------------------------------------------
class url_router
{
public:
        // -------------------------------------------------
        // Scoped Classes
        // -------------------------------------------------
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"  // ignored because std::iterator has non-virtual dtor
        class const_iterator : public std::iterator <std::forward_iterator_tag, std::pair <std::string, const void*> >
        {
#pragma GCC diagnostic pop                // back to default behaviour
        public:
                // -------------------------------------------------
                // Public methods
                // -------------------------------------------------
                const_iterator(const const_iterator& a_iterator);

                const value_type operator*() const;
                const value_type* operator->() const;

                const_iterator& operator++();
                const_iterator operator++(int);

                bool operator==(const const_iterator& a_iterator);
                bool operator!=(const const_iterator& a_iterator);

                // get the full url as a string
                std::string get_full_url(void) const;

                // -------------------------------------------------
                // Public members
                // -------------------------------------------------
                uint32_t depth;

        private:
                typedef std::stack <std::pair <edge_list_t::const_iterator, edge_list_t::const_iterator> > iterator_stack_t;

                // -------------------------------------------------
                // Private members
                // -------------------------------------------------
                const url_router& m_router;

                // worth some explanation
                // this is the stasck of iterators over the edge lists for every node in the trie
                // .first is the current position, .second is the end
                iterator_stack_t m_edge_iterators_stack;
                value_type m_cur_value;


                // -------------------------------------------------
                // Private methods
                // -------------------------------------------------
                const_iterator(const url_router& a_router);


                // Friends
                friend class url_router;
        };



        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        url_router();
        ~url_router();

        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        int32_t add_route(const std::string &a_route, const void *a_data);
        const void *find_route(const std::string &a_route, url_pmap_t &ao_url_pmap);
        void display(void);

        const_iterator begin() const;
        const_iterator end() const;


private:

        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy and assign
        url_router& operator=(const url_router &);
        url_router(const url_router &);

        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        node *m_root_node;


};


} //namespace ns_hlx {

#endif // #ifndef _URL_ROUTER_H
