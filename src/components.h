
// Tree structures with custom node data.
#include <set>
#include <queue>
#include <vector>
#include <unordered_set>

#ifdef DEBUG
#include <iostream>
#include <iomanip>
#endif

#include <opencv2/core/core.hpp>

#include <boost/polygon/polygon.hpp>
#include <boost/polygon/point_concept.hpp>
#include <boost/polygon/rectangle_concept.hpp>
#include <boost/geometry.hpp>

#include <boost/graph/adjacency_list.hpp>

//#include "tree.h"

template <class Container>
  class push_insert_iterator:
    public std::iterator<std::output_iterator_tag,void,void,void,void>
{
protected:
  Container* container;

public:
  typedef Container container_type;
  explicit push_insert_iterator(Container& x) : container(&x) {}
  push_insert_iterator<Container>& operator=(
      typename Container::const_reference value)
    { container->push(value); return *this; }
  push_insert_iterator<Container>& operator* (){ return *this; }
  push_insert_iterator<Container>& operator++ (){ return *this; }
  push_insert_iterator<Container> operator++ (int){ return *this; }
};

template<typename Container>
push_insert_iterator<Container> push_inserter(Container& container){
  return push_insert_iterator<Container>(container);
}

namespace components
{

namespace b  = boost;
namespace bp = boost::polygon;
namespace bg = boost::geometry;

template<class Tp_>
struct Edge
{
  typedef typename bp::rectangle_traits<Tp_> traits;
  typedef Tp_ coordinate_type;

  coordinate_type coord; // actual coordinate
  typename bp::direction_1d dir; // LOW or HIGH
  int rect_index; // parent rectangle
  mutable int depth; // does not affect sorting

  Edge() : coord(), dir(), rect_index(-1), depth(-1) {}
  Edge(coordinate_type const& c, bp::direction_1d const& d, int idx)
    : coord(c), dir(d), rect_index(idx), depth(-1) {}
  Edge(Edge const& other, int new_idx)
    : coord(other.coord), dir(other.dir), rect_index(new_idx),
      depth(other.depth) {}

  inline bool operator==(Edge const& e) const {
    return rect_index == e.rect_index
      && dir == e.dir
      && coord == e.coord; // epsilon compare?
    // depth compare?
  }

#ifdef DEBUG
  template<typename U>
    friend std::ostream& operator<<(std::ostream& os, Edge<U> const& e) {
      os << "<[" << std::setw(2) << std::setfill(' ') << e.rect_index
        << "] " << (e.dir == bp::LOW ? "LOW " : "HIGH")
        << " " << e.coord << " d=" << e.depth << ">";
      return os;
    }
#endif
};

template<class Tp_>
struct RectComponent : bp::rectangle_data<Tp_>
{
  typedef typename bp::rectangle_data<Tp_> super_type;
  typedef typename bp::rectangle_traits<Tp_> traits;
  typedef Edge<Tp_> edge_type;

  size_t index;
  //int component; // ID of the component to which this rect belongs.

  template <class HInterval, class VInterval>
    RectComponent(const HInterval& hrange, const VInterval& vrange, size_t idx)
      : super_type(hrange, vrange), index(idx)/*, component(-1)*/ {}

  inline edge_type edge(bp::orientation_2d orient, bp::direction_1d dir) const
  {
    return edge_type(bp::get(*this, orient, dir), dir, index);
  };

  template<class OutIter> void
    add_edges(OutIter out, bp::orientation_2d orient) {
      *out++ = edge(orient, bp::HIGH);
      *out++ = edge(orient, bp::LOW);
    }
};

} // we interrupt your regularly scheduled components namespace to bring you...

namespace boost { namespace polygon {
  template <typename Tp_>
    struct geometry_concept<components::RectComponent<Tp_> >
      { typedef rectangle_concept type; };
}}

#ifdef DEBUG
template<typename T, typename C>
  std::ostream& operator<<(std::ostream& os, std::set<T, C> const& s) {
    os << "set<" << std::endl;
    for (auto it = s.begin(); it != s.end(); ++it)
      os << "  " << *it << std::endl;
    os << ">";
    return os;
  }

template<typename T>
  std::ostream& operator<<(std::ostream& os, std::unordered_set<T> const& s) {
    os << "set{" << std::endl;
    for (auto it = s.begin(); it != s.end(); ++it)
      os << *it << std::endl;
    os << "}";
    return os;
  }

template<typename T>
  std::ostream& operator<<(std::ostream& os, std::vector<T> const& v) {
    os << "[ ";
    for (auto it = v.begin(); it != v.end(); ++it)
      os << *it << ", ";
    os << " ]";
    return os;
  }

template<typename T>
  std::ostream& operator<<(std::ostream& os, std::list<T> const& l) {
    os << "[ ";
    for (auto it = l.begin(); it != l.end(); ++it)
      os << *it << ", ";
    os << " ]";
    return os;
  }
#endif

template<typename Tp_>
std::ostream& operator<<(std::ostream& os,
    boost::polygon::rectangle_data<Tp_> const& r)
{
  namespace bp = boost::polygon;
  os << "|" << bp::get(r, bp::HORIZONTAL, bp::LOW) << ","
            << bp::get(r, bp::VERTICAL, bp::HIGH)
     << "|";
  return os;
}


namespace components
{

template<class Tp_>
struct EdgeCompare
{
  typedef Edge<Tp_> edge_type;
  inline bool operator()(edge_type const& e1, edge_type const& e2) const
  {
    return e1.coord < e2.coord
      || ((e1.coord == e2.coord) && (e1.rect_index < e2.rect_index));
  }
};

template<class Node_>
struct depth_compare
{
  typedef Node_ node;
  inline bool operator()(const node &d1, const node &d2) const
  {
    return d1.depth < d2.depth;
  }
};

// Node to track 'cell depths'. These form the tree T(V_k) from the paper,
// the leaves of which represent g_k[j]: the depth of the cell at position
// j in the current sweep partition.
/*
template<class Tp_>
class DepthNode : public tree::avl_node<DepthNode<Tp_> >
{
public:
  typedef Tp_ coordinate_type;
  typedef DepthNode<Tp_> my_type;
  typedef tree::avl_node<my_type> base_type;
  DECLARE_TRAITS(tree::avl_node_traits<my_type>);
  typedef Edge<coordinate_type> edge_type;
  typedef depth_compare<my_type> compare;

  // Internal depth value of the node.
  int depth;
  // If we are a leaf node, which X edge we correspond to.
  edge_type leaf_edge;

  DepthNode() : base_type(), depth(0), leaf_edge() {}

  // Whether we are a leaf node.
  inline bool leaf(void) const {
    return !node_traits::get_left(this) && !node_traits::get_right(this);
  }
};
*/

template<class Iter1, class Iter2>
Iter1 sync_iters(Iter1 begin1, Iter2 begin2, Iter2 end2) {
  while (begin2++ != end2)
    ++begin1;
  return begin1;
}

template<class Iter1, class Iter2>
Iter1 rsync_iters(Iter1 end1, Iter2 end2, Iter2 begin2) {
  while (end2-- != begin2)
    --end1;
  return end1;
}

template<class Tp_>
struct SolutionCompare;

template<class Tp_>
struct SolutionEdge
{
  typedef Edge<Tp_> edge_type;
  typedef EdgeCompare<Tp_> edge_comparator;
  typedef typename edge_type::coordinate_type coordinate_type;
  typedef SolutionEdge<Tp_> sedge_type;
  typedef SolutionCompare<Tp_> sedge_compare;

  edge_type edge;
  int solution;

  SolutionEdge(edge_type const& e, int sidx=-1)
    : edge(e), solution(sidx)
  {
  }

  inline bool operator<(sedge_type const& other) const {
    return sedge_compare()(*this, other);
  }
  inline bool operator<(edge_type const& other) const {
    return sedge_compare()(*this, other);
  }
  inline bool operator==(sedge_type const& other) const {
    return !sedge_compare()(*this, other) && !sedge_compare()(other, *this);
  }
  inline bool operator==(edge_type const& other) const {
    return !sedge_compare()(*this, other) && !sedge_compare()(other, *this);
  }

#ifdef DEBUG
  template<typename U>
    friend std::ostream& operator<<(std::ostream& os, SolutionEdge<U> const& e)
    {
      os << "[" << std::setw(2) << std::setfill(' ') << e.solution << "] from "
        << e.edge;
      return os;
    }
#endif
};

template<class Tp_>
struct SolutionCompare
{
  typedef SolutionEdge<Tp_> sedge_type;
  typedef typename sedge_type::edge_type edge_type;
  typedef typename sedge_type::edge_comparator edge_comparator;

  inline bool operator()(sedge_type const& s1, sedge_type const& s2) const {
    return edge_comparator()(s1.edge, s2.edge)
      || (!edge_comparator()(s2.edge, s1.edge)
          && s1.solution == s2.solution);
  }
  inline bool operator()(sedge_type const& s1, edge_type const& e2) const {
    return edge_comparator()(s1.edge, e2);
  }
  inline bool operator()(edge_type const& e1, sedge_type const& s2) const {
    return edge_comparator()(e1, s2.edge);
  }
};

template<class Tp_>
class ConnectedComponents;

template<class Tp_>
struct SolutionCell
{
public:
  typedef ConnectedComponents<Tp_> cctype;
  typedef typename cctype::coordinate_type coordinate_type;
  typedef typename cctype::point_type      point_type;
  typedef typename cctype::rect_type       rect_type;
  typedef typename cctype::edge_type       edge_type;
  typedef typename cctype::edge_comparator edge_comparator;
  typedef typename cctype::pure_rect_type  pure_rect_type;

  typedef SolutionEdge<Tp_> sedge_type;
  typedef SolutionCompare<Tp_> sedge_compare;

  // indexes of rects which form the maximal intersection
  typedef typename std::unordered_set<size_t> index_set;
  typedef typename index_set::const_iterator index_iterator;
  typedef typename index_set::size_type size_type;

private:
  index_set source_rects_;
  int top_, bot_, left_, right_;
  bool hit_left_, hit_right_;

public:
  SolutionCell(int top, int left, int right)
    : source_rects_(),
      top_(top), bot_(-1), left_(left), right_(right),
      hit_left_(false), hit_right_(false)
  {
    source_rects_.insert(top_);
    source_rects_.insert(left_);
    source_rects_.insert(right_);
#ifdef DEBUG
    std::cerr << "new max from rect " << top_
      << " and edges: " << left_ << " and " << right_ << std::endl;
#endif
  }

  inline void found(bp::direction_1d dir) {
    if (dir == bp::LEFT)  hit_left_  = true;
    //else
    if (dir == bp::RIGHT) hit_right_ = true;
  }
  inline bool marked(void) const { return hit_left_ || hit_right_; }
  inline bool marked(int bot) {
    if (marked())
    {
      set_bot(bot);
      return true;
    }
    return false;
  }

  inline int top(void) const { return top_; }
  inline int bot(void) const { return bot_; }
  inline int left(void) const { return left_; }
  inline int right(void) const { return right_; }
  inline void set_bot(int new_bot) {
    // only update bottom once
    if (bot_ < 0 && new_bot >= 0)
      source_rects_.insert(bot_ = new_bot);
  }

  inline index_iterator begin(void) const { return source_rects_.cbegin(); }
  inline index_iterator end(void) const { return source_rects_.cend(); }
  inline size_type size(void) const { return source_rects_.size(); }

  // solution cell
  template<class RectArray>
  pure_rect_type cell(RectArray const& rects, size_t rects_size) const {
    auto rit = source_rects_.begin();
    if (rit == source_rects_.end())
      return pure_rect_type();
    int idx = *rit++;
    if (idx < 0 || static_cast<size_t>(idx) >= rects_size)
      return pure_rect_type();

    pure_rect_type retcell = rects[idx];
    while (rit != source_rects_.end())
    {
      idx = *rit++;
      if (idx >= 0 && static_cast<size_t>(idx) < rects_size)
        /*assert(*/bp::intersect(retcell, rects[idx])/*)*/;
    }
    return retcell;
  }
};

template<class Tp_>
class ConnectedComponents
{
public:
  typedef Tp_ coordinate_type;
  typedef typename cv::Point_<Tp_> point_type;
  typedef RectComponent<Tp_> rect_type;
  typedef typename rect_type::super_type pure_rect_type;
  typedef Edge<Tp_> edge_type;
  typedef EdgeCompare<Tp_> edge_comparator;

  typedef typename std::vector<edge_type> edge_container;
  typedef typename std::vector<rect_type> rect_container;
  typedef typename std::set<edge_type, edge_comparator>
    edge_set;
  typedef std::priority_queue<edge_type, edge_container, edge_comparator>
    edge_queue;
  // T(V_k) from the paper - TODO
  //typedef typename tree::make_avltree<DepthNode<Tp_> >::type depth_tree_type;
  // for now we take a short-cut and keep the entire array gk[j]

  // Vertexes are rect_type objects.
  typedef typename b::property<b::vertex_index_t, int> VertexProps;
  typedef typename b::adjacency_list<
      b::hash_setS, b::listS, b::undirectedS, VertexProps
    > components_graph;
  typedef typename b::graph_traits<components_graph>::vertex_descriptor
    vertex_descriptor;
  typedef typename b::property_map<components_graph, b::vertex_index_t>::type
    id_map;
  typedef typename std::vector<vertex_descriptor> descriptor_list;

  typedef typename edge_set::iterator edge_iterator;

  typedef SolutionCell<Tp_> solution_type;
  typedef typename solution_type::sedge_type sedge_type;
  typedef typename solution_type::sedge_compare sedge_compare;
  typedef typename std::set<sedge_type, sedge_compare> solution_edge_set;
  typedef typename std::vector<solution_type> solution_list;
  typedef typename solution_list::const_iterator solution_iterator;

  typedef typename std::vector<pure_rect_type> pure_rect_list;
  typedef typename pure_rect_list::const_iterator pure_rect_iterator;

private:
  rect_container rects;
  edge_queue edges_y; // horizontal rect edges which are the sweep events
  edge_set edges_x;   // vertical rect edges within each sweep line event
  //depth_tree_type depth_tree;
  components_graph graph;
  id_map component_ids;
  descriptor_list vertexes;
  int max_depth = -1;

  solution_edge_set solution_edges;
  solution_list solutions;
  pure_rect_list solution_cells;

  inline vertex_descriptor vd(int idx) const { return vertexes[idx]; }

  inline rect_type &rect(edge_type const& edge) {
    return rects[edge.rect_index];
  }

  template<typename EdgeSetIter>
    int check_max_depth(rect_type const& r, EdgeSetIter edge_lb, int depth);

  void insert_rect(rect_type const& r);
  void remove_rect(rect_type const& r);

public:
  // No inputs yet.
  ConnectedComponents();

  inline int index(vertex_descriptor v) const {
    return b::get(component_ids, v);
  }

  // Construct from a list of input rectangles (may be in no particular order).
  template<class RectIter>
    ConnectedComponents(RectIter begin, RectIter end)
    : rects(), edges_y(), edges_x(), graph(),
      component_ids(b::get(b::vertex_index_t(), graph))
    {
      add_rects(begin, end);
    }

  components_graph const& adj_graph(void) const { return graph; }

  // Add rectangles. Same as the constructor form, if you're lazy and want to
  // do it sometime after construction.
  template<class RectIter>
    void add_rects(RectIter begin, RectIter end)
    {
      size_t idx = rects.size();
      while (begin != end)
      {
        // Construct our custom rectangle wrappers.
        //vertex_descriptor vd = b::add_vertex(graph);
        vertexes.push_back(b::add_vertex(graph));
        b::put(component_ids, vd(idx), b::vertex_index_t(idx));
        // Make sure our intervals are in the proper order, otherwise
        // the algorithm WILL fail.
        auto hivl = bp::get(*begin, bp::HORIZONTAL);
        auto hl = bp::get(hivl, bp::LOW), hh = bp::get(hivl, bp::HIGH);
        auto vivl = bp::get(*begin, bp::VERTICAL);
        auto vl = bp::get(vivl, bp::LOW), vh = bp::get(vivl, bp::HIGH);
        hivl = bp::construct<decltype(hivl)>(std::min(hl,hh), std::max(hl,hh));
        vivl = bp::construct<decltype(vivl)>(std::min(vl,vh), std::max(vl,vh));
#ifdef DEBUG
        std::cerr << "ADD [" << idx << "]"
          << " h(" << bp::get(hivl, bp::LOW) << "," << bp::get(hivl, bp::HIGH)
          << ")"
          << " v(" << bp::get(vivl, bp::LOW) << "," << bp::get(vivl, bp::HIGH)
          << ")" << std::endl;
#endif
        rects.emplace_back(hivl, vivl, idx++);
        ++begin;
        // Queue up the horizontal edges. Vertical edges go in at each event.
        rects.back().add_edges(push_inserter(edges_y), bp::VERTICAL);
      }
      // We can never have more solutions than rects so just reserve this upper
      // bound to prevent implicitly resizing the solutions vector.
      solutions.reserve(rects.size());
    }

  // Run the algorithm and compute the connected components.
  void compute(void);

  inline int depth(void) const { return max_depth; }

  // Return the solution cells.
  inline size_t size(void) const { return solution_cells.size(); }

  inline pure_rect_iterator begin(void) const { return solution_cells.cbegin(); }
  inline pure_rect_iterator end(void) const { return solution_cells.cend(); }
  inline pure_rect_type const& cell(size_t i) { return solution_cells[i]; }

  inline solution_iterator sol_begin(void) const { return solutions.cbegin(); }
  inline solution_iterator sol_end(void) const { return solutions.cend(); }
  inline solution_type const& solution(size_t i) const { return solutions[i];}
};

extern template class ConnectedComponents<double>;

} // end namespace components
