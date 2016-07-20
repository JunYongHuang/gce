///
/// Copyright (c) 2009-2014 Nous Xiong (348944179 at qq dot com)
///
/// Distributed under the Boost Software License, Version 1.0. (See accompanying
/// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///
/// See https://github.com/nousxiong/gce for latest version.
///

namespace gce
{
class router_link_ut
{
public:
  static void run()
  {
    std::cout << "router_link_ut begin." << std::endl;
    for (std::size_t i=0; i<test_count; ++i)
    {
      test_base();
      if (test_count > 1) std::cout << "\r" << i;
    }
    if (test_count > 1) std::cout << std::endl;
    std::cout << "router_link_ut end." << std::endl;
  }

public:
  static void test_base()
  {
    try
    {
      std::size_t quiter_num = 100;

      attributes attrs;
      attrs.id_ = atom("router");
      context ctx(attrs);
      attrs.id_ = atom("one");
      context ctx1(attrs);
      attrs.id_ = atom("two");
      context ctx2(attrs);
      
      threaded_actor base = spawn(ctx);
      threaded_actor base1 = spawn(ctx1);
      threaded_actor base2 = spawn(ctx2);
      
      netopt_t opt = make_netopt();
      opt.is_router = 1;
      gce::bind(base, "tcp://127.0.0.1:14923", remote_func_list_t(), opt);

      opt.reconn_period = seconds(1);
      connect(base1, "router", "tcp://127.0.0.1:14923", opt);
      connect(base2, "router", "tcp://127.0.0.1:14923", opt);
      base2.sleep_for(millisecs(100));

      std::vector<aid_t> quiter_list(quiter_num);
      for (std::size_t i=0; i<quiter_num; ++i)
      {
        quiter_list[i] =
          spawn(
            base2,
            boost::bind(
              &router_link_ut::quiter, _arg1
              ),
            monitored
            );
        base1.link(quiter_list[i]);
      }

      for (std::size_t i=0; i<quiter_num; ++i)
      {
        base1->send(quiter_list[i]);
      }

      for (std::size_t i=0; i<quiter_num; ++i)
      {
        base1->recv(exit);
        base2->recv(exit);
      }
    }
    catch (std::exception& ex)
    {
      std::cerr << "test_base except: " << ex.what() << std::endl;
    }
  }

  static void quiter(stackful_actor self)
  {
    try
    {
      self->recv();
    }
    catch (std::exception& ex)
    {
      std::cerr << "quiter except: " << ex.what() << std::endl;
    }
  }
};
}

