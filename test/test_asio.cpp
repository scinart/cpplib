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
using namespace oy;

constexpr int packet_num = 10;

BOOST_AUTO_TEST_SUITE(asio_test)

/**
 * client_thread
 * send `packet_num` packet, containing binary 0, 1, 2, ...
 * read 6 characters, compare with ['H' 'e' 'l' 'l' 'o' 0]
 * sleep 2s
 * exit
 */
void client_thread(int port, boost::asio::io_service* io_service)
{
    std::this_thread::sleep_for(1s);
    Socket client_socket(*io_service);
    client_socket.connect("localhost", port);

    // client side
    // send some packet
    for(int i=0;i<packet_num;i++)
        try {
            std::stringstream ss;
            std::this_thread::sleep_for(200ms);
#ifdef SCINART_CPPLIB_DEBUG_ASIO
            ss<<"sending "<<i<<'\n';
            std::cout<<ss.str()<<std::flush;
#endif
            client_socket.write(i);
        }
        catch(...)
        {
            // currently there is no write failed.
            std::cout<<"write failed "<<i<<std::endl;
        }

    // read some packet.
    std::vector<char> hello {'H','e','l','l','o',0};
    std::vector<char> recv(6,0);
    client_socket.read(recv);
    BOOST_CHECK(recv == hello);

#ifdef SCINART_CPPLIB_DEBUG_ASIO
    std::cout<<"client stop sending\n"<<std::flush;
#endif
    // being idle for 2 seconds;
    std::this_thread::sleep_for(2s);
#ifdef SCINART_CPPLIB_DEBUG_ASIO
    std::cout<<"client exiting\n"<<std::flush;
#endif
}

/**
 * server_thread
 * receive `packet_num` packet, containing binary 0, 1, 2, ...
 * send 6 characters, compare with ['H' 'e' 'l' 'l' 'o' 0]
 * sleep 1s
 * read something, should fail due to timeout, Resource temporarily unavailable
 * sleep 2s
 * read something, should fail due to closed socket.
 * exit
 */

void server_thread(Socket* server_sock, boost::asio::ip::tcp::acceptor* acceptor)
{
    acceptor->accept(*(server_sock->get_sock_ptr()));
    int value;
    for(int i=0;i<packet_num;i++) {
        server_sock->read(value);
        BOOST_CHECK(value==i);
#ifdef SCINART_CPPLIB_DEBUG_ASIO
        std::stringstream ss;
        ss<<i<< " read\n";
        std::cout<<ss.str()<<std::flush;
#endif
    }
    server_sock->write("Hello", 6);
    try {
        server_sock->read(value);
        BOOST_CHECK_MESSAGE(false, "read should not have succeed");
    } catch(boost::system::system_error& e) {
#ifdef SCINART_CPPLIB_DEBUG_ASIO
        std::cout<<e.what()<<std::endl;
#endif
        BOOST_CHECK(e.code() == boost::system::errc::resource_unavailable_try_again);
    }
    std::this_thread::sleep_for(2s);
    try {
        server_sock->read(value);
        BOOST_CHECK_MESSAGE(false, "read should not have succeed");
    } catch(boost::system::system_error& e) {
#ifdef SCINART_CPPLIB_DEBUG_ASIO
        std::cout<<e.what()<<std::endl;
#endif
        BOOST_CHECK(e.code() == boost::asio::error::eof);
    }
}


BOOST_AUTO_TEST_CASE(asio_socket)
{
    int port = 0;
    boost::asio::io_service io_service;
    auto io_service_work = std::make_unique<boost::asio::io_service::work>(io_service);
    auto io_service_run = std::async(std::launch::async, [&io_service](){io_service.run();});
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);
    boost::asio::ip::tcp::acceptor acceptor(io_service, endpoint);
    acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    port = acceptor.local_endpoint().port();

    auto server_sock = Socket(std::make_unique<boost::asio::ip::tcp::socket>(io_service));

    auto client_thread_future = std::async(std::launch::async, client_thread, port, &io_service);
    auto server_thread_future = std::async(std::launch::async, server_thread, &server_sock, &acceptor);

    client_thread_future.get();
    server_thread_future.get();
    io_service_work.reset(nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
