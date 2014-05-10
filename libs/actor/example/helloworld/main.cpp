﻿///
/// Copyright (c) 2009-2014 Nous Xiong (348944179 at qq dot com)
///
/// Distributed under the Boost Software License, Version 1.0. (See accompanying
/// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///
/// See https://github.com/nousxiong/gce for latest version.
///

#include <gce/actor/all.hpp>
#include <iostream>
#include <string>

void mirror(gce::self_t self)
{
  /// wait for messages
  gce::message msg;
  gce::aid_t sender = self.recv(msg);
  std::string what;
  msg >> what;

  /// prints "Hello World!"
  std::cout << what << std::endl;

  /// replies "!dlroW olleH"
  gce::message m;
  m << std::string(what.rbegin(), what.rend());
  self.reply(sender, m);
}

int main()
{
  /// everything begin here
  gce::context ctx;

  /// create a hello_world actor, using thread-base actor
  gce::mixin_t hello_world = gce::spawn(ctx);

  /// create a new actor that calls ’mirror(gce::self_t)’, using coroutine-base actor
  gce::aid_t mirror_actor = gce::spawn(hello_world, boost::bind(&mirror, _1));

  /// send "Hello World!" to mirror
  gce::message m;
  m << std::string("Hello World!");
  gce::response_t res = hello_world.request(mirror_actor, m);

  /// ... and wait for a response
  gce::message msg;
  hello_world.recv(res, msg);
  std::string reply_str;
  msg >> reply_str;

  /// prints "!dlroW olleH"
  std::cout << reply_str << std::endl;

  return 0;
}