#ifndef _GITHUB_SCINART_CPPLIB_ASIO_HPP_
#define _GITHUB_SCINART_CPPLIB_ASIO_HPP_

#include "type_trait.hpp"
#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <string>
#include <type_traits>
#include <memory>
#include <chrono>
#include <iostream>
#include <sys/ioctl.h>

#include "semaphore.hpp"

struct SocketException: public std::runtime_error
{
    // SocketException(std::string const& message, int done, int intended)
    //     : std::runtime_error(message + "[" + std::to_string(done) + "/" + std::to_string(intended) + "]"){}
    // SocketException(std::string const& message, int ret)
    //     : std::runtime_error(message + "[" + std::to_string(ret) + "]"){}
    SocketException(std::string const& message)
        : std::runtime_error(message){}
};


void ttttttt(const boost::system::error_code& ec)
{
}

class Socket
{
    bool r_timeout = false;
    boost::system::error_code r_ec;
public:
    using sock_ptr = std::unique_ptr<boost::asio::ip::tcp::socket>;

    Socket(boost::asio::io_service& io_service_):io_service(io_service_){}
    Socket(Socket&& rhs) = default;
    Socket(sock_ptr&& ptr):io_service(ptr->get_io_service()), sock(std::move(ptr)){}

    template <typename SyncStream, typename MutableBufferSequence>
    void read_(SyncStream& s, const MutableBufferSequence& buffer)
    {
        Semaphore r_sem;
        boost::asio::async_read(s,buffer,
                                [this, &r_sem](const boost::system::error_code& ec_, std::size_t sz) {
                                    this->r_ec=ec_;
                                    r_sem.notify();
                                });
        //boost::asio::deadline_timer deadline_(s.get_io_service());
        //deadline_.async_wait([this](){});
        r_sem.wait();
        if(ec)
            throw SocketException(ec.message());
        //boost::asio::read(s, buffer);
    }
    template <typename SyncStream, typename MutableBufferSequence>
    void write_(SyncStream& s, const MutableBufferSequence& buffer)
    {
        boost::asio::write(s, buffer);
    }


    void connect(std::string ip, int port)
    {
        sock = std::make_unique<boost::asio::ip::tcp::socket>(io_service);
        boost::asio::ip::tcp::resolver resolver(io_service);
        boost::asio::ip::tcp::resolver::query query(boost::asio::ip::tcp::v4(), ip, std::to_string(port));
        boost::asio::ip::tcp::resolver::iterator iterator = resolver.resolve(query);
        boost::asio::connect(*sock, iterator);
    }

    void set_socket(sock_ptr&& ptr){sock = std::move(ptr);}

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

    sock_ptr& get_sock_ptr(){return sock;}

    template<typename Duration> void set_read_timeout (Duration d){rtimeout = std::chrono::duration_cast<decltype(rtimeout)>(d);}
    template<typename Duration> void set_write_timeout(Duration d){wtimeout = std::chrono::duration_cast<decltype(rtimeout)>(d);}
private:
    boost::asio::io_service & io_service;
    sock_ptr sock;
    boost::system::error_code ec;
    std::chrono::milliseconds rtimeout = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(1));
    std::chrono::milliseconds wtimeout = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(1));
};

#endif