//usr/bin/g++ -g boost_block_tcp_client_server.cpp -lboost_system -pthread -lboost_unit_test_framework -lboost_timer && ./a.out "$@"; rm a.out; exit
#include <atomic>
#include <boost/asio.hpp>
#include <boost/format.hpp>
#include <boost/optional.hpp>
#include <boost/timer/timer.hpp>
#include <condition_variable>
#include <chrono>
#include <future>
#include <iostream>
#include <thread>
#include <memory>
#include <mutex>
#include <string>

#define BOOST_TEST_MAIN
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>

namespace oy
{

template<typename...> using __my_void_t = void;

template <typename T, typename=void> struct is_container : std::false_type {};
template <typename T>                struct is_container <T, __my_void_t< typename T::value_type > > : std::true_type {};

class Semaphore {
public:
    Semaphore (int count_ = 0)
        : count(count_) {}

    inline void notify()
    {
        std::unique_lock<std::mutex> lock(mtx);
        count++;
        cv.notify_one();
    }

    inline void wait()
    {
        std::unique_lock<std::mutex> lock(mtx);

        while(count == 0){
            cv.wait(lock);
        }
        count--;
    }
    template <typename Duration>
    bool wait_for(const Duration duration)
    {
        std::unique_lock<std::mutex> lock(mtx);
        auto wait_for_it_success = cv.wait_for(lock, duration, [this]() { return count > 0; });
        return wait_for_it_success;
    }
    inline void reset()
    {
        if(mtx.try_lock())
        {
            count=0;
            mtx.unlock();
        }
        else
        {
            throw std::runtime_error("reset Semaphore failed.");
        }
    }

private:
    std::mutex mtx;
    std::condition_variable cv;
    int count;
};

/**
 * Class Socket:
 * A wrapper of sync boost socket. designed for ONE SOCKET PER THREAD!
 *
 * As you know, one socket per thread is very inefficient,
 * So this should not be used in high network IO programs.
 *
 * It is useful when one socket per thread is acceptable.
 * For example, you have a server program where computing is heavy and IO is negligible.
 * Or you are just prototyping something.
 */
class Socket
{
    using sock_ptr = std::unique_ptr<boost::asio::ip::tcp::socket>;
    boost::asio::io_service & io_service;
    sock_ptr sock;
    boost::system::error_code ec;
    std::chrono::milliseconds ctimeout = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(1));
    std::chrono::milliseconds rtimeout = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(1));
    // std::chrono::milliseconds wtimeout = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(1));
public:
    Socket(boost::asio::io_service& io_service_):io_service(io_service_){}
    Socket(Socket&& rhs) = default;
    Socket& operator=(Socket&& rhs) = default;
    Socket(sock_ptr&& ptr):io_service(ptr->get_io_service()), sock(std::move(ptr)){}

    void connect(std::string ip, int port)
    {
        using namespace boost::asio::ip;
        sock = std::make_unique<tcp::socket>(io_service);
        tcp::resolver resolver(io_service);
        tcp::resolver::query query(tcp::v4(), ip, std::to_string(port));
        tcp::resolver::iterator iterator = resolver.resolve(query);
        Semaphore r_sem;
        boost::system::error_code r_ec;
        boost::asio::async_connect(*sock, iterator,
                                   [this, &r_ec, &r_sem](const boost::system::error_code& ec_, tcp::resolver::iterator iter) {
                                       (void)iter;
                                       r_ec=ec_;
                                       r_sem.notify();
                                });
        if(!r_sem.wait_for(ctimeout))
        {
            // http://www.boost.org/doc/libs/1_66_0/doc/html/boost_asio/reference/basic_socket/cancel/overload1.html
            sock->close(); // Use the close() function to simultaneously cancel the outstanding operations and close the socket.
            r_sem.wait();
            throw boost::system::system_error(boost::asio::error::connection_aborted);
        }
        else if(r_ec)
            throw boost::system::system_error(r_ec);
    }
    void set_socket(sock_ptr&& ptr){sock = std::move(ptr);}
    sock_ptr& get_sock_ptr(){return sock;}
    template<typename Duration> void set_read_timeout (Duration d){rtimeout = std::chrono::duration_cast<decltype(rtimeout)>(d);}
    template<typename Duration> void set_connection_timeout (Duration d){ctimeout = std::chrono::duration_cast<decltype(ctimeout)>(d);}

    template<typename Container> std::enable_if_t<is_container<std::remove_reference_t<Container>>::value, void> write (Container&& t) { return write(t, t.size()); }
    template<typename Container> std::enable_if_t<is_container<std::remove_reference_t<Container>>::value, void> write (Container&& t, size_t sz) {
        auto buffer = boost::asio::buffer(t, sz * sizeof(typename std::remove_reference_t<Container>::value_type));
        write_(*sock, buffer);
    }

    template<typename T> std::enable_if_t<!is_container<T>::value, void> write (T& t) { return write(&t, 1); }
    template<typename T> std::enable_if_t<!is_container<T>::value, void> write (T* p, size_t nmemb) {
        auto buffer = boost::asio::buffer(p, nmemb * sizeof(T));
        write_(*sock, buffer);
    }


    template<typename Container> std::enable_if_t<is_container<std::remove_reference_t<Container>>::value, void> read (Container&& t) { return read(t, t.size()); }
    template<typename Container> std::enable_if_t<is_container<std::remove_reference_t<Container>>::value, void> read (Container&& t, size_t sz) {
        auto buffer = boost::asio::buffer(t, sz * sizeof(typename std::remove_reference_t<Container>::value_type));
        read_(*sock, buffer);
    }

    template<typename T> std::enable_if_t<!is_container<T>::value, void> read (T& t) { return read(&t, 1); }
    template<typename T> std::enable_if_t<!is_container<T>::value, void> read (T* p, size_t nmemb) {
        auto buffer = boost::asio::buffer(p, nmemb * sizeof(T));
        read_(*sock, buffer);
    }

private:
    template <typename SyncStream, typename MutableBufferSequence>
    void read_(SyncStream& s, const MutableBufferSequence& buffer)
    {
        Semaphore r_sem;
        boost::system::error_code r_ec;
        boost::asio::async_read(s,buffer,
                                [this, &r_ec, &r_sem](const boost::system::error_code& ec_, size_t) {
                                    r_ec=ec_;
                                    r_sem.notify();
                                });
        if(!r_sem.wait_for(rtimeout))
        {
            s.cancel();
            r_sem.wait();
            throw boost::system::system_error(boost::asio::error::try_again);
        }
        else if(r_ec)
            throw boost::system::system_error(r_ec);
    }
    template <typename SyncStream, typename MutableBufferSequence>
    void write_(SyncStream& s, const MutableBufferSequence& buffer)
    {
        boost::asio::write(s, buffer);
    }
};

}

using namespace std::chrono_literals;
using namespace oy;

constexpr int packet_num = 10;

BOOST_AUTO_TEST_SUITE(asio_test)

void client_thread(std::string ip, int port, boost::asio::io_service* io_service)
{
    std::this_thread::sleep_for(1s);
    Socket client_socket(*io_service);
    client_socket.set_connection_timeout(1s);
    client_socket.connect(ip, port);

    // client side
    // send some packet
    for(int i=0;i<packet_num;i++)
        try {
            std::cout<<(boost::format("client sending  %1%\n") % i).str()<<std::flush;
            std::this_thread::sleep_for(10ms);
            client_socket.write(i);
            std::this_thread::sleep_for(10ms);
            client_socket.read(i);
            std::cout<<(boost::format("client received %1%\n") % i).str()<<std::flush;
        }
        catch(...)
        {
            // currently there is no write failed.
            std::cout<<"write failed "<<i<<std::endl;
        }

    std::cout<<"client stop sending"<<std::endl;
    std::this_thread::sleep_for(2s);
    std::cout<<"client exiting"<<std::endl;
}

/**
 * server_thread
 * read integers from client thread, reply the integer increased by 1;
 * until some time, it should fail due to timeout, Resource temporarily unavailable
 * sleep 2s
 * read something, should fail due to closed socket.
 * exit
 */

void server_thread(Socket* server_sock, boost::asio::ip::tcp::acceptor* acceptor)
{
    server_sock->set_read_timeout(1s);
    acceptor->accept(*(server_sock->get_sock_ptr()));
    int value;
    try {
        while(true) {
            int i;
            std::this_thread::sleep_for(10ms);
            server_sock->read(i);
            std::cout<<(boost::format("server received %1%\n") % i).str()<<std::flush;
            i++;
            std::cout<<(boost::format("server sending  %1%\n") % i).str()<<std::flush;
            std::this_thread::sleep_for(10ms);
            server_sock->write(i);
        }
    } catch(boost::system::system_error& e) {
        std::cout<<"server reading error " << e.what()<<std::endl;
        BOOST_CHECK(e.code() == boost::system::errc::resource_unavailable_try_again);
        std::this_thread::sleep_for(2s);
    }
    try {
        server_sock->read(value);
    } catch(boost::system::system_error& e) {
        std::cout<<"server reading error " << e.what()<<std::endl;
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

    auto client_thread_future = std::async(std::launch::async, client_thread, acceptor.local_endpoint().address().to_string(), port, &io_service);
    auto server_thread_future = std::async(std::launch::async, server_thread, &server_sock, &acceptor);

    client_thread_future.get();
    server_thread_future.get();
    io_service_work.reset(nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
