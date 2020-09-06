# LFU 解释和实现细节

LFU全称是最不经常使用算法（Least Frequently Used），LFU算法的基本思想和所有的缓存算法一样，都是基于locality假设(局部性原理):

> 如果一个信息项正在被访问，那么在近期它很可能还会被再次访问。

LFU是基于这种思想进行设计：**一定时期内被访问次数最少的页，在将来被访问到的几率也是最小的**。



相比于LRU（Least Recently Use）算法，LFU更加注重于使用的频率。

### 原理

LFU将数据和数据访问的频次保存在一个容量有限的容器中，当访问其中的一个元素时：

* 该元素在容器中，则将其的访问频次加1。
* 该元素不在容器中，则加入到容器中，且访问频次为1。

当数据量达到容器的限制后，会剔除掉访问频次最低的数据。

### 实现

```c++
#include <iostream>
#include <vector>
#include <list>
#include <unordered_map>
#include <unordered_set>
using namespace std;
///
// LFU - Least Frequently Used
class FreqNode{
public:
        int freq;
        // We use a SET in lieu of a linked list for storing elements with the same access frequency
    // for simplicitly of implementation.
        // HASH SET structure which holds the keys of such elements that have the same access frequency.
    // Its insertion, lookup and deletion runtime complexity is O(1).
        unordered_set<int> items;
        FreqNode(int f){
                this->freq = f;
        }
};
 
// LFU - Least Frequently Used Cache Algorithm
// 1 Linked List + 1 Set + 1 Hash
// O(1) for all: Insert, Delete and Lookup
class LFUCache{
public:
        list<FreqNode*> freqNodes;
        // key   : item (key of element)
        // value : point to its parent freqNode.
        //         Those nodes with the same access frequency share a single parent.
        unordered_map<int, list<FreqNode*>::iterator> bykey;
 
        FreqNode* createNode(int freq,
                list<FreqNode*>::iterator next){
                FreqNode* newNode = new FreqNode(freq);
                if(newNode == NULL){
                        return NULL;
                }
                freqNodes.insert(next, newNode);
                return newNode;
        }
 
        void deleteNode(list<FreqNode*>::iterator it){
                freqNodes.erase(it);
        }
public:
        LFUCache(){
        }
        ~LFUCache(){
                for(auto it = freqNodes.begin(); it != freqNodes.end(); it++){
                        delete(*it);
                }
        }
 
        // Access (fetch) an element from the LFU cache,
        // simultaneously incrementing its usage count.
        // Assuming the element(key) already exists beforehand.
        // O(1)
        void access(int key){
                if(bykey.find(key) == bykey.end()){
                        // not exist
                        return ;
                }
                list<FreqNode*>::iterator nodeIt = bykey[key];
                int freqNow =(*nodeIt)->freq;
 
                list<FreqNode*>::iterator nextNodeIt = nodeIt;
                nextNodeIt++;
                if(nextNodeIt == freqNodes.end() || (*nextNodeIt)->freq != (freqNow + 1)){
                        createNode(freqNow+1, nextNodeIt);
                }
                nextNodeIt = nodeIt;
                nextNodeIt++;
                bykey[key] = nextNodeIt;
                (*nextNodeIt)->items.insert(key);
 
                (*nodeIt)->items.erase(key);
                if((*nodeIt)->items.empty()){
                        deleteNode(nodeIt);
                }
        }
 
        // Insert a new element.
        // You must make sure no existing element(key) before calling this.
        // O(1)
        void insert(int key){
                if(freqNodes.empty() || (*(freqNodes.begin()))->freq != 1){
                        FreqNode* newNode = new FreqNode(1);
                        freqNodes.insert(freqNodes.begin(), newNode);
                }
                auto nodeIt = freqNodes.begin();
                (*nodeIt)->items.insert(key);
                bykey[key] = nodeIt;
        }
 
        // Look up an item with key, return its usage count
        // O(1)
        int query(int key){
                if(bykey.find(key) == bykey.end()){
                        // error out
                        return -1;
                }
                return (*bykey[key])->freq;
        }
 
        // Fetches an item with the least usage count (the least frequently used item)
        // in the cache
        // O(1)
        int getLFUItem(){
                if(freqNodes.empty()){
                        // error out
                        return -1;
                }
                return (*freqNodes.begin())->freq;
        }
};
```

