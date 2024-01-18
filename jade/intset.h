#ifndef _INTSET_H_
#define _INTSET_H_

#include <set>
#include <string>

class INTSET
{
  private:
    friend class INTSET_ITR;
    friend class INTSET_PAIR_ITR;
    typedef std::set<int>::const_iterator const_iterator;

    std::set<int>	_data;

  public:
    INTSET() {}
    INTSET(const INTSET& i) : _data(i._data) {}
    ~INTSET() {}

    bool empty() const { return _data.empty(); }
    void insert(int x);
    void remove(int x);
    void remove_all();
    int pop_smallest();
    bool contains(int x) const;
    size_t size() const { return _data.size(); }
    bool operator==(const INTSET& a) const;
    bool operator!=(const INTSET& a) const { return !(*this == a); }

    static INTSET combine(const INTSET& a, const INTSET& b);
    static INTSET full_set(int n);

    bool subset_of(const INTSET& bigger) const;
    bool superset_of(const INTSET& bigger) const;
};


class INTSET_ITR
{
  private:
    const INTSET&          _store;
    INTSET::const_iterator _itr;

  public:
    INTSET_ITR(const INTSET& iset);
    ~INTSET_ITR() {}

    bool more() const;
    int current() const;
    void next();
};


class INTSET_PAIR_ITR
{
  private:
    const INTSET&          _a, _b;
    INTSET::const_iterator _a_itr, _b_itr;
    bool _a_only, _b_only, _both;

    void calc_only();

  public:
    INTSET_PAIR_ITR(const INTSET& a, const INTSET& b);
    ~INTSET_PAIR_ITR() {}

    bool more() const;
    int current() const;
    bool a_only() { return _a_only; }
    bool b_only() { return _b_only; }
    bool both() { return _both; }
    void next();
};

std::string intset_to_string(const INTSET& intset);

#endif // _INTSET_H_
