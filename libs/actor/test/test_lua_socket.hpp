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
class lua_socket_ut
{
public:
  static void run()
  {
    std::cout << "lua lua_socket_ut begin." << std::endl;
    for (std::size_t i=0; i<test_count; ++i)
    {
      test_base();
      if (test_count > 1) std::cout << "\r" << i;
    }
    if (test_count > 1) std::cout << std::endl;
    std::cout << "lua lua_socket_ut end." << std::endl;
  }

public:
  static void test_base()
  {
    try
    {
      std::size_t echo_num = 10;

      gce::log::asio_logger lg;
      attributes attrs;
      attrs.lg_ = boost::bind(&gce::log::asio_logger::output, &lg, _arg1, "");
      
      attrs.id_ = atom("one");
      context ctx1(attrs);
      attrs.id_ = atom("two");
      context ctx2(attrs);
      
      threaded_actor base1 = spawn(ctx1);
      threaded_actor base2 = spawn(ctx2);

      aid_t bind_aid = spawn(base2, "test_lua_actor/socket_bind.lua", monitored);
      base2->send(bind_aid, "init");
      lua_Number port = -1;
      base2->recv("port", port);
      base2->recv(gce::exit);

      aid_t svr_aid = spawn(base2, "test_lua_actor/socket_echo_server.lua", monitored);

      aid_t conn_aid = spawn(base1, "test_lua_actor/socket_conn.lua", monitored);
      base1->send(conn_aid, "init", port);
      base1->recv(gce::exit);

      aid_t cln_aid = spawn(base1, "test_lua_actor/socket_echo_client.lua");
      base1->send(cln_aid, "init", svr_aid);

      for (std::size_t i=0; i<echo_num; ++i)
      {
        base1->send(svr_aid, "echo");
        base1->recv("echo");
      }
      base1->send(svr_aid, "end");

      base2->recv(gce::exit);
    }
    catch (std::exception& ex)
    {
      std::cerr << "test_base except: " << ex.what() << std::endl;
    }
  }
};
}
