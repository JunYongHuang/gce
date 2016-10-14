///
/// Copyright (c) 2009-2015 Nous Xiong (348944179 at qq dot com)
///
/// Distributed under the Boost Software License, Version 1.0. (See accompanying
/// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///
/// See https://github.com/nousxiong/gce for latest version.
///

#ifndef GCE_HTTP_CONFIG_HPP
#define GCE_HTTP_CONFIG_HPP

#include <gce/config.hpp>
#include <gce/actor/all.hpp>
#include <gce/assert/all.hpp>
#include <gce/log/all.hpp>
#include <gce/format/all.hpp>
#include <gce/http_parser/all.hpp>
#include <gce/detail/asio_alloc_handler.hpp>
#include <boost/asio.hpp>

#ifndef GCE_HTTP_SERVER_RECV_BUFFER_SIZE 
# define GCE_HTTP_SERVER_RECV_BUFFER_SIZE 8192
#endif

#ifndef GCE_HTTP_CLIENT_RECV_BUFFER_SIZE 
# define GCE_HTTP_CLIENT_RECV_BUFFER_SIZE 8192
#endif

namespace gce
{
namespace http
{
static match_t const as_close = atom("http_close");
static match_t const as_request = atom("http_request");
static match_t const as_reply = atom("http_reply");

namespace misc_strings
{
char const name_value_separator[] = { ':', ' ' };
char const crlf[] = { '\r', '\n' };
char const space[] = { ' ' };
char const point[] = { '.' };
char const http_str[] = { 'H','T','T','P','/' };
} /// namespace misc_strings
}
}

#endif /// GCE_HTTP_CONFIG_HPP
