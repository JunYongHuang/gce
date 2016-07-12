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
class relay_ut
{
public:
  static void run()
  {
    std::cout << "relay_ut begin." << std::endl;
    for (std::size_t i=0; i<test_count; ++i)
    {
      test_common();
      if (test_count > 1) std::cout << "\r" << i;
    }
    if (test_count > 1) std::cout << std::endl;
    std::cout << "relay_ut end." << std::endl;
  }

private:
  static void my_actor(stackful_actor self, aid_t last_id)
  {
    message msg;
    aid_t sender = self.recv(msg);
    if (last_id != aid_nil)
    {
      self.relay(last_id, msg);
    }
    else
    {
      message m("hello");
      self.reply(sender, m);
    }
  }

  static void root(stackful_actor self)
  {
    std::size_t free_actor_num = 20;

    aid_t last_id;
    aid_t first_id;
    for (std::size_t i=0; i<free_actor_num; ++i)
    {
      aid_t aid =
        spawn(
          self,
          boost::bind(
            &relay_ut::my_actor, _arg1, last_id
            )
          );
      if (i == 0)
      {
        first_id = aid;
      }
      last_id = aid;
    }

    int i = 0;
    resp_t res = self->request(last_id, "hi", i);
    message msg;
    aid_t sender = self.respond(res, msg);
    BOOST_ASSERT(sender == first_id);
    BOOST_ASSERT(msg.get_type() == atom("hello"));
  }

  static void test_common()
  {
    try
    {
      std::size_t root_num = 100;
      context ctx;
      threaded_actor base = spawn(ctx);

      for (std::size_t i=0; i<root_num; ++i)
      {
        spawn(
          base,
          boost::bind(
            &relay_ut::root, _arg1
            ),
          monitored
          );
      }

      /// Just for vc test compile
      base.relay(base.get_aid(), message());

      for (std::size_t i=0; i<root_num; ++i)
      {
        base->recv(exit);
      }
    }
    catch (std::exception& ex)
    {
      std::cerr << ex.what() << std::endl;
    }
  }
};
}
