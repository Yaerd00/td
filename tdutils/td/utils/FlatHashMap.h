//
// Copyright Aliaksei Levin (levlam@telegram.org), Arseny Smirnov (arseny30@gmail.com) 2014-2022
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "td/utils/common.h"

#include <cstddef>
#include <functional>
#include <new>
#include <unordered_map>
#include <utility>

namespace td {

template <class KeyT, class ValueT, class HashT = std::hash<KeyT>, class EqualT = std::equal_to<KeyT>>
class FlatHashMapImpl {
  struct Node {
    KeyT first{};
    union {
      ValueT second;
    };
    const auto &key() const {
      return first;
    }
    auto &value() {
      return second;
    }

    Node() {
    }
    ~Node() {
      if (!empty()) {
        second.~ValueT();
      }
    }
    Node(Node &&other) noexcept {
      *this = std::move(other);
    }
    Node &operator=(Node &&other) noexcept {
      DCHECK(empty());
      DCHECK(!other.empty());
      first = std::move(other.first);
      other.first = KeyT{};
      new (&second) ValueT(std::move(other.second));
      other.second.~ValueT();
      return *this;
    }
    bool empty() const {
      return is_key_empty(key());
    }
    void clear() {
      DCHECK(!empty());
      first = KeyT();
      second.~ValueT();
      DCHECK(empty());
    }
    template <class... ArgsT>
    void emplace(KeyT key, ArgsT &&...args) {
      DCHECK(empty());
      first = std::move(key);
      new (&second) ValueT(std::forward<ArgsT>(args)...);
      CHECK(!empty());
    }
  };
  using Self = FlatHashMapImpl<KeyT, ValueT, HashT, EqualT>;
  using NodeIterator = typename std::vector<Node>::iterator;
  using ConstNodeIterator = typename std::vector<Node>::const_iterator;

 public:
  struct Iterator {
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = Node;
    using pointer = Node *;
    using reference = Node &;

    friend class FlatHashMapImpl;
    Iterator &operator++() {
      do {
        ++it_;
      } while (it_ != map_->nodes_.end() && it_->empty());
      return *this;
    }
    Iterator &operator--() {
      do {
        --it_;
      } while (it_->empty());
      return *this;
    }
    Node &operator*() {
      return *it_;
    }
    Node *operator->() {
      return &*it_;
    }
    bool operator==(const Iterator &other) const {
      DCHECK(map_ == other.map_);
      return it_ == other.it_;
    }
    bool operator!=(const Iterator &other) const {
      DCHECK(map_ == other.map_);
      return it_ != other.it_;
    }

    Iterator() = default;
    Iterator(const Iterator &other) = default;
    Iterator &operator=(const Iterator &other) = default;
    Iterator(Iterator &&other) = default;
    Iterator &operator=(Iterator &&other) = default;
    Iterator(NodeIterator it, Self *map) : it_(std::move(it)), map_(map) {
    }

   private:
    NodeIterator it_;
    Self *map_;
  };
  struct ConstIterator {
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = Node;
    using pointer = const Node *;
    using reference = const Node &;

    friend class FlatHashMapImpl;
    ConstIterator &operator++() {
      do {
        ++it_;
      } while (it_ != map_->nodes_.end() && it_->empty());
      return *this;
    }
    ConstIterator &operator--() {
      do {
        --it_;
      } while (it_->empty());
      return *this;
    }
    const Node &operator*() {
      return *it_;
    }
    const Node *operator->() {
      return &*it_;
    }
    bool operator==(const ConstIterator &other) const {
      DCHECK(map_ == other.map_);
      return it_ == other.it_;
    }
    bool operator!=(const ConstIterator &other) const {
      DCHECK(map_ == other.map_);
      return it_ != other.it_;
    }

    ConstIterator() = default;
    ConstIterator(const ConstIterator &other) = default;
    ConstIterator &operator=(const ConstIterator &other) = default;
    ConstIterator(ConstIterator &&other) = default;
    ConstIterator &operator=(ConstIterator &&other) = default;
    ConstIterator(ConstNodeIterator it, const Self *map) : it_(std::move(it)), map_(map) {
    }
    ConstIterator(Iterator iterator) : it_(std::move(iterator.it_)), map_(iterator.map_) {
    }

   private:
    ConstNodeIterator it_;
    const Self *map_;
  };

  using iterator = Iterator;
  using key_type = KeyT;
  using value_type = std::pair<const KeyT, ValueT>;

  FlatHashMapImpl() = default;
  FlatHashMapImpl(FlatHashMapImpl &&other) noexcept : nodes_(std::move(other.nodes_)), used_nodes_(other.used_nodes_) {
    other.used_nodes_ = 0;
  }
  FlatHashMapImpl &operator=(FlatHashMapImpl &&other) noexcept {
    nodes_ = std::move(other.nodes_);
    used_nodes_ = other.used_nodes_;
    other.used_nodes_ = 0;
    return *this;
  }
  template <class ItT>
  FlatHashMapImpl(ItT begin, ItT end) {
    assign(begin, end);
  }
  FlatHashMapImpl(const FlatHashMapImpl &other) : FlatHashMapImpl(other.begin(), other.end()) {
  }
  FlatHashMapImpl &operator=(const FlatHashMapImpl &other) {
    assign(other.begin(), other.end());
    return *this;
  }

  template <class ItT>
  void assign(ItT begin, ItT end) {
    resize(std::distance(begin, end));  // TODO: should be conditional
    for (; begin != end; ++begin) {
      emplace(begin->first, begin->second);
    }
  }

  Iterator find(const KeyT &key) {
    if (empty()) {
      return end();
    }
    auto it = find_bucket_for_insert(key);
    if (it->empty()) {
      return end();
    }
    return Iterator(it, this);
  }
  ConstIterator find(const KeyT &key) const {
    if (empty()) {
      return end();
    }
    auto it = find_bucket_for_insert(key);
    if (it->empty()) {
      return end();
    }
    return ConstIterator(it, this);
  }
  size_t size() const {
    return used_nodes_;
  }
  bool empty() const {
    return size() == 0;
  }
  auto begin() {
    if (empty()) {
      return end();
    }
    auto it = nodes_.begin();
    while (it->empty()) {
      ++it;
    }
    return Iterator(it, this);
  }
  auto end() {
    return Iterator(nodes_.end(), this);
  }
  auto begin() const {
    if (empty()) {
      return end();
    }
    auto it = nodes_.begin();
    while (it->empty()) {
      ++it;
    }
    return ConstIterator(it, this);
  }
  auto end() const {
    return ConstIterator(nodes_.end(), this);
  }

  template <class... ArgsT>
  std::pair<Iterator, bool> emplace(KeyT key, ArgsT &&...args) {
    if (should_resize()) {
      resize(used_nodes_ + 1);
    }
    auto it = find_bucket_for_insert(key);
    if (it->empty()) {
      it->emplace(std::move(key), std::forward<ArgsT>(args)...);
      used_nodes_++;
      return std::make_pair(Iterator(it, this), true);
    }
    return std::make_pair(Iterator(it, this), false);
  }

  ValueT &operator[](const KeyT &key) {
    DCHECK(!is_key_empty(key));

    if (should_resize()) {
      resize(used_nodes_ + 1);
    }

    auto it = find_bucket_for_insert(key);
    if (it->empty()) {
      it->emplace(key);
      used_nodes_++;
    }
    return it->second;
  }

  size_t erase(const KeyT &key) {
    auto it = find(key);
    if (it == end()) {
      return 0;
    }
    erase(it);
    return 1;
  }
  size_t count(const KeyT &key) const {
    return find(key) != end();
  }

  void clear() {
    used_nodes_ = 0;
    nodes_.clear();
  }

  void erase(Iterator it) {
    DCHECK(it != end());
    DCHECK(!is_key_empty(it->key()));
    size_t empty_i = it.it_ - nodes_.begin();
    auto empty_bucket = empty_i;
    DCHECK(0 <= empty_i && empty_i < nodes_.size());
    nodes_[empty_bucket].clear();
    used_nodes_--;

    for (size_t test_i = empty_i + 1;; test_i++) {
      auto test_bucket = test_i;
      if (test_bucket >= nodes_.size()) {
        test_bucket -= nodes_.size();
      }

      if (is_key_empty(nodes_[test_bucket].key())) {
        break;
      }

      auto want_i = HashT()(nodes_[test_bucket].key()) % nodes_.size();
      if (want_i < empty_i) {
        want_i += nodes_.size();
      }

      if (want_i <= empty_i || want_i > test_i) {
        nodes_[empty_bucket] = std::move(nodes_[test_bucket]);
        empty_i = test_i;
        empty_bucket = test_bucket;
      }
    }
  }

 private:
  static bool is_key_empty(const KeyT &key) {
    return key == KeyT();
  }
  std::vector<Node> nodes_;
  size_t used_nodes_{};

  bool should_resize() const {
    return (used_nodes_ + 1) * 10 > nodes_.size() * 6;
  }
  size_t calc_bucket(const KeyT &key) const {
    return HashT()(key) % nodes_.size();
  }
  auto find_bucket_for_insert(const KeyT &key) {
    size_t bucket = calc_bucket(key);
    while (!(nodes_[bucket].key() == key) && !is_key_empty(nodes_[bucket].key())) {
      bucket++;
      if (bucket == nodes_.size()) {
        bucket = 0;
      }
    }
    return nodes_.begin() + bucket;
  }
  auto find_bucket_for_insert(const KeyT &key) const {
    size_t bucket = calc_bucket(key);
    while (!EqualT()(nodes_[bucket].key(), key) && !is_key_empty(nodes_[bucket].key())) {
      bucket++;
      if (bucket == nodes_.size()) {
        bucket = 0;
      }
    }
    return nodes_.begin() + bucket;
  }
  void resize(size_t size) {
    auto old_nodes = std::move(nodes_);
    nodes_.resize(td::max(old_nodes.size(), size) * 2 + 1);  // TODO: some other logic
    for (auto &node : old_nodes) {
      if (is_key_empty(node.key())) {
        continue;
      }
      auto new_node = find_bucket_for_insert(node.key());
      *new_node = std::move(node);
    }
  }
};

//template <class KeyT, class ValueT, class HashT = std::hash<KeyT>, class EqualT = std::equal_to<KeyT>>
//using FlatHashMap = FlatHashMapImpl<KeyT, ValueT, HashT, EqualT>;

template <class KeyT, class ValueT, class HashT = std::hash<KeyT>, class EqualT = std::equal_to<KeyT>>
using FlatHashMap = std::unordered_map<KeyT, ValueT, HashT, EqualT>;

}  // namespace td
