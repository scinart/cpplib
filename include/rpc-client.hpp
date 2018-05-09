#pragma once

#include "asio.hpp"
#include "concurrentqueue.hpp"
#include "thread_pool.hpp"
#include "trivial_copyable_tuple.hpp"
#include "param.hpp"

#include <functional>
#include <iostream>
#include <map>
#include <atomic>
#include <memory>

#include <cassert>

// #define DEBUG_SLOW_CALL

namespace oy
{

namespace rpc
{
class Client
{
    using deadline_timer_t = boost::asio::basic_waitable_timer<std::chrono::steady_clock>;
    using callback_t = std::function<void(const boost::system::system_error&, nlohmann::json j)>;
    using tuple_t = augs::trivially_copyable_tuple<bool,unsigned long long, callback_t*, deadline_timer_t*>;
    template <typename ... Args>
    decltype(auto) make_tuple_t(Args... args){ return augs::trivially_copyable_tuple<Args...>(args...); }

    static constexpr bool OCCUPIED = true;
    static constexpr bool AVAILABLE = false;
    template <typename T>
    class DecAtomicOnDistruct
    {
    public:
        DecAtomicOnDistruct(std::atomic<T>& a_):a(a_){}
        ~DecAtomicOnDistruct(){--a;}
    private:
        std::atomic<T> & a;
    };
    enum class Status { CONNECTING, READY, TRANSIENT_FAILURE, IDLE, SHUTDOWN };
    std::atomic<Status> status;
    static constexpr int HANDLE_POOL_SIZE = 1000;

    class ReturnHelper
    {
        friend class Client;
    public:
        template <typename T>
        T as() {
            if(j.is_object() && j.count("result"))
                try {
                    T t = j["result"];
                    return t;
                } catch (const nlohmann::json::exception& e) {
                    std::cout << "json is " << j.dump() << std::endl;
                    return T();
                }
            else
            {
                std::cout << "json is " << j.dump() << std::endl;
                return T();
            }
        }
    private:
        template<typename JSON>
        ReturnHelper(JSON&& j_):j(std::forward<JSON>(j_)){}
        nlohmann::json j;
    };
public:
    Client(std::string ip_, unsigned short port_):
        io_service_work(std::make_unique<boost::asio::io_service::work>(io_service)),
        socket(io_service),
        ip(ip_),
        port(port_),
        pool_vacancy(HANDLE_POOL_SIZE),
        has_pending_callback(0)
    {
        std::cout << '^' << std::flush;
        async_run(1);
        status = Status::CONNECTING;
        if(!socket.connect_b(ip,port))
        {
            std::cout << '#' << std::flush;
        }
        else
        {
            socket.set_option(boost::asio::socket_base::linger(true,0)); // avoid TIME_WAIT
            has_pending_callback++;
            boost::asio::async_read(*socket.get_sock_ptr(), boost::asio::buffer(head_buffer),
                                    [this](const boost::system::system_error& e, size_t sz) { this->boost_callback_1(e,sz); });
            status = Status::READY;
            std::cout << '$' << std::flush;
        }
    }
    ~Client() {
        // std::cout << "Destructor Called" << std::endl;
        socket.shutdown();
        socket.cancel();
        while(has_pending_callback)
        {
            std::cout<<'?'<<std::flush;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        io_service_work.reset(nullptr);
    }
    void async_run(unsigned int n) {
        while(n--)
            async_io_service_run_thread.emplace_back(std::async(std::launch::async,[this](){this->io_service.run();}));
    }
    template <typename Duration> void set_connection_timeout(Duration&& t) { socket.set_connection_timeout(t); }
    template <typename Duration> void set_read_timeout(Duration&& t) { socket.set_read_timeout(t); }

    void boost_callback_1(const boost::system::system_error& e, size_t) {
        std::cout << '1' << std::flush;
        DecAtomicOnDistruct<decltype(has_pending_callback.load())> d(has_pending_callback);
        if(e.code())
            shutdown(e);
        else {
            size_t sz = *reinterpret_cast<size_t*>(&head_buffer[MAGIC_HEADER_SIZE]);
            read_buffer.resize(sz);
            has_pending_callback++;
            boost::asio::async_read(*socket.get_sock_ptr(), boost::asio::buffer(read_buffer),
                                    [this](const boost::system::system_error& e, size_t sz) { this->boost_callback_2(e, sz); });
        }
    }
    void boost_callback_2(const boost::system::system_error& e, size_t) {
        std::cout << '2' << std::flush;
        DecAtomicOnDistruct<decltype(has_pending_callback.load())> d(has_pending_callback);
        if(e.code())
            shutdown(e);
        else {
            unsigned long long local_id;
            nlohmann::json j;
            try {
                j = nlohmann::json::from_cbor(read_buffer);
                local_id = j["id"];
            } catch (const nlohmann::json::exception& e) {
                return shutdown(boost::system::system_error(boost::asio::error::fault, "server response no id"));
            }
            tuple_t expected = handle_pool[local_id % HANDLE_POOL_SIZE].load();
            if (!occupied(expected))
            {
                // maybe canceled;
            }
            else if (handle_pool[local_id % HANDLE_POOL_SIZE].compare_exchange_strong(expected, empty_tuple()))
                clean_tuple(expected, boost::system::system_error(boost::system::error_code()), j);

            has_pending_callback++;
            boost::asio::async_read(*socket.get_sock_ptr(), boost::asio::buffer(head_buffer),
                                    [this](const boost::system::system_error& e, size_t sz) { this->boost_callback_1(e,sz); });
        }
    }

    template <typename ...Args>
    ReturnHelper call(std::string name, Args&& ... args)
    {
        std::chrono::seconds* p = nullptr;
        return call_with_timeout(name, p, std::forward<Args>(args)...);
    }
    template <typename Duration, typename ...Args>
    ReturnHelper call_with_timeout(std::string name, Duration d, Args&& ... args)
    {
        return call_with_timeout(name, &d, std::forward<Args>(args)...);
    }

    // return true if function is registered; registered functions are guaranteed to be called-back.
    template <typename Functor, typename ...Args>
    bool callback(std::string name, Functor f, Args... args) {
        std::chrono::seconds* p = nullptr;
        return callback_with_timeout(std::move(name), p, std::forward<Functor>(f), std::forward<Args>(args)...);
    }
    template <typename Duration, typename Functor, typename ...Args>
    bool callback_with_timeout(std::string name, Duration d, Functor&& f, Args&& ... args) {
        return callback_with_timeout(name, &d, f, std::forward<Args>(args)...);
    }
private:
    template <typename Duration, typename ...Args>
    ReturnHelper call_with_timeout(std::string name, Duration *d, Args&& ... args)
    {
        Semaphore sem;
        nlohmann::json ret;
        if(!callback_with_timeout(name, d, [&sem, &ret](const boost::system::system_error& e, nlohmann::json j){
                    if(e.code())
                        ret["boost error"] = e.what();
                    else
                        ret = j;
                    sem.notify();
                }, std::forward<Args>(args)...))
            return ReturnHelper({"call back failed"});
        else
            sem.wait();
        return ReturnHelper(ret);
    }

    template <typename Duration, typename Functor, typename ...Args>
    bool callback_with_timeout(std::string name, Duration* d, Functor&& f, Args&& ... args) {
    // return true if function is registered; registered functions are guaranteed to be called-back.
    // template <typename Functor, typename ...Args>
    // bool callback(std::string name, Functor f, Args... args) {
        static_assert(std::is_same<typename func_traits<Functor>::args_type, std::tuple<boost::system::system_error, nlohmann::json> >::value,
                      "Callback function must of type void (boost::system::system_error, nlohmann::json)");
        static_assert(std::is_same<typename func_traits<Functor>::result_type, void>::value,
                      "Callback function must of type void (boost::system::system_error, nlohmann::json)");

        if (status.load() != Status::READY)
        {
            f(boost::system::system_error(boost::asio::error::bad_descriptor, "TODO: "), {});
            return true;
        }
        auto previous = pool_vacancy.load();
        while(true) {
            if (previous == 0)
                return false;
            if (pool_vacancy.compare_exchange_weak(previous, previous - 1))
                break;
        }

        auto local_id = id;
        std::unique_ptr<deadline_timer_t> timer;
        if(d)
            timer.reset(new deadline_timer_t(io_service));
        std::unique_ptr<callback_t> pf(new callback_t(f));
        while(true) {
            local_id = id++;
            auto expected = handle_pool[local_id % HANDLE_POOL_SIZE].load();
            if (occupied(expected))
                continue;
            auto desired = make_tuple_t(OCCUPIED, local_id, pf.get(), timer.get());
            if (handle_pool[local_id % HANDLE_POOL_SIZE].compare_exchange_strong(expected, desired)) {
                if(d) {
                    timer->expires_from_now(*d);
                    timer->async_wait([local_id, this](const boost::system::system_error& e){
                            assert (e.code() == boost::asio::error::timed_out ||
                                    e.code() == boost::asio::error::operation_aborted);
                            if(e.code() == boost::asio::error::timed_out)
                                this->clean_up(local_id, boost::system::system_error(boost::asio::error::timed_out, "TODO: "), {});});
                }
                pf.release();
                timer.release();
                break;
            }
        }
        if (status.load() != Status::READY)
        {
            clean_up(local_id, boost::system::system_error(boost::asio::error::bad_descriptor, "TODO: "), {});
            return true;
        }

        nlohmann::json j;
        j["func"] = name;
        j["id"] = local_id;
        j["args"] = serialize(std::forward<Args>(args)...);

#ifdef DEBUG_SLOW_CALL
        socket.write(vector<uint8_t>(MAGIC_HEADER_SIZE));
        socket.write(s.size());
        std::this_thread::sleep_for(call_sleep);
        socket.write(s);
#else
        std::vector<uint8_t> buf(sizeof(size_t) + MAGIC_HEADER_SIZE);
        auto old_size = buf.size();
        nlohmann::json::to_cbor(j, buf);
        *(reinterpret_cast<size_t*>(&buf[MAGIC_HEADER_SIZE])) = buf.size()-old_size;

        std::cout << '+' << std::flush;
        boost::asio::async_write(*socket.get_sock_ptr(), boost::asio::buffer(buf),
                                 [this, local_id](const boost::system::system_error& e, size_t) {
                                     std::cout << '-' << std::flush;
                                     if(e.code())
                                     {
                                         std::cout<<'o'<<std::flush;
                                         this->clean_up(local_id,e,{});
                                     }});

#endif
        return true;
    }
    bool occupied(tuple_t& t)
    {
        return std::get<0>(t)==true;
    }
    void clean_tuple(tuple_t& t, const boost::system::system_error& e, nlohmann::json j)
    {
        std::unique_ptr<std::remove_reference_t<decltype(*std::get<2>(t))> > pf   (std::get<2>(t));
        std::unique_ptr<std::remove_reference_t<decltype(*std::get<3>(t))> > timer(std::get<3>(t));
        if (pf) (*pf)(e, j);
        if (timer) timer->cancel();
        pool_vacancy++;
    }
    void shutdown(const boost::system::system_error& e)
    {
        while(status != Status::SHUTDOWN)
        {
            auto old_status = status.load();
            if (old_status == Status::SHUTDOWN)
                return;
            if(status.compare_exchange_strong(old_status, Status::SHUTDOWN))
            {
                auto local_id = id;
                while(pool_vacancy != HANDLE_POOL_SIZE) {
                    local_id = (local_id+HANDLE_POOL_SIZE-1) % HANDLE_POOL_SIZE;
                    auto expected = handle_pool[local_id].load();
                    if (!occupied(expected))
                        continue;
                    if (handle_pool[local_id].compare_exchange_strong(expected, empty_tuple()))
                        clean_tuple(expected, e, {});
                }
            }
        }
    }

    // this function is called from multiple processes.
    // if handle_pool[local_id % HANDLE_POOL_SIZE] is actually local_id;
    //     clean it
    void clean_up(unsigned long long local_id, const boost::system::system_error& e, nlohmann::json j)
    {
        auto previous = handle_pool[local_id % HANDLE_POOL_SIZE].load();
        if (!occupied(previous) || std::get<1>(previous) != local_id)
            return;
        if (handle_pool[local_id % HANDLE_POOL_SIZE].compare_exchange_strong(previous, empty_tuple()))
            clean_tuple(previous, e, j);
    }
    tuple_t empty_tuple()
    {
        //auto static t = make_tuple_t(AVAILABLE, 0ull, nullptr, nullptr); // not working.
        auto static t = make_tuple_t(AVAILABLE, 0ull, static_cast<callback_t*>(nullptr), static_cast<deadline_timer_t*>(nullptr));
        return t;
    }
private:
    boost::asio::io_service io_service;
    std::unique_ptr<boost::asio::io_service::work> io_service_work;
    oy::Socket socket;
    std::string ip;
    unsigned short port;
    std::vector<std::future<void> > async_io_service_run_thread;
    unsigned long long id = 0;
    std::atomic<unsigned int> pool_vacancy; // roughly vacancy in handle_pool, may be less than real value.
    constexpr static int MAGIC_HEADER_SIZE = 8;
    std::array<uint8_t, sizeof(size_t) + MAGIC_HEADER_SIZE> head_buffer;
    std::vector<uint8_t> read_buffer;
    std::array<std::atomic<tuple_t>, HANDLE_POOL_SIZE> handle_pool;
    std::atomic<int> has_pending_callback;
    std::chrono::milliseconds connection_timeout = std::chrono::milliseconds(1000);
    std::chrono::milliseconds read_timeout = std::chrono::milliseconds(1000);
#ifdef DEBUG_SLOW_CALL
    std::chrono::milliseconds call_sleep = std::chrono::milliseconds(2000);
#endif
};

}

}