#include "asio.hpp"
#include <vector>
#include <thread>
#include <future>
#include <chrono>
#include <iostream>
#include <sstream>

#include <boost/test/unit_test.hpp>
#include <boost/timer/timer.hpp>

using namespace std::chrono_literals;

BOOST_AUTO_TEST_SUITE(asio_test)

BOOST_AUTO_TEST_CASE(asio_socket)
{
    boost::timer::auto_cpu_timer timer;
    int port = 19844;
    boost::asio::io_service io_service;
    auto io_service_work = std::make_unique<boost::asio::io_service::work>(io_service);
    auto io_service_run = std::async(std::launch::async, [&io_service](){io_service.run();});
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);
    boost::asio::ip::tcp::acceptor acceptor(io_service, endpoint);
    boost::asio::socket_base::reuse_address option(true);
    boost::system::error_code ec;
    acceptor.set_option(option, ec);
    BOOST_CHECK(!ec);
    auto server_sock = Socket(std::make_unique<boost::asio::ip::tcp::socket>(io_service));
    auto client_thread = [port](boost::asio::io_service* io_service){
        std::this_thread::sleep_for(1s);
        Socket client_socket(*io_service);
        client_socket.connect("localhost", port);
        for(int i=0;i<10;i++)
            try{
                std::stringstream ss;
                ss<<"sending "<<i<<'\n';
                std::this_thread::sleep_for(200ms);
#ifdef SCINART_CPPLIB_DEBUG
                std::cout<<ss.str()<<std::flush;
#endif
                client_socket.write(i);
            }
            catch(...)
            {
                std::cout<<"write failed "<<i<<std::endl;
            }
        std::vector<char> hello {'H','e','l','l','o',0};
        std::vector<char> recv(6,0);
        client_socket.read(recv);
        BOOST_CHECK(recv == hello);
        std::this_thread::sleep_for(2s);
#ifdef SCINART_CPPLIB_DEBUG
                std::cout<<"client exiting\n"<<std::flush;
#endif
    };
    auto client_thread_future = std::async(std::launch::async, client_thread, &io_service);
    acceptor.accept(*server_sock.get_sock_ptr());
    for(int i=0;i<10;i++)
    {
        int value=0;
        server_sock.read(value);
        std::stringstream ss;
        ss<<i<< " read\n";
#ifdef SCINART_CPPLIB_DEBUG
        std::cout<<ss.str()<<std::flush;
#endif
        BOOST_CHECK(value==i);
    }
    server_sock.write("Hello", 6);
    int value;
    try
    {
        std::cout<<"11111111 "<<std::endl;
        server_sock.read(value);
        std::cout<<"22222222 "<<std::endl;
    }
    catch(std::runtime_error& e)
    {
        std::cout<<e.what()<<std::endl;
    }
    // std::this_thread::sleep_for(1s);
    client_thread_future.get();
    io_service_work.reset(nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
