//
// Created by 唐宇杰 on 2020/10/27.
//

#ifndef CONCURRENT_LRU_CACHE_CONCURRENT_LINKED_LIST_H
#define CONCURRENT_LRU_CACHE_CONCURRENT_LINKED_LIST_H

#include <mutex>
#include <list>
#include <algorithm>

namespace concurrent_linked_list {
    using namespace std;

    template<class Value>
    class ThreadSafeLinkedList {
        mutex m_mutex;
        list<Value> m_list;
    public:
        typename list<Value>::iterator erase(const typename list<Value>::iterator &value);
        void push_back(const Value &value);
        void push_front(const Value &value);
        void pop_back();
        Value& back();
        typename list<Value>::iterator begin();
        unsigned long size();
        bool exist(const Value &value);
        typedef typename list<Value>::iterator iterator;
    };

    template<class Value>
    typename list<Value>::iterator ThreadSafeLinkedList<Value>::erase(const typename list<Value>::iterator &value) {
        lock_guard<mutex> lock(m_mutex);
        return m_list.erase(value);
    }

    template<class Value>
    void ThreadSafeLinkedList<Value>::push_back(const Value &value) {
        lock_guard<mutex> lock(m_mutex);
        return m_list.push_back(value);
    }

    template<class Value>
    void ThreadSafeLinkedList<Value>::push_front(const Value &value) {
        lock_guard<mutex> lock(m_mutex);
        m_list.push_front(value);
    }

    template<class Value>
    typename list<Value>::iterator ThreadSafeLinkedList<Value>::begin() {
        lock_guard<mutex> lock(m_mutex);
        return m_list.begin();
    }

    template<class Value>
    unsigned long ThreadSafeLinkedList<Value>::size() {
        lock_guard<mutex> lock(m_mutex);
        return m_list.size();
    }

    template<class Value>
    void ThreadSafeLinkedList<Value>::pop_back() {
        lock_guard<mutex> lock(m_mutex);
        return m_list.pop_back();
    }

    template<class Value>
    Value& ThreadSafeLinkedList<Value>::back() {
        lock_guard<mutex> lock(m_mutex);
        return m_list.back();
    }

    template<class Value>
    bool ThreadSafeLinkedList<Value>::exist(const Value &value) {
        return find(m_list.back(), m_list.end(), value) != m_list.end();
    }

}


#endif //CONCURRENT_LRU_CACHE_CONCURRENT_LINKED_LIST_H
