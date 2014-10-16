///
/// Copyright (c) 2009-2014 Nous Xiong (348944179 at qq dot com)
///
/// Distributed under the Boost Software License, Version 1.0. (See accompanying
/// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///
/// See https://github.com/nousxiong/gce for latest version.
///

#ifndef GCE_ACTOR_DETAIL_SPAWN_ACTOR_HPP
#define GCE_ACTOR_DETAIL_SPAWN_ACTOR_HPP

#include <gce/actor/config.hpp>
#include <gce/actor/detail/stackful_actor.hpp>
#include <gce/actor/detail/stackless_actor.hpp>
#include <gce/actor/detail/actor_function.hpp>
#include <gce/actor/detail/actor_ref.hpp>

namespace gce
{
namespace detail
{
template <typename Context>
aid_t make_stackful_actor(
  aid_t const& sire, typename Context::stackful_service_t& svc,
  actor_func<stackful, Context> const& f, std::size_t stack_size
  )
{
  typedef Context context_t;
  context_t& ctx = svc.get_context();
  stackful_actor<Context>* a = svc.make_actor();
  a->init(f.f_);
  if (sire)
  {
    send(*a, sire, msg_new_actor);
  }
  aid_t aid = a->get_aid();
  a->start(stack_size);
  return aid;
}

template <typename Context>
aid_t make_stackless_actor(
  aid_t const& sire, typename Context::stackless_service_t& svc,
  actor_func<stackless, Context> const& f
  )
{
  typedef Context context_t;
  context_t& ctx = svc.get_context();
  stackless_actor<Context>* a = svc.make_actor();
  a->init(f.f_);
  if (sire)
  {
    send(*a, sire, msg_new_actor);
  }
  aid_t aid = a->get_aid();
  a->start();
  return aid;
}

template <typename Service, typename ActorRef>
Service& select_service(ActorRef sire, bool sync_sire)
{
  typedef typename ActorRef::context_t context_t;
  context_t& ctx = sire.get_context();
  if (sync_sire)
  {
    return ctx.select_service<Service>(sire.get_service().get_index());
  }
  else
  {
    return ctx.select_service<Service>();
  }
}

template <typename ActorRef>
aid_t end_spawn(ActorRef sire, link_type type)
{
  pattern patt(msg_new_actor);
  message msg;
  aid_t aid = sire.recv(msg, patt);
  if (!aid)
  {
    throw std::runtime_error("spawn actor failed!");
  }

  if (type == linked)
  {
    sire.link(aid);
  }
  else if (type == monitored)
  {
    sire.monitor(aid);
  }
  return aid;
}

template <typename Context>
void handle_spawn(
  actor_ref<stackless, Context> self, aid_t aid, message msg, 
  link_type type, boost::function<void (actor_ref<stackless, Context>, aid_t)> const& hdr
  )
{
  if (aid)
  {
    if (type == linked)
    {
      self.link(aid);
    }
    else if (type == monitored)
    {
      self.monitor(aid);
    }
  }

  hdr(self, aid);
}

/// spawn stackful_actor using NONE stackless_actor
template <typename ActorRef, typename F>
aid_t spawn(
  stackful,
  ActorRef sire, F f, bool sync_sire,
  link_type type, std::size_t stack_size
  )
{
  typedef typename ActorRef::context_t context_t;
  typedef typename context_t::stackful_service_t service_t;

  service_t& svc = select_service<service_t>(sire, sync_sire);
  svc.get_strand().post(
    boost::bind(
      &make_stackful_actor<context_t>,
      sire.get_aid(), boost::ref(svc),
      make_actor_func<stackful, context_t>(f), stack_size
      )
    );
  return end_spawn(sire, type);
}

/// spawn stackless_actor using NONE stackless_actor
template <typename ActorRef, typename F>
aid_t spawn(
  stackless,
  ActorRef sire, F f, bool sync_sire,
  link_type type, std::size_t
  )
{
  typedef typename ActorRef::context_t context_t;
  typedef typename context_t::stackless_service_t service_t;

  service_t& svc = select_service<service_t>(sire, sync_sire);
  svc.get_strand().post(
    boost::bind(
      &make_stackless_actor<context_t>,
      sire.get_aid(), boost::ref(svc),
      make_actor_func<stackless, context_t>(f)
      )
    );
  return end_spawn(sire, type);
}

#ifdef GCE_LUA
/// spawn lua_actor using NONE stackless_actor
template <typename ActorRef>
aid_t spawn(
  luaed,
  ActorRef sire, std::string const& script, 
  bool sync_sire, link_type type, std::size_t
  )
{
  typedef typename ActorRef::context_t context_t;
  typedef typename context_t::lua_service_t service_t;

  service_t& svc = select_service<service_t>(sire, sync_sire);
  svc.get_strand().post(
    boost::bind(
    &service_t::spawn_actor, &svc,
      script, sire.get_aid(), type
      )
    );
  return end_spawn(sire, type);
}
#endif

/// spawn stackless_actor using stackless_actor
template <typename Context, typename F, typename SpawnHandler>
void spawn(
  stackless,
  actor_ref<stackless, Context> sire, F f, SpawnHandler h,
  typename Context::stackless_service_t& svc,
  link_type type
  )
{
  typedef Context context_t;
  typedef boost::function<void (actor_ref<stackless, context_t>, aid_t)> spawn_handler_t;

  svc.get_strand().post(
    boost::bind(
      &make_stackless_actor<context_t>,
      sire.get_aid(), boost::ref(svc),
      make_actor_func<stackless, context_t>(f)
      )
    );

  pattern patt(detail::msg_new_actor);
  sire.recv(
    boost::bind(
      &handle_spawn<context_t>, _1, _2, _3,
      type, spawn_handler_t(h)
      ),
    patt
    );
}

#ifdef GCE_LUA
/// spawn lua_actor using stackless_actor
template <typename Context, typename SpawnHandler>
void spawn(
  luaed,
  actor_ref<stackless, Context> sire, SpawnHandler h, 
  std::string const& script, typename Context::lua_service_t& svc, 
  link_type type
  )
{
  typedef Context context_t;
  typedef typename context_t::lua_service_t service_t;
  typedef boost::function<void (actor_ref<stackless, context_t>, aid_t)> spawn_handler_t;

  svc.get_strand().post(
    boost::bind(
    &service_t::spawn_actor, &svc,
      script, sire.get_aid(), type
      )
    );

  pattern patt(detail::msg_new_actor);
  sire.recv(
    boost::bind(
      &handle_spawn<context_t>, _1, _2, _3,
      type, spawn_handler_t(h)
      ),
    patt
    );
}
#endif

template <typename Context>
void handle_remote_spawn(
  actor_ref<stackless, Context> self, aid_t aid,
  message msg, link_type type,
  boost::chrono::system_clock::time_point begin_tp,
  sid_t sid, seconds_t tmo, duration_t curr_tmo,
  boost::function<void (actor_ref<stackless, Context>, aid_t)> const& hdr
  )
{
  typedef Context context_t;

  boost::uint16_t err = 0;
  sid_t ret_sid = sid_nil;
  if (msg.get_type() == match_nil)
  {
    /// timeout
    hdr(self, aid);
    return;
  }

  msg >> err >> ret_sid;
  do
  {
    if (err != 0 || (aid && sid == ret_sid))
    {
      break;
    }

    if (tmo != infin)
    {
      duration_t pass_time = boost::chrono::system_clock::now() - begin_tp;
      curr_tmo -= pass_time;
    }

    begin_tp = boost::chrono::system_clock::now();
    pattern patt(detail::msg_spawn_ret, curr_tmo);
    self.recv(
      boost::bind(
        &handle_remote_spawn<context_t>, _1, _2, _3,
        type, begin_tp, sid, tmo, curr_tmo, hdr
        ),
      patt
      );
    return;
  }
  while (false);

  detail::spawn_error error = (detail::spawn_error)err;
  if (error != detail::spawn_ok)
  {
    aid = aid_t();
  }

  if (aid)
  {
    if (type == linked)
    {
      self.link(aid);
    }
    else if (type == monitored)
    {
      self.monitor(aid);
    }
  }

  hdr(self, aid);
}

template <typename ActorRef>
aid_t spawn_remote(
  spawn_type spw,
  ActorRef sire, std::string const& func, match_t ctxid,
  link_type type, std::size_t stack_size, seconds_t tmo
  )
{
  aid_t aid;
  sid_t sid = sire.spawn(spw, func, ctxid, stack_size);
  boost::uint16_t err = 0;
  sid_t ret_sid = sid_nil;

  duration_t curr_tmo = tmo;
  typedef boost::chrono::system_clock clock_t;
  clock_t::time_point begin_tp;

  do
  {
    begin_tp = clock_t::now();
    aid = sire->recv(detail::msg_spawn_ret, err, ret_sid, curr_tmo);
    if (err != 0 || (aid && sid == ret_sid))
    {
      break;
    }

    if (tmo != infin)
    {
      duration_t pass_time = clock_t::now() - begin_tp;
      curr_tmo -= pass_time;
    }
  }
  while (true);

  detail::spawn_error error = (detail::spawn_error)err;
  if (error != detail::spawn_ok)
  {
    switch (error)
    {
    case detail::spawn_no_socket:
      {
        throw std::runtime_error("no router socket available");
      }break;
    case detail::spawn_func_not_found:
      {
        throw std::runtime_error("remote func not found");
      }break;
    default:
      BOOST_ASSERT(false);
      break;
    }
  }

  if (type == linked)
  {
    sire.link(aid);
  }
  else if (type == monitored)
  {
    sire.monitor(aid);
  }
  return aid;
}

/// spawn remote stackful_actor using NONE stackless_actor
template <typename ActorRef>
aid_t spawn_remote(
  stackful,
  ActorRef sire, std::string const& func, match_t ctxid,
  link_type type, std::size_t stack_size, seconds_t tmo
  )
{
  return spawn_remote(spw_stackful, sire, func, ctxid, type, stack_size, tmo);
}

/// spawn remote stackless_actor using NONE stackless_actor
template <typename ActorRef>
aid_t spawn_remote(
  stackless,
  ActorRef sire, std::string const& func, match_t ctxid,
  link_type type, std::size_t stack_size, seconds_t tmo
  )
{
  return spawn_remote(spw_stackless, sire, func, ctxid, type, stack_size, tmo);
}

#ifdef GCE_LUA
/// spawn remote lua_actor using NONE stackless_actor
template <typename ActorRef>
aid_t spawn_remote(
  luaed,
  ActorRef sire, std::string const& func, match_t ctxid,
  link_type type, std::size_t stack_size, seconds_t tmo
  )
{
  return spawn_remote(spw_luaed, sire, func, ctxid, type, stack_size, tmo);
}
#endif

template <typename Context, typename SpawnHandler>
void spawn_remote(
  spawn_type spw,
  actor_ref<stackless, Context> sire, std::string const& func, SpawnHandler h,
  match_t ctxid = ctxid_nil,
  link_type type = no_link,
  std::size_t stack_size = default_stacksize(),
  seconds_t tmo = seconds_t(GCE_DEFAULT_REQUEST_TIMEOUT_SEC)
  )
{
  typedef Context context_t;
  typedef boost::function<void (actor_ref<stackless, context_t>, aid_t)> spawn_handler_t;

  aid_t aid;
  sid_t sid = sire.spawn(spw, func, ctxid, stack_size);

  duration_t curr_tmo = tmo;
  typedef boost::chrono::system_clock clock_t;
  clock_t::time_point begin_tp = clock_t::now();
  pattern patt(detail::msg_spawn_ret, curr_tmo);
  sire.recv(
    boost::bind(
      &handle_remote_spawn<context_t>, _1, _2, _3,
      type, begin_tp, sid, tmo, curr_tmo, spawn_handler_t(h)
      ),
    patt
    );
}

/// spawn remote stackful_actor using stackless_actor
template <typename Context, typename SpawnHandler>
void spawn_remote(
  stackful,
  actor_ref<stackless, Context>& sire, std::string const& func, SpawnHandler h,
  match_t ctxid, link_type type, std::size_t stack_size, seconds_t tmo
  )
{
  spawn_remote(spw_stackful, sire, func, h, ctxid, type, stack_size, tmo);
}

/// spawn remote stackless_actor using stackless_actor
template <typename Context, typename SpawnHandler>
aid_t spawn_remote(
  stackless,
  actor_ref<stackless, Context> sire, std::string const& func, SpawnHandler h,
  match_t ctxid, link_type type, std::size_t stack_size, seconds_t tmo
  )
{
  return spawn_remote(spw_stackless, sire, func, h, ctxid, type, stack_size, tmo);
}

#ifdef GCE_LUA
/// spawn remote lua_actor using stackless_actor
template <typename Context, typename SpawnHandler>
aid_t spawn_remote(
  luaed,
  actor_ref<stackless, Context> sire, std::string const& func, SpawnHandler h,
  match_t ctxid, link_type type, std::size_t stack_size, seconds_t tmo
  )
{
  return spawn_remote(spw_luaed, sire, func, h, ctxid, type, stack_size, tmo);
}
#endif
} /// namespace detail
} /// namespace gce

#endif /// GCE_ACTOR_DETAIL_SPAWN_ACTOR_HPP
