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

template <typename Client>
int call(Client&& client, int x, std::string name)
{
    try {
        return client.call(name, x).template as<int>();
    } catch (const boost::system::system_error& e) {
        cerr << e.what() << endl;
        return 0;
    }
}

void ff(int x, oy::rpc::Client* c)
{
    int y;
    if(!c)
        y = call(oy::rpc::Client().set_connection_timeout(std::chrono::milliseconds(50)).set_read_timeout(std::chrono::milliseconds(1000)).connect(g_ip, g_port),
                 x, "inc");
    else
        y=call(*c, x, "inc");
    if(y==x+1)
        cout<<'.';
    else
        cout<<',';
    cout << flush;
}

void f(int x)
{
    oy::rpc::Client client;
    client.set_connection_timeout(std::chrono::milliseconds(50));
    client.set_read_timeout(std::chrono::milliseconds(1000));
    client.connect(g_ip, g_port);

    ff(x,&client);
}

int main(int argc, char* argv[] )
{
    g_ip = argv[1];
    g_port = stoi(argv[2]);
    oy::Distributor<int> d(f, std::stoi(argv[3]), std::stoi(argv[3]));
    unsigned count = (argc >= 5)?stoi(argv[4]):std::numeric_limits<unsigned>::max();

    oy::RandInt i(std::numeric_limits<int>::min()+1, std::numeric_limits<int>::max()-1);
    while(count--)
        d(i());
    return 0;
}
