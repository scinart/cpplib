#ifndef _GITHUB_SCINART_CPPLIB_ASIO_HPP_
#define _GITHUB_SCINART_CPPLIB_ASIO_HPP_

#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <string>
#include <type_traits>
#include <memory>
#include <chrono>
#include <future>

#include "semaphore.hpp"

namespace oy
{

// https://stackoverflow.com/questions/27687389/how-does-void-t-work
// https://www.v2ex.com/t/387904#reply12

#if __GNUC__ <= 4
template <typename...  > struct __make_void { using type = void; };
// putting a T here \\_// bypass a gcc4.9 bug.
//                   |||
template <typename... T> using __my_void_t = typename __make_void<T...>::type;
#else
template<typename...> using __my_void_t = void;
#endif

template <typename T, typename=void> struct is_container : std::false_type {};
template <typename T>                struct is_container <T, __my_void_t< typename T::value_type > > : std::true_type {};

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

template <typename Transport = boost::asio::ip::tcp>
class BasicSocket
{
    static_assert(std::is_same<Transport, boost::asio::ip::tcp>::value, "only tcp supported");
    using sock_ptr = std::unique_ptr<typename Transport::socket>;
    boost::asio::io_service & io_service;
    sock_ptr sock;
    boost::system::error_code ec;
    std::chrono::milliseconds ctimeout = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(1));
    std::chrono::milliseconds rtimeout = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(1));
    // std::chrono::milliseconds wtimeout = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(1));
public:
    BasicSocket(boost::asio::io_service& io_service_):io_service(io_service_){}
    BasicSocket(BasicSocket&& rhs) = default;
    BasicSocket& operator=(BasicSocket&& rhs) = default;
    BasicSocket(sock_ptr&& ptr):io_service(ptr->get_io_service()), sock(std::move(ptr)){}
    ~BasicSocket(){ if(sock) sock->shutdown(Transport::socket::shutdown_both, ec); }
    template<typename SettableSocketOption> auto set_option(const SettableSocketOption & option){ return sock->set_option(option); }
    void connect(std::string ip, int port)
    {
        using namespace boost::asio::ip;
        sock = std::make_unique<typename Transport::socket>(io_service);
        typename Transport::resolver resolver(io_service);
        typename Transport::resolver::query query(Transport::v4(), ip, std::to_string(port));
        typename Transport::resolver::iterator iterator = resolver.resolve(query);
        Semaphore r_sem;
        boost::system::error_code r_ec;
        boost::asio::async_connect(*sock, iterator,
                                   [this, &r_ec, &r_sem](const boost::system::error_code& ec_, typename Transport::resolver::iterator iter) {
                                       (void)iter;
                                       r_ec=ec_;
                                       r_sem.notify();
                                });
        if(!r_sem.wait_for(ctimeout))
        {
            // http://www.boost.org/doc/libs/1_66_0/doc/html/boost_asio/reference/basic_socket/cancel/overload1.html
            sock->close(); // Use the close() function to simultaneously cancel the outstanding operations and close the socket.
            r_sem.wait(); // The handlers for cancelled operations will be passed the boost::asio::error::operation_aborted error.
            throw boost::system::system_error(boost::asio::error::connection_aborted);
        }
        else if(r_ec)
            throw boost::system::system_error(r_ec);
    }
    void set_socket(sock_ptr&& ptr){sock = std::move(ptr);}
    sock_ptr& get_sock_ptr(){return sock;}
    template<typename Duration> void set_read_timeout (Duration d){rtimeout = std::chrono::duration_cast<decltype(rtimeout)>(d);}
    template<typename Duration> void set_connection_timeout (Duration d){ctimeout = std::chrono::duration_cast<decltype(ctimeout)>(d);}
    // template<typename Duration> void set_write_timeout(Duration d){wtimeout = std::chrono::duration_cast<decltype(wtimeout)>(d);}

    template<typename Container> std::enable_if_t<is_container<std::remove_reference_t<Container>>::value, void> write (Container&& t) { return write(t, t.size()); }
    template<typename Container> std::enable_if_t<is_container<std::remove_reference_t<Container>>::value, void> write (Container&& t, size_t sz) {
        auto buffer = boost::asio::buffer(t, sz * sizeof(typename std::remove_reference_t<Container>::value_type));
        write_(buffer);
    }

    template<typename T> std::enable_if_t<!is_container<T>::value, void> write (const T& t) { return write(&t, 1); }
    template<typename T> std::enable_if_t<!is_container<T>::value, void> write (T* p, size_t nmemb) {
        auto buffer = boost::asio::buffer(p, nmemb * sizeof(T));
        write_(buffer);
    }

    template<typename T> T read () {
        T t;
        read(t);
        return t;
    }

    template<typename T> std::vector<T> read (size_t n) {
        std::vector<T> v(n);
        read(v);
        return v;
    }

    template<typename Container> std::enable_if_t<is_container<std::remove_reference_t<Container>>::value, void> read (Container& t) { return read(t, t.size()); }
    template<typename Container> std::enable_if_t<is_container<std::remove_reference_t<Container>>::value, void> read (Container& t, size_t sz) {
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
        s.async_receive(buffer, [this, &r_ec, &r_sem](const boost::system::error_code& ec_, size_t) { r_ec=ec_; r_sem.notify(); });
        if(!r_sem.wait_for(rtimeout))
        {
            s.cancel(); // This function causes all outstanding asynchronous connect, send and receive operations to finish immediately,
            r_sem.wait(); // and the handlers for cancelled operations will be passed the boost::asio::error::operation_aborted error.
            throw boost::system::system_error(boost::asio::error::try_again);
        }
        else if(r_ec)
            throw boost::system::system_error(r_ec);
    }
    template <typename MutableBufferSequence>
    void write_(const MutableBufferSequence& buffer) {
        sock->send(buffer);
    }
};

using Socket = BasicSocket<>;

template <typename Transport = boost::asio::ip::tcp>
class BasicSyncBoostIO
{
public:
    BasicSyncBoostIO(){}
    BasicSyncBoostIO(boost::asio::io_service& io_service) { init(io_service); }
    boost::asio::io_service* p_io_service = nullptr;
    void init(boost::asio::io_service& io_service)
    {
        if(p_io_service != nullptr)
            throw std::runtime_error("Init twice is probably an error.");
        p_io_service = &io_service;
    }

    /**
     * listen to ipv4 INADDR_ANY:$port
     * on success, return a newly created sockfd.
     * on error, a negative number is returned.
     */
    void listen(unsigned short& port)
    {
        assert(p_io_service && "io_service is nullptr");
        boost::asio::io_service& io_service = *p_io_service;
        typename Transport::endpoint endpoint(Transport::v4(), port);
        p_acceptor = std::make_unique<typename Transport::acceptor>(io_service, endpoint);
        p_acceptor->set_option(typename Transport::acceptor::reuse_address(true));
        port = p_acceptor->local_endpoint().port();
    }

    BasicSocket<Transport> accept()
    {
        assert(p_io_service && "must init before listen.");
        assert(p_acceptor && "must listen before accept");
        auto server_sock = BasicSocket<Transport>(std::make_unique<typename Transport::socket>(*p_io_service));
        p_acceptor->accept(*(server_sock.get_sock_ptr()));
        return server_sock;
    }

    BasicSocket<Transport> connect(const std::string& ip, int port)
    {
        assert(p_io_service && "io_service is nullptr");
        BasicSocket<Transport> client_socket(*p_io_service);
        client_socket.connect(ip, port);
        return client_socket;
    }
private:
    std::unique_ptr<typename Transport::acceptor> p_acceptor;
};

using SyncBoostIO = BasicSyncBoostIO<>;

}

#endif
