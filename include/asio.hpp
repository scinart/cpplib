#ifndef _GITHUB_SCINART_CPPLIB_ASIO_HPP_
#define _GITHUB_SCINART_CPPLIB_ASIO_HPP_

#include "type_trait.hpp"
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
    ~Socket(){ if(sock) sock->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec); }
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
            throw boost::system::system_error(boost::asio::error::try_again);
        }
        else if(r_ec)
            throw boost::system::system_error(r_ec);
        //boost::asio::read(s, buffer);
    }
    template <typename SyncStream, typename MutableBufferSequence>
    void write_(SyncStream& s, const MutableBufferSequence& buffer)
    {
        boost::asio::write(s, buffer);
    }
};

class SyncBoostIO
{
public:
    SyncBoostIO(){}
    SyncBoostIO(boost::asio::io_service& io_service) { init(io_service); }
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
    void listen(int& port)
    {
        assert(p_io_service && "io_service is nullptr");
        boost::asio::io_service& io_service = *p_io_service;
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);
        p_acceptor = std::make_unique<boost::asio::ip::tcp::acceptor>(io_service, endpoint);
        p_acceptor->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        port = p_acceptor->local_endpoint().port();
    }

    Socket accept()
    {
        assert(p_io_service && "must init before listen.");
        assert(p_acceptor && "must listen before accept");
        auto server_sock = Socket(std::make_unique<boost::asio::ip::tcp::socket>(*p_io_service));
        p_acceptor->accept(*(server_sock.get_sock_ptr()));
        return server_sock;
    }

    Socket connect(const std::string& ip, int port)
    {
        assert(p_io_service && "io_service is nullptr");
        Socket client_socket(*p_io_service);
        client_socket.connect(ip, port);
        return client_socket;
    }
private:
    std::unique_ptr<boost::asio::ip::tcp::acceptor> p_acceptor;
};

}

#endif
