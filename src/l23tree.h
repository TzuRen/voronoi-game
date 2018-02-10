//
// l23tree.h
//
// Implementation of a "leafy" 2-3 tree. This tree follows the "classical"
// definition of a 2-3 tree, where internal nodes contain only copies of the
// keys, and data nodes are only present in the leaves.
// We call it a "leafy 2-3 tree", or "l23tree", since data are only in leaves
// (and since C++ identifiers cannot start with numbers).
//

#include <utility> // pair
#include <array>
#include <list>
#include <algorithm>
#include <functional>
#include <stdexcept>

#include <cstdint>

namespace tree
{

//! Tree parameters. You can make your own, but they should contain the members
//  that the base one here contains at least.
template<class Key, class Data, class KeyCompare,
  class Value=std::pair<Key, Data> >
struct l23_default_params
{
  typedef Key key_type;
  typedef Data data_type;
  typedef Value value_type;
  typedef KeyCompare key_compare;
  typedef uint8_t size_type;

  // Must implement for your Value class.
  static const key_type &key(const value_type& v) { return v.first; }
  static key_type &key(value_type& v) { return v.first; }
  static const data_type &data(const value_type& v) { return v.second; }
  static data_type &data(value_type& v) { return v.second; }
};

// Some forward class declarations.
template<class Params> class l23_inode;
template<class Params> class l23_lnode;
template<class Params> class l23_iterator;
template<class Params> class l23_tree;

//! 2-3 tree (i)nternal node.
template<class Params>
class l23_inode
{
public:
  // Typedefs.
  typedef typename Params::key_type key_type;
  typedef typename Params::key_type data_type;
  typedef typename Params::value_type value_type;
  typedef typename Params::key_type key_compare;
  typedef typename Params::size_type size_type;
  typedef l23_inode<Params> inode_type;
  typedef l23_lnode<Params> lnode_type;
  typedef l23_tree<Params> tree_type;
  friend class lnode_type;
  friend class tree_type;

  // Various array types. We never hold more than 3 of anything.
  typedef std::array<key_type, 2> key_array; // key copies for indexing
  typedef std::array<inode_type*, 3> node_array; // internal node children
  typedef std::array<lnode_type*, 3> leaf_array; // leaf children

public:
  // Methods.
  inline inode_type* parent(void) const { return parent_; }
  inline size_type pos(void) const { return ppos_; }
  static inline size_type max_count(void) { return 3u; }
  static inline size_type max_keys(void) { return 2u; }
  // Number of internal child nodes. This will still return the number of leaf
  // children if our children are leaves. Always 0, 2, or 3 for non-leaves.
  inline size_type child_count(void) const { return child_count_; }
  // Get internal child i. Unchecked: valid only for 0 <= i < child_count().
  inline const inode_type* child(size_type i) const { return u_.children[i]; }
  inline inode_type* child(size_type i) { return u_.children[i]; }

  inline inode_type* left_sibling(void) {
    if (parent() && pos() > 0)
      return parent()->child(pos()-1);
    return nullptr;
  }
  inline inode_type* right_sibling(void) {
    if (parent() && pos() < max_count()-1)
      return parent()->child(pos()+1);
    return nullptr;
  }

  inline inode_type* left_child(void) { return child(0); }
  inline inode_type* middle_child(void) { return child(1); }
  inline inode_type* right_child(void) { return child(2); }
  inline const inode_type* left_child(void) const { return child(0); }
  inline const inode_type* middle_child(void) const { return child(1); }
  inline const inode_type* right_child(void) const { return child(2); }

  // Whether our children are leaf nodes (true) or internal nodes (false).
  inline bool has_leaves(void) const { return leaf_; }
  // Number of leaves. 0 unless has_leaves() returns true. At most 3.
  inline size_type leaf_count(void) const
    { return has_leaves() ? child_count() : 0; }
  // Get leaf node i. Unchecked: valid only for 0 <= i < leaf_count().
  inline const lnode_type* leaf(size_type i) const { return u_.leaves[i]; }
  inline lnode_type* leaf(size_type i) { return u_.leaves[i]; }

  // Insert at leaf at the given index. MUST be in sorted order,
  // or the tree will be FUBAR.
  bool insert_leaf(lnode_type* new_leaf, size_type index)
  {
    if (full() || index >= max_count())
      return false;
    // Scoot over the nodes starting at index and insert the new leaf.
    // We MUST have a null pointer in the end slot which is overwritten
    // (checked by verifying we are not full() above).
    for (size_type lidx = max_count()-1; lidx > index; --lidx)
      set_leaf(lidx, leaf(lidx-1));
    set_leaf(index, new_leaf);
    ++child_count_;
    // Now fix the keys. Note if the insert index was 2,
    // the caller will have to fix parent keys recursively.
    fix_leaf_keys();
    return (leaves_ = true);
  }
  inline bool insert_leaf(const value_type& value, size_type index) {
    if (full() || index >= max_count())
      return false;
    lnode_type *new_leaf = new lnode_type(value);
    return insert_leaf(new_leaf, index);
  }

  // Append a leaf as the new middle or max leaf.
  // Note that if it is the new max leaf, parent keys must be fixed by the
  // caller.
  inline bool add_leaf(const value_type& value)
    { return insert_leaf(value, leaf_count()); }
  inline bool add_leaf(lnode_type *new_leaf)
    { return insert_leaf(new_leaf, leaf_count()); }

  // Remove and return the current max leaf. Updates count.
  // Returns nullptr iff there are no leaves.
  // Note that if there were only two leaves, the node is now invalidated
  // (nodes should always have 2 or 3 children).
  lnode_type* pop_leaf(void) {
    if (leaf_count() == 0)
      return nullptr;
    lnode_type* l = leaf(--child_count_);
    l->link(nullptr);
    set_leaf(child_count(), nullptr);
    return l;
  }

  // Swap out a leaf with a new one and fix the keys.
  // You must ensure the sort invariant is maintained. Returns NULL for
  // out-of-bounds index, or if the leaf at index was already null.
  lnode_type* swap_leaf(size_type index, lnode_type* new_leaf) {
    if (index >= max_count())
      return nullptr;
    lnode_type* old_leaf = leaf(index);
    set_leaf(index, new_leaf);
    if (new_leaf->parent())
      new_leaf->parent()->set_leaf(index, old_leaf);
    // Unless we swapped the max key, we must fixup our internal keys.
    // If we are a max key, parent keys must be fixed by the caller.
    if (index != max_count()-1)
      fix_leaf_keys();
    return old_leaf;
  }

  inline inode_type* left_leaf(void) { return leaf(0); }
  inline inode_type* middle_leaf(void) { return leaf(1); }
  inline inode_type* right_leaf(void) { return leaf(2); }
  inline const inode_type* left_leaf(void) const { return leaf(0); }
  inline const inode_type* middle_leaf(void) const { return leaf(1); }
  inline const inode_type* right_leaf(void) const { return leaf(2); }

private:
  // Set child or leaf. No bounds checking, and size is not updated.
  // However, the new node's parent pointer/pos are updated.
  inline void set_child(size_type index, inode_type* node) {
    node->link(this, index);
    u_.children[index] = node;
  }
  inline void set_leaf(size_type index, lnode_type* node) {
    node->link(this, index);
    u_.leaves[index] = node;
  }

  // Fix our internal keys to match our leaf nodes' keys.
  // Note that key(i) is a copy of key(leaf(i)).
  inline void fix_leaf_keys(void) {
    for (size_type kidx = 0u; kidx < key_count(); ++kidx)
      keys_[kidx] = leaf(kidx)->key();
  }

public:
  // If we are full, we have no more room for another child.
  inline bool full(void) const { return child_count() == max_count(); }

  // Link to a new parent node.
  inline void link(inode_type* parent, size_type pos=0) {
    parent_ = parent;
    if (parent) ppos_ = pos;
    else ppos_ = -1;
  }

  // Boundary keys. All nodes in child(i) are less than key(i).
  // Generally all nodes always have exactly two keys, and either two or three
  // children. However, nodes which
  //   0 children    -> 0 keys;
  //   1, 2 children -> 1, 2 keys;
  //   3 children    -> 2 keys.
  inline size_type key_count(void) const { return std::max(child_count(), 2u); }
  // Get key i. Unchecked: valid only for 0 <= i < key_count().
  inline key_type& key(size_type i) { return keys_[i]; }
  inline const key_type& key(size_type i) const { return keys_[i]; }
  inline key_type& left_key(void) { return key(0); }
  inline key_type& right_key(void) { return key(1); }
  inline const key_type& left_key(void) const { return key(0); }
  inline const key_type& right_key(void) const { return key(1); }

  // Unchecked.
  inline void set_key(const key_type& k, size_type i) { keys_[i] = k; }

public:
  // Constructors.
  l23_inode()
    : parent_(nullptr), keys_({0}),
      u_.children({0}), ppos_(-1), leaves_(false), child_count_(0)
  {}

  // Construct a leaf-container with two leaves in the given sorted order.
  l23_inode(lnode_type* left, lnode_type* middle)
    : parent_(nullptr), keys_(left->key(), middle->key()),
      u_.leaves({left, middle}), ppos_(-1), leaves_(true), child_count_(2)
  {
  }

private:
  ////// Members.
  // Parent back-pointer. Only NULL for the root node.
  inode_type* parent_;

  // Copies of keys used to sort the children. We never have more than 2 keys,
  // since we never have more than three children.
  key_array keys_;

  // Pointers to our child nodes. Always have 0, 2, or 3 children.
  // If our child nodes are leaves, we instead store pointers to leaf nodes.
  union {
    node_array children;
    leaf_array leaves;
  } u_;

  // Position of this node in the parent.
  int ppos_;

  // If leaves_ is true, the children of this node are leaves.
  // Otherwise the children of this node are internal nodes.
  // This saves space by storing data only in the leaves, rather than
  // in internal nodes.
  bool leaves_;

  // Number of valid children (generally 2 or 3).
  size_type child_count_;
};

//! 2-3 tree (l)eaf node.
template<class Params>
class l23_lnode
{
public:
  // Typedefs.
  typedef typename Params::key_type key_type;
  typedef typename Params::key_type data_type;
  typedef typename Params::value_type value_type;
  typedef typename Params::key_type key_compare;
  typedef typename Params::size_type size_type;
  typedef l23_inode<Params> inode_type;
  typedef l23_lnode<Params> lnode_type;
  typedef l23_tree<Params> tree_type;
  friend class inode_type;
  friend class tree_type;

public:
  inline inode_type* parent(void) { return parent_; }
  inline const inode_type* parent(void) const { return parent_; }
  inline size_type pos(void) const { return ppos_; }

  inline value_type& value(void) { return value_; }
  inline const value_type& value(void) const { return value_; }

  inline key_type& key(void) { return Params::key(value()); }
  inline const key_type& key(void) const { return Params::key(value()); }

  inline data_type& data(void) { return Params::data(value()); }
  inline const data_type& data(void) const { return Params::data(value()); }

  inline lnode_type* left_sibling(void) {
    if (parent() && pos() > 0)
      return parent()->leaf(pos()-1);
    return nullptr;
  }
  inline lnode_type* right_sibling(void) {
    if (parent() && pos() < inode_type::max_count()-1)
      return parent()->leaf(pos()+1);
    return nullptr;
  }

  inline void link(inode_type* parent, size_type pos=0) {
    parent_ = parent;
    if (parent) ppos_ = pos;
    else ppos_ = -1;
  }

public:
  // Constructors.
  l23_lnode()
    : parent_(nullptr), value_()
  {}

  template<ValueType>
  l23_lnode(ValueType v)
    : parent_(nullptr), value_(v)
  {}

private:
  // Members. A leaf node just has a value and a back-pointer to its parent.
  inode_type* parent_;
  // Position in parent.
  int ppos_;
  value_type value_;
};

/*
template<class Nt_, class Node_algorithms>
  class _bfs_iterator : public _node_iterator<Nt_, Node_algorithms>
{
public:
  DECLARE_TRAITS(Nt_);
  typedef Node_algorithms node_algorithms;

private:
  std::list<node_ptr> q;

public:
  _bfs_iterator(node_ptr header, node_ptr current)
    : _node_iterator<node_traits, node_algorithms>(header, current)
  {
    if (current && current != header) {
      q.push_back(current);
      ++(*this);
    }
  }
  inline _bfs_iterator &operator++(void)
  {
    if (q.empty()) {
      this->current_ = this->header_;
      return *this;
    }
    this->current_ = q.front(); q.pop_front();
    node_ptr l = node_traits::get_left(this->current_);
    if (l) q.push_back(l);
    node_ptr r = node_traits::get_right(this->current_);
    if (r) q.push_back(r);
    return *this;
  }
  inline _bfs_iterator operator++(int) { // post-fix
    _bfs_iterator ret = *this;
    ++(*this);
    return ret;
  }
};
*/

template<class Params>
class l23_iterator
{
public:
  // Typedefs.
  typedef typename Params::key_type key_type;
  typedef typename Params::key_type data_type;
  typedef typename Params::key_type key_compare;
  typedef typename Params::size_type size_type;
  typedef l23_inode<Params> inode_type;
  typedef l23_lnode<Params> lnode_type;

  typedef inode_type* value_type;

public:
  // Constructors.

  // Iterator pointing to an internal node.
  l23_iterator(inode_type* nptr)
    : node_(nptr), lpos_(-1) {}

  // Iterator pointing to a leaf node.
  l23_iterator(lnode_type* lptr)
    : node_(lptr->parent()), lpos_(lptr->pos()) {}

  // Invalid iterator.
  l23_iterator()
    : node_(nullptr), lpos_(-1) {}

  // Copy constructor.
  l23_iterator(const l23_iterator& other)
    : node_(other.node_), lpos_(other.lpos_) {}

  // Whether this points to a leaf node.
  // Note that the actual leaf node pointed to may be NULL.
  inline bool is_leaf(void) const {
    return node_ && node_->has_leaves();
  }

  // Only valid when is_leaf() is true.
  inline lnode_type* leaf(void) { return node_->leaf(lpos_); }
  inline value_type& value(void) { return leaf()->value(); }
  inline key_type& key(void) { return leaf()->key(); }
  // Negative if is_leaf() is false, otherwise index of current leaf.
  inline int pos(void) const { return lpos_; }

  // Return the internal node pointed to by this iterator (if any).
  // Only valid when leaf() is false.
  inline inode_type* node(void) { return node_; }
  // Return the parent of node().
  inline inode_type* parent(void) { return node_->parent(); }

  // Whether this points to a valid node (false for tree.end()).
  inline operator bool(void) const { return node_ != nullptr; }

  // pre-fix
  iterator& operator++(void)
  {
    // Short-circuit the null case.
    if (!*this)
      return *this;

    // Leaves move onto the next leaf until we run out.
    if (is_leaf() && ++lpos_ < node()->leaf_count())
      return *this;

    // Go to the next sibling.
    // Either we are an internal node, or we've just finished a leaf subtree.
    if ((node_ = node()->right_sibling()))
      return *this;

    // Once we run out of siblings, check the queue for the next node.
    if (q.empty())
    {
      node_ = nullptr;
      lpos_ = -1;
      return *this;
    }

    // If the next node from the queue has leaves, reset our leaf position.
    // Otherwise queue our children (only need to queue the left child, since
    // children will traverse to their right siblings as per above).
    node_ = q.front(); q.pop_front();
    if (node()->has_leaves())
      lpos_ = 0;
    else
      q.push_back(node()->left_child());
    return *this;
  }

  // post-fix
  inline iterator operator++(int)
  {
    iterator it(*this);
    ++(*this);
    return it;
  }

  inline inode_type* operator*(void) const {
    if (!node()) throw std::runtime_error("l23_iterator: null dereference");
    return node();
  }

  inline bool operator==(const iterator& other) const
    { return (pos_ == other.pos_ && node_ == other.node_); }
  inline bool operator!=(const iterator& other) const
    { return !(*this == other); }

private:
  // Members.
  inode_type* node_;
  // If pos is negative, points to node (which is the root node).
  int pos_;
  // Node queue for BFS traversal.
  std::list<inode_type*> q;
};

//! 2-3 tree with data in the leaves.
template<class Params>
class l23_tree
{
public:
  // Typedefs.
  typedef typename Params::key_type key_type;
  typedef typename Params::key_type data_type;
  typedef typename Params::value_type value_type;
  typedef typename Params::key_type key_compare;
  typedef typename Params::size_type size_type;
  typedef l23_inode<Params> inode_type;
  typedef l23_lnode<Params> lnode_type;
  typedef l23_iterator<Params> iterator;

public:
  // Constructors.
  template<class KeyCompare>
  l23_tree(KeyCompare kcmp=key_compare())
    : root_(nullptr), compare_(kcmp)
  {}

  template<typename InputIter>
  l23_tree(InputIter begin, InputIter end, KeyCompare kcmp=key_compare())
    : root_(nullptr), compare_(kcmp)
  {
    while (begin != end)
      insert_unique(*begin++);
  }

  inode_type* root(void) { return root_; }

  // Iterators.
  iterator begin(void) { return iterator(root, 0); }
  iterator end(void) { return iterator(nullptr, 0); }

  // Lookup.
public:
  template<class KeyType=const key_type&>
    iterator lower_bound(KeyType k);
  inline iterator lower_bound(const value_type& v) {
    return lower_bound(Params::key(v));
  }

  template<class KeyType=const key_type&>
    iterator upper_bound(KeyType k);
  inline iterator upper_bound(const value_type& v) {
    return upper_bound(Params::key(v));
  }

  template<class KeyType=const key_type&>
    iterator find_unique(KeyType k);
  inline iterator find_unique(const value_type& v) {
    return find_unique(Params::key(v));
  }

  // Insert.
  iterator insert_unique(const value_type& v);

  // Remove.
  template<class KeyType=const key_type&>
    void erase_unique(KeyType k);
  inline void erase_unique(const value_type& v)
    { erase_unique(Params::key(v)); }
  void erase(iterator it);

private:
  // Simple helpers for comparing without having to remember the STL
  // comparators.
  template<class KeyType1, class KeyType2, class KeyCompare>
  inline bool compare(KeyType1 k1, KeyType2 k2, KeyCompare kcmp) const
    { return kcmp(k1, k2); }
  template<class KeyType1, class KeyType2>
  inline bool compare(KeyType1 k1, KeyType2 k2) const
    { return compare_(k1, k2); }
  template<class KeyType1, class KeyType2, class KeyCompare>
  inline bool equal(KeyType1 k1, KeyType2 k2, KeyCompare kcmp) const
    { return compare(k1, k2, kcmp) && compare(k2, k1, kcmp); }
  template<class KeyType1, class KeyType2>
  inline bool equal(KeyType1 k1, KeyType2 k2) const
    { return compare(k1, k2) && compare(k2, k1); }
  template<class KeyType1, class KeyType2, class KeyCompare>
  inline bool less_equal(KeyType1 k1, KeyType2 k2, KeyCompare kcmp) const
    { return !compare(k2, k1, kcmp); }
  template<class KeyType1, class KeyType2>
  inline bool less_equal(KeyType1 k1, KeyType2 k2) const
    { return !compare(k2, k1); }
  template<class KeyType1, class KeyType2, class KeyCompare>
  inline bool less(KeyType1 k1, KeyType2 k2, KeyCompare kcmp) const
    { return compare(k1, k2, kcmp); }
  template<class KeyType1, class KeyType2>
  inline bool less(KeyType1 k1, KeyType2 k2) const
    { return compare(k1, k2); }
  template<class KeyType1, class KeyType2, class KeyCompare>
  inline bool greater(KeyType1 k1, KeyType2 k2, KeyCompare kcmp) const
    { return compare(k2, k1); }
  template<class KeyType1, class KeyType2>
  inline bool greater(KeyType1 k1, KeyType2 k2) const
    { return compare(k2, k1); }
  template<class KeyType1, class KeyType2, class KeyCompare>
  inline bool greater_equal(KeyType1 k1, KeyType2 k2, KeyCompare kcmp) const
    { return !compare(k1, k2); }
  template<class KeyType1, class KeyType2>
  inline bool greater_equal(KeyType1 k1, KeyType2 k2) const
    { return !compare(k1, k2); }

  // Lower bound helpers.
  template<class KeyType>
    iterator lower_bound_checked(KeyType k) const;
  template<class KeyType>
    iterator lower_bound_generic(KeyType k) const;
  template<class KeyType>
    iterator lower_bound_insert(KeyType k) const;

  template<class KeyType>
    inode_type* child_index_generic(inode_type* node, KeyType k) const;
  template<class KeyType>
    inode_type* child_index_insert(inode_type* node, KeyType k) const;

  // Insert a leaf in the general case (all nodes have 2 or 3 nodes).
  iterator insert_leaf(const value_type& v);
  // Insert an internal node recursively.
  void insert_internal(inode_type* parent, inode_type* new_node);

  // Fix keys along a branch from a new max key insertion.
  void fix_branch(const key_type& max_key, inode_type* pnode);

private:
  // Members.
  inode_type* root_;
  key_compare compare_;
};

// Tree implementations.
template<class Params> template<class KeyType>
typename l23_tree<Params>::iterator
l23_tree<Params>::lower_bound_checked(KeyType k) const
{
  // Special cases: no root or root has 1 child.
  if (!root() || !root()->count())
    return end();
  if (root()->count() == 1)
  {
    if (equal(k, root()->key(0)))
      return iterator(root(), 0);
    return end();
  }

  // Otherwise we can assume all nodes have 2 or 3 children.
  return lower_bound_generic(k);
}

template<class Params> template<class KeyType>
typename l23_tree<Params>::inode_type*
l23_tree<Params>::child_index_generic(inode_type* node, KeyType k) const
{
  inode_type::size_type i = 0;
  for (; i < inode_type::max_count(); ++i)
    if (less_equal(k, node->key(i)))
      break;
  return i;
}

template<class Params> template<class KeyType>
typename l23_tree<Params>::inode_type*
l23_tree<Params>::child_index_insert(inode_type* node, KeyType k) const
{
  inode_type::size_type i = 0;
  for (; i < node->child_count(); ++i)
    if (less(k, node->key(i)))
      break;
  return i;
}

template<class Params> template<class KeyType>
typename l23_tree<Params>::iterator
l23_tree<Params>::lower_bound_generic(KeyType k) const
{
  const inode_type* node = root();
  while (node && !node->has_leaves())
    node = node->child(child_index_generic(node, k));
  if (!node || !node->has_leaves())
    return end();
  // Now we must have landed on a non-null node containing 2 or 3 leaves.
  // Return an iterator pointing to the appropriate lower-bound leaf node.
  // Note that the leaf itself may not actually exist.
  return iterator(node->leaf(child_index_generic(node, k)));
}

template<class Params> template<class KeyType>
typename l23_tree<Params>::iterator
l23_tree<Params>::lower_bound(KeyType k) const
{
  iterator leafp = lower_bound_checked(k);
  if (leafp.leaf() == nullptr)
    return end();
  return leafp;
}

template<class Params> template<class KeyType>
typename l23_tree<Params>::iterator
l23_tree<Params>::upper_bound(KeyType k) const
{
  iterator leafp = lower_bound_checked(k);
  // Increment the iterator until we find a key value greater than k.
  while (leafp != end() && !compare(k, leafp.key()))
    ++leafp;
  return leafp; // may be end()
}

template<class Params> template<class KeyType>
typename l23_tree<Params>::iterator
l23_tree<Params>::find_unique(KeyType k)
{
  iterator leafp = lower_bound_checked(k);
  if (leafp == end() || leafp.leaf() == nullptr)
    return end();
  // Only return the given node if its key is equal to the query key.
  // We already know compare(k, leafp.key()) is false by definition of
  // lower_bound (k NOT < leaf). If the converse is false we have equality
  // by STL convention.
  if (!compare(leafp.key(), k))
    return iterator(node, leaf_idx);
  return end();
}

template<class Params>
typename l23_tree<Params>::iterator
l23_tree<Params>::lower_bound_insert(const key_type& k)
{
  const inode_type* node = root();
  while (node && !node->has_leaves())
    node = node->child(child_index_insert(node, k));
  if (!node || !node->has_leaves())
    return end();
  // Now we must have landed on a non-null node containing 2 or 3 leaves.
  // Return an iterator pointing to the appropriate lower-bound leaf node.
  // Note that the leaf itself may not actually exist.
  return iterator(node->leaf(child_index_insert(node, k)));
}

template<class Params>
typename l23_tree<Params>::iterator
l23_tree<Params>::insert_unique(const value_type& value)
{
  // Special case: root doesn't exist yet, or root has only one node.
  // These cases only occur for the first two nodes.
  if (!root())
  {
    root_ = new inode_type(value);
    return iterator(root(), 0);
  }

  if (root()->count() == 1)
  {
    // Insert the node in the right spot.
    const key_type& key = Params::key(value);
    size_t pos = compare(key, root()->leaf(0)->key()) ? 0 : 1;
    root()->insert_leaf(value, pos);
    return iterator(root(), pos);
  }

  // Handle the general case.
  return insert_internal(value);
}

template<class Params>
void
l23_tree<Params>::fix_branch(const key_type& max_key, inode_type* pnode)
{
  while (pnode)
  {
    if (greater(max_key, pnode->right_key()))
      pnode->set_key(1, max_key);
    pnode = pnode->parent();
  }
}

template<class Params>
typename l23_tree<Params>::iterator
l23_tree<Params>::insert_leaf(const value_type& value)
{
  // Without loss of generality, every node has either 2 or 3 children.
  // Find the proper parent node to contain the new leaf.
  iterator leafp = lower_bound_insert(Params::key(value));
  if (leafp == end())
    return end();

  // If the node is not full, we can just add the leaf directly.
  inode_type* parent = leafp.parent();
  lnode_type* new_leaf = new lnode_type(value);
  inode_type* new_parent = parent;
  int new_pos = leafp.pos();

  if (parent->insert_leaf(new_leaf, leafp.pos()))
  {
    // But, if we added a new max leaf, we must fix our ancestors' keys
    // because the new upper bound for this subtree may be higher than the
    // upper bound key from our parent.
    if (leafp.pos() == inode_type::max_count() - 1)
    {
      fix_branch(Params::key(value), parent->parent());
    }
    return iterator(parent, leafp.pos());
  }

  // Here the immediate parent of the new leaf is full, so we must create a new
  // node to hold the largest two children and push it upwards.

  // One of the two max leaves must be the current max leaf.
  inode_type* newnode_middle = parent->pop_leaf();
  // The other max leaf is either:
  // (1) the new leaf, in which case
  lnode_type* newnode_left = nullptr;
  if (greater(new_leaf->key(), parent->right_key()))
  {
    newnode_left = new_leaf;
    new_parent = nullptr;
  }
  // or (2) the current middle leaf, in which case the new leaf becomes the
  // middle leaf of this parent.
  else
    newnode_left = parent->swap_leaf((new_pos = 1), new_leaf);

  // Construct the new node with the two new leaves in the right order.
  if (greater(newnode_left->key(), newnode_middle->key()))
  {
    inode_type* tmp = newnode_left;
    newnode_left = newnode_middle;
    newnode_middle = tmp;
  }
  inode_type* new_node = new inode_type(newnode_left, newnode_middle);

  // If we d
  inode_type* new_parent = (newnode_left == new_leaf) ? new_node : parent;

  // Insert the new node into the parent recursively.
  insert_internal(parent->parent(), new_node);
  return iterator(new_parent, new_pos);
}

template<class Params> void
l23_tree<Params>::insert_internal(inode_type* parent, inode_type* new_node)
{
}

} // end namespace tree