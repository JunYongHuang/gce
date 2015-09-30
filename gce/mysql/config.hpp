///
/// Copyright (c) 2009-2014 Nous Xiong (348944179 at qq dot com)
///
/// Distributed under the Boost Software License, Version 1.0. (See accompanying
/// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///
/// See https://github.com/nousxiong/gce for latest version.
///

#ifndef GCE_MYSQL_CONFIG_HPP
#define GCE_MYSQL_CONFIG_HPP

#include <gce/config.hpp>

/// must include these before gce/actor/message.hpp
#include <gce/mysql/context_id.adl.h>
#include <gce/mysql/conn.adl.h>
#include <gce/mysql/conn_option.adl.h>

#include <gce/actor/all.hpp>
#include <gce/assert/all.hpp>
#include <gce/log/all.hpp>
#include <mysql.h>

#endif /// GCE_MYSQL_CONFIG_HPP
