#pragma once

#include <assert.h>
#include <stdlib.h>
#include <algorithm>
#include <atomic>

#include "Allocator.hpp"

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

    Splice* AllocateSplice();
    bool Insert(const char* key);
    bool InsertConcurrently(const char* key);

	template <bool UseCAS>
  	bool Insert(const char* key, Splice* splice, bool allow_partial_splice_fix);

    bool Contains(const char* key) const;
    uint64_t EstimateCount(const char* key) const;
	void TEST_Validate() const;

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
	Allocator* const allocator_;
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

// template <class Comparator>
// char* InlineSkipList<Comparator>::AllocateKey(size_t key_size) {
//   return const_cast<char*>(AllocateNode(key_size, RandomHeight())->Key());
// }

template<class Comparator>
typename InlineSkipList<Comparator>::Node*
InlineSkipList<Comparator>::AllocateNode(size_t key_size, int height) {
	auto prefix = sizeof(std::atomic<Node*>) * (height - 1);
	char* raw = allocator_->AllocateAligned(prefix + sizeof(Node) + key_size);
	Node* x = reinterpret_cast<Node*>(raw + prefix);
	x->StashHeight(height);
	reutrn x;
}


template <class Comparator>
typename InlineSkipList<Comparator>::Splice*
InlineSkipList<Comparator>::AllocateSplice() {
  // size of prev_ and next_
	size_t array_size = sizeof(Node*) * (kMaxHeight_ + 1);
	char* raw = allocator_->AllocateAligned(sizeof(Splice) + array_size * 2);
	Splice* splice = reinterpret_cast<Splice*>(raw);
	splice->height_ = 0;
	splice->prev_ = reinterpret_cast<Node**>(raw + sizeof(Splice));
	splice->next_ = reinterpret_cast<Node**>(raw + sizeof(Splice) + array_size);
	return splice;
}

template <class Comparator>
bool InlineSkipList<Comparator>::Insert(const char* key) {
  	return Insert<false>(key, seq_splice_, false);
}

template <class Comparator>
bool InlineSkipList<Comparator>::InsertConcurrently(const char* key) {
	Node* prev[kMaxPossibleHeight];
	Node* next[kMaxPossibleHeight];
	Splice splice;
	splice.prev_ = prev;
	splice.next_ = next;
	return Insert<true>(key, &splice, false);
}

template <class Comparator>
template <bool prefetch_before>
void InlineSkipList<Comparator>::FindSpliceForLevel(const DecodedKey& key,
                                                    Node* before, Node* after,
                                                    int level, Node** out_prev,
                                                    Node** out_next) {
	while (true) {
		Node* next = before->Next(level);
		if (next != nullptr) {
		PREFETCH(next->Next(level), 0, 1);
		}
		if (prefetch_before == true) {
		if (next != nullptr && level>0) {
			PREFETCH(next->Next(level-1), 0, 1);
		}
		}
		assert(before == head_ || next == nullptr ||
			KeyIsAfterNode(next->Key(), before));
		assert(before == head_ || KeyIsAfterNode(key, before));
		if (next == after || !KeyIsAfterNode(key, next)) {
		// found it
		*out_prev = before;
		*out_next = next;
		return;
		}
		before = next;
	}
}

template <class Comparator>
void InlineSkipList<Comparator>::RecomputeSpliceLevels(const DecodedKey& key,
                                                       Splice* splice,
                                                       int recompute_level) {
	assert(recompute_level > 0);
	assert(recompute_level <= splice->height_);
	for (int i = recompute_level - 1; i >= 0; --i) {
		FindSpliceForLevel<true>(key, splice->prev_[i + 1], splice->next_[i + 1], i,
						&splice->prev_[i], &splice->next_[i]);
	}
}

template <class Comparator>
template <bool UseCAS>
bool InlineSkipList<Comparator>::Insert(const char* key, Splice* splice,
                                        bool allow_partial_splice_fix) {
	Node* x = reinterpret_cast<Node*>(const_cast<char*>(key)) - 1;
	const DecodedKey key_decoded = compare_.decode_key(key);
	int height = x->UnstashHeight();
	assert(height >= 1 && height <= kMaxHeight_);

	int max_height = max_height_.load(std::memory_order_relaxed);
	while (height > max_height) {
		if (max_height_.compare_exchange_weak(max_height, height)) {
		// successfully updated it
		max_height = height;
		break;
		}
	}
	assert(max_height <= kMaxPossibleHeight);

	int recompute_height = 0;
	if (splice->height_ < max_height) {
		splice->prev_[max_height] = head_;
		splice->next_[max_height] = nullptr;
		splice->height_ = max_height;
		recompute_height = max_height;
	} else {
		while (recompute_height < max_height) {
		if (splice->prev_[recompute_height]->Next(recompute_height) !=
			splice->next_[recompute_height]) {
			++recompute_height;
		} else if (splice->prev_[recompute_height] != head_ &&
					!KeyIsAfterNode(key_decoded,
									splice->prev_[recompute_height])) {
			// key is from before splice
			if (allow_partial_splice_fix) {
			// skip all levels with the same node without more comparisons
			Node* bad = splice->prev_[recompute_height];
			while (splice->prev_[recompute_height] == bad) {
				++recompute_height;
			}
			} else {
			// we're pessimistic, recompute everything
			recompute_height = max_height;
			}
		} else if (KeyIsAfterNode(key_decoded,
									splice->next_[recompute_height])) {
			// key is from after splice
			if (allow_partial_splice_fix) {
			Node* bad = splice->next_[recompute_height];
			while (splice->next_[recompute_height] == bad) {
				++recompute_height;
			}
			} else {
			recompute_height = max_height;
			}
		} else {
			// this level brackets the key, we won!
			break;
		}
		}
	}
	assert(recompute_height <= max_height);
	if (recompute_height > 0) {
		RecomputeSpliceLevels(key_decoded, splice, recompute_height);
	}

	bool splice_is_valid = true;
	if (UseCAS) {
		for (int i = 0; i < height; ++i) {
		while (true) {
			// Checking for duplicate keys on the level 0 is sufficient
			if (UNLIKELY(i == 0 && splice->next_[i] != nullptr &&
						compare_(x->Key(), splice->next_[i]->Key()) >= 0)) {
			// duplicate key
			return false;
			}
			if (UNLIKELY(i == 0 && splice->prev_[i] != head_ &&
						compare_(splice->prev_[i]->Key(), x->Key()) >= 0)) {
			// duplicate key
			return false;
			}
			assert(splice->next_[i] == nullptr ||
				compare_(x->Key(), splice->next_[i]->Key()) < 0);
			assert(splice->prev_[i] == head_ ||
				compare_(splice->prev_[i]->Key(), x->Key()) < 0);
			x->NoBarrier_SetNext(i, splice->next_[i]);
			if (splice->prev_[i]->CASNext(i, splice->next_[i], x)) {
			// success
			break;
			}
			FindSpliceForLevel<false>(key_decoded, splice->prev_[i], nullptr, i,
									&splice->prev_[i], &splice->next_[i]);
			if (i > 0) {
			splice_is_valid = false;
			}
		}
		}
	} else {
		for (int i = 0; i < height; ++i) {
		if (i >= recompute_height &&
			splice->prev_[i]->Next(i) != splice->next_[i]) {
			FindSpliceForLevel<false>(key_decoded, splice->prev_[i], nullptr, i,
									&splice->prev_[i], &splice->next_[i]);
		}
		// Checking for duplicate keys on the level 0 is sufficient
		if (UNLIKELY(i == 0 && splice->next_[i] != nullptr &&
					compare_(x->Key(), splice->next_[i]->Key()) >= 0)) {
			// duplicate key
			return false;
		}
		if (UNLIKELY(i == 0 && splice->prev_[i] != head_ &&
					compare_(splice->prev_[i]->Key(), x->Key()) >= 0)) {
			// duplicate key
			return false;
		}
		assert(splice->next_[i] == nullptr ||
				compare_(x->Key(), splice->next_[i]->Key()) < 0);
		assert(splice->prev_[i] == head_ ||
				compare_(splice->prev_[i]->Key(), x->Key()) < 0);
		assert(splice->prev_[i]->Next(i) == splice->next_[i]);
		x->NoBarrier_SetNext(i, splice->next_[i]);
		splice->prev_[i]->SetNext(i, x);
		}
	}
	if (splice_is_valid) {
		for (int i = 0; i < height; ++i) {
		splice->prev_[i] = x;
		}
		assert(splice->prev_[splice->height_] == head_);
		assert(splice->next_[splice->height_] == nullptr);
		for (int i = 0; i < splice->height_; ++i) {
		assert(splice->next_[i] == nullptr ||
				compare_(key, splice->next_[i]->Key()) < 0);
		assert(splice->prev_[i] == head_ ||
				compare_(splice->prev_[i]->Key(), key) <= 0);
		assert(splice->prev_[i + 1] == splice->prev_[i] ||
				splice->prev_[i + 1] == head_ ||
				compare_(splice->prev_[i + 1]->Key(), splice->prev_[i]->Key()) <
					0);
		assert(splice->next_[i + 1] == splice->next_[i] ||
				splice->next_[i + 1] == nullptr ||
				compare_(splice->next_[i]->Key(), splice->next_[i + 1]->Key()) <
					0);
		}
	} else {
		splice->height_ = 0;
	}
	return true;
}

template <class Comparator>
bool InlineSkipList<Comparator>::Contains(const char* key) const {
	Node* x = FindGreaterOrEqual(key);
	if (x != nullptr && Equal(key, x->Key())) {
		return true;
	} else {
		return false;
	}
}

template <class Comparator>
void InlineSkipList<Comparator>::TEST_Validate() const {
	Node* nodes[kMaxPossibleHeight];
	int max_height = GetMaxHeight();
	assert(max_height > 0);
	for (int i = 0; i < max_height; i++) {
		nodes[i] = head_;
	}
	while (nodes[0] != nullptr) {
		Node* l0_next = nodes[0]->Next(0);
		if (l0_next == nullptr) {
		break;
		}
		assert(nodes[0] == head_ || compare_(nodes[0]->Key(), l0_next->Key()) < 0);
		nodes[0] = l0_next;

		int i = 1;
		while (i < max_height) {
		Node* next = nodes[i]->Next(i);
		if (next == nullptr) {
			break;
		}
		auto cmp = compare_(nodes[0]->Key(), next->Key());
		assert(cmp <= 0);
		if (cmp == 0) {
			assert(next == nodes[0]);
			nodes[i] = next;
		} else {
			break;
		}
		i++;
		}
	}
	for (int i = 1; i < max_height; i++) {
		assert(nodes[i] != nullptr && nodes[i]->Next(i) == nullptr);
	}
}