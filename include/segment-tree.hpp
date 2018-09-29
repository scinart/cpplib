#ifndef GITHUB_SCINART_CPPLIB_SEGMENT_TREE_HPP_
#define GITHUB_SCINART_CPPLIB_SEGMENT_TREE_HPP_

#include <vector>
#include <iostream>

namespace oy {

/**
   Suppose N = 5; the tree like this:
   [01, 02, 12, 05, 23, 25, 34, 35, 45]
     \       |       \       \       |
      \______|        \       \______|
             \         \             |
              \         \____________|
               \                     |
                \____________________|
                                     |
                                     |
   So index of any [l,r), we just put this value just prior to the right child-tree.
   So index of [l,r) = ((l+r)/2)*2-1,
   And indef of [l,l+1) = 2*l
   this can be compressed to (l-1+r)-((r-l==1)?0:((l+r)&1));
                             (l+1==r)?(2*l):((l+r)/2*2-1)

 */

template <typename T>
class RangeMaxValue
{
    using ValueType = T;
    ValueType val;
public:
    ValueType get(){ return val; }
    RangeMaxValue():val(0){}
    RangeMaxValue(ValueType v):val(v){}
    RangeMaxValue operator+(const RangeMaxValue& rhs){return {std::max(val, rhs.val)};}
};

template <typename T>
class RangeMaxMarkReset
{
    using ValueType = RangeMaxValue<T>;
    ValueType val;
    bool isMark = false;
public:
    RangeMaxMarkReset(){}
    RangeMaxMarkReset(ValueType v):val(v), isMark(true){}
    explicit operator bool () const { return isMark; }
    void clear() { isMark = false; }
    // old_mark += new_mark
    void operator+= (const RangeMaxMarkReset& rhs){ val = rhs.val; isMark = rhs.isMark; }
    ValueType apply(ValueType, size_t){ return val; }
};

template <typename T>
class RangeSumMarkAdd // query for range sum, op on range add
{
    using ValueType = T;
    ValueType val;
public:
    RangeSumMarkAdd():val(0){}
    RangeSumMarkAdd(ValueType t):val(t){}
    explicit operator bool () const { return static_cast<bool>(val); }
    const ValueType& get() const { return val; }
    void clear() { val = 0; }
    void operator+= (const RangeSumMarkAdd& rhs){ val += rhs.val; }
    ValueType apply(ValueType v, size_t r){ return v + r*val; }
};

template <typename T>
class RangeSumMarkReset // query for range sum, op on range reset
{
    using ValueType = T;
    ValueType val;
    bool isMark = false;
public:
    RangeSumMarkReset(){}
    RangeSumMarkReset(ValueType t):val(t), isMark(true){}
    explicit operator bool () const { return isMark; }
    const ValueType& get() const { return val; }
    void clear() { isMark = false; }
    // old_mark += new_mark
    void operator+= (const RangeSumMarkReset& rhs){ val = rhs.val; isMark = rhs.isMark; }
    ValueType apply(ValueType, size_t r){ return r*val; }
};

template <typename ValueType_, typename MarkType_>
class SegmentTree
{

#pragma push_macro("ind")
#ifdef ind
#undef ind
#endif
#define indexof(l,r) ((l+1==r)?(2*l):((l+r)/2*2-1))

public:
    using ValueType = ValueType_;
    using MarkType = MarkType_;
private:
    const int N;
    std::vector<ValueType> v;
    std::vector<MarkType> m;
public:
    void debug()
    {
        for(int i=0;i<v.size();i++)
            std::cout << v[i] << ' ';std::cout<<'\n';
        for(int i=0;i<v.size();i++)
            std::cout << m[i].get() << ' ';std::cout<<'\n';
    }
public:
    SegmentTree(size_t N_):N(N_), v(2*N-1), m(2*N-1){}
    SegmentTree(size_t N_, const std::vector<ValueType>& init_value):N(N_), v(2*N-1), m(2*N-1){
        build(0, N, init_value);
    }
    ValueType query(size_t l,size_t r) {
        return query_(0, N, l, r);
    }
    void update(size_t l,size_t r,const MarkType& mark) {
        update_(0, N, l, r, mark);
    }
private:
    void build(size_t l, size_t r, const std::vector<ValueType> & init_value) {
        size_t ind = indexof(l,r);
        if(l+1<r) {
            build(l, (l+r)>>1, init_value);
            build((l+r)>>1, r, init_value);
            v[ind]=v[indexof(l,(l+r)/2)] + v[indexof((l+r)/2,r)];
        }
        else {
            v[ind]=init_value[l];
        }
    }
    void pushDown(size_t l, size_t r) {
        // if this node is marked then pushDown and clear the mark.
        auto ind = indexof(l, r);
        MarkType& mark=m[ind];
        if(static_cast<bool>(mark)) {
            if(r-l>1) {
                //not leaf, push down Mark;
                m[indexof(l,(l+r)/2)] += mark;
                m[indexof((l+r)/2,r)] += mark;
            }
            v[ind] = mark.apply(v[ind], r-l);
            mark.clear();
        }
    }
    ValueType query_(size_t l, size_t r, size_t query_l, size_t query_r)
    {
        auto ind = indexof(l, r);
        auto mid = (l+r)/2;
        pushDown(l, r);//necessary.
        if(query_l<=l && r<=query_r) // [l,r) ⊂ [query_l, query_r)
            return v[ind];
        if (query_l>=mid)
            return query_(mid, r, query_l, query_r);
        if (query_r<=mid)
            return query_(l, mid, query_l, query_r);
        return query_(l, mid, query_l, query_r) + query_(mid, r, query_l, query_r);
    }

    void update_(size_t l,size_t r, size_t update_l, size_t update_r, const MarkType& mark)
    {
        auto ind = indexof(l, r);
        auto mid = (l+r)/2;

        // after update_, node[ind] is clean and value updated.
        if(update_l<=l && r<=update_r) // [l,r) ⊂ [update_l, update_r)
        {
            m[ind] += mark;
            pushDown(l,r);
        }
        else
        {
            pushDown(l,r);
            if (update_l>=mid)
                update_(mid, r, update_l, update_r, mark);
            else if (update_r<=mid)
                update_(l, mid, update_l, update_r, mark);
            else
            {
                update_(l, mid, update_l, update_r, mark);
                update_(mid, r, update_l, update_r, mark);
            }
            pushDown(l,mid);
            pushDown(mid,r);
            v[ind] = v[indexof(l,mid)] + v[indexof(mid,r)];
        }
    }
};

}

#pragma pop_macro("ind")
#endif