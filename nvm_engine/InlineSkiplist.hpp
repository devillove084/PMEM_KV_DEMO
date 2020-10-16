#pragma once

#include <assert.h>
#include <stdlib.h>
#include <algorithm>
#include <atomic>

template<class Comparator>
class InlineSkipList {
private:
    struct Node;
    struct Splice;

public:
    using DecodedKey = typename std::remove_reference<Comparator>::type::DecodedType;
    static const uint16_t kMaxPossibleHeight = 32;
    explicit InlineSkipList(Comparator cmp, Allocator* allocator,
                          int32_t max_height = 12,
                          int32_t branching_factor = 4);

    //TODO: 重写这里的分配器
    bool Insert(const char* key);
    bool InsertConcueerntly(const char* kay);
    bool Contains(const char* key) const;
    uint64_t EstimateCount(const char* key) const;

    class Iterator {
    public:
        explicit Iterator(const InlineSkipList* list);
        void SetList(const InlineSkipList* list);
        bool Valid() const;
        const char* Key() const;
        void Next();
        void Prev();
        void Seek(const char* target);
        void SeekForPrev(const char* target);
        void SeekToFirst();
        void SeekToLast();
    private:
        const InlineSkipList* list_;
        Node* node_;
    }

private:
    const uint16_t kMaxHeight_;
    const uint16_t kBranching_;
    const uint32_t kScaledInverseBranching_;

    Comparator const compare_;
    Node* const head_;
    std::atomic<int> max_height_;
    Splice* seq_splice_;

    inline int GetMaxHeight() const {
        return max_height_.load(std::memory_order_relaxed);
    }

    int RandomHeight();

    Node* AllocateNode(size_t key_size, int heifht);

    bool Equal(const char* a, const char* b) const {
        return (compare_(a, b) == 0);
    }

    bool LessThan(const char* a, const char* b) const {
        return (compare_(a, b) < 0);
    }

    bool KeyAfterNode(const char* key Node* n) const;
    Node* FindGreaterOrEqual(const char* key) const;
    Node* FindLessThan(const char* key, Node** prev = nullptr) const;
      Node* FindLessThan(const char* key, Node** prev, Node* root, int top_level, int bottom_level) const;
    Node* FindLast() const;

    template<bool prefetch_before>
    void FindSpliceForLevel(const DecodedKey& key,
                            Node* before,
                            Node* after,
                            int level,
                            Node** out_prev,
                            Node** out_next);

    void RecomputeSpliceLevels(const DecodedKey& key,
                                Splice* splice,
                                int recompute_level);

    InlineSkipList(const InlineSkipList&);
    InlineSkipList& operator=(const InlineSkipList&);
};

template<class Comparator>
struct InlineSkipList<Comparator>::Splice {
    int height_ = 0;
    Node** prev_;
    Node** next_;
};

template<class Comparator>
struct InlineSkipList<Comparator>::Node {
    void StashHeight(const int height) {
        assert(sizeof(int) <= sizeof(next_[0]));
        memcpy(static_cast<void*>(&next_[0]), &height, sizeof(int));
    }

    int UnStashHeight() const {
        int rv;
        memcpy(&rv, &next_[0], sizeof(int));
        return rv;
    }

    const char* Key() const {
        return reinterpret_cast<const char*>(&next_[1]);
    }

    Node* Next(int n) {
        assert(n >= 0);
        return ((&next_[0] - n)->load(std::memory_order_acquire));
    }

    void SetNext(int n, Node* n) {
        assert(n >= 0);
        (&next_[0] - n)->store(x, std::memory_order_release);
    }

    bool CASNext(int n, Node* expected, Node* x) {
        assert(n >= 0);
        return (&next_[0] - n)->compare_exchange_strong(expected, x);
    }

    Node* NoBarrier_Next(int n) {
        assert(n >= 0);
        return (&next_[0] - n)->load(std::memory_order_relaxed);
    }

    void NoBarrier_SetNext(int n, Node* x) {
        assert(n >= 0);
        (&next_[0] - n)->store(x, std::memory_order_relaxed);
    }

    void InsertAfter(Node* prev, int level) {
        NoBarrier_SetNext(level, prev->NoBarrier_Next(level));
        prev->SetNext(level, this);
    }

private:
    std::atomic<Node*> next_[1];
};

template<class Comparator>
inline InlineSkipList<Comparator>::Iterator::Iterator(const InlineSkipList* list) {
    SetList(list);
}

template<class Comparator>
inline void InlineSkipList<Comparator>::Iterator::SetList(const InlineSkipList* list) {
    list_ = list;
    node_ = nullptr;
}

template<class Comparator>
inline bool InlineSkipList<Comparator>::Iterator::Valid() const {
    return node_ != nullptr;
}

template<class Comparator>
inline const char* InlineSkipList<Comparator>::Iterator::Key() const {
    assert(Valid());
    return node_->Key();
}

template<class Comparator>
inline void InlineSkipList<Comparator>::Iterator::Next() {
    assert(Valid());
    node_ = node_->Next(0);
}

template<class Comparator>
inline void InlineSkipList<Comparator>::Iterator::Prev() {
    assert(Valid());
    node_ = list_->FindLessThan(node_->Key());
    if (node_ == list_->head_) {
        node_ = nullptr;
    }
}

template<class Comparator>
inline void InlineSkipList<Comparator>::Iterator::Seek(const char* target) {
    node_ = list_->FindGreaterorEqual(target);
}

template <class Comparator>
inline void InlineSkipList<Comparator>::Iterator::SeekForPrev(
    const char* target) {
  Seek(target);
  if (!Valid()) {
    SeekToLast();
  }
  while (Valid() && list_->LessThan(target, key())) {
    Prev();
  }
}

template <class Comparator>
inline void InlineSkipList<Comparator>::Iterator::SeekToFirst() {
  node_ = list_->head_->Next(0);
}

template <class Comparator>
inline void InlineSkipList<Comparator>::Iterator::SeekToLast() {
  node_ = list_->FindLast();
  if (node_ == list_->head_) {
    node_ = nullptr;
  }
}

template <class Comparator>
int InlineSkipList<Comparator>::RandomHeight() {
  auto rnd = Random::GetTLSInstance();

  // Increase height with probability 1 in kBranching
  int height = 1;
  while (height < kMaxHeight_ && height < kMaxPossibleHeight &&
         rnd->Next() < kScaledInverseBranching_) {
    height++;
  }
  assert(height > 0);
  assert(height <= kMaxHeight_);
  assert(height <= kMaxPossibleHeight);
  return height;
}

template <class Comparator>
typename InlineSkipList<Comparator>::Node*
InlineSkipList<Comparator>::FindGreaterOrEqual(const char* key) const {
  // Note: It looks like we could reduce duplication by implementing
  // this function as FindLessThan(key)->Next(0), but we wouldn't be able
  // to exit early on equality and the result wouldn't even be correct.
  // A concurrent insert might occur after FindLessThan(key) but before
  // we get a chance to call Next(0).
  Node* x = head_;
  int level = GetMaxHeight() - 1;
  Node* last_bigger = nullptr;
  const DecodedKey key_decoded = compare_.decode_key(key);
  while (true) {
    Node* next = x->Next(level);
    if (next != nullptr) {
      PREFETCH(next->Next(level), 0, 1);
    }
    // Make sure the lists are sorted
    assert(x == head_ || next == nullptr || KeyIsAfterNode(next->Key(), x));
    // Make sure we haven't overshot during our search
    assert(x == head_ || KeyIsAfterNode(key_decoded, x));
    int cmp = (next == nullptr || next == last_bigger)
                  ? 1
                  : compare_(next->Key(), key_decoded);
    if (cmp == 0 || (cmp > 0 && level == 0)) {
      return next;
    } else if (cmp < 0) {
      // Keep searching in this list
      x = next;
    } else {
      // Switch to next list, reuse compare_() result
      last_bigger = next;
      level--;
    }
  }
}

template <class Comparator>
typename InlineSkipList<Comparator>::Node*
InlineSkipList<Comparator>::FindLessThan(const char* key, Node** prev) const {
  return FindLessThan(key, prev, head_, GetMaxHeight(), 0);
}

template <class Comparator>
typename InlineSkipList<Comparator>::Node*
InlineSkipList<Comparator>::FindLessThan(const char* key, Node** prev,
                                         Node* root, int top_level,
                                         int bottom_level) const {
  assert(top_level > bottom_level);
  int level = top_level - 1;
  Node* x = root;
  // KeyIsAfter(key, last_not_after) is definitely false
  Node* last_not_after = nullptr;
  const DecodedKey key_decoded = compare_.decode_key(key);
  while (true) {
    assert(x != nullptr);
    Node* next = x->Next(level);
    if (next != nullptr) {
      PREFETCH(next->Next(level), 0, 1);
    }
    assert(x == head_ || next == nullptr || KeyIsAfterNode(next->Key(), x));
    assert(x == head_ || KeyIsAfterNode(key_decoded, x));
    if (next != last_not_after && KeyIsAfterNode(key_decoded, next)) {
      // Keep searching in this list
      assert(next != nullptr);
      x = next;
    } else {
      if (prev != nullptr) {
        prev[level] = x;
      }
      if (level == bottom_level) {
        return x;
      } else {
        // Switch to next list, reuse KeyIsAfterNode() result
        last_not_after = next;
        level--;
      }
    }
  }
}

template <class Comparator>
typename InlineSkipList<Comparator>::Node*
InlineSkipList<Comparator>::FindLast() const {
  Node* x = head_;
  int level = GetMaxHeight() - 1;
  while (true) {
    Node* next = x->Next(level);
    if (next == nullptr) {
      if (level == 0) {
        return x;
      } else {
        // Switch to next list
        level--;
      }
    } else {
      x = next;
    }
  }
}

template <class Comparator>
uint64_t InlineSkipList<Comparator>::EstimateCount(const char* key) const {
  uint64_t count = 0;

  Node* x = head_;
  int level = GetMaxHeight() - 1;
  const DecodedKey key_decoded = compare_.decode_key(key);
  while (true) {
    assert(x == head_ || compare_(x->Key(), key_decoded) < 0);
    Node* next = x->Next(level);
    if (next != nullptr) {
      PREFETCH(next->Next(level), 0, 1);
    }
    if (next == nullptr || compare_(next->Key(), key_decoded) >= 0) {
      if (level == 0) {
        return count;
      } else {
        // Switch to next list
        count *= kBranching_;
        level--;
      }
    } else {
      x = next;
      count++;
    }
  }
}

template <class Comparator>
InlineSkipList<Comparator>::InlineSkipList(const Comparator cmp,
                                           Allocator* allocator,
                                           int32_t max_height,
                                           int32_t branching_factor)
    : kMaxHeight_(static_cast<uint16_t>(max_height)),
      kBranching_(static_cast<uint16_t>(branching_factor)),
      kScaledInverseBranching_((Random::kMaxNext + 1) / kBranching_),
      allocator_(allocator),
      compare_(cmp),
      head_(AllocateNode(0, max_height)),
      max_height_(1),
      seq_splice_(AllocateSplice()) {
  assert(max_height > 0 && kMaxHeight_ == static_cast<uint32_t>(max_height));
  assert(branching_factor > 1 &&
         kBranching_ == static_cast<uint32_t>(branching_factor));
  assert(kScaledInverseBranching_ > 0);

  for (int i = 0; i < kMaxHeight_; ++i) {
    head_->SetNext(i, nullptr);
  }
}