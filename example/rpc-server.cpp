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

int sleep_inc(int sleep_in_seconds, int x)
{
    std::this_thread::sleep_for(std::chrono::seconds(sleep_in_seconds));
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
    srv.run();
    return 0;
}
