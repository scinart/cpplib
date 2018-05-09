#include <string>
#include <vector>
#include <sstream>
#include <deque>
#include <iostream>
#include "rpc-server.hpp"

using namespace std;

int count_words(const string& s)
{
    cout << s + "\n" << flush;
    stringstream ss;
    ss<<s;
    string x;
    unsigned int c=0;
    while(ss>>x)
        c++;
    return c;
}

int main(int argc, char* argv[] )
{
    static_cast<void>(argc);
     // Creating a server that listens on port 8080
    const int capacity = 40;
    oy::rpc::Server srv(stoi(argv[1]), capacity*2, capacity);
    srv.set_read_timeout(std::chrono::milliseconds(1000));

    // Binding
    srv.bind("count_words", count_words);

    // Run the server loop.
    srv.run();
    return 0;
}
