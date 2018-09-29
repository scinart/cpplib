#ifndef GITHUB_SCINART_CPPLIB_RAND_HPP_
#define GITHUB_SCINART_CPPLIB_RAND_HPP_

#include <algorithm>
#include <limits>
#include <vector>

namespace oy {

/**
   In ctor DisjointSet(unsigned int n),
   n isolated node (0...n-1) is initialized.
   no range check are done.
 */
class DisjointSet
{
    struct Node
    {
        unsigned int parent;
        unsigned int rank;
    };
    std::vector<Node> nodes;
public:
    DisjointSet(unsigned int n){
        nodes.resize(n, Node{0,0});
        for(auto& n : nodes)
            n.parent = &n - &nodes.front();
    }
    bool is_same(unsigned int x, unsigned int y)
    {
        return find(x)==find(y);
    }
    void unite(unsigned int x, unsigned int y)
    {
        auto rx = find(x), ry = find(y);
        if (rx == ry) return;
        if (nodes[rx].rank < nodes[ry].rank)
            std::swap(rx,ry);
        nodes[ry].parent = nodes[rx].parent;
        if (nodes[rx].rank == nodes[ry].rank)
            nodes[rx].rank++;
    }
private:
    unsigned int find(unsigned x)
    {
        if (nodes[x].parent != x)
            nodes[x].parent = find(nodes[x].parent);
        return nodes[x].parent;
    }
};

}
#endif