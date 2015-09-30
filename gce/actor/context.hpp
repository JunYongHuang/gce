///
/// Copyright (c) 2009-2014 Nous Xiong (348944179 at qq dot com)
///
/// Distributed under the Boost Software License, Version 1.0. (See accompanying
/// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///
/// See https://github.com/nousxiong/gce for latest version.
///

#ifndef GCE_ACTOR_CONTEXT_HPP
#define GCE_ACTOR_CONTEXT_HPP

#include <gce/actor/config.hpp>
#include <gce/actor/actor_id.hpp>
#include <gce/actor/attributes.hpp>
#include <gce/actor/detail/threaded_actor.hpp>
#include <gce/actor/detail/stackful_actor.hpp>
#include <gce/actor/detail/stackless_actor.hpp>
#include <gce/actor/detail/nonblocked_actor.hpp>
#ifdef GCE_LUA
# include <gce/actor/detail/lua_actor.hpp>
#endif
#include <gce/actor/detail/socket_actor.hpp>
#include <gce/actor/detail/acceptor_actor.hpp>
#include <gce/actor/detail/actor_function.hpp>
#include <gce/actor/detail/msg_pool.hpp>
#include <gce/detail/dynarray.hpp>
#include <gce/detail/unique_ptr.hpp>
#include <gce/detail/linked_pool.hpp>
#include <gce/detail/linked_queue.hpp>
#include <boost/atomic.hpp>
#include <boost/optional.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/ref.hpp>
#include <boost/bind.hpp>
#include <boost/core/null_deleter.hpp>
#include <vector>

namespace gce
{
class context
{
public:
  typedef detail::basic_service<context> service_t;

  typedef detail::stackful_actor<context> stackful_actor_t;
  typedef detail::actor_service<stackful_actor_t, true> stackful_service_t;
  typedef detail::stackless_actor<context> stackless_actor_t;
  typedef detail::actor_service<stackless_actor_t, true> stackless_service_t;
#ifdef GCE_LUA
  typedef detail::lua_actor<context> lua_actor_t;
  typedef detail::lua_service<lua_actor_t> lua_service_t;
#endif
  typedef detail::nonblocked_actor<context> nonblocked_actor_t;
  typedef detail::actor_service<nonblocked_actor_t, false> nonblocked_service_t;
  typedef detail::threaded_actor<context> threaded_actor_t;
  typedef detail::actor_service<threaded_actor_t, false> threaded_service_t;
  typedef detail::socket_actor<context> socket_actor_t;
  typedef detail::network_service<socket_actor_t> socket_service_t;
  typedef detail::acceptor_actor<context> acceptor_actor_t;
  typedef detail::network_service<acceptor_actor_t> acceptor_service_t;

  typedef std::map<match_t, detail::remote_func<context> > native_func_list_t;
  typedef detail::linked_queue<detail::pack> tick_queue_t;
  typedef detail::linked_pool<detail::pack> tick_pool_t;

  struct tick_t
  {
    tick_t(size_t pool_reserve_size, size_t pool_max_size)
      : pool_(pool_reserve_size, pool_max_size)
      , on_(false)
    {
    }

    ~tick_t()
    {
    }

    tick_pool_t pool_;
    tick_queue_t que_;
    bool on_;
  };

  struct init_t
  {
    init_t()
    {
    }

    template <typename Tag, typename Match, typename F>
    void add_native_func(Match type, F f)
    {
      native_func_list_.insert(
        std::make_pair(
          to_match(type), 
          detail::make_actor_func<Tag, context>(f)
          )
        );
    }

    attributes attrs_;
    native_func_list_t native_func_list_;
  };

public:
  explicit context(attributes const& attrs = attributes())
    : attrs_(attrs)
    , timestamp_((timestamp_t)sysclock_t::now().time_since_epoch().count())
    , stopped_(false)
    , service_size_(
        attrs_.thread_num_ == 0 ? 
          attrs_.per_thread_service_num_ : 
          attrs_.thread_num_ * attrs_.per_thread_service_num_
        )
    , concurrency_size_(service_size_ + attrs_.nonblocked_num_)
    , strand_list_(concurrency_size_)
    , tick_list_(service_size_)
    , msg_pool_list_(concurrency_size_)
    , mailbox_pool_set_list_(concurrency_size_)
    , threaded_service_list_(service_size_)
    , curr_threaded_svc_(0)
    , stackful_service_list_(service_size_)
    , curr_stackful_svc_(0)
    , stackless_service_list_(service_size_)
    , curr_stackless_svc_(0)
#ifdef GCE_LUA
    , lua_service_list_(service_size_)
    , curr_lua_svc_(0)
#endif
    , nonblocked_service_list_(attrs_.nonblocked_num_)
    , nonblocked_actor_list_(attrs_.nonblocked_num_)
    , curr_nonblocked_actor_(0)
    , socket_service_list_(service_size_)
    , curr_socket_svc_(0)
    , acceptor_service_list_(service_size_)
    , curr_acceptor_svc_(0)
    , threaded_actor_list_(service_size_)
    , lg_(attrs_.lg_)
  {
    init();
  }

  explicit context(init_t const& in)
    : attrs_(in.attrs_)
    , timestamp_((timestamp_t)sysclock_t::now().time_since_epoch().count())
    , stopped_(false)
    , service_size_(
        attrs_.thread_num_ == 0 ? 
          attrs_.per_thread_service_num_ : 
          attrs_.thread_num_ * attrs_.per_thread_service_num_
        )
    , concurrency_size_(service_size_ + attrs_.nonblocked_num_)
    , strand_list_(concurrency_size_)
    , tick_list_(service_size_)
    , msg_pool_list_(concurrency_size_)
    , mailbox_pool_set_list_(concurrency_size_)
    , threaded_service_list_(service_size_)
    , curr_threaded_svc_(0)
    , stackful_service_list_(service_size_)
    , curr_stackful_svc_(0)
    , stackless_service_list_(service_size_)
    , curr_stackless_svc_(0)
#ifdef GCE_LUA
    , lua_service_list_(service_size_)
    , curr_lua_svc_(0)
#endif
    , nonblocked_service_list_(attrs_.nonblocked_num_)
    , nonblocked_actor_list_(attrs_.nonblocked_num_)
    , curr_nonblocked_actor_(0)
    , socket_service_list_(service_size_)
    , curr_socket_svc_(0)
    , acceptor_service_list_(service_size_)
    , curr_acceptor_svc_(0)
    , threaded_actor_list_(service_size_)
    , lg_(attrs_.lg_)
  {
    init(in.native_func_list_);
  }

  ~context()
  {
    stop();
  }

public:
  io_service_t& get_io_service()
  {
    GCE_ASSERT(ios_);
    return *ios_;
  }

public:
  /// internal use
  attributes const& get_attributes() const
  {
    return attrs_; 
  }

  ctxid_t get_ctxid() const
  {
    return attrs_.id_;
  }

  timestamp_t get_timestamp() const
  { 
    return timestamp_; 
  }

  log::logger_t& get_logger()
  {
    return lg_;
  }

  size_t get_service_size() const
  {
    return service_size_;
  }

  size_t get_concurrency_size() const
  {
    return concurrency_size_; 
  }

  service_t& get_service(detail::actor_index const& ai) const
  {
    switch (ai.type_)
    {
    case detail::actor_stackful:
      return (service_t&)stackful_service_list_[ai.svc_id_];
    case detail::actor_stackless:
      return (service_t&)stackless_service_list_[ai.svc_id_];
#ifdef GCE_LUA
    case detail::actor_luaed:
      return (service_t&)lua_service_list_[ai.svc_id_];
#endif
    case detail::actor_socket:
      return (service_t&)socket_service_list_[ai.svc_id_];
    case detail::actor_acceptor:
      return (service_t&)acceptor_service_list_[ai.svc_id_];
    default:
      GCE_ASSERT(false)(ai.type_)(ai.svc_id_)(ai.ptr_)
        .log(lg_, "out of actor type");
      // just suppress vc's warning
      throw 1;
    }
  }

  template <typename Service>
  Service& select_service(size_t index)
  {
    typename Service::type ty;
    return get_service(ty, index);
  }

  template <typename Service>
  Service& select_service()
  {
    typename Service::type ty;
    return select_service(ty);
  }

  threaded_actor_t& make_threaded_actor()
  {
    threaded_actor_t* a = 
      new threaded_actor_t(select_service<threaded_service_t>());
    threaded_actor_list_.push(a);
    return *a;
  }

  nonblocked_actor_t& make_nonblocked_actor()
  {
    size_t i = curr_nonblocked_actor_.fetch_add(1, boost::memory_order_relaxed);
    GCE_VERIFY(i < nonblocked_actor_list_.size())(i)(nonblocked_actor_list_.size())
      .log(lg_, "out of nonblocked actor list size");
    return nonblocked_actor_list_[i];
  }

  void register_service(
    match_t name, aid_t const& svc, detail::actor_type type, size_t concurrency_index
    )
  {
    for (size_t i=0; i<service_size_; ++i)
    {
      register_service(name, svc, threaded_service_list_[i]);
      register_service(name, svc, stackful_service_list_[i]);
      register_service(name, svc, stackless_service_list_[i]);
#ifdef GCE_LUA
      register_service(name, svc, lua_service_list_[i]);
#endif
      register_service(name, svc, socket_service_list_[i]);
      register_service(name, svc, acceptor_service_list_[i]);
    }

    for (size_t i=0, size=nonblocked_actor_list_.size(); i<size; ++i)
    {
      nonblocked_actor_list_[i].register_service(name, svc, type, concurrency_index);
    }
  }
  
  void deregister_service(
    match_t name, aid_t const& svc, detail::actor_type type, size_t concurrency_index
    )
  {
    for (size_t i=0; i<service_size_; ++i)
    {
      deregister_service(name, svc, threaded_service_list_[i]);
      deregister_service(name, svc, stackful_service_list_[i]);
      deregister_service(name, svc, stackless_service_list_[i]);
#ifdef GCE_LUA
      deregister_service(name, svc, lua_service_list_[i]);
#endif
      deregister_service(name, svc, socket_service_list_[i]);
      deregister_service(name, svc, acceptor_service_list_[i]);
    }

    for (size_t i=0, size=nonblocked_actor_list_.size(); i<size; ++i)
    {
      nonblocked_actor_list_[i].deregister_service(name, svc, type, concurrency_index);
    }
  }

  void register_socket(
    detail::ctxid_pair_t ctxid_pr, aid_t const& skt, detail::actor_type type, size_t concurrency_index
    )
  {
    for (size_t i=0; i<service_size_; ++i)
    {
      register_socket(ctxid_pr, skt, threaded_service_list_[i]);
      register_socket(ctxid_pr, skt, stackful_service_list_[i]);
      register_socket(ctxid_pr, skt, stackless_service_list_[i]);
#ifdef GCE_LUA
      register_socket(ctxid_pr, skt, lua_service_list_[i]);
#endif
      register_socket(ctxid_pr, skt, socket_service_list_[i]);
      register_socket(ctxid_pr, skt, acceptor_service_list_[i]);
    }

    for (size_t i=0, size=nonblocked_actor_list_.size(); i<size; ++i)
    {
      nonblocked_actor_list_[i].register_socket(ctxid_pr, skt, type, concurrency_index);
    }
  }

  void deregister_socket(
    detail::ctxid_pair_t ctxid_pr, aid_t const& skt, detail::actor_type type, size_t concurrency_index
    )
  {
    for (size_t i=0; i<service_size_; ++i)
    {
      deregister_socket(ctxid_pr, skt, threaded_service_list_[i]);
      deregister_socket(ctxid_pr, skt, stackful_service_list_[i]);
      deregister_socket(ctxid_pr, skt, stackless_service_list_[i]);
#ifdef GCE_LUA
      deregister_socket(ctxid_pr, skt, lua_service_list_[i]);
#endif
      deregister_socket(ctxid_pr, skt, socket_service_list_[i]);
      deregister_socket(ctxid_pr, skt, acceptor_service_list_[i]);
    }

    for (size_t i=0, size=nonblocked_actor_list_.size(); i<size; ++i)
    {
      nonblocked_actor_list_[i].deregister_socket(ctxid_pr, skt, type, concurrency_index);
    }
  }

  void register_acceptor(std::string const& ep, aid_t const& acpr, size_t concurrency_index)
  {
    for (size_t i=0; i<service_size_; ++i)
    {
      register_acceptor(ep, acpr, threaded_service_list_[i]);
      register_acceptor(ep, acpr, stackful_service_list_[i]);
      register_acceptor(ep, acpr, stackless_service_list_[i]);
#ifdef GCE_LUA
      register_acceptor(ep, acpr, lua_service_list_[i]);
#endif
      register_acceptor(ep, acpr, socket_service_list_[i]);
      register_acceptor(ep, acpr, acceptor_service_list_[i]);
    }

    for (size_t i=0, size=nonblocked_actor_list_.size(); i<size; ++i)
    {
      nonblocked_actor_list_[i].register_acceptor(ep, acpr, detail::actor_acceptor, concurrency_index);
    }
  }

  void deregister_acceptor(std::string const& ep, aid_t const& acpr, size_t concurrency_index)
  {
    for (size_t i=0; i<service_size_; ++i)
    {
      deregister_acceptor(ep, acpr, threaded_service_list_[i]);
      deregister_acceptor(ep, acpr, stackful_service_list_[i]);
      deregister_acceptor(ep, acpr, stackless_service_list_[i]);
#ifdef GCE_LUA
      deregister_acceptor(ep, acpr, lua_service_list_[i]);
#endif
      deregister_acceptor(ep, acpr, socket_service_list_[i]);
      deregister_acceptor(ep, acpr, acceptor_service_list_[i]);
    }

    for (size_t i=0, size=nonblocked_actor_list_.size(); i<size; ++i)
    {
      nonblocked_actor_list_[i].deregister_acceptor(ep, acpr, detail::actor_acceptor, concurrency_index);
    }
  }

  void conn_socket(
    detail::ctxid_pair_t ctxid_pr, aid_t const& skt, detail::actor_type type, size_t concurrency_index
    )
  {
    for (size_t i=0; i<service_size_; ++i)
    {
      conn_socket(ctxid_pr, skt, threaded_service_list_[i]);
      conn_socket(ctxid_pr, skt, stackful_service_list_[i]);
      conn_socket(ctxid_pr, skt, stackless_service_list_[i]);
#ifdef GCE_LUA
      conn_socket(ctxid_pr, skt, lua_service_list_[i]);
#endif
      conn_socket(ctxid_pr, skt, socket_service_list_[i]);
      conn_socket(ctxid_pr, skt, acceptor_service_list_[i]);
    }

    for (size_t i=0, size=nonblocked_actor_list_.size(); i<size; ++i)
    {
      nonblocked_actor_list_[i].conn_socket(ctxid_pr, skt, type, concurrency_index);
    }
  }

  void disconn_socket(
    detail::ctxid_pair_t ctxid_pr, aid_t const& skt, detail::actor_type type, size_t concurrency_index
    )
  {
    for (size_t i=0; i<service_size_; ++i)
    {
      disconn_socket(ctxid_pr, skt, threaded_service_list_[i]);
      disconn_socket(ctxid_pr, skt, stackful_service_list_[i]);
      disconn_socket(ctxid_pr, skt, stackless_service_list_[i]);
#ifdef GCE_LUA
      disconn_socket(ctxid_pr, skt, lua_service_list_[i]);
#endif
      disconn_socket(ctxid_pr, skt, socket_service_list_[i]);
      disconn_socket(ctxid_pr, skt, acceptor_service_list_[i]);
    }

    for (size_t i=0, size=nonblocked_actor_list_.size(); i<size; ++i)
    {
      nonblocked_actor_list_[i].disconn_socket(ctxid_pr, skt, type, concurrency_index);
    }
  }

#ifdef GCE_SCRIPT
  template <typename Type>
  void register_script(std::string const& name, std::string const& script)
  {
    Type ty;
    for (size_t i=0; i<service_size_; ++i)
    {
      register_script(ty, name, script, i);
    }
  }
#endif

  detail::msg_pool_t& get_msg_pool(size_t index)
  {
    return msg_pool_list_[index];
  }

  detail::mailbox_pool_set& get_mailbox_pool_set_list(size_t index)
  {
    return mailbox_pool_set_list_[index];
  }

  detail::pack& alloc_tick_pack(size_t index)
  {
    return *tick_list_[index].pool_.get();
  }

  void push_tick(size_t index, detail::pack& pk)
  {
    tick_list_[index].que_.push(&pk);
  }

  void on_tick(size_t index)
  {
    tick_t& tick = tick_list_[index];
    if (tick.on_)
    {
      return;
    }

    tick_queue_t& que = tick.que_;
    detail::scoped_bool<bool> scp(tick.on_);
    for (size_t i=0; !que.empty(); ++i)
    {
      if (i >= attrs_.max_tick_handle_size_)
      {
        strand_list_[index].post(on_tick_binder(*this, index));
        break;
      }

      detail::pack* pk = que.pop();
      service_t& svc = get_service(pk->ai_);
      svc.handle_tick(*pk);
      tick.pool_.free(pk);
    }
  }

private:
  struct on_tick_binder
  {
    on_tick_binder(context& ctx, size_t index)
      : ctx_(ctx)
      , index_(index)
    {
    }

    void operator()() const
    {
      ctx_.on_tick(index_);
    }

    context& ctx_;
    size_t index_;
  };

  void init(native_func_list_t const& native_func_list = native_func_list_t())
  {
    if (attrs_.ios_)
    {
      ios_.reset(attrs_.ios_, boost::null_deleter());
    }
    else
    {
      ios_.reset(new io_service_t(attrs_.thread_num_));
    }
    work_ = boost::in_place(boost::ref(*ios_));

    try
    {
      size_t index = 0;
      for (size_t i=0; i<service_size_; ++i, ++index)
      {
        strand_list_.emplace_back(boost::ref(*ios_));
        strand_t& snd = strand_list_.back();
        tick_list_.emplace_back(attrs_.tick_pool_reserve_size_, attrs_.tick_pool_max_size_);
        msg_pool_list_.emplace_back(attrs_.msg_pool_reserve_size_, attrs_.msg_pool_max_size_);
        mailbox_pool_set_list_.emplace_back(
          attrs_.mailbox_recv_pool_reserve_size_, attrs_.mailbox_recv_pool_grow_size_, 
          attrs_.mailbox_mq_pool_reserve_size_, attrs_.mailbox_mq_pool_grow_size_, 
          attrs_.mailbox_res_pool_reserve_size_, attrs_.mailbox_res_pool_grow_size_
          );
        threaded_service_list_.emplace_back(boost::ref(*this), boost::ref(snd), index);
        stackful_service_list_.emplace_back(boost::ref(*this), boost::ref(snd), index);
        stackless_service_list_.emplace_back(boost::ref(*this), boost::ref(snd), index);
#ifdef GCE_LUA
        lua_service_list_.emplace_back(boost::ref(*this), boost::ref(snd), index, native_func_list);
#endif
        socket_service_list_.emplace_back(boost::ref(*this), boost::ref(snd), index);
        acceptor_service_list_.emplace_back(boost::ref(*this), boost::ref(snd), index);
      }

      for (size_t i=0; i<attrs_.nonblocked_num_; ++i, ++index)
      {
        strand_list_.emplace_back(boost::ref(*ios_));
        strand_t& snd = strand_list_.back();
        msg_pool_list_.emplace_back(attrs_.msg_pool_reserve_size_, attrs_.msg_pool_max_size_);
        mailbox_pool_set_list_.emplace_back(
          attrs_.mailbox_recv_pool_reserve_size_, attrs_.mailbox_recv_pool_grow_size_, 
          attrs_.mailbox_mq_pool_reserve_size_, attrs_.mailbox_mq_pool_grow_size_, 
          attrs_.mailbox_res_pool_reserve_size_, attrs_.mailbox_res_pool_grow_size_
          );
        nonblocked_service_list_.emplace_back(boost::ref(*this), boost::ref(snd), index);
        nonblocked_actor_list_.emplace_back(boost::ref(*this), boost::ref(nonblocked_service_list_[i]), index);
      }

#ifdef GCE_LUA
      if (attrs_.lua_gce_path_list_.empty())
      {
        attrs_.lua_gce_path_list_.push_back(".");
      }
      std::string lua_gce_path;
      BOOST_FOREACH(std::string& path, attrs_.lua_gce_path_list_)
      {
        path += "?.lua";
        lua_gce_path += path;
        lua_gce_path += ";";
      }
      lua_gce_path.erase(--lua_gce_path.end());

      std::vector<lua_register_t>& lua_reg_list = attrs_.lua_reg_list_;
      for (size_t i=0; i<service_size_; ++i)
      {
        lua_service_list_[i].make_libgce(lua_gce_path);
        lua_State* L = lua_service_list_[i].get_lua_state();
        BOOST_FOREACH(lua_register_t& lua_reg, lua_reg_list)
        {
          lua_reg(L);
        }
      }
#endif

      for (size_t i=0; i<attrs_.thread_num_; ++i)
      {
        thread_group_.create_thread(
          boost::bind(
            &context::run, this, i,
            attrs_.thread_begin_cb_list_,
            attrs_.thread_end_cb_list_
            )
          );
      }
    }
    catch (...)
    {
      GCE_ERROR(lg_)(__FILE__)(__LINE__) << 
        boost::current_exception_diagnostic_information();
      stop();
      throw;
    }
  }

  void run(
    thrid_t id,
    std::vector<thread_callback_t> const& begin_cb_list,
    std::vector<thread_callback_t> const& end_cb_list
    )
  {
    BOOST_FOREACH(thread_callback_t const& cb, begin_cb_list)
    {
      cb(id);
    }

    while (true)
    {
      try
      {
        ios_->run();
        break;
      }
      catch (...)
      {
        GCE_ERROR(lg_)(__FILE__)(__LINE__) <<
          boost::current_exception_diagnostic_information();
      }
    }

    BOOST_FOREACH(thread_callback_t const& cb, end_cb_list)
    {
      cb(id);
    }
  }

  void stop()
  {
    work_ = boost::none;
    stopped_ = true;
    for (size_t i=0; i<service_size_; ++i)
    {
      stop_service(threaded_service_list_[i]);
      stop_service(stackful_service_list_[i]);
      stop_service(stackless_service_list_[i]);
#ifdef GCE_LUA
      stop_service(lua_service_list_[i]);
#endif
      stop_service(socket_service_list_[i]);
      stop_service(acceptor_service_list_[i]);
    }

    thread_group_.join_all();

    threaded_actor_t* thra = 0;
    while (threaded_actor_list_.pop(thra))
    {
      delete thra;
    }
    nonblocked_actor_list_.clear();

    threaded_service_list_.clear();
    stackful_service_list_.clear();
    stackless_service_list_.clear();
#ifdef GCE_LUA
    lua_service_list_.clear();
#endif
    nonblocked_service_list_.clear();
    socket_service_list_.clear();
    acceptor_service_list_.clear();
  }

  template <typename Service>
  void stop_service(Service& svc)
  {
    svc.get_strand().post(boost::bind(&Service::stop, &svc));
  }

  struct tag_reg {};
  struct tag_dereg {};
  struct tag_con {};
  struct tag_decon {};

  template <typename Tag, typename Service>
  struct service_binder
  {
    service_binder(Service& s, match_t const& name, aid_t const& svc)
      : s_(s)
      , name_(name)
      , svc_(svc)
    {
    }

    service_binder(Service& s, match_t const& name, ctxid_t const& ctxid)
      : s_(s)
      , name_(name)
      , ctxid_(ctxid)
    {
    }

    void operator()() const
    {
      invoke(Tag());
    }

  private:
    void invoke(tag_reg) const
    {
      s_.register_service(name_, svc_);
    }

    void invoke(tag_dereg) const
    {
      s_.deregister_service(name_, svc_);
    }
    
    Service& s_;
    match_t name_;
    aid_t svc_;
    ctxid_t ctxid_;
  };

  template <typename Service>
  void register_service(match_t name, aid_t const& svc, Service& s)
  {
    s.get_strand().dispatch(service_binder<tag_reg, Service>(s, name, svc));
  }

  template <typename Service>
  void deregister_service(match_t name, aid_t const& svc, Service& s)
  {
    s.get_strand().dispatch(service_binder<tag_dereg, Service>(s, name, svc));
  }

  template <typename Tag, typename Service>
  struct socket_binder
  {
    socket_binder(Service& s, detail::ctxid_pair_t const& ctxid_pr, aid_t const& skt)
      : s_(s)
      , ctxid_pr_(ctxid_pr)
      , skt_(skt)
    {
    }

    void operator()() const
    {
      invoke(Tag());
    }

  private:
    void invoke(tag_reg) const
    {
      s_.register_socket(ctxid_pr_, skt_);
    }

    void invoke(tag_dereg) const
    {
      s_.deregister_socket(ctxid_pr_, skt_);
    }

    void invoke(tag_con) const
    {
      s_.conn_socket(ctxid_pr_, skt_);
    }

    void invoke(tag_decon) const
    {
      s_.disconn_socket(ctxid_pr_, skt_);
    }
    
    Service& s_;
    detail::ctxid_pair_t ctxid_pr_;
    aid_t skt_;
  };

  template <typename Tag, typename Service>
  struct acceptor_binder
  {
    acceptor_binder(Service& s, std::string const& ep, aid_t const& acpr)
      : s_(s)
      , ep_(ep)
      , acpr_(acpr)
    {
    }

    void operator()() const
    {
      invoke(Tag());
    }

  private:
    void invoke(tag_reg) const
    {
      s_.register_acceptor(ep_, acpr_);
    }

    void invoke(tag_dereg) const
    {
      s_.deregister_acceptor(ep_, acpr_);
    }
    
    Service& s_;
    std::string const ep_;
    aid_t acpr_;
  };

  template <typename Service>
  void register_socket(detail::ctxid_pair_t ctxid_pr, aid_t const& skt, Service& s)
  {
    s.get_strand().dispatch(socket_binder<tag_reg, Service>(s, ctxid_pr, skt));
  }

  template <typename Service>
  void deregister_socket(detail::ctxid_pair_t ctxid_pr, aid_t const& skt, Service& s)
  {
    s.get_strand().dispatch(socket_binder<tag_dereg, Service>(s, ctxid_pr, skt));
  }

  template <typename Service>
  void register_acceptor(std::string const& ep, aid_t const& acpr, Service& s)
  {
    s.get_strand().dispatch(acceptor_binder<tag_reg, Service>(s, ep, acpr));
  }

  template <typename Service>
  void deregister_acceptor(std::string const& ep, aid_t const& acpr, Service& s)
  {
    s.get_strand().dispatch(acceptor_binder<tag_dereg, Service>(s, ep, acpr));
  }

  template <typename Service>
  void conn_socket(detail::ctxid_pair_t ctxid_pr, aid_t const& skt, Service& s)
  {
    s.get_strand().dispatch(socket_binder<tag_con, Service>(s, ctxid_pr, skt));
  }

  template <typename Service>
  void disconn_socket(detail::ctxid_pair_t ctxid_pr, aid_t const& skt, Service& s)
  {
    s.get_strand().dispatch(socket_binder<tag_decon, Service>(s, ctxid_pr, skt));
  }

#ifdef GCE_LUA
  struct lua_script_binder
  {
    lua_script_binder(lua_service_t& s, std::string const& name, std::string const& script)
      : s_(s)
      , name_(name)
      , script_(script)
    {
    }

    void operator()() const
    {
      s_.set_script(name_, script_);
    }

    lua_service_t& s_;
    std::string name_;
    std::string script_;
  };

  void register_script(luaed, std::string const& name, std::string const& script, size_t i)
  {
    lua_service_t& s = lua_service_list_[i];
    s.get_strand().dispatch(lua_script_binder(s, name, script));
  }
#endif

  threaded_service_t& select_service(threaded)
  {
    return select_service<threaded_service_t>(curr_threaded_svc_, threaded_service_list_);
  }

  stackful_service_t& select_service(stackful)
  {
    return select_service<stackful_service_t>(curr_stackful_svc_, stackful_service_list_);
  }

  stackless_service_t& select_service(stackless)
  {
    return select_service<stackless_service_t>(curr_stackless_svc_, stackless_service_list_);
  }

#ifdef GCE_LUA
  lua_service_t& select_service(luaed)
  {
    return select_service<lua_service_t>(curr_lua_svc_, lua_service_list_);
  }
#endif

  socket_service_t& select_service(socket)
  {
    return select_service<socket_service_t>(curr_socket_svc_, socket_service_list_);
  }

  acceptor_service_t& select_service(acceptor)
  {
    return select_service<acceptor_service_t>(curr_acceptor_svc_, acceptor_service_list_);
  }

  template <typename Service, typename ServiceList>
  Service& select_service(size_t& curr_svc, ServiceList& svc_list)
  {
    size_t curr = curr_svc;
    ++curr;
    if (curr >= service_size_)
    {
      curr = 0;
    }
    curr_svc = curr;
    return svc_list[curr];
  }

  threaded_service_t& get_service(threaded, size_t index)
  {
    return threaded_service_list_.at(index);
  }

  stackful_service_t& get_service(stackful, size_t index)
  {
    return stackful_service_list_.at(index);
  }

  stackless_service_t& get_service(stackless, size_t index)
  {
    return stackless_service_list_.at(index);
  }

#ifdef GCE_LUA
  lua_service_t& get_service(luaed, size_t index)
  {
    return lua_service_list_.at(index);
  }
#endif
  
  socket_service_t& get_service(socket, size_t index)
  {
    return socket_service_list_.at(index);
  }

  acceptor_service_t& get_service(acceptor, size_t index)
  {
    return acceptor_service_list_.at(index);
  }

private:
  /// Ensure start from a new cache line.
  byte_t pad0_[GCE_CACHE_LINE_SIZE];

  GCE_CACHE_ALIGNED_VAR(attributes, attrs_)
  GCE_CACHE_ALIGNED_VAR(timestamp_t const, timestamp_)

  typedef volatile bool volatile_bool_t;
  GCE_CACHE_ALIGNED_VAR(volatile_bool_t, stopped_)

  /// select service
  GCE_CACHE_ALIGNED_VAR(size_t, service_size_)

  GCE_CACHE_ALIGNED_VAR(size_t, concurrency_size_)

  GCE_CACHE_ALIGNED_VAR(detail::unique_ptr<io_service_t>, ios_)
  GCE_CACHE_ALIGNED_VAR(boost::optional<io_service_t::work>, work_)

  GCE_CACHE_ALIGNED_VAR(boost::thread_group, thread_group_)

  /// strand list
  GCE_CACHE_ALIGNED_VAR(detail::dynarray<strand_t>, strand_list_)

  /// tick queue list
  GCE_CACHE_ALIGNED_VAR(detail::dynarray<tick_t>, tick_list_)

  /// message pool list
  GCE_CACHE_ALIGNED_VAR(detail::dynarray<detail::msg_pool_t>, msg_pool_list_)

  /// mailbox's recv_pair pool list
  GCE_CACHE_ALIGNED_VAR(detail::dynarray<detail::mailbox_pool_set>, mailbox_pool_set_list_)

  /// threaded actor services
  GCE_CACHE_ALIGNED_VAR(detail::dynarray<threaded_service_t>, threaded_service_list_)
  GCE_CACHE_ALIGNED_VAR(size_t, curr_threaded_svc_)

  /// stackful actor services
  GCE_CACHE_ALIGNED_VAR(detail::dynarray<stackful_service_t>, stackful_service_list_)
  GCE_CACHE_ALIGNED_VAR(size_t, curr_stackful_svc_)

  /// stackless actor services
  GCE_CACHE_ALIGNED_VAR(detail::dynarray<stackless_service_t>, stackless_service_list_)
  GCE_CACHE_ALIGNED_VAR(size_t, curr_stackless_svc_)

#ifdef GCE_LUA
  /// lua actor services
  GCE_CACHE_ALIGNED_VAR(detail::dynarray<lua_service_t>, lua_service_list_)
  GCE_CACHE_ALIGNED_VAR(size_t, curr_lua_svc_)
#endif

  /// nonblocked actors
  GCE_CACHE_ALIGNED_VAR(detail::dynarray<nonblocked_service_t>, nonblocked_service_list_)
  GCE_CACHE_ALIGNED_VAR(detail::dynarray<nonblocked_actor_t>, nonblocked_actor_list_)
  GCE_CACHE_ALIGNED_VAR(boost::atomic_size_t, curr_nonblocked_actor_)

  /// socket actors
  GCE_CACHE_ALIGNED_VAR(detail::dynarray<socket_service_t>, socket_service_list_)
  GCE_CACHE_ALIGNED_VAR(size_t, curr_socket_svc_)

  /// acceptor actors
  GCE_CACHE_ALIGNED_VAR(detail::dynarray<acceptor_service_t>, acceptor_service_list_)
  GCE_CACHE_ALIGNED_VAR(size_t, curr_acceptor_svc_)

  /// threaded actors
  GCE_CACHE_ALIGNED_VAR(boost::lockfree::queue<threaded_actor_t*>, threaded_actor_list_)

  /// logger
  GCE_CACHE_ALIGNED_VAR(log::logger_t, lg_);
};
}

#endif /// GCE_ACTOR_CONTEXT_HPP
