//
// Created by 唐宇杰 on 2020/10/20.
//

#ifndef CONCURRENT_LRU_CACHE_CONCURRENT_HASH_MAP_H
#define CONCURRENT_LRU_CACHE_CONCURRENT_HASH_MAP_H

#include <mutex>
#include <unordered_map>

namespace concurrent_hash_map {

    using namespace std;

    template<class Key, class Value>
    class ThreadSafeHashMap {
        mutex m_mutex;
        unordered_map<Key, Value> m_map;

    public:
        void put(const Key &key, Value value);

        Value get(const Key &key);

        typename unordered_map<Key, Value>::iterator find(const Key &key);

        typename unordered_map<Key, Value>::iterator end();

        bool remove(const Key &key);

        void clear();

        Value& operator[](const Key &key);

        unsigned long size();

        unsigned long erase(const Key &key);

    };
    template<class Key, class Value>
    void ThreadSafeHashMap<Key, Value>::put(const Key &key, Value value) {
        lock_guard<mutex> lock(m_mutex);
        m_map.emplace(key, value);
    }

    template<class Key, class Value>
    Value ThreadSafeHashMap<Key, Value>::get(const Key &key) {
        lock_guard<mutex> lock(m_mutex);
        auto it = m_map.find(key);
        if (it != m_map.end())
            return (it -> second);
        return {};
    }

    template<class Key, class Value>
    typename unordered_map<Key, Value>::iterator ThreadSafeHashMap<Key, Value>::find(const Key &key) {
        lock_guard<mutex> lock(m_mutex);
        return m_map.find(key);
    }

    template<class Key, class Value>
    typename unordered_map<Key, Value>::iterator ThreadSafeHashMap<Key, Value>::end() {
        lock_guard<mutex> lock(m_mutex);
        return m_map.end();
    }

    template<class Key, class Value>
    bool ThreadSafeHashMap<Key, Value>::remove(const Key &key) {
        lock_guard<mutex> lock(m_mutex);
        auto n = m_map.erase(key);
        return n;
    }

    template<class Key, class Value>
    void ThreadSafeHashMap<Key, Value>::clear() {
        m_map.clear();
    }

    template<class Key, class Value>
    Value& ThreadSafeHashMap<Key, Value>::operator[](const Key &key) {
        lock_guard<mutex> lock(m_mutex);
        return m_map[key];
    }

    template<class Key, class Value>
    unsigned long ThreadSafeHashMap<Key, Value>::size() {
        return m_map.size();
    }

    template<class Key, class Value>
    unsigned long ThreadSafeHashMap<Key, Value>::erase(const Key &key) {
        lock_guard<mutex> lock(m_mutex);
        return m_map.erase(key);
    }
}


#endif //CONCURRENT_LRU_CACHE_CONCURRENT_HASH_MAP_H
