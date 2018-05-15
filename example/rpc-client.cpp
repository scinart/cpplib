#include <string>
#include <vector>
#include <sstream>
#include <deque>
#include <fstream>
#include "thread_pool.hpp"
#include "rand.hpp"
#include "rpc-client.hpp"

#include <chrono>

using namespace std::chrono_literals;

using namespace std;

string g_ip;
unsigned short g_port;
oy::rpc::Client g_client;

template <typename Client, typename... Args>
int call(Client&& client, Args... args)
{
    try {
        return client.call(args...).template as<int>();
    } catch (const boost::system::system_error& e) {
        cerr << e.what() << endl;
        return 0;
    }
}

template <typename Client, typename Duration, typename... Args>
int call_with_timeout(Client&& client, Duration d, Args... args)
{
    try {
        return client.call_with_timeout(d, args...).template as<int>();
    } catch (const boost::system::system_error& e) {
        cerr << e.what() << endl;
        return 0;
    }
}

void ff(int x, int sleep, oy::rpc::Client* c)
{
    const int TIMEOUT = 1000;
    const int TOLERANCE = 200;
    auto y = c->call_with_timeout(std::chrono::milliseconds(TIMEOUT), "sinc", sleep, x);
    // std::cout << y.dump() << std::endl;
    char ok = '-';
    if (y.count("error"))
    {
        if (sleep + TOLERANCE > TIMEOUT)
            ok = ','; // perhaps timeout
        else if (y["error"] == "timeout")
            ok = '^';
        else
            ok = '#';
    }
    else
    {
        ok = (y["result"]==x+1)?'.':'x';
    }
    cout << ok << flush;
}

void f(std::pair<int,int> xy)
{
    ff(xy.first, xy.second, &g_client);
}

int main(int argc, char* argv[] )
{
    g_ip = argv[1];
    g_port = stoi(argv[2]);

    g_client.set_connection_timeout(std::chrono::milliseconds(50));
    g_client.set_read_timeout(std::chrono::milliseconds(1000));
    g_client.connect(g_ip, g_port);

    //timeout_test();
    // return 0;
    oy::Distributor<std::pair<int,int> > d(f, std::stoi(argv[3]), std::stoi(argv[3]));
    unsigned count = (argc >= 5)?stoi(argv[4]):std::numeric_limits<unsigned>::max();

    oy::RandInt i(std::numeric_limits<int>::min()+1, std::numeric_limits<int>::max()-1);
    oy::RandInt j(0, 1500);
    while(count--)
        d(make_pair(i(), j()));
    return 0;
}
