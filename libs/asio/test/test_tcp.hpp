///
/// Copyright (c) 2009-2015 Nous Xiong (348944179 at qq dot com)
///
/// Distributed under the Boost Software License, Version 1.0. (See accompanying
/// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///
/// See https://github.com/nousxiong/gce for latest version.
///

#include <gce/amsg/all.hpp>
#include <boost/make_shared.hpp>

namespace gce
{
namespace asio
{
struct echo_header
{
  uint32_t size_;
};
}
}

AMSG(gce::asio::echo_header, (v.size_&sfix));

namespace gce
{
namespace asio
{
class tcp_ut
{
typedef std::basic_string<byte_t, std::char_traits<byte_t>, std::allocator<byte_t> > bytes_t;
typedef boost::asio::ip::tcp::socket tcp_socket_t;
typedef boost::asio::ip::tcp::resolver tcp_resolver_t;
public:
  static void run()
  {
    std::cout << "tcp_ut begin." << std::endl;
    for (std::size_t i=0; i<test_count; ++i)
    {
      test_base();
      if (test_count > 1) std::cout << "\r" << i;
    }
    if (test_count > 1) std::cout << std::endl;
    std::cout << "tcp_ut end." << std::endl;
  }

private:
  static void echo_client(stackful_actor self)
  {
    log::logger_t& lg = self.get_context().get_logger();
    try
    {
      size_t ecount = 10;
      errcode_t ec;

      boost::shared_ptr<tcp_resolver_t::iterator> eitr;
      self->match("init").recv(eitr);

      tcp::socket skt(self);
      skt.async_connect(*eitr);
      self->match(tcp::as_conn).recv(ec);
      GCE_VERIFY(!ec).except(ec);

      echo_header hdr;
      size_t hdr_len = amsg::size_of(hdr);
      amsg::zero_copy_buffer zbuf;

      char buff[256];
      std::string str("hello world!");
      hdr.size_ = amsg::size_of(str);

      zbuf.set_write(buff, 256);
      amsg::write(zbuf, hdr);
      amsg::write(zbuf, str);

      for (size_t i=0; i<ecount; ++i)
      {
        skt.async_write(boost::asio::buffer(buff, hdr_len + hdr.size_));
        self->match(tcp::as_send).recv(ec);
        GCE_VERIFY(!ec).except(ec);

        skt.async_read(hdr_len + hdr.size_);
        message::chunk ch(hdr_len + hdr.size_);
        self->match(tcp::as_recv).recv(ec, ch);

        zbuf.set_read(ch.data(), hdr_len + hdr.size_);
        std::string echo_str;
        amsg::read(zbuf, hdr);
        amsg::read(zbuf, echo_str);
        if (zbuf.bad())
        {
          break;
        }
        GCE_VERIFY(str == echo_str);
      }

      str.assign("bye");
      hdr.size_ = amsg::size_of(str);

      zbuf.set_write(buff, 256);
      amsg::write(zbuf, hdr);
      amsg::write(zbuf, str);
      skt.async_write(boost::asio::buffer(buff, hdr_len + hdr.size_));
      self->match(tcp::as_send).recv(ec);
      GCE_VERIFY(!ec).except(ec);
    }
    catch (std::exception& ex)
    {
      GCE_ERROR(lg) << ex.what();
    }
  }
  
  static void echo_session(stackful_actor self)
  {
    log::logger_t& lg = self.get_context().get_logger();
    try
    {
      boost::shared_ptr<tcp_socket_t> tcp_skt;
      self->match("init").recv(tcp_skt);

      tcp::socket skt(self, tcp_skt);

      amsg::zero_copy_buffer zbuf;
      byte_t read_buff[256];
      std::deque<bytes_t> write_queue;

      echo_header hdr;
      size_t const hdr_len = amsg::size_of(hdr);
      match_t const recv_header = atom("header");
      match_t const recv_body = atom("body");

      skt.async_read(boost::asio::buffer(read_buff, hdr_len), message(recv_header));
      while (true)
      {
        match_t type;
        errcode_t ec;
        self->match(recv_header, recv_body, tcp::as_send, type).recv(ec);
        GCE_VERIFY(!ec).except(ec);

        if (type == recv_header)
        {
          zbuf.set_read(read_buff, hdr_len);
          amsg::read(zbuf, hdr);
          if (zbuf.bad())
          {
            break;
          }

          skt.async_read(boost::asio::buffer(read_buff + hdr_len, hdr.size_), message(recv_body));
        }
        else if (type == recv_body)
        {
          zbuf.set_read(read_buff + hdr_len, hdr.size_);
          std::string str;
          amsg::read(zbuf, str);
          if (zbuf.bad())
          {
            break;
          }

          //GCE_INFO(lg) << "server recved echo: " << str;
          if (str == "bye")
          {
            break;
          }

          bool write_in_progress = !write_queue.empty();
          write_queue.push_back(bytes_t(read_buff, hdr_len + hdr.size_));
          if (!write_in_progress)
          {
            bytes_t const& echo = write_queue.front();
            skt.async_write(boost::asio::buffer(echo.data(), echo.size()));
          }
          
          skt.async_read(boost::asio::buffer(read_buff, hdr_len), message(recv_header));
        }
        else
        {
          write_queue.pop_front();
          if (!write_queue.empty())
          {
            bytes_t const& echo = write_queue.front();
            skt.async_write(boost::asio::buffer(echo.data(), echo.size()));
          }
        }
      }
    }
    catch (std::exception& ex)
    {
      GCE_ERROR(lg) << ex.what();
    }
  }

  static void echo_server(stackful_actor self)
  {
    context& ctx = self.get_context();
    log::logger_t& lg = ctx.get_logger();
    try
    {
      aid_t sender = self->recv("init");

      size_t scount = 0;
      errcode_t ec;

      tcp::resolver rsv(self);
      tcp_resolver_t::query qry("0.0.0.0", "23333");
      rsv.async_resolve(qry);
      boost::shared_ptr<tcp_resolver_t::iterator> eitr;
      self->match(tcp::as_resolve).recv(ec, eitr);
      GCE_VERIFY(!ec).except(ec);

      tcp::acceptor acpr(self);
      boost::asio::ip::tcp::endpoint ep = **eitr;

      acpr->open(ep.protocol());

      acpr->set_option(boost::asio::socket_base::reuse_address(true));
      acpr->bind(ep);

      acpr->set_option(boost::asio::socket_base::receive_buffer_size(640000));
      acpr->set_option(boost::asio::socket_base::send_buffer_size(640000));

      acpr->listen(boost::asio::socket_base::max_connections);

      acpr->set_option(boost::asio::ip::tcp::no_delay(true));
      acpr->set_option(boost::asio::socket_base::keep_alive(true));
      acpr->set_option(boost::asio::socket_base::enable_connection_aborted(true));

      self->send(sender, "ready");

      while (true)
      {
        boost::shared_ptr<tcp_socket_t> skt = 
          boost::make_shared<tcp_socket_t>(boost::ref(ctx.get_io_service()));
        acpr.async_accept(*skt);

        match_t type;
        errcode_t ec;
        message msg;
        self->match(tcp::as_accept, "end", type).raw(msg).recv();
        if (type == atom("end"))
        {
          break;
        }

        msg >> ec;
        if (!ec)
        {
          aid_t cln = spawn(self, boost::bind(&tcp_ut::echo_session, _arg1), monitored);
          self->send(cln, "init", skt);
          ++scount;
        }
      }

      for (size_t i=0; i<scount; ++i)
      {
        self->recv(exit);
      }
    }
    catch (std::exception& ex)
    {
      GCE_ERROR(lg) << ex.what();
    }
  }

  static void test_base()
  {
    log::asio_logger lgr;
    log::logger_t lg = boost::bind(&gce::log::asio_logger::output, &lgr, _arg1, "");

    try
    {
      size_t cln_count = 100;
      errcode_t ec;
      attributes attrs;
      attrs.lg_ = lg;
      context ctx_svr(attrs);
      context ctx_cln(attrs);

      threaded_actor base_svr = spawn(ctx_svr);
      threaded_actor base_cln = spawn(ctx_cln);

      aid_t svr = spawn(base_svr, boost::bind(&tcp_ut::echo_server, _arg1), monitored);
      base_svr->send(svr, "init");
      base_svr->recv("ready");

      tcp::resolver rsv(base_cln);
      boost::asio::ip::tcp::resolver::query qry("127.0.0.1", "23333");
      rsv.async_resolve(qry);
      boost::shared_ptr<tcp_resolver_t::iterator> eitr;
      base_cln->match(tcp::as_resolve).recv(ec, eitr);
      GCE_VERIFY(!ec).except(ec);
      
      for (size_t i=0; i<cln_count; ++i)
      {
        aid_t cln = spawn(base_cln, boost::bind(&tcp_ut::echo_client, _arg1), monitored);
        base_cln->send(cln, "init", eitr);
      }

      for (size_t i=0; i<cln_count; ++i)
      {
        base_cln->recv(exit);
      }

      base_svr->send(svr, "end");
      base_svr->recv(exit);
    }
    catch (std::exception& ex)
    {
      GCE_ERROR(lg) << ex.what();
    }
  }
};
}
}
