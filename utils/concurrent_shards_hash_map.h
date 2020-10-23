//
// Created by 唐宇杰 on 2020/10/20.
//

#ifndef CONCURRENT_LRU_CACHE_CONCURRENT_SHARDS_HASH_MAP_H
#define CONCURRENT_LRU_CACHE_CONCURRENT_SHARDS_HASH_MAP_H

#include <vector>
#include "concurrent_hash_map.h"

namespace concurrent_shards_hash_map {

    using namespace std;
    using namespace concurrent_hash_map;

    template<class Key, class Value>
    class ShardsThreadSafeHashMap {
        const size_t m_mask;
        std::vector<ThreadSafeHashMap <Key, Value>> m_shards;

        ThreadSafeHashMap <Key, Value> &get_shard(const Key &key) {
            std::hash<Key> hash_fn;
            auto h = hash_fn(key);
            return m_shards[h & m_mask];
        }

    public:
        ShardsThreadSafeHashMap(size_t num_shard) : m_mask(num_shard - 1), m_shards(num_shard) {
            if ((num_shard & m_mask) != 0)
                throw std::runtime_error("num_shard must be a power of two");
        }

        void put(const Key &key, Value value);

        typename unordered_map<Key, Value>::iterator find(const Key &key);

        typename unordered_map<Key, Value>::iterator end();

        bool remove(const Key &key);

        void clear();

        Value& operator[](const Key &key);

        unsigned long size();

        unsigned long erase(const Key &key);
    };

    template<class Key, class Value>
    void ShardsThreadSafeHashMap<Key, Value>::put(const Key &key, Value value) {
        get_shard(key).put(key, value);
    }

    template<class Key, class Value>
    typename unordered_map<Key, Value>::iterator ShardsThreadSafeHashMap<Key, Value>::find(const Key &key) {
        return get_shard(key).find(key);
    }

    template<class Key, class Value>
    typename unordered_map<Key, Value>::iterator ShardsThreadSafeHashMap<Key, Value>::end() {
        return m_shards.back().end();
    }

    template<class Key, class Value>
    bool ShardsThreadSafeHashMap<Key, Value>::remove(const Key &key) {
        return get_shard(key).remove(key);
    }

    template<class Key, class Value>
    void ShardsThreadSafeHashMap<Key, Value>::clear() {
        m_shards.clear();
    }

    template<class Key, class Value>
    Value& ShardsThreadSafeHashMap<Key, Value>::operator[](const Key &key) {
        return get_shard(key)[key];
    }

    template<class Key, class Value>
    unsigned long ShardsThreadSafeHashMap<Key, Value>::size() {
        unsigned long size = 0;
        for (typename std::vector<ThreadSafeHashMap <Key, Value>>::iterator it = m_shards.begin();
        it != m_shards.end();it++) {
            size += (*it).size();
        }
        return size;
    }

    template<class Key, class Value>
    unsigned long ShardsThreadSafeHashMap<Key, Value>::erase(const Key &key) {
        return get_shard(key).erase(key);
    }
}


#endif //CONCURRENT_LRU_CACHE_CONCURRENT_SHARDS_HASH_MAP_H
