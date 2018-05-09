#include <string>
#include <vector>
#include <sstream>
#include <deque>
#include <iostream>
#include "rpc-server.hpp"

using namespace std;

int inc(int x)
{
    cout<<to_string(x)+"\n"<<flush;
    return x+1;
}

int sleep_inc(int sleep_in_ms, int x)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_in_ms));
    return x+1;
}

int main(int argc, char* argv[] )
{
    static_cast<void>(argc);
     // Creating a server that listens on port 8080
    const int capacity = 40;
    oy::rpc::Server srv(stoi(argv[1]), capacity*2, capacity);
    srv.set_read_timeout(std::chrono::milliseconds(1000));

    // Binding
    srv.bind("sinc", sleep_inc);
    srv.bind("inc", inc);

    srv.async_accept();
    // Run the server loop.

    if(false)
    {
        srv.async_run(1);
        std::this_thread::sleep_for(std::chrono::seconds(10));
        srv.stop();
    }
    else
        srv.run();

    return 0;
}
