#define _HAS_ITERATOR_DEBUGGING 0

/*++
  Copyright (c) Microsoft Corporation

  This hash template is borrowed from Microsoft Z3
  (https://github.com/Z3Prover/z3).

  Simple implementation of bucket-list hash tables conforming roughly
  to SGI hash_map and hash_set interfaces, though not all members are
  implemented.

  These hash tables have the property that insert preserves iterators
  and references to elements.

  This package lives in namespace hash_space. Specializations of
  class "hash" should be made in this namespace.

  --*/

#ifndef HASH_H
#define HASH_H

#ifdef _WINDOWS
#pragma warning(disable:4267)
#endif

#include <string>
#include <vector>
#include <iterator>
#include <fstream>

namespace hash_space {

    unsigned string_hash(const char * str, unsigned length, unsigned init_value);

    template <typename T> class hash {
    public:
        size_t operator()(const T &s) const {
            return s.__hash();
        }
    };

    template <>
        class hash<int> {
    public:
        size_t operator()(const int &s) const {
            return s;
        }
    };

    template <>
        class hash<bool> {
    public:
        size_t operator()(const bool &s) const {
            return s;
        }
    };

    template <>
        class hash<std::string> {
    public:
        size_t operator()(const std::string &s) const {
            return string_hash(s.c_str(), (unsigned)s.size(), 0);
        }
    };

    template <>
        class hash<std::pair<int,int> > {
    public:
        size_t operator()(const std::pair<int,int> &p) const {
            return p.first + p.second;
        }
    };

    template <typename T>
        class hash<std::vector<T> > {
    public:
        size_t operator()(const std::vector<T> &p) const {
            hash<T> h;
            size_t res = 0;
            for (unsigned i = 0; i < p.size(); i++)
                res += h(p[i]);
            return res;
        }
    };

    template <class T>
        class hash<std::pair<T *, T *> > {
    public:
        size_t operator()(const std::pair<T *,T *> &p) const {
            return (size_t)p.first + (size_t)p.second;
        }
    };

    template <class T>
        class hash<T *> {
    public:
        size_t operator()(T * const &p) const {
            return (size_t)p;
        }
    };

    enum { num_primes = 29 };

    static const unsigned long primes[num_primes] =
        {
            7ul,
            53ul,
            97ul,
            193ul,
            389ul,
            769ul,
            1543ul,
            3079ul,
            6151ul,
            12289ul,
            24593ul,
            49157ul,
            98317ul,
            196613ul,
            393241ul,
            786433ul,
            1572869ul,
            3145739ul,
            6291469ul,
            12582917ul,
            25165843ul,
            50331653ul,
            100663319ul,
            201326611ul,
            402653189ul,
            805306457ul,
            1610612741ul,
            3221225473ul,
            4294967291ul
        };

    inline unsigned long next_prime(unsigned long n) {
        const unsigned long* to = primes + (int)num_primes;
        for(const unsigned long* p = primes; p < to; p++)
            if(*p >= n) return *p;
        return primes[num_primes-1];
    }

    template<class Value, class Key, class HashFun, class GetKey, class KeyEqFun>
        class hashtable
    {
    public:

        typedef Value &reference;
        typedef const Value &const_reference;
    
        struct Entry
        {
            Entry* next;
            Value val;
      
        Entry(const Value &_val) : val(_val) {next = 0;}
        };
    

        struct iterator
        {      
            Entry* ent;
            hashtable* tab;

            typedef std::forward_iterator_tag iterator_category;
            typedef Value value_type;
            typedef std::ptrdiff_t difference_type;
            typedef size_t size_type;
            typedef Value& reference;
            typedef Value* pointer;

        iterator(Entry* _ent, hashtable* _tab) : ent(_ent), tab(_tab) { }

            iterator() { }

            Value &operator*() const { return ent->val; }

            Value *operator->() const { return &(operator*()); }

            iterator &operator++() {
                Entry *old = ent;
                ent = ent->next;
                if (!ent) {
                    size_t bucket = tab->get_bucket(old->val);
                    while (!ent && ++bucket < tab->buckets.size())
                        ent = tab->buckets[bucket];
                }
                return *this;
            }

            iterator operator++(int) {
                iterator tmp = *this;
                operator++();
                return tmp;
            }


            bool operator==(const iterator& it) const { 
                return ent == it.ent;
            }

            bool operator!=(const iterator& it) const {
                return ent != it.ent;
            }
        };

        struct const_iterator
        {      
            const Entry* ent;
            const hashtable* tab;

            typedef std::forward_iterator_tag iterator_category;
            typedef Value value_type;
            typedef std::ptrdiff_t difference_type;
            typedef size_t size_type;
            typedef const Value& reference;
            typedef const Value* pointer;

        const_iterator(const Entry* _ent, const hashtable* _tab) : ent(_ent), tab(_tab) { }

            const_iterator() { }

            const Value &operator*() const { return ent->val; }

            const Value *operator->() const { return &(operator*()); }

            const_iterator &operator++() {
                Entry *old = ent;
                ent = ent->next;
                if (!ent) {
                    size_t bucket = tab->get_bucket(old->val);
                    while (!ent && ++bucket < tab->buckets.size())
                        ent = tab->buckets[bucket];
                }
                return *this;
            }

            const_iterator operator++(int) {
                const_iterator tmp = *this;
                operator++();
                return tmp;
            }


            bool operator==(const const_iterator& it) const { 
                return ent == it.ent;
            }

            bool operator!=(const const_iterator& it) const {
                return ent != it.ent;
            }
        };

    private:

        typedef std::vector<Entry*> Table;

        Table buckets;
        size_t entries;
        HashFun hash_fun ;
        GetKey get_key;
        KeyEqFun key_eq_fun;
    
    public:

    hashtable(size_t init_size) : buckets(init_size,(Entry *)0) {
            entries = 0;
        }
    
        hashtable(const hashtable& other) {
            dup(other);
        }

        hashtable& operator= (const hashtable& other) {
            if (&other != this)
                dup(other);
            return *this;
        }

        ~hashtable() {
            clear();
        }

        size_t size() const { 
            return entries;
        }

        bool empty() const { 
            return size() == 0;
        }

        void swap(hashtable& other) {
            buckets.swap(other.buckets);
            std::swap(entries, other.entries);
        }
    
        iterator begin() {
            for (size_t i = 0; i < buckets.size(); ++i)
                if (buckets[i])
                    return iterator(buckets[i], this);
            return end();
        }
    
        iterator end() { 
            return iterator(0, this);
        }

        const_iterator begin() const {
            for (size_t i = 0; i < buckets.size(); ++i)
                if (buckets[i])
                    return const_iterator(buckets[i], this);
            return end();
        }
    
        const_iterator end() const { 
            return const_iterator(0, this);
        }
    
        size_t get_bucket(const Value& val, size_t n) const {
            return hash_fun(get_key(val)) % n;
        }
    
        size_t get_key_bucket(const Key& key) const {
            return hash_fun(key) % buckets.size();
        }

        size_t get_bucket(const Value& val) const {
            return get_bucket(val,buckets.size());
        }

        Entry *lookup(const Value& val, bool ins = false)
        {
            resize(entries + 1);

            size_t n = get_bucket(val);
            Entry* from = buckets[n];
      
            for (Entry* ent = from; ent; ent = ent->next)
                if (key_eq_fun(get_key(ent->val), get_key(val)))
                    return ent;
      
            if(!ins) return 0;

            Entry* tmp = new Entry(val);
            tmp->next = from;
            buckets[n] = tmp;
            ++entries;
            return tmp;
        }

        Entry *lookup_key(const Key& key) const
        {
            size_t n = get_key_bucket(key);
            Entry* from = buckets[n];
      
            for (Entry* ent = from; ent; ent = ent->next)
                if (key_eq_fun(get_key(ent->val), key))
                    return ent;
      
            return 0;
        }

        const_iterator find(const Key& key) const {
            return const_iterator(lookup_key(key),this);
        }

        iterator find(const Key& key) {
            return iterator(lookup_key(key),this);
        }

        std::pair<iterator,bool> insert(const Value& val){
            size_t old_entries = entries;
            Entry *ent = lookup(val,true);
            return std::pair<iterator,bool>(iterator(ent,this),entries > old_entries);
        }
    
        iterator insert(const iterator &it, const Value& val){
            Entry *ent = lookup(val,true);
            return iterator(ent,this);
        }

        size_t erase(const Key& key)
        {
            Entry** p = &(buckets[get_key_bucket(key)]);
            size_t count = 0;
            while(*p){
                Entry *q = *p;
                if (key_eq_fun(get_key(q->val), key)) {
                    ++count;
                    *p = q->next;
                    delete q;
                }
                else
                    p = &(q->next);
            }
            entries -= count;
            return count;
        }

        void resize(size_t new_size) {
            const size_t old_n = buckets.size();
            if (new_size <= old_n) return;
            const size_t n = next_prime(new_size);
            if (n <= old_n) return;
            Table tmp(n, (Entry*)(0));
            for (size_t i = 0; i < old_n; ++i) {
                Entry* ent = buckets[i];
                while (ent) {
                    size_t new_bucket = get_bucket(ent->val, n);
                    buckets[i] = ent->next;
                    ent->next = tmp[new_bucket];
                    tmp[new_bucket] = ent;
                    ent = buckets[i];
                }
            }
            buckets.swap(tmp);
        }
    
        void clear()
        {
            for (size_t i = 0; i < buckets.size(); ++i) {
                for (Entry* ent = buckets[i]; ent != 0;) {
                    Entry* next = ent->next;
                    delete ent;
                    ent = next;
                }
                buckets[i] = 0;
            }
            entries = 0;
        }

        void dup(const hashtable& other)
        {
            clear();
            buckets.resize(other.buckets.size());
            for (size_t i = 0; i < other.buckets.size(); ++i) {
                Entry** to = &buckets[i];
                for (Entry* from = other.buckets[i]; from; from = from->next)
                    to = &((*to = new Entry(from->val))->next);
            }
            entries = other.entries;
        }
    };

    template <typename T> 
        class equal {
    public:
        bool operator()(const T& x, const T &y) const {
            return x == y;
        }
    };

    template <typename T>
        class identity {
    public:
        const T &operator()(const T &x) const {
            return x;
        }
    };

    template <typename T, typename U>
        class proj1 {
    public:
        const T &operator()(const std::pair<T,U> &x) const {
            return x.first;
        }
    };

    template <typename Element, class HashFun = hash<Element>, 
        class EqFun = equal<Element> >
        class hash_set
        : public hashtable<Element,Element,HashFun,identity<Element>,EqFun> {

    public:

    typedef Element value_type;

    hash_set()
    : hashtable<Element,Element,HashFun,identity<Element>,EqFun>(7) {}
    };

    template <typename Key, typename Value, class HashFun = hash<Key>, 
        class EqFun = equal<Key> >
        class hash_map
        : public hashtable<std::pair<Key,Value>,Key,HashFun,proj1<Key,Value>,EqFun> {

    public:

    hash_map()
    : hashtable<std::pair<Key,Value>,Key,HashFun,proj1<Key,Value>,EqFun>(7) {}

    Value &operator[](const Key& key) {
	std::pair<Key,Value> kvp(key,Value());
	return 
	hashtable<std::pair<Key,Value>,Key,HashFun,proj1<Key,Value>,EqFun>::
        lookup(kvp,true)->val.second;
    }
    };

}
#endif
typedef std::string __strlit;
extern std::ofstream __ivy_out;
void __ivy_exit(int);

template <typename D, typename R>
struct thunk {
    virtual R operator()(const D &) = 0;
    int ___ivy_choose(int rng,const char *name,int id) {
        return 0;
    }
};
template <typename D, typename R, class HashFun = hash_space::hash<D> >
struct hash_thunk {
    thunk<D,R> *fun;
    hash_space::hash_map<D,R,HashFun> memo;
    hash_thunk() : fun(0) {}
    hash_thunk(thunk<D,R> *fun) : fun(fun) {}
    ~hash_thunk() {
//        if (fun)
//            delete fun;
    }
    R &operator[](const D& arg){
        std::pair<typename hash_space::hash_map<D,R>::iterator,bool> foo = memo.insert(std::pair<D,R>(arg,R()));
        R &res = foo.first->second;
        if (foo.second && fun)
            res = (*fun)(arg);
        return res;
    }
};
	class udp_reader;

        class udp_config {
	public:
	    virtual void get(int id, unsigned long &inetaddr, unsigned long &inetport);
        };
    	class sec_timer;
    

    class reader;
    class timer;

class raft {
  public:
    typedef raft ivy_class;

#ifdef _WIN32
    void *mutex;  // forward reference to HANDLE
#else
    pthread_mutex_t mutex;
#endif
    void __lock();
    void __unlock();

#ifdef _WIN32
    std::vector<DWORD> thread_ids;

#else
    std::vector<pthread_t> thread_ids;

#endif
    void install_reader(reader *);
    void install_timer(timer *);
    virtual ~raft();
    std::vector<int> ___ivy_stack;
    int ___ivy_choose(int rng,const char *name,int id);
    virtual void ivy_assert(bool,const char *){}
    virtual void ivy_assume(bool,const char *){}
    virtual void ivy_check_progress(int,int){}
    typedef int node;
    struct node__iter__t {
    bool is_end;
    node val;
        size_t __hash() const { return hash_space::hash<bool>()(is_end)+hash_space::hash<node>()(val);}
    };
    class nset__arr : public std::vector<node>{
        public: size_t __hash() const { return hash_space::hash<std::vector<node> >()(*this);};
    };
    struct nset {
    nset__arr repr;
        size_t __hash() const { return hash_space::hash<nset__arr>()(repr);}
    };
    struct hist__ent {
    int ent_logt;
    __strlit ent_value;
        size_t __hash() const { return hash_space::hash<int>()(ent_logt)+hash_space::hash<__strlit>()(ent_value);}
    };
    class hist__arrlog : public std::vector<hist__ent>{
        public: size_t __hash() const { return hash_space::hash<std::vector<hist__ent> >()(*this);};
    };
    struct hist {
    hist__arrlog repr;
        size_t __hash() const { return hash_space::hash<hist__arrlog>()(repr);}
    };
    enum msgkind{rqvote,rqvotenolog,vtcandidate,appentry,enappeneded,keepalive,nack};
    struct msg {
    msgkind m_kind;
    int m_ix;
    int m_term;
    node m_node;
    node m_cand;
    __strlit m_val;
    int m_logt;
    int m_prevlogt;
    bool m_isrecover;
        size_t __hash() const { return hash_space::hash<int>()(m_kind)+hash_space::hash<int>()(m_ix)+hash_space::hash<int>()(m_term)+hash_space::hash<node>()(m_node)+hash_space::hash<node>()(m_cand)+hash_space::hash<__strlit>()(m_val)+hash_space::hash<int>()(m_logt)+hash_space::hash<int>()(m_prevlogt)+hash_space::hash<bool>()(m_isrecover);}
    };
    class replierslog : public std::vector<nset>{
        public: size_t __hash() const { return hash_space::hash<std::vector<nset> >()(*this);};
    };
    struct commited_ix {
    int last_ix;
    int comm_term;
    bool non_empty;
    nset commit_q;
        size_t __hash() const { return hash_space::hash<int>()(last_ix)+hash_space::hash<int>()(comm_term)+hash_space::hash<bool>()(non_empty)+hash_space::hash<nset>()(commit_q);}
    };
    struct node_term {
    int _term;
        size_t __hash() const { return hash_space::hash<int>()(_term);}
    };
    node node__size;
    int localstate__mytime;
    node_term localstate__nter;
    hist hist__empty;
    hist localstate__myhist;
    nset localstate__my_voters;
    node n;
    commited_ix localstate__commited;
    nset nset__all;
    replierslog localstate__my_repliers;
    bool localstate__is_leader;
    hash_thunk<nset,int> nset__card;
    int localstate__last_heard_from_leader;
    int __CARD__index;
    int __CARD__nset__index;
    int __CARD__term;
    int __CARD__value;
    int __CARD__localstate__time;
    virtual bool nset__member(const node& N, const nset& S);
    virtual bool nset__majority(const nset& S);
    virtual bool hist__logtix(const hist& H, int IX, int LOGT);
    virtual bool hist__filled(const hist& H, int IX);
    virtual __strlit hist__valix(const hist& H, int IX);
    virtual node __num0();
    virtual bool nset__arr__value(const nset__arr& a, int i, const node& v);
    virtual int nset__arr__end(const nset__arr& a);
    virtual hist__ent hist__arrlog__value(const hist__arrlog& a, int i);
    virtual int hist__arrlog__end(const hist__arrlog& a);
    virtual nset replierslog__value(const replierslog& a, int i);
    virtual int replierslog__end(const replierslog& a);
	udp_reader *net__impl__rdr;

        udp_config *the_udp_config;        

        udp_config *get_udp_config() {
	    if (!the_udp_config) 
	        the_udp_config = new udp_config();
	    return the_udp_config; 
	}

        void set_udp_config(udp_config *conf) {
	    the_udp_config = conf;
        }
    	sec_timer *sec__impl__tmr;
        raft(node node__size, node n);
    virtual node_term ext__node_term__empty();
    virtual hist ext__hist__clear();
    virtual void ext__nset__add(nset& s, const node& n);
    virtual void ext__node__iter__next(node__iter__t& x);
    virtual node_term ext__node_term__set(int t);
    virtual void imp__shim__recv_debug(const msg& m);
    virtual nset ext__localstate__get_voters();
    virtual int ext__hist__get_logt(const hist& h, int ix);
    virtual node__iter__t ext__node__iter__end();
    virtual void ext__shim__handle_rqst_vote(int logt, int ix, int t, const node& cand);
    virtual bool ext__system__req_replicate_entry_from_log(int ix, bool isrecover, const node& recovernode);
    virtual void ext__sec__timeout();
    virtual void ext__shim__recv_debug(const msg& m);
    virtual bool ext__localstate__is_leader_too_quiet();
    virtual int ext__commited_ix__get_first_uncommited(const commited_ix& cix);
    virtual void ext__localstate__add_vote(const node& voter);
    virtual void ext__safetyproof__update_term_hist_ghost(int t, const hist& h);
    virtual void ext__localstate___initialize_term();
    virtual void ext__shim__send_debug(const msg& m);
    virtual int ext__hist__get_next_ix(const hist& h);
    virtual int ext__node_term__get(const node_term& nter);
    virtual void ext__hist__strip(hist& h, int ix);
    virtual void imp__system__commited_at_ix(int ix, __strlit v);
    virtual void ext__localstate__become_leader();
    virtual void ext__shim__handle_keepalive(int t);
    virtual void ext__safetyproof__set_valid_ghost(const hist& h);
    virtual void ext__system__announce_candidacy();
    virtual nset ext__nset__emptyset();
    virtual __strlit ext__system__get_value(int i);
    virtual void ext__send_appended_reply_msg(const node& leader, int t, const node& n, int ix, bool isrecover);
    virtual void ext__shim__broadcast(const msg& m);
    virtual void ext__shim__handle_appended_reply_msg(int t, const node& replier, int ix, bool isrecover);
    virtual void ext__safetyproof__remember_previx_ghost(const msg& m, int previx);
    virtual void ext__shim__handle_rqst_vote_nolog(int t, const node& cand);
    virtual void ext__net__send(const node& dst, const msg& v);
    virtual node ext__node__next(const node& x);
    virtual void ext__system__report_commits(int firstix, int lastix);
    virtual nset__arr ext__nset__arr__empty();
    virtual void imp__shim__send_debug(const msg& m);
    virtual int ext__term__add(int x, int y);
    virtual void ext__hist__append(hist& h, int ix, int logt, __strlit v);
    virtual int ext__index__next(int x);
    virtual void ext__send_vote_cand_msg(const node& n, int t, const node& cand);
    virtual void ext__safetyproof__set_leader_ghost(int t, const node& leader, const nset& voters);
    virtual nset ext__replierslog__get(const replierslog& a, int x);
    virtual void ext__nset__arr__append(nset__arr& a, const node& v);
    virtual bool ext__node__is_max(const node& x);
    virtual node__iter__t ext__node__iter__create(const node& x);
    virtual void ext__net__recv(const msg& v);
    virtual nset ext__localstate__get_repliers(int ix);
    virtual hist__arrlog ext__hist__arrlog__empty();
    virtual replierslog ext__replierslog__empty();
    virtual void sec__impl__handle_timeout();
    virtual int ext__typeconvert__from_nodeid_to_term(const node& n);
    virtual commited_ix ext__commited_ix__set(int maxix, int maxt, const nset& q);
    virtual void ext__replierslog__resize(replierslog& a, int s, const nset& v);
    virtual void ext__shim__handle_nack(const node& n, int t, int ix);
    virtual void ext__send_rqst_vote_m_nolog(const node& cand, int t);
    virtual int ext__index__prev(int x);
    virtual void ext__localstate__delay_leader_election();
    virtual void ext__replierslog__set(replierslog& a, int x, const nset& y);
    virtual void ext__localstate__increase_time();
    virtual void __init();
    virtual void net__impl__handle_recv(const msg& x);
    virtual int ext__localstate__get_term();
    virtual void ext__localstate__move_to_term(int t);
    virtual void ext__send_rqst_vote_msg(const node& cand, int logt, int ix, int t);
    virtual void ext__system__commited_at_ix(int ix, __strlit v);
    virtual void ext__hist__arrlog__resize(hist__arrlog& a, int s, const hist__ent& v);
    virtual void ext__send_keepalive(int t);
    virtual bool ext__system__req_append_new_entry(__strlit v);
    virtual msg ext__msg_append_send(int t, __strlit v, int logt, int ix, int prevlogt, bool isrecover, const node& recovernode);
    virtual void ext__send_nack(const node& leader, int t, int ix);
    virtual void ext__localstate__add_replier(int ix, const node& replier);
    virtual commited_ix ext__commited_ix__empty();
    virtual void ext__shim__handle_append_entries(const msg& m);
    virtual void ext__shim__handle_vote_cand_msg(const node& cand, int t, const node& voter);
    void __tick(int timeout);
};
inline bool operator ==(const raft::node__iter__t &s, const raft::node__iter__t &t){
    return ((s.is_end == t.is_end) && (s.val == t.val));
}
inline bool operator ==(const raft::nset &s, const raft::nset &t){
    return ((s.repr == t.repr));
}
inline bool operator ==(const raft::hist__ent &s, const raft::hist__ent &t){
    return ((s.ent_logt == t.ent_logt) && (s.ent_value == t.ent_value));
}
inline bool operator ==(const raft::hist &s, const raft::hist &t){
    return ((s.repr == t.repr));
}
inline bool operator ==(const raft::msg &s, const raft::msg &t){
    return ((s.m_kind == t.m_kind) && (s.m_ix == t.m_ix) && (s.m_term == t.m_term) && (s.m_node == t.m_node) && (s.m_cand == t.m_cand) && (s.m_val == t.m_val) && (s.m_logt == t.m_logt) && (s.m_prevlogt == t.m_prevlogt) && (s.m_isrecover == t.m_isrecover));
}
inline bool operator ==(const raft::commited_ix &s, const raft::commited_ix &t){
    return ((s.last_ix == t.last_ix) && (s.comm_term == t.comm_term) && (s.non_empty == t.non_empty) && (s.commit_q == t.commit_q));
}
inline bool operator ==(const raft::node_term &s, const raft::node_term &t){
    return ((s._term == t._term));
}
