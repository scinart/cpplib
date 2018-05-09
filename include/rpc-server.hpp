#pragma once

#include "concurrentqueue.hpp"
#include "asio.hpp"
#include "thread_pool.hpp"
#include "param.hpp"
#include <functional>
#include <iostream>
#include <map>

namespace oy
{

namespace rpc
{

class Server
{
    using QueueType = std::pair<nlohmann::json, std::shared_ptr<oy::Socket> >;
public:
    template<typename Functor>
    void bind(std::string name, Functor f) {
        function_map[name] = [f](nlohmann::json j) -> nlohmann::json {
            return call(f,j.template get<typename func_traits<Functor>::args_type>());
        };
    }

public:
    boost::asio::io_service io_service;
    oy::SyncBoostIO sbio;
    std::unique_ptr<boost::asio::io_service::work> io_service_work;
    unsigned short port;

    std::vector<std::future<void> > async_io_service_run_thread;

    // pending_connection is the max number of threads which the server accepted but not yet attach a runner to it.
    // pending_connection 是已经accept，但服务器没有线程去处理它的线程的最大个数
    Server(unsigned short port_, unsigned short n_thread, unsigned short pending_connection):
        sbio(io_service),
        io_service_work(std::make_unique<boost::asio::io_service::work>(io_service)),
        port(port_),
        thread_pool(std::function<void(oy::SyncBoostIO*)>([this](oy::SyncBoostIO* sbio){
                    try {
                        std::shared_ptr<oy::Socket> ptr_sock(new oy::Socket(sbio->accept()));
                        this->handle_call(std::move(ptr_sock));
                    } catch (const boost::system::system_error& e) {
                        if(e.code() != boost::asio::error::eof)
                            std::cout << "server side io error: " << e.what() << std::endl;
                    }}), n_thread, std::max(decltype(pending_connection)(1), pending_connection))
    {
        bind("", [](int)->int{return 0;}); // used for heartbeat.
        for(auto i=0u; i<n_thread; i++)
            exec_threads.emplace_back([this](){this->execution();});
        sbio.listen(port);
    }
    ~Server() {
        _exit = true;
        for(auto& i : exec_threads) i.join();
        io_service_work.reset(nullptr);
    }
    void async_run(unsigned int n) {
        while(n--)
            async_io_service_run_thread.emplace_back(std::async(std::launch::async,[this](){this->io_service.run();}));
    }
    // will stop after current accept();
    void stop() { _stop = true; }
    void run() {
        _stop = false;
        if(async_io_service_run_thread.empty())
            async_run(1);
        while(!_stop)
            thread_pool(&sbio);
    }
    template <typename T> void set_connection_timeout(T t) { connection_timeout = std::chrono::duration_cast<std::chrono::milliseconds>(t); }
    template <typename T> void set_read_timeout(T t) { read_timeout = std::chrono::duration_cast<std::chrono::milliseconds>(t); }
private:
    constexpr static int MAGIC_HEADER_SIZE = 8;
    void write_json(std::shared_ptr<oy::Socket> socket, nlohmann::json j) {
        std::vector<uint8_t> buf(sizeof(size_t) + MAGIC_HEADER_SIZE);
        auto old_size = buf.size();
        nlohmann::json::to_cbor(j, buf);
        *(reinterpret_cast<size_t*>(&buf[MAGIC_HEADER_SIZE])) = buf.size() - old_size;
        socket->write(buf);
    }
    void write_error(std::shared_ptr<oy::Socket> socket, int id, std::string s) {
        return write_json(std::move(socket), {{"id", id}, {"error",s}});
    }
    void handle_call(std::shared_ptr<oy::Socket> socket) {
        do {
            nlohmann::json in_json;
            int id = -1;
            try {
                socket->read<std::array<uint8_t, MAGIC_HEADER_SIZE> >();
                in_json = nlohmann::json::from_cbor(socket->read<uint8_t>(socket->read<size_t>()));
                id = in_json["id"];
            } catch ( const nlohmann::json::exception& e) {
                write_error(socket, id, e.what());
                continue;
            }
            if(!function_map.count(in_json["func"]))
            {
                write_error(socket, id, "404");
                continue;
            }
            q.enqueue(std::make_pair(std::move(in_json), socket));
        } while(true);
    }

    void execution_once(QueueType qt)
    {
        int id = -1;
        try {
            auto& in_json = qt.first;
            id = in_json["id"];
            auto out_json = function_map[in_json["func"]](in_json["args"]);
            write_json(qt.second, nlohmann::json({{"result", out_json}, {"id", id}}));
        } catch (const nlohmann::json::exception& e) {
            std::cerr << "server side json error: " << e.what() << " json is " << qt.first.dump() << std::endl;
            write_json(qt.second, nlohmann::json({{"error", "wrong arg"}, {"id", id}}));
        } catch (const boost::system::system_error& e) {
            std::cout << "server side asio error: " << e.what() << std::endl;
        }
    }

    void execution() {
        while(!_exit) {
            QueueType qt;
            while(q.try_dequeue(qt))
                execution_once(qt);
            std::this_thread::sleep_for(execution_spin_wait_time);
        }
    }
    std::map<std::string, std::function<nlohmann::json(nlohmann::json)> > function_map;
    oy::Distributor<oy::SyncBoostIO*> thread_pool; // thread_pool
    std::vector<std::thread> exec_threads;

    moodycamel::ConcurrentQueue<QueueType> q;
    std::chrono::milliseconds connection_timeout = std::chrono::milliseconds(1000);
    std::chrono::milliseconds read_timeout = std::chrono::milliseconds(1000);
    std::chrono::milliseconds execution_spin_wait_time = std::chrono::milliseconds(1);
    bool _stop = true;  // used to interruct run();
    bool _exit = false; // used in destructor;
};

}

}