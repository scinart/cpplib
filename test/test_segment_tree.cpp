#include <boost/test/unit_test.hpp>

#include "segment-tree.hpp"
#include "rand.hpp"
#include <algorithm>
#include <iostream>

using namespace oy;

BOOST_AUTO_TEST_SUITE(segment_tree_test)

auto debug = [](auto c, auto b, auto e, auto val, const auto& v)
{
    if(c!=1)
        std::cout<<c<<' '<<b<<' '<<e<<' '<<val<<'\n';
    else
        std::cout<<c<<' '<<b<<' '<<e<<' '<<'\n';
    for (const auto i : v)
        std::cout<<i<<' ';
    std::cout<<'\n';
};

BOOST_AUTO_TEST_CASE(range_sum_range_add)
{
    size_t test_size = 165;
    size_t test_count = 1000;
    std::vector<int> v(test_size);
    oy::SegmentTree<int, oy::RangeSumMarkAdd<int> > segtree(test_size);

    oy::RandInt op_gen(1,3);
    oy::RandInt range_gen(0ul, test_size-1);
    oy::RandInt value_gen(-5, 5);
    for (size_t i = 0; i < test_count; i++) {
        auto b = range_gen.get();
        auto e = range_gen.get();
        if (b==e) // bad luck
            continue;
        else if(b>e)
            std::swap(b,e);
        auto c = op_gen.get();
        auto val = value_gen.get();
        switch(c)
        {
          case 1: // query
          {
              BOOST_CHECK(std::accumulate(v.begin()+b, v.begin()+e, 0) == segtree.query(b,e));
              break;
          }
          case 2: // point update
          {
              v[b] += val;
              segtree.update(b, b+1, val);
              break;
          }
          case 3: // range update
          {
              for(auto i=b;i<e;i++) v[i] += val;
              segtree.update(b, e, val);
              break;
          }
        }
        // debug(c, b, e, val, v);
    }
}

BOOST_AUTO_TEST_CASE(range_max_range_reset)
{
    size_t test_size = 198;
    size_t test_count = 1000;
    std::vector<int> v(test_size);
    oy::SegmentTree<RangeMaxValue<int>, oy::RangeMaxMarkReset<int> > segtree(test_size);
    oy::RandInt op_gen(1,3);
    oy::RandInt range_gen(0ul, test_size-1);
    oy::RandInt value_gen(-5, 5);
    for (size_t i = 0; i < test_count; i++) {
        auto b = range_gen.get();
        auto e = range_gen.get();
        if (b==e) // bad luck
            continue;
        else if(b>e)
            std::swap(b,e);
        auto c = op_gen.get();
        auto val = value_gen.get();
        switch(c)
        {
          case 1: // query
          {
              BOOST_CHECK(*std::max_element(v.begin()+b, v.begin()+e) == segtree.query(b,e).get());
              break;
          }
          case 2: // point update
          {
              v[b] = val;
              segtree.update(b, b+1, decltype(segtree)::ValueType(val));
              break;
          }
          case 3: // range update
          {
              for(auto i=b;i<e;i++) v[i] = val;
              segtree.update(b, e, decltype(segtree)::ValueType(val));
              break;
          }
        }
        // debug(c, b, e, val, v);
    }
}

BOOST_AUTO_TEST_CASE(range_sum_range_reset)
{
    size_t test_size = 117;
    size_t test_count = 1000;
    std::vector<int> v(test_size);
    oy::SegmentTree<int, oy::RangeSumMarkReset<int> > segtree(test_size);
    oy::RandInt op_gen(1,3);
    oy::RandInt range_gen(0ul, test_size-1);
    oy::RandInt value_gen(-5, 5);
    for (size_t i = 0; i < test_count; i++) {
        auto b = range_gen.get();
        auto e = range_gen.get();
        if (b==e) // bad luck
            continue;
        else if(b>e)
            std::swap(b,e);
        auto c = op_gen.get();
        auto val = value_gen.get();
        switch(c)
        {
          case 1: // query
          {
              BOOST_CHECK(std::accumulate(v.begin()+b, v.begin()+e, 0) == segtree.query(b,e));
              break;
          }
          case 2: // point update
          {
              v[b] = val;
              segtree.update(b, b+1, decltype(segtree)::ValueType(val));
              break;
          }
          case 3: // range update
          {
              for(auto i=b;i<e;i++) v[i] = val;
              segtree.update(b, e, decltype(segtree)::ValueType(val));
              break;
          }
        }
        // debug(c, b, e, val, v);
    }
}

BOOST_AUTO_TEST_SUITE_END()
