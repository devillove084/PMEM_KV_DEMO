//
// Created by 唐宇杰 on 2020/10/20.
//

#ifndef CONCURRENT_LRU_CACHE_LFU_H
#define CONCURRENT_LRU_CACHE_LFU_H

#include <list>
#include <unordered_map>
#include "concurrent_shards_hash_map.h"

namespace lfu {
    using namespace std;
    using namespace concurrent_shards_hash_map;

    template<class Key, class Value>
    struct Node {
        Key key;
        Value value;
        int freq;

        Node(Key _key, Value _val, int _freq) : key(_key), value(_val), freq(_freq) {}
    };

    template<class Key, class Value>
    class LFUCache {
        int minfreq, capacity;
        ThreadSafeHashMap<Key, typename list<Node<Key, Value>>::iterator> key_table;
        ThreadSafeHashMap<int, list<Node<Key, Value>>> freq_table;
    public:
        LFUCache(int _capacity) {
            minfreq = 0;
            capacity = _capacity;
            key_table.clear();
            freq_table.clear();
        }

        Value get(Key key);

        void put(Key key, Value value);
        unsigned long size();
    };

    template <typename Key, typename Value>
    Value LFUCache<Key, Value>::get(Key key) {
        if (capacity == 0) return NULL;
        auto it = key_table.find(key);
        if (it == key_table.end()) return NULL;
        Node<Key, Value> node = *(it -> second);
        Value val = node.value;
        int freq = node.freq;
        freq_table[freq].erase(it -> second);
        // 如果当前链表为空，我们需要在哈希表中删除，且更新minFreq
        if (freq_table[freq].size() == 0) {
            freq_table.erase(freq);
            if (minfreq == freq) minfreq += 1;
        }
        // 插入到 freq + 1 中
        freq_table[freq + 1].push_front(Node<Key, Value>(key, val, freq + 1));
        key_table[key] = freq_table[freq + 1].begin();
        return val;
    }

    template <typename Key, typename Value>
    void LFUCache<Key, Value>::put(Key key, Value value) {
        if (capacity == 0) return;
        auto it = key_table.find(key);
        if (it == key_table.end()) {
            // 缓存已满，需要进行删除操作
            if (key_table.size() == capacity) {
                // 通过 minFreq 拿到 freq_table[minFreq] 链表的末尾节点
                auto it2 = freq_table[minfreq].back();
                key_table.erase(it2.key);
                freq_table[minfreq].pop_back();
                if (freq_table[minfreq].size() == 0) {
                    freq_table.erase(minfreq);
                }
            }
            freq_table[1].push_front(Node<Key, Value>(key, value, 1));
            key_table[key] = freq_table[1].begin();
            minfreq = 1;
        } else {
            // 与 get 操作基本一致，除了需要更新缓存的值
            Node<Key, Value> node = *(it -> second);
            int freq = node.freq;
            freq_table[freq].erase(it -> second);
            if (freq_table[freq].size() == 0) {
                freq_table.erase(freq);
                if (minfreq == freq) minfreq += 1;
            }
            freq_table[freq + 1].push_front(Node<Key, Value>(key, value, freq + 1));
            key_table[key] = freq_table[freq + 1].begin();
        }
    }

    template <typename Key, typename Value>
    unsigned long LFUCache<Key, Value>::size() {
        return key_table.size();
    }

}
#endif //CONCURRENT_LRU_CACHE_LFU_H
