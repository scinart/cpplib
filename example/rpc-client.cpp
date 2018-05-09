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
    int y;
    if(!c)
        y = call(oy::rpc::Client().set_connection_timeout(std::chrono::milliseconds(50)).set_read_timeout(std::chrono::milliseconds(1000)).connect(g_ip, g_port),
                 "inc", x);
    else
        y=call_with_timeout(*c, std::chrono::milliseconds(500), "sinc", sleep, x);
    if(y==x+1)
        cout<<'.';
    else
        cout<<',';
    cout << flush;
}

void f(std::pair<int,int> xy)
{
    oy::rpc::Client client;
    client.set_connection_timeout(std::chrono::milliseconds(50));
    client.set_read_timeout(std::chrono::milliseconds(1000));
    client.connect(g_ip, g_port);

    ff(xy.first, xy.second, &client);
}

void timeout_test()
{
    oy::rpc::Client client;
    client.set_connection_timeout(std::chrono::milliseconds(50));
    client.set_read_timeout(std::chrono::milliseconds(1000));
    client.connect(g_ip, g_port);
    int y = call_with_timeout(client, std::chrono::milliseconds(500), "sinc", 4000, 12);
    if (y==13)
    {
        std::cout<<"不科学"<<std::endl;
    }
    else
    {
        std::cout<<"科学！"<<std::endl;
    }
}


int main(int argc, char* argv[] )
{
    g_ip = argv[1];
    g_port = stoi(argv[2]);

    timeout_test();
    return 0;
    oy::Distributor<std::pair<int,int> > d(f, std::stoi(argv[3]), std::stoi(argv[3]));
    unsigned count = (argc >= 5)?stoi(argv[4]):std::numeric_limits<unsigned>::max();

    oy::RandInt i(std::numeric_limits<int>::min()+1, std::numeric_limits<int>::max()-1);
    oy::RandInt j(0, 1500);
    while(count--)
        d(make_pair(i(), j()));
    return 0;
}
