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

    boost::asio::io_service io_service;
    oy::SyncBoostIO sbio;
    std::unique_ptr<boost::asio::io_service::work> io_service_work;

    std::vector<std::future<void> > async_io_service_run_thread;

    std::map<std::string, std::function<nlohmann::json(nlohmann::json)> > function_map;
    boost::asio::ip::tcp::socket socket;

    oy::Distributor<boost::asio::ip::tcp::socket&&> thread_pool; // threads for read socket.
    std::vector<std::thread> exec_threads;

    moodycamel::ConcurrentQueue<QueueType> q;
    std::chrono::milliseconds connection_timeout = std::chrono::milliseconds(1000);
    std::chrono::milliseconds read_timeout = std::chrono::milliseconds(300000);
    std::chrono::milliseconds execution_spin_wait_time = std::chrono::milliseconds(1);
    bool _exit = false; // used in destructor;

    constexpr static std::array<uint8_t, 8> MAGIC_HEADER = {'p','h','i','l','b','a','b','a'};
    constexpr static int MAGIC_HEADER_SIZE = MAGIC_HEADER.size();
    constexpr static int MAX_BODY_LENGTH = 12345678;

public:
    // pending_connection is the max number of threads which the server accepted but not yet attach a runner to it.
    // pending_connection 是已经accept，但服务器没有线程去处理它的线程的最大个数
    Server(unsigned short n_thread, unsigned short pending_connection):
        sbio(io_service),
        io_service_work(std::make_unique<boost::asio::io_service::work>(io_service)),
        socket(io_service),
        thread_pool(std::function<void(boost::asio::ip::tcp::socket&&)>([this](boost::asio::ip::tcp::socket&& sock){
                    try {
                        this->handle_call(std::make_shared<oy::Socket>(std::move(sock)));
                    } catch (const boost::system::system_error& e) {
                        if(e.code() != boost::asio::error::eof)
                            std::cout << "server side io error: " << e.what() << std::endl;
                    }}), n_thread, std::max(decltype(pending_connection)(1), pending_connection)) {
        bind("", [](int)->int{return 0;}); // used for heartbeat.
        for(auto i=0u; i<n_thread; i++)
            exec_threads.emplace_back([this](){this->execution();});
    }

    unsigned short listen() {
        unsigned short port=0;
        sbio.listen(port);
        return port;
    }

    void listen(unsigned short port) {
        sbio.listen(port);
    }
    ~Server() {
        _exit = true;
        for(auto& i : exec_threads) i.join();
        io_service_work.reset(nullptr);
    }
    void accept_handler(const boost::system::system_error& e) {
        if (e.code() == boost::asio::error::operation_aborted)
            return;
        else if(e.code())
            std::cerr << "accept error: " << e.what() << std::endl;
        thread_pool(std::move(socket));
        async_accept();
    }
    void async_accept(){
        sbio.get_acceptor().async_accept(socket, std::bind(&Server::accept_handler, this, std::placeholders::_1));
    }
    void async_run(unsigned int n) {
        while(n--)
            async_io_service_run_thread.emplace_back(std::async(std::launch::async,[this](){this->io_service.run();}));
    }

    template<typename Functor>
    void bind(std::string name, Functor f) {
        function_map[name] = [f](nlohmann::json j) -> nlohmann::json {
            return call(f,j.template get<typename func_traits<Functor>::args_type>());
        };
    }

    // will stop after current accept();
    void stop() { sbio.get_acceptor().cancel(); }

    void run(){this->io_service.run();}
    template <typename T> void set_connection_timeout(T t) { connection_timeout = std::chrono::duration_cast<std::chrono::milliseconds>(t); }
    template <typename T> void set_read_timeout(T t) { read_timeout = std::chrono::duration_cast<std::chrono::milliseconds>(t); }
private:
    void write_json(std::shared_ptr<oy::Socket> socket, nlohmann::json j) {
        std::vector<uint8_t> buf(sizeof(size_t) + MAGIC_HEADER_SIZE);
        std::copy(MAGIC_HEADER.begin(), MAGIC_HEADER.end(), buf.begin());
        auto old_size = buf.size();
        nlohmann::json::to_cbor(j, buf);
        *(reinterpret_cast<size_t*>(&buf[MAGIC_HEADER_SIZE])) = buf.size() - old_size;
        socket->write(buf);
    }
    void write_error(std::shared_ptr<oy::Socket> socket, int id, std::string s) {
        return write_json(std::move(socket), {{"id", id}, {"error",s}});
    }
    void handle_call(std::shared_ptr<oy::Socket> socket) {
        socket->set_connection_timeout(connection_timeout);
        socket->set_read_timeout(read_timeout);
        do {
            nlohmann::json in_json;
            int id = -1;
            try {
                auto header_buffer = socket->read<std::array<uint8_t, MAGIC_HEADER_SIZE + sizeof(size_t)> >();
                size_t sz = *reinterpret_cast<size_t*>(&header_buffer[MAGIC_HEADER_SIZE]);
                if (!std::equal(MAGIC_HEADER.begin(), MAGIC_HEADER.end(), header_buffer.begin()))
                    throw nlohmann::json::other_error::create(0, "magic number wrong.");
                if (sz > MAX_BODY_LENGTH)
                    throw nlohmann::json::other_error::create(0, "request body too large.");
                in_json = nlohmann::json::from_cbor(socket->read<uint8_t>(sz));
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
};

}

}