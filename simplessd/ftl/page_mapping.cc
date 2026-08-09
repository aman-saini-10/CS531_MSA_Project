#include "ftl/page_mapping.hh"

#include <bits/stdc++.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <limits>
#include <random>
#include <vector>

#include "ftl/config.hh"
#include "icl/abstract_cache.hh"
#include "icl/generic_cache.hh"
#include "icl/global_point.hh"
#include "util/algorithm.hh"
#include "util/bitset.hh"
#include "util/def.hh"
// #include "headers/global.hh"


namespace LeaFTL {
// std::string filename2 = "../LeaFTLout2.txt";
// // Open the file for writing
// std::ofstream outputFile(filename2);

// constants

long long LPN_BYTES     = 4;
long long SUBLPN_BYTES  = 4;  // = 1 (if framePLR is utilised)
long long PPN_BYTES     = 4;
long long FLOAT16_BYTES = 2;
long long LENGTH_BYTES  = 1;  // max segment length = 256
bool LPN_TO_DEBUG       = 0;
// Data_Sizes
long long KB = 1024;          // Bytes
long long MB = KB << 10;
long long GB = MB << 10;
long long TB = GB << 10;

// Functions
std::string bitsetToString(std::bitset<256> &bitset) {
  std::string result;
  for (size_t i = 0; i < bitset.size(); ++i) {
    result += bitset.test(i) ? '1' : '0';
  }
  return result;
}
std::string points_str(std::vector<std::pair<long long, long long>> *points) {
  if (!points)
    return "";  // Return empty string if points is null

  std::string result = "[";
  for (auto &pair : *points) {
    result += "(" + std::to_string(pair.first) + ", " +
              std::to_string(pair.second) + "), ";
  }
  // Remove the extra ", " at the end
  if (!points->empty()) {
    result.pop_back();
    result.pop_back();
  }
  result += "]";
  return result;
}

class SimpleSegment {
 public:
  double k;      // slope (2B Floating Point)
  double b;      // intercept
  long long x1;  // starting LPN
  long long x2;  // ending LPN

  // Initializer
  SimpleSegment(double _k, double _b, long long _x1, long long _x2)
      : k(_k), b(_b), x1(_x1), x2(_x2) {}

  std::string to_string() {  // express SimpleSegment in form of a string
    return "(" + std::to_string(b) + ", " + std::to_string(k) + ", " +
           std::to_string(x1) + ", " + std::to_string(x2) + ")";
  }

  long long get_y(long long x) {  // predict function from segment
    long long predict = round(x * k + b);
    return predict;
  }

  // Intersection method is required for the Piecewise Linear Regression method
  // utilised to build segment
  static std::pair<double, double> intersection(SimpleSegment s1,
                                                SimpleSegment s2) {
    double x = (s2.b - s1.b) / (s1.k - s2.k);
    double y = (s1.k * s2.b - s2.k * s1.b) / (s1.k - s2.k);
    return std::make_pair(x, y);
  }
  // check if the point is above a simplesegment
  static bool is_above(std::pair<double, double> pt, SimpleSegment s) {
    return pt.second > s.k * pt.first + s.b;
  }
  // check if the point is below a simplesegment
  static bool is_below(std::pair<double, double> pt, SimpleSegment s) {
    return pt.second < s.k * pt.first + s.b;
  }
  static bool is_on(std::pair<double, double> pt, SimpleSegment s) {
    return pt.second == s.k * pt.first + s.b;    
  }
  // considering gamma > 0, upperbound gives, the point offset by +gamma on ppa
  // axis
  static std::pair<double, double> get_upper_bound(std::pair<double, double> pt,
                                                   double gamma) {
    return std::make_pair(pt.first, pt.second + gamma);
  }
  // offset by -gamma
  static std::pair<double, double> get_lower_bound(std::pair<double, double> pt,
                                                   double gamma) {
    return std::make_pair(pt.first, pt.second - gamma);
  }

  // simple_segment constructor from points
  static SimpleSegment frompoints(std::pair<long long, long long> p1,
                                  std::pair<long long, long long> p2) {
    double k =
        static_cast<double>(p2.second - p1.second) / (p2.first - p1.first);
    double b = -k * p1.first + p1.second;
    return SimpleSegment(k, b, p1.first, p2.first);
  }
};

class Segment {
 public:
  static constexpr double FPR = 0.01;  // Floating point resolution
  static constexpr long long PAGE_PER_BLOCK = 256;  // 256 --> 1 Byte for page representation in a group
  static constexpr bool BITMAP = true;

  double k;                 // Slope
  double b;                 // Intercept
  long long x1;             // Lower LPN Limit
  long long x2;             // Upper LPN Limit
  bool accurate;            // Approximate / accurate segment
  bool consecutive_;
  std::bitset<256> *filter = nullptr;  // BitMap
  std::vector<std::pair<long long, long long>> *points;  // points

  Segment(double _k, double _b, long long _x1, long long _x2,
          std::vector<std::pair<long long, long long>> *points = nullptr)
      : k(_k), b(_b), x1(_x1), x2(_x2), points(points) {
    accurate = true;
    if (points) {

      std::vector<std::pair<long long, long long>> &pt = *points;
      std::tie(accurate, consecutive_) = check_properties(pt);

      if (!consecutive_) {
        if (BITMAP) {
          filter = new std::bitset<256>;
          (*filter).reset();
          for (auto &p : pt) {
            if(p.first - x1 >= 256) {
              printf("k: %f, b: %f, x1: %lld, x2: %lld\n", k, b, x1, x2);
              printf("%lld\n", p.first - x1);
            }
            (*filter).set(p.first - x1);
          }
          if(pt.size() == 1) {
            // outputFile << "SEGMENT MAKE " << x1 << " " << x2 << endl;
            // outputFile << filter << endl << endl;
          }
          
        }
        else {
          // BloomFilter code not executed as BITMAP = true
        }
      }
    }
  }

  std::string to_string() {  // Conversion to string
    return std::to_string(k) + ", " + std::to_string(b) + ", [" +
           std::to_string(x1) + ", " + std::to_string(x2) +
           "], memory: " + std::to_string(memory()) +
           "B, accuracy: " + (accurate ? "True" : "False") +
           ", bitmap: " + bitsetToString((*filter));
  }
  std::string repr() {  // Representation
    return "(" + std::to_string(b) + ", " + std::to_string(k) + ", " +
           std::to_string(x1) + ", " + std::to_string(x2) + ", " +
           (accurate ? "True" : "False") + ")";
  }
  std::string short_repr() {  // Short representation
    return std::to_string(x1) + "," + std::to_string(x2 - x1) + "," +
           std::to_string(k) + "," + std::to_string(b);
  }
  std::string full_str() {  // Full string representation
    return "(" + std::to_string(b) + ", " + std::to_string(k) + ", " +
           std::to_string(x1) + ", " + std::to_string(x2) + ", " +
           (accurate ? "True" : "False") + ") " + points_str(points);
  }

  bool is_valid(long long x) {

    if (!(x1 <= x && x <= x2)) {
      return false;
    }
    if (consecutive()) {
      return ((x - x1) % rec_k() == 0);
    }
    else {
      if (BITMAP) {
        return (*filter).test(x - x1);
      }
      else {
        // BloomFilter code not executed as BITMAP = true
        return false;  // Placeholder return value
      }
    }
    return false;
  }
  double get_y(long long x, bool check = true) {
    if (!check || is_valid(x)) {
      // printf("DEBUG: x=%lld, k=%f, b=%f\n", x, k, b);
      double predict = round(x * k + b);
      return predict;
    }
    return NAN;  // Return NaN if check fails (CRITICAL)
  }

  std::pair<bool, bool> check_properties(
    std::vector<std::pair<long long, long long>> &points) {
    bool is_accurate = true;
    bool is_consecutive = true;
    // printf("Checking properties\n"); // debug
    for (auto &pt : points) {
      if (get_y(pt.first, false) != pt.second) {
        is_accurate = false;
        break;
      }
    }
    std::vector<long long> x_values;
    for (auto &pt : points) {
      x_values.push_back(pt.first);
    }
    std::sort(x_values.begin(), x_values.end());

    for (size_t i = 1; i < x_values.size(); ++i) {
      if (x_values[i] - x_values[i - 1] != 1) {
        is_consecutive = false;
        break;
      }
    }
    return std::make_pair(is_accurate, is_consecutive);
  }
  // Method to check if segments overlap
  bool overlaps(Segment &other) {
    return std::min(x2, other.x2) - std::max(x1, other.x1) >= 0;
  }
  // Method to check if the segment overlaps with a given range [x1, x2]
  bool overlaps_with_range(long long x1, long long x2) {
    return std::min(this->x2, x2) - std::max(this->x1, x1) >= 0;
  }

  // Method to perform bitwise merge of segments
  static std::tuple<Segment *, Segment *, bool> merge(Segment &newSeg,
                                                      Segment &oldSeg) {
    Segment *NULL_PTR = nullptr;
    Segment *newSegptr = &newSeg;
    Segment *oldSegptr = &oldSeg;
    if (!newSeg.overlaps(oldSeg)) {
      return {newSegptr, oldSegptr, 1};
      // return {const_cast<Segment*>(&newSeg), const_cast<Segment*>(&oldSeg),
      // 1};
    }

    if (!newSeg.mergable() || !oldSeg.mergable()) {
      return {newSegptr, oldSegptr, 0};
      // return {const_cast<Segment*>(&newSeg), const_cast<Segment*>(&oldSeg),
      // 0};
    }

    if (newSeg.consecutive() && oldSeg.consecutive()) {
      if (newSeg.rec_k() == oldSeg.rec_k() &&
          (newSeg.x1 - oldSeg.x1) % oldSeg.rec_k() == 0) {
        if (newSeg.x1 <= oldSeg.x1 && oldSeg.x2 <= newSeg.x2) {
          return {newSegptr, NULL_PTR, 1};
          
        }
        else if (oldSeg.x1 < newSeg.x1 && newSeg.x2 < oldSeg.x2) {
          return {newSegptr, oldSegptr, 0};

        }
        else if (newSeg.x1 <= oldSeg.x1) {
          oldSeg.x1 = newSeg.x2 + newSeg.rec_k();
          oldSegptr = &oldSeg;
          return {newSegptr, oldSegptr, 1};

        }
        else if (oldSeg.x1 < newSeg.x1) {
          oldSeg.x2 = newSeg.x1 - newSeg.rec_k();
          oldSegptr = &oldSeg;
          return {newSegptr, oldSegptr, 1};

        }
      }
    }
    auto tmpseg = bitwise_merge(newSeg, oldSeg);
    auto mergedNew = tmpseg.first;
    auto mergedOld = tmpseg.second;
    // auto [mergedNew, mergedOld] = bitwise_merge(newSeg, oldSeg);
    if (!mergedOld) {
      return {mergedNew, NULL_PTR, 1};
    }

    if (!(*mergedNew).overlaps((*mergedOld))) {
      return {mergedNew, mergedOld, 1};
    }
    else {
      return {mergedNew, mergedOld, 0};
    }
  }

  // Function to perform bitwise merge of segments
  static std::pair<Segment *, Segment *> bitwise_merge(Segment &new_seg,
                                                       Segment &old_seg) {
    long long lo = std::min(old_seg.x1, new_seg.x1);
    long long hi = std::max(old_seg.x2, new_seg.x2);
    std::bitset<256> new_bm, old_bm;
    new_bm.reset();
    old_bm.reset();

    if (new_seg.consecutive()) {
      for (long long i = new_seg.x1 - lo; i <= new_seg.x2 - lo; i += new_seg.rec_k()) {
        new_bm.set(i);
      }
    }
    else {
      for (long long i = new_seg.x1 - lo; i <= new_seg.x2 - lo; ++i) {
        new_bm[i] = (*new_seg.filter)[i - (new_seg.x1 - lo)];
      }
    }

    if (old_seg.consecutive()) {
      for (long long i = old_seg.x1 - lo; i <= old_seg.x2 - lo; i += old_seg.rec_k()) {
        old_bm.set(i);
      }
    }
    else {
      for (long long i = old_seg.x1 - lo; i <= old_seg.x2 - lo; ++i) {
        old_bm[i] = (*old_seg.filter)[i - (old_seg.x1 - lo)];
      }
    }

    old_bm &= ~new_bm;
    Segment *NULL_PTR = nullptr;
    long long first_valid = old_bm._Find_first();
    if (static_cast<std::size_t>(first_valid) == old_bm.size()) {
      Segment *newSegptr = &new_seg;
      return std::make_pair(newSegptr, NULL_PTR);
    }

    long long last_valid = -1;
    for (long long i = hi - lo; i >= 0; --i) {
      if (old_bm.test(i)) {
        last_valid = i;
        break;
      }
    }

    old_seg.x1 = first_valid + lo;
    old_seg.x2 = last_valid + lo;

    if (!old_seg.consecutive() && BITMAP) {
      for (long long i = first_valid; i <= last_valid; ++i) {
        (*old_seg.filter)[i - first_valid] = old_bm[i];
      }
    }

    // TODO: re-check accuracy and consecutive
    Segment *newSegptr = &new_seg;
    Segment *oldSegptr = &old_seg;
    return (std::make_pair(newSegptr, oldSegptr));
    // return std::make_pair(const_cast<Segment*>(&new_seg),
    // const_cast<Segment*>(&old_seg));
  }

  // Function to calculate the memory required for the segment
  long long memory() {
    long long result = 0;
    if (x1 == x2) {
      result = SUBLPN_BYTES + PPN_BYTES;
    }
    else {
      if (consecutive()) {
        result = SUBLPN_BYTES + PPN_BYTES + FLOAT16_BYTES + LENGTH_BYTES;
      }
      else {
        if (BITMAP) {
          // long long ones = filter.count(); // Count the number of set bits
          long long non_consec_ones = 0;
          for (size_t i = 1; i < (*filter).size() - 1; ++i) {
            if ((*filter)[i] && !(*filter)[i - 1]) {
              ++non_consec_ones;
            }
          }
          result = SUBLPN_BYTES + PPN_BYTES + FLOAT16_BYTES +
                   non_consec_ones * 1 + LENGTH_BYTES;
        }
        else {
          // Calculate memory usage based on filter size
          result = SUBLPN_BYTES + PPN_BYTES + FLOAT16_BYTES +
                   round((*filter).size() / 8.0);
        }
      }
    }
    return result;
  }

  bool consecutive() { 
    return (filter == nullptr);
  }

  bool mergable() { return consecutive() || BITMAP; }

  long long length() { return (x2 - x1) / rec_k() + 1; }

  long long rec_k() { return static_cast<long long>(round(1.0 / k)); }

  long long blocknum() {
    double mid = (x2 + x1) / 2.0;
    double predict = round(mid * k + b);
    return static_cast<long long>(predict / Segment::PAGE_PER_BLOCK);
  }
};

template <typename T>
class KeyWrapper {
 public:
  std::vector<T> &iterable;
  std::function<long long(T &)> key;
  // Constructor
  KeyWrapper(std::vector<T> &iterable, std::function<long long(T &)> key)
      : iterable(iterable), key(key) {}

  // Overload [] operator to apply key function
  long long operator[](size_t i) { return key(iterable[i]); }

  // Get the size of the iterable
  size_t size() { return iterable.size(); }
};

// piecewise linear Regression Class
class PLR {
 public:
  // 3 states of PLR
  string FIRST  = "FIRST";
  string SECOND = "SECOND";
  string READY  = "READY";
  double gamma;
  long long max_length;  // max length of segment
  std::vector<Segment *> segments;
  std::pair<double, double> *s0;
  std::pair<double, double> *s1;
  SimpleSegment *rho_upper;
  SimpleSegment *rho_lower;
  std::pair<double, double> *sint;
  string state;
  std::vector<std::pair<long long, long long>> points;

  PLR(double _gamma = 0) : gamma(_gamma), max_length(256) { init(); }
  // Initialize method
  void init() {
    // temp states to build one next segment
    segments.clear();
    s0 = nullptr;
    s1 = nullptr;
    rho_upper = nullptr;
    rho_lower = nullptr;
    sint = nullptr;
    state = FIRST;
    points.clear();
  }
  std::vector<Segment *> learn(std::vector<std::pair<long long, long long>> &points_) {

    std::vector<Segment *> rejs;
    // long long count = 0;
    long long size = points_.size();
    for (long long i = 0; i < size; ++i) {

      auto tmp_process = process(points_[i]);
      auto seg = tmp_process.first;
      auto rej = tmp_process.second;
      if (seg != nullptr) {
        segments.push_back(seg);
      }
      if (rej != nullptr) {
        rejs.push_back(rej);
      }
    }

    Segment *seg = build_segment();
    if (seg != nullptr) {
      segments.push_back(seg);
    }

    return segments;
  }
  // checking if the max_lpa_range that a segment covers is satisfied
  bool should_stop(std::pair<long long, long long> &point) {
    if (point.first >= s0->first + max_length) {
      return true;
    }
    return false;
  }

  Segment *build_segment() {  // Building segment
    Segment *seg = nullptr;
    if (state == SECOND) {  // make with 2 points
      delete seg;
      seg = new Segment(1, s0->second - s0->first, s0->first, s0->first, &points);
    }
    else if (state == READY) {  // include the new point
      double avg_slope = (rho_lower->k + rho_upper->k) / 2.0;
      double intercept = -sint->first * avg_slope + sint->second;

      if(isnan(sint->first) or isnan(sint->second)) {
        exit(0);
      }
      delete seg;
      seg = new Segment(avg_slope, intercept, s0->first, s1->first, &points);
    }
    return seg;
  }

  std::pair<Segment *, Segment *> process(std::pair<long long, long long> &point) {
    Segment *prev_segment = nullptr;
    if (state == FIRST) {
      delete s0;
      s0 = new std::pair<double, double>(point);
      state = SECOND;
    }
    else if (state == SECOND) {  // construct the segment using 2 points
      if (should_stop(point)) {
        prev_segment = build_segment();
        delete s0;
        s0 = new std::pair<double, double>(point);
        state = SECOND;
        points.clear();
      }
      else {

        delete s1;
        s1 = new std::pair<double, double>(point);
        state = READY;
        // update rho_lower (low slope) & rho_upper (high slope)
        delete rho_lower;
        rho_lower = new SimpleSegment(SimpleSegment::frompoints(
            SimpleSegment::get_upper_bound(*s0, gamma),
            SimpleSegment::get_lower_bound(*s1, gamma)));
        delete rho_upper;
        rho_upper = new SimpleSegment(SimpleSegment::frompoints(
            SimpleSegment::get_lower_bound(*s0, gamma),
            SimpleSegment::get_upper_bound(*s1, gamma)));
        delete sint;
        if(gamma != 0){
          sint = new std::pair<double, double>(
              SimpleSegment::intersection(*rho_upper, *rho_lower));
        } else {
          sint = new std::pair<double, double>{};
          *sint = *s0;
        }
        if(isnan(sint->first) or isnan(sint->second)) {
          printf("NAN SINT\n");
          exit(0);
        }
        state = READY;
      }
    }
    else if (state == READY) {
      // if cannot be approximated using the current segment. (build the
      // previous segment) and create new
      // printf("DEBUG_CHECK2: %f %f\n", s0->first, s0->second); // debug check

      if ((gamma != 0 && (!SimpleSegment::is_above(point, *rho_lower) || !SimpleSegment::is_below(point, *rho_upper)))
          || (gamma == 0 && !SimpleSegment::is_on(point, *rho_lower))
          || should_stop(point)) {

        prev_segment = build_segment();
        delete s0;
        s0 = new std::pair<double, double>(point);
        // printf("IF, point: %f, %f\n", s0->first, s0->second);

        state = SECOND;
        points.clear();
      }
      else {  // update rho_lower and rho_upper in accordance with new point tolerance
        delete s1;
        s1 = new std::pair<double, double>(point);
        auto s_upper = SimpleSegment::get_upper_bound(point, gamma);
        auto s_lower = SimpleSegment::get_lower_bound(point, gamma);
        if (SimpleSegment::is_below(s_upper, *rho_upper)) {
          delete rho_upper;
          rho_upper =
              new SimpleSegment(SimpleSegment::frompoints(*sint, s_upper));
        }
        if (SimpleSegment::is_above(s_lower, *rho_lower)) {
          delete rho_lower;
          rho_lower =
              new SimpleSegment(SimpleSegment::frompoints(*sint, s_lower));
        }
      }
    }
    points.push_back(point);  // insert the point to the array
    return std::make_pair(prev_segment, nullptr);
  }
};

class LogPLR {
 public:
  PLR plr;  // PLR object (initialized with gamma)
  std::vector<std::vector<Segment *>> runs;  // one run is one level of segments (with non-overlapping LPAs)
  long long frame_no;
  // Default constructor
  LogPLR() : frame_no(0) {}

  LogPLR(double gamma, long long frame_no) : plr(gamma), frame_no(frame_no) {}

  void update(std::vector<std::pair<long long, long long>> entries) {
    // Make sure no same LPNs exist in the entries
    // std::sort(entries.begin(), entries.end());
    // Make sure no same 'x1's exist in the new_segments
    plr.init();
    auto new_segments = plr.learn(entries);
    if(new_segments.size() > 1) {
      std::sort(new_segments.begin(), new_segments.end(),
                [](Segment *x, Segment *y) { return x->x1 < y->x1; });
    }
    // self.block_map[blocknum].extend(new_segments);
    // Make sure no overlap at each level
    add_segments(0, new_segments, false);  // Add segments at level 0
    // outputFile << endl;
  }

// LOG_MERGE
  void merge(LogPLR *old_plr) {
    if(old_plr->frame_no != frame_no) {
      printf("%lld, %lld\n", frame_no, old_plr->frame_no);
    }
    assert(frame_no == old_plr->frame_no);
    runs.insert(runs.end(), old_plr->runs.begin(), old_plr->runs.end());
  }

  std::tuple<std::vector<std::tuple<long long, bool, Segment *>>, long long>
  lookup(long long LPA, bool first = true) {
    std::vector<long long> empty_levels;
    std::vector<std::tuple<long long, bool, Segment *>> results;
    long long lookup = 0;

    for (size_t level = 0; level < runs.size(); level++) {
      auto &run = runs[level];
      if (run.empty()) {
        empty_levels.push_back(level);
        continue;
      }
      ++lookup;
      auto index = std::lower_bound(
          run.begin(), run.end(), LPA,
          [](Segment *seg, long long val) { return seg->x1 < val; });
      if (index == run.begin() || (index != run.end() && (*index)->x1 == LPA)) {
        Segment *seg = *index;
        // printf("searching for segment: {%lld, %lld} for LPA: %lld\n", (seg)->x1, (seg)->x2, LPA);
        if(!isnan(seg->get_y(LPA))){
          // printf("Found!!\n");
          results.emplace_back((long long)seg->get_y(LPA), seg->accurate, seg);
          if (first) {
            break;
          }
        }
      }
      else {
        Segment *seg = *(--index);
        // printf("searching for segment: {%lld, %lld} for LPA: %lld\n", (seg)->x1, (seg)->x2, LPA);
        if(!isnan(seg->get_y(LPA))){
          // printf("Found!!\n");
          results.emplace_back((long long)seg->get_y(LPA), seg->accurate, seg);
          if (first) {
            break;
          }
        }
      }
    }

    // Erase empty levels
    for (auto it = empty_levels.rbegin(); it != empty_levels.rend(); ++it) {
      runs.erase(runs.begin() + *it);
    }

    return {results, lookup};
  }

  std::unordered_map<long long, std::vector<Segment *>> lookup_range(
      long long start, long long end) {
    std::unordered_map<long long, std::vector<Segment *>> results;
    for (size_t level = 0; level < runs.size(); ++level) {
      auto &run = runs[level];
      if(run.size() == 0) {
        continue;
      }
      auto it = std::lower_bound(run.begin(), run.end(), start, 
                            [](const Segment* s, int val) { return s->x1 < val; });
      
      if (!(it == run.begin() || (static_cast<long>(it - run.begin()) < static_cast<long>(run.size()) && (*it)->x1 == start))) {
        it--;
      }

      while(it != run.end()) {
        auto seg = *it;
        if(seg->overlaps_with_range(start, end)) {
          results[level].push_back(seg);
          it++;
        } else {
          break;
        }
      }
    }
    return results;
  }

  void add_segments(long long level, std::vector<Segment *> &segments, bool recursive = false) {

    while (runs.size() <=static_cast<std::vector<std::vector<LeaFTL::Segment *>>::size_type>(level)) {
      runs.push_back(std::vector<Segment *>());
    } 

    std::vector<Segment *> &run = runs[level];
    std::vector<Segment *> conflicts;
    for (Segment *new_seg : segments) {
      if (run.empty()) {
        run.push_back(new_seg);
        continue;
      }

      auto it = std::lower_bound(
          run.begin(), run.end(), new_seg,
          [](Segment *a, Segment *b) { return a->x1 < b->x1; });

      size_t index = it - run.begin();
      run.insert(it, new_seg);
      std::vector<std::pair<size_t, Segment *>> overlaps;
      if (index != 0) {
        overlaps.push_back({index - 1, run[index - 1]});
      }
      for (size_t i = index + 1; i < run.size(); ++i) {
        if (run[i]->x1 > new_seg->x2) {
          break;
        }
        overlaps.push_back({i, run[i]});
      }

      std::vector<size_t> indices_to_delete;
      // =============================================================================================
      // debug
      for (auto it = overlaps.begin(); it != overlaps.end(); ++it) {

        auto ind = it->first;
        auto old_seg = it->second;

        bool same_level;
        std::tie(new_seg, old_seg, same_level) = Segment::merge(*new_seg, *old_seg);
        if (old_seg == nullptr) {
          indices_to_delete.push_back(static_cast<uint64_t>(ind));
        }
        else if (!same_level) {
          conflicts.push_back(old_seg);
          indices_to_delete.push_back(static_cast<uint64_t>(ind));
        }
      } 
      // =============================================================================================
      for (auto it = indices_to_delete.rbegin(); it != indices_to_delete.rend();
           ++it) {
        run.erase(run.begin() + *it);
      }
    }

    if (recursive) {
      printf("RECURSIVE\n");
      exit(0);
    }
    else {
      if (!conflicts.empty()) {
        runs.insert(runs.begin() + level + 1, conflicts);
      }
    }
  }

  std::vector<Segment *> segments() {
    std::vector<Segment *> all_segments;
    for (auto &run : runs) {
      for (Segment *seg : run) {
        all_segments.push_back(seg);
      }
    }
    return all_segments;
  }

  long long memory() {
    long long total_memory = 0;
    for (auto &run : runs) {
      for (Segment *seg : run) {
        total_memory += seg->memory();
      }
    }
    return total_memory + LPN_BYTES;
  }

  size_t levels() { return runs.size(); }

  void promote() { // clean
    if (runs.empty() || levels() <= 15) {
      return;
    }

    std::vector<std::vector<Segment *>> layers = runs;
    for (size_t i = 1; i < layers.size(); ++i) {
      auto &lower_layer = layers[i];
      for (auto it = lower_layer.rbegin(); it != lower_layer.rend(); ++it) {
        Segment *old_seg = *it;
        long long promoted_layer = i;
        long long promoted_index = -1;
        for (long long j = i - 1; j >= 0; --j) {
          auto &upper_layer = layers[j];
          auto index = std::lower_bound(
              upper_layer.begin(), upper_layer.end(), old_seg->x1,
              [](Segment *seg, long long val) { return seg->x1 < val; });
          bool overlaps = false;
          for (auto k = std::max(
                   0LL, static_cast<long long>(
                            std::distance(upper_layer.begin(), index) - 1));
               k < static_cast<long long>(upper_layer.size()); ++k) {
            if (upper_layer[k]->x1 > old_seg->x2) {
              break;
            }
            if (upper_layer[k]->overlaps(*old_seg)) {
              overlaps = true;
              break;
            }
          }
          if (overlaps) {
            break;
          }
          else {
            promoted_layer = j;
            promoted_index = static_cast<long long>(
                std::distance(upper_layer.begin(), index));
          }
        }
        if (promoted_layer < static_cast<long long int>(i) &&
            promoted_index != -1) {
          layers[promoted_layer].insert(
              layers[promoted_layer].begin() + promoted_index, old_seg);
          lower_layer.erase(
              std::find(lower_layer.begin(), lower_layer.end(), old_seg));
        }
      }
    }

    runs.clear();
    for (auto &layer : layers) {
      if (!layer.empty()) {
        runs.push_back(layer);
      }
    }
  }

  void compact(bool promote = false) {
    if (runs.empty()) {
      return;
    }

    compact_range(runs.front());

    runs.erase(std::remove_if(
                  runs.begin(), runs.end(),
                  [](std::vector<Segment *> &layer) { return layer.empty(); }),
                  runs.end());

    if (promote) {
      this->promote();
    }
  }
  // Helper method to compact a range
  void compact_range(std::vector<Segment *> &layer) {
    for (Segment *seg : layer) {
      //  std::unordered_map<long long, std::vector<Segment*>> is returned by
      //  lookup_range
      auto results = lookup_range(seg->x1, seg->x2);
      for (auto &pair : results) {
        long long upper_layer = pair.first;
        std::vector<Segment *> &new_segs = pair.second;
        for(auto &pair2: results) {
          long long lower_layer = pair2.first;
          std::vector<Segment *> &old_segs = pair2.second;
          if(upper_layer < lower_layer) {
            for (auto &new_seg: new_segs) {
              for(auto &old_seg: old_segs) {
                LeaFTL::Segment *new_seg_ptr;
                LeaFTL::Segment *updated_old_seg;
                bool same_level; 
                std::tie(new_seg_ptr, updated_old_seg, same_level) =
                    Segment::merge(*new_seg, *old_seg);     
                if(!updated_old_seg) {
                  runs[lower_layer].erase(
                      std::remove(runs[lower_layer].begin(),
                                  runs[lower_layer].end(), old_seg),
                      runs[lower_layer].end());
                  results[lower_layer].erase(
                      std::remove(results[lower_layer].begin(),
                                  results[lower_layer].end(), old_seg),
                      results[lower_layer].end());                  
                }          
              }
            }
          }
        }
      }
    }
  }
};

template <typename T_>
void move_to_head(long long key, std::map<long long, T_> &cache) {
  // Check if the key exists in the cache
  auto it = cache.find(key);
  if (it != cache.end()) {
    // Move the element to the front of the map
    auto value = std::move(it->second);
    cache.erase(it);
    cache.insert(cache.begin(), std::make_pair(key, std::move(value)));
  }
}
template <typename T__>
void evict(std::map<long long, T__> &cache) {
  // Remove the least recently used (LRU) element from the cache
  if (!cache.empty()) {
    auto it = cache.end();
    --it;  // Last element is the LRU one
    cache.erase(it);
  }
}
enum FrameLogPLRState { ON_FLASH, CLEAN, DIRTY };

class FrameLogPLR {
 private:
 public:
  std::unordered_map<long long, LogPLR> frame_on_flash;
  std::unordered_map<long long, long long> memory_counter;
  std::map<long long, LogPLR> frames;
  std::map<std::string, long long> counter;
  double gamma;
  long long max_size;
  long long frame_length;
  long long n_pages_per_block;
  std::string type;
  std::unordered_map<long long, long long> GTD;
  long long current_trans_block;
  long long current_trans_page_offset;
  long long total_memory;
  long long hits;
  long long misses;
  std::unordered_map<long long, bool> dirty;

  FrameLogPLR(std::map<std::string, long long> counter, double gamma,
              long long max_size = 1024 * 1024, long long frame_length = 256,
              std::string ftl_type = "learnedftl",
              long long n_pages_per_block = 256)
      : counter(counter),
        gamma(gamma),
        max_size(max_size),
        frame_length(frame_length),
        n_pages_per_block(n_pages_per_block) {
    SUBLPN_BYTES = 1;
    // n_pages_per_block = 256;
    if (ftl_type == "learnedftl") {
      type = "learnedftl";
      // frame_length = 256;
    }
    else {
      type = "sftl";
      // frame_length = 256;
    }
  }

  LogPLR create_frame(long long frame_no) {
    if (type == "learnedftl") {
      return LogPLR(gamma, frame_no);
    }
    else {
      throw std::runtime_error("NotImplementedError");
    }
  }

  std::map<long long, std::vector<std::pair<long long, long long>>>
                            split_into_frame(long long frame_length, std::vector<std::pair<long long, long long>> &entries) {
    std::map<long long, std::vector<std::pair<long long, long long>>> split_results;

    for (auto &entry : entries) {
      long long lpn = entry.first;
      long long ppn = entry.second;
      split_results[lpn / frame_length].push_back(std::make_pair(lpn, ppn));
    }
    return split_results;
  }

// UPDATE SEGMENTS
  std::pair<std::vector<long long>, std::vector<long long>> update(std::vector<std::pair<long long, long long>> &entries) {

    std::vector<long long> pages_to_write;
    std::vector<long long> pages_to_read;
    auto split_entries = split_into_frame(frame_length, entries);
    // std::vector<long long> frame_nos;
    for (auto it : split_entries) {
      auto frame_no = it.first;
      auto entries = it.second;
      // frame_nos.push_back(frame_no);
      if (frames.find(frame_no) == frames.end()) {
        frames[frame_no] = create_frame(frame_no);
        counter["mapping_table_write_miss"] += 1;
      }
      else {
        counter["mapping_table_write_hit"] += 1;
      } 
      frames[frame_no].update(entries);
      if (frames[frame_no].levels() > 0) {
        // frames[frame_no].compact();
        frames[frame_no].promote();
      }

      dirty[frame_no] = true;

      change_size_of_frame(frame_no, frames[frame_no].memory());
    }
    if (should_flush()) {
      std::tie(pages_to_write, pages_to_read) = flush();
    }
    return std::make_pair(pages_to_write, pages_to_read);
  }

  // look_up operation
  std::tuple<std::vector<std::tuple<long long, bool, Segment *>>, bool,
             std::vector<long long>, std::vector<long long>>
  lookup(long long lpn, bool first = true) {
    // bool should_print = false;
    std::vector<std::tuple<long long, bool, Segment *>> results;
    bool lookup;
    std::vector<Segment *> segmentList1;
    std::vector<Segment *> segmentList2;
    std::vector<long long> pages_to_read;
    std::vector<long long> pages_to_write;
    long long frame_no =
        lpn / frame_length;  // getting the frame_no to which lpn belongs

    if (frames.find(frame_no) != frames.end()) {
      LogPLR frame = frames[frame_no];  // frames is LRU_Cache
      move_to_head(frame_no, frames);
      std::tie(results, lookup) = frame.lookup(lpn, first);
    } else {
      printf("Frame_Number not found");
      std::terminate();
    }

    if (results.size() != 0) {
      counter["mapping_table_read_hit"] += 1;
    }
    else {
      // printf("LOOKUP FRAMES ON FLASH %lld\n", frame_no);
      return std::tie(results, lookup, pages_to_read, pages_to_write);

      counter["mapping_table_read_miss"] += 1;
      LogPLR frame = frame_on_flash[frame_no];
      auto block_num = GTD[frame_no];
      std::tie(results, lookup) = frame.lookup(lpn, first);
      pages_to_read.push_back(block_num);

      if (frames.find(frame_no) != frames.end()) {
        frames[frame_no].merge(&frame);
        frame_on_flash.erase(frame_no);
      }
      else {
        dirty[frame_no] = false;
        frames[frame_no] = frame;
        frame_on_flash.erase(frame_no);
      }

      move_to_head(frame_no, frames);
      change_size_of_frame(frame_no, frames[frame_no].memory());
    }

// NOT USED FOR MAPPING TABLE IN CACHE
    if (should_flush()) {
      std::vector<long long> mapping_pages_to_write;
      std::vector<long long> mapping_pages_to_read;
      std::tie(mapping_pages_to_write, mapping_pages_to_read) = flush();

      pages_to_read.insert(pages_to_read.end(), mapping_pages_to_read.begin(),
                           mapping_pages_to_read.end());
      pages_to_write.insert(pages_to_write.end(),
                            mapping_pages_to_write.begin(),
                            mapping_pages_to_write.end());
    }

    return std::tie(results, lookup, pages_to_read, pages_to_write);
  }

  // Allocation of ppn for a translation page needs to be integrated with
  // SimpleSSD This function is subject to change
  std::pair<long long, long long> allocate_ppn_for_frame(long long frame_no) {
    // Allocating a page in the SSD for translation page
    // Translation page stores the log-table corresponding to one frame

    long long next_free_ppn =
        n_pages_per_block * current_trans_block + current_trans_page_offset;
    current_trans_page_offset += 1;

    long long old_ppn = -1;
    long long new_ppn = -1;

    auto it = GTD.find(frame_no);
    if (it == GTD.end()) {
      old_ppn = -1;
      GTD[frame_no] = next_free_ppn;
      new_ppn = GTD[frame_no];
    }
    else {
      old_ppn = it->second;
      GTD[frame_no] = next_free_ppn;
      new_ppn = GTD[frame_no];
    }

    return {new_ppn, old_ppn};
  }

  std::pair<std::vector<long long>, std::vector<long long>> flush() {
    std::vector<long long> evicted_frames;
    std::vector<long long> pages_to_read;
    std::vector<long long> pages_to_write;

    long long original_memory = memory();
    // assert(memory == std::accumulate(frames.begin(), frames.end(), 0, [](long
    // long sum, const auto& kv) { return sum + kv.second.memory; }));
    while (original_memory > max_size) {
      auto it = frames.begin();
      long long frame_no = it->first;
      LogPLR evict_frame = it->second;
      frames.erase(it);
      // log_msg(frame_no, "evicted")
      long long freed_mem = memory_counter[frame_no];
      original_memory -= freed_mem;
      change_size_of_frame(frame_no, 0);
      evicted_frames.push_back(frame_no);

      std::pair<long long, long long> alloc_result =
          allocate_ppn_for_frame(frame_no);
      long long new_ppn = alloc_result.first;
      long long old_ppn = alloc_result.second;
      if (dirty[frame_no]) {
        counter["flush mapping table"] += 1;
      }
      dirty[frame_no] = false;
      pages_to_write.push_back(new_ppn);

      auto it_flash = frame_on_flash.find(frame_no);
      if (it_flash != frame_on_flash.end()) {
        // log_msg(frame_no, "merged with flash")
        LogPLR old_frame = it_flash->second;
        pages_to_read.push_back(old_ppn);
        evict_frame.merge(&old_frame);
      }
      frame_on_flash[frame_no] = evict_frame;
    }

    // log_msg("%.2f miss ratio, %s evicted, %d memory, %d in cache, %d on
    // flash" % (misses / float(misses + hits), evicted_frames, memory,
    // frames.size(), frame_on_flash.size())); log_msg("%d miss, %d memory
    // flushed, %s evicted, %d memory, %d in cache, %d on flash" % (misses,
    // original_memory - memory, evicted_frames, memory, frames.size(),
    // frame_on_flash.size()));

    return std::make_pair(pages_to_write, pages_to_read);
  }

  // compact function
  void compact(bool promote = false,
               std::vector<long long> *frame_nos = nullptr) {
    if (!frame_nos) {
      for (auto frame : frames) {
        frame.second.compact(promote = promote);
        change_size_of_frame(frame.first, frame.second.memory());
      }
    }
    else {
      for (auto frame_no : *frame_nos) {
        auto frame = frames[frame_no];
        frame.compact(promote = promote);
        change_size_of_frame(frame_no, frame.memory());
      }
    }
  }

  void promote() {
    for (auto frame : frames) {
      frame.second.promote();
    }
  }

  void change_size_of_frame(long long frame_no, long long new_mem) {
    long long old_mem = 0;
    auto it = memory_counter.find(frame_no);
    if (it != memory_counter.end()) {
      old_mem = it->second;
    }
    memory_counter[frame_no] = new_mem;
    total_memory += (new_mem - old_mem);
  }
  // PROPERTIES---
  bool should_flush() {  // if the frames are needed to be flushed to flash
    // if the complete table is in cache
    return false;
    if (memory() > max_size) {
      return true;
    }
    return false;
  }
  long long memory() {  // returns the total_memory of Frames(LRU cache)
    return total_memory;
  }
  long long groups() {  // number of frames stored in map
    return frames.size();
  }
  std::vector<Segment *> segments() {
    std::vector<Segment *> result;
    for (auto frame : frames) {
      auto segs = frame.second.segments();
      result.insert(result.end(), segs.rbegin(), segs.rend());
    }
    return result;
  }
  // Implement levels property
  long long levels() {
    if (frames.empty()) {
      return 0;
    }
    long long maxLevels = 0;
    for (auto &frame : frames) {
      maxLevels = std::max(maxLevels, (long long)(frame.second.levels()));
    }
    return maxLevels;
  }
  // Implement avg_levels property
  std::pair<double, double> avg_levels() {
    if (frames.empty()) {
      return {0, 0};
    }
    std::vector<long long> dist;
    for (auto &frame : frames) {
      long long levels = frame.second.levels();
      if (levels != 0) {
        dist.push_back(levels);
      }
    }
    if (dist.empty()) {
      return {0, 0};
    }
    double sum = 0;
    for (long long level : dist) {
      sum += level;
    }
    double avg = sum / dist.size();
    double variance = 0;
    for (long long level : dist) {
      variance += (level - avg) * (level - avg);
    }
    variance /= dist.size();
    return {avg, std::sqrt(variance)};
  }
};
}  // namespace LeaFTL
#include<chrono>
using namespace std::chrono;
namespace SimpleSSD {

namespace FTL {

// std::string filename = "../LeaFTLout.txt";
// // Open the file for writing
// std::ofstream outputFile(filename);

double gamma = 0.0;
std::map<std::string, long long> counter;
// mapping table utility for LeaFTL
uint64_t pagesInBlock = 256;
uint64_t block_size = pagesInBlock;  // number of pages in a block

LeaFTL::FrameLogPLR mapping_table(counter, gamma, pagesInBlock);

PageMapping::PageMapping(ConfigReader &c, Parameter &p, PAL::PAL *l,
                         DRAM::AbstractDRAM *d)
    : AbstractFTL(p, l, d),
      pPAL(l),
      conf(c),
      lastFreeBlock(param.pageCountToMaxPerf),
      lastFreeBlockIOMap(param.ioUnitInPage),
      bReclaimMore(false),
      pageMoveStats(param.pagesInBlock) {
  blocks.reserve(param.totalPhysicalBlocks);  // number of blocks
  table.reserve(param.totalLogicalBlocks *
                param.pagesInBlock);  // overall table for mapping number of
                                      // pages in the SSD.

  for (uint32_t i = 0; i < param.totalPhysicalBlocks; i++) {
    freeBlocks.emplace_back(
        Block(i, param.pagesInBlock,
              param.ioUnitInPage));  // free block list with the defualt values
  }

  nFreeBlocks = param.totalPhysicalBlocks;

  status.totalLogicalPages = param.totalLogicalBlocks * param.pagesInBlock;
  static const EVICT_POLICY policy =
      (EVICT_POLICY)conf.readInt(CONFIG_FTL, FTL_GC_EVICT_POLICY);
  if (policy == CACHED_GC) {
    printf("VictimBlockSelctionPolicy: CachedGC_ActualPaperCGCT\n");
    cachedGC = true;
    victimSelectionPolicy = 4;
  }
  else if (policy == HOTBLOCK) {
    printf("VictimBlockSellctionPolicy: HotData\n");
    victimSelectionPolicy = 5;
  }
  else {
    if (optimalreplacementPolicy) {
      printf("VictimBlockSelctionPolicy: OptimalReplacement\n");
    }
    else {
      printf("VictimBlockSelctionPolicy: LruPolicy\n");
    }
    cachedGC = false;
    victimSelectionPolicy = 0;
    ;
    if (useDeadOnArrivalPredictor) {
      printf("DeadOnArrivalPredictior: Enabled\n");
    }
    else {
      printf("DeadOnArrivalPredictior: Disabled\n");
    }
    if (usePortionOfCacheAsShadow) {
      printf("CachePortionusedAsShadow\n");
    }
  }
  // Allocate free blocks
  // printf("####Pages is in block##### %lu\n",param.pagesInBlock);

  for (uint32_t i = 0; i < param.pageCountToMaxPerf;
       i++) {  // get free blocks equal to the maxPref
    lastFreeBlock.at(i) = getFreeBlock(i);
  }

  lastFreeBlockIndex = 0;
  // pCache->write(Request &, uint64_t &);
  memset(&stat, 0, sizeof(stat));
  memset(&cmtStats, 0, sizeof(cmtStats));
  memset(&ftlStats, 0, sizeof(ftlStats));

  // cmtSize= 262144;
  // cmtSize = 131136 ; 65536
  // cmtSize = 16392 ;
  predictorThreshold = 2;
  shadowwCacheSize = (5 * cmtSize) / 100;  // shadowCache is 5% of cmt table;
  if (usePortionOfCacheAsShadow) {
    cmtSize = cmtSize - shadowwCacheSize;
  }
  // cmtSize=8;
  // optimalreplacementPolicy = true ;
  bRandomTweak = conf.readBoolean(CONFIG_FTL, FTL_USE_RANDOM_IO_TWEAK);
  bitsetSize = bRandomTweak ? param.ioUnitInPage : 1;
  pFTL->getInfo();
  // pCache = new GenericCache(conf, pFTL, pDRAM);
}

PageMapping::~PageMapping() {}

bool PageMapping::initialize() {
  uint64_t nPagesToWarmup;
  uint64_t nPagesToInvalidate;
  uint64_t nTotalLogicalPages;
  uint64_t maxPagesBeforeGC;
  uint64_t tick;
  uint64_t valid;
  uint64_t invalid;
  FILLING_MODE mode;
  startTime = std::chrono::high_resolution_clock::now();
  Request req(param.ioUnitInPage);

  debugprint(LOG_FTL_PAGE_MAPPING, "Initialization started");

  nTotalLogicalPages = param.totalLogicalBlocks * param.pagesInBlock;
  nPagesToWarmup =
      nTotalLogicalPages *
      conf.readFloat(CONFIG_FTL,
                     FTL_FILL_RATIO);  // percet of pages ued for warrmup
  nPagesToInvalidate =
      nTotalLogicalPages * conf.readFloat(CONFIG_FTL, FTL_INVALID_PAGE_RATIO);
  mode = (FILLING_MODE)conf.readUint(CONFIG_FTL, FTL_FILLING_MODE);
  maxPagesBeforeGC =
      param.pagesInBlock *
      (param.totalPhysicalBlocks *
           (1 - conf.readFloat(CONFIG_FTL, FTL_GC_THRESHOLD_RATIO)) -
       param.pageCountToMaxPerf);  // # free blocks to maintain

  if (nPagesToWarmup + nPagesToInvalidate > maxPagesBeforeGC) {
    warn("ftl: Too high filling ratio. Adjusting invalidPageRatio.");
    nPagesToInvalidate = maxPagesBeforeGC - nPagesToWarmup;
    ;
    ;
  }
  printf("CMTSize: %lu\n", cmtSize);

  printf("ShadowCacheSize: %lu\n", shadowwCacheSize);
  printf("predictionThrushold: %lu\n", predictorThreshold);

  printf("Total LogicalPages: %lu\n", nTotalLogicalPages);
  printf("WarmUp Pages: %lu\n", nPagesToWarmup);
  printf("PagesToInvalidate: %lu\n", nPagesToInvalidate);
  printf("pages avaialbe for writes %lu\n",
         (nTotalLogicalPages - (nPagesToWarmup + nPagesToInvalidate)));

  debugprint(LOG_FTL_PAGE_MAPPING, "Total logical pages: %" PRIu64,
             nTotalLogicalPages);
  debugprint(LOG_FTL_PAGE_MAPPING,
             "Total logical pages to fill: %" PRIu64 " (%.2f %%)",
             nPagesToWarmup, nPagesToWarmup * 100.f / nTotalLogicalPages);
  debugprint(LOG_FTL_PAGE_MAPPING,
             "Total invalidated pages to create: %" PRIu64 " (%.2f %%)",
             nPagesToInvalidate,
             nPagesToInvalidate * 100.f / nTotalLogicalPages);

  req.ioFlag.set();
  printf("Ration of free blocks available Before Warmup %f nFreeBlocks %u "
         "Pages %lu\n",
         freeBlockRatio(), nFreeBlocks, nFreeBlocks * param.pagesInBlock);
  ;
  // Step 1. Filling
  if (mode == FILLING_MODE_0 ||
      mode == FILLING_MODE_1) {  // for 0 and 1 it is sequential fill
    // Sequential
    for (uint64_t i = 0; i < nPagesToWarmup; i++) {
      tick = 0;
      req.lpn = i;
      writeInternal(req, tick, false);
    }
  }
  else {
    // Random
    std::random_device rd;
    // std::mt19937_64 gen(rd());
    std::mt19937_64 gen(42);
    std::uniform_int_distribution<uint64_t> dist(0, nTotalLogicalPages - 1);

    for (uint64_t i = 0; i < nPagesToWarmup; i++) {
      tick = 0;
      req.lpn = dist(gen);              // generate the logical addreses
      writeInternal(req, tick, false);  // just fill the storage
    }
  }
  // printf("Number of page to invalidate %lu\n",nPagesToInvalidate);
  //  Step 2. Invalidating
  if (mode == FILLING_MODE_0) {
    // Sequential
    for (uint64_t i = 0; i < nPagesToInvalidate; i++) {
      tick = 0;
      req.lpn = i;
      writeInternal(req, tick, false);
    }
  }
  else if (mode == FILLING_MODE_1) {
    // Random
    // We can successfully restrict range of LPN to create exact number of
    // invalid pages because we wrote in sequential mannor in step 1.
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist(0, nPagesToWarmup - 1);

    for (uint64_t i = 0; i < nPagesToInvalidate; i++) {
      tick = 0;
      req.lpn = dist(gen);
      // writeInternal(req, tick, false);
      writeInternal(req, tick, false);
    }
  }
  else {
    // Random
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist(0, nTotalLogicalPages - 1);

    for (uint64_t i = 0; i < nPagesToInvalidate; i++) {
      tick = 0;
      req.lpn = dist(gen);
      writeInternal(req, tick, false);
    }
  }

  // Report
  calculateTotalPages(valid, invalid);  //
  memset(&ftlStats, 0, sizeof(ftlStats));

  printf("Valid Pages %lu Invalid Page %lu\n", valid, invalid);
  debugprint(LOG_FTL_PAGE_MAPPING, "Filling finished. Page status:");
  debugprint(LOG_FTL_PAGE_MAPPING,
             "  Total valid physical pages: %" PRIu64
             " (%.2f %%, target: %" PRIu64 ", error: %" PRId64 ")",
             valid, valid * 100.f / nTotalLogicalPages, nPagesToWarmup,
             (int64_t)(valid - nPagesToWarmup));
  debugprint(LOG_FTL_PAGE_MAPPING,
             "  Total invalid physical pages: %" PRIu64
             " (%.2f %%, target: %" PRIu64 ", error: %" PRId64 ")",
             invalid, invalid * 100.f / nTotalLogicalPages, nPagesToInvalidate,
             (int64_t)(invalid - nPagesToInvalidate));
  debugprint(LOG_FTL_PAGE_MAPPING, "Initialization finished");
  printf("Ration of free blocks available After Warmup %f nFreeBlocks %u Pages "
         "%lu\n",
         freeBlockRatio(), nFreeBlocks, nFreeBlocks * param.pagesInBlock);
/*
  int count = 0;
  for(auto i:table) {
    count++;
    printf("Size of table = %zu\n", i.second.size());
    for(int k = 0; k<static_cast<int>(i.second.size()); k++) {
      printf("LPN = %lu Block Id %u PageId %u\n",i.first, i.second[k].first, i.second[k].second);
    }
    if(count >= 1000) {
      break;
    }
  }
  printf("Table Size: %zu", table.size());
  panic("Warmup complete");*/
  nlogicalpagesinSSD = nTotalLogicalPages;
  return true;
}

void PageMapping::read(Request &req, uint64_t &tick) {
  uint64_t begin = tick;
  ;

  if ((iclCount % 500000 == 0) && printDeadOnArrival) {
    DeadPagePercentage.push_back(returnDeadPagePErcent());
  }

  if (req.ioFlag.count() > 0) {
    // addLbaToOptMap(req.lpn);
    if (!optimalreplacementPolicy) {
      addLbaToOptMap(req.lpn);
    }
    readInternal(req, tick);
    ftlStats.TotalFtlReadRequests++;

    debugprint(LOG_FTL_PAGE_MAPPING,
               "READ  | LPN %" PRIu64 " | %" PRIu64 " - %" PRIu64 " (%" PRIu64
               ")",
               req.lpn, begin, tick, tick - begin);
  }
  else {
    warn("FTL got empty request");
  }

  tick += applyLatency(CPU::FTL__PAGE_MAPPING, CPU::READ);
  // printf(" AFter REad %lu Cached Page Count
  // %lu\n",req.lpn,getBlockCachedPageCount());
}
void PageMapping ::undoCached(uint64_t lpn) {
  auto mappingList =
      table.find(lpn);  // mappinglist is the iterator of the table.
  std::unordered_map<uint32_t, Block>::iterator block;

  if (mappingList != table.end()) {  // agr entry mil gayi
    // printf("In write Mapping found for the LPN %lu\n",req.lpn);
    for (uint32_t idx = 0; idx < bitsetSize; idx++) {
      auto &mapping = mappingList->second.at(idx);

      if (mapping.first < param.totalPhysicalBlocks &&  // blockid and
          mapping.second < param.pagesInBlock) {        // page id
        block = blocks.find(mapping.first);
        if (block != blocks.end()) {
          undoCaching++;
          block->second.deCache(mapping.second);
        }
      }
    }
  }
}
float PageMapping::returnDeadPagePErcent() {
  size_t totalEntries = cmt.size();
  size_t countAccessCountOne = 0;

  for (const auto &entry : cmt) {
    if (entry.second.cmtEntryAccessCount == 1) {
      countAccessCountOne++;
    }
  }

  // Calculate the percentage
  return (countAccessCountOne * 100.0) / totalEntries;
}
void PageMapping::addLbaToOptMap(uint64_t lpn) {
  /*auto it = optMap.find(lpn);

    // If the key is present, append the entry to the vector
    if (it != optMap.end()) {
        it->second.push_back(iclCount);
    } else {
        // If the key is not present, create a new vector with the entry and
    insert it into the mapq optMap[lpn] = {iclCount};
    }*/
  auto it = AddressReuseDistanceMap.find(lpn);
  uint64_t reuseDistance = 0;
  // If the key is present, append the entry to the vector
  if (it != AddressReuseDistanceMap.end()) {
    // it->second.push_back(iclCount);
    reuseDistance = iclCount - it->second;
    if (reuseDistance >= 8196 && reuseDistance < 16391) {
      AddressReuseDistanceClusters[16000]++;
    }
    else if (reuseDistance >= 16392 && reuseDistance < 32783) {
      AddressReuseDistanceClusters[32000]++;
    }
    else if (reuseDistance >= 32784 && reuseDistance < 65567) {
      AddressReuseDistanceClusters[64000]++;
    }
    else if (reuseDistance >= 65568) {
      AddressReuseDistanceClusters[131136]++;
    }
    else {
      AddressReuseDistanceClusters[4000]++;
    }
    it->second = iclCount;
  }
  else {
    // If the key is not present, create a new vector with the entry and insert
    // it into the mapq
    AddressReuseDistanceMap[lpn] = {iclCount};
  }
}
void PageMapping::write(Request &req, uint64_t &tick) {
  uint64_t begin = tick;
  if ((iclCount % 500000 == 0) && printDeadOnArrival) {
    DeadPagePercentage.push_back(returnDeadPagePErcent());
  }

  if (req.ioFlag.count() > 0) {
    if (!optimalreplacementPolicy) {
      addLbaToOptMap(req.lpn);
    }
    writeInternal(req, tick);
    // addLbaToOptMap(req.lpn);

    ftlStats.TotalFtlWriteRequests++;
    debugprint(LOG_FTL_PAGE_MAPPING,
               "WRITE | LPN %" PRIu64 " | %" PRIu64 " - %" PRIu64 " (%" PRIu64
               ")",
               req.lpn, begin, tick, tick - begin);
  }
  else {
    warn("FTL got empty request");
  }

  tick += applyLatency(CPU::FTL__PAGE_MAPPING, CPU::WRITE);
}

void PageMapping::trim(Request &req, uint64_t &tick) {
  uint64_t begin = tick;

  trimInternal(req, tick);

  debugprint(LOG_FTL_PAGE_MAPPING,
             "TRIM  | LPN %" PRIu64 " | %" PRIu64 " - %" PRIu64 " (%" PRIu64
             ")",
             req.lpn, begin, tick, tick - begin);

  tick += applyLatency(CPU::FTL__PAGE_MAPPING, CPU::TRIM);
}

void PageMapping::format(LPNRange &range, uint64_t &tick) {  //
  PAL::Request req(param.ioUnitInPage);
  std::vector<uint32_t> list;

  req.ioFlag.set();

  for (auto iter = table.begin(); iter != table.end();) {
    if (iter->first >= range.slpn && iter->first < range.slpn + range.nlp) {
      auto &mappingList = iter->second;

      // Do trim
      for (uint32_t idx = 0; idx < bitsetSize; idx++) {
        auto &mapping = mappingList.at(idx);
        auto block = blocks.find(mapping.first);

        if (block == blocks.end()) {
          panic("Block is not in use");
        }

        block->second.invalidate(mapping.second, idx);

        // Collect block indices
        list.push_back(mapping.first);
      }

      iter = table.erase(iter);
    }
    else {
      iter++;
    }
  }

  // Get blocks to erase
  std::sort(list.begin(), list.end());  // victim block
  auto last = std::unique(list.begin(), list.end());
  list.erase(last, list.end());

  // Do GC only in specified blocks
  doGarbageCollection(list, tick);

  tick += applyLatency(CPU::FTL__PAGE_MAPPING, CPU::FORMAT);
}

Status *PageMapping::getStatus(uint64_t lpnBegin, uint64_t lpnEnd) {
  status.freePhysicalBlocks = nFreeBlocks;

  if (lpnBegin == 0 && lpnEnd >= status.totalLogicalPages) {
    status.mappedLogicalPages = table.size();
  }
  else {
    status.mappedLogicalPages = 0;

    for (uint64_t lpn = lpnBegin; lpn < lpnEnd; lpn++) {
      if (table.count(lpn) > 0) {
        status.mappedLogicalPages++;
      }
    }
  }

  return &status;
}

float PageMapping::freeBlockRatio() {
  return (float)nFreeBlocks / param.totalPhysicalBlocks;
}

void PageMapping::resetBlockAccesCount() {
  // Check if 5 minutes have passed
  for (auto &block : blocks) {
    block.second.setBlockAccessCountZero();
  }

  // Update the start time to the current time
}
uint32_t PageMapping::convertBlockIdx(uint32_t blockIdx) {
  return blockIdx % param.pageCountToMaxPerf;
}

uint32_t PageMapping::getFreeBlock(uint32_t idx) {
  uint32_t blockIndex = 0;

  if (idx >= param.pageCountToMaxPerf) {
    panic("Index out of range");
  }
  // printf("n Free blocks %u\n",nFreeBlocks);
  if (nFreeBlocks > 0) {
    // Search block which is blockIdx % param.pageCountToMaxPerf == idx
    auto iter = freeBlocks.begin();

    for (; iter != freeBlocks.end(); iter++) {
      blockIndex = iter->getBlockIndex();

      if (blockIndex % param.pageCountToMaxPerf == idx) {
        break;
      }
    }
    // printf("ReqBlockId %u gerblockid %u BlockSize %ld LastFreeBlocks %ld
    // NfreeBlocks
    // %ld\n",idx,blockIndex,blocks.size(),lastFreeBlock.size(),freeBlocks.size());
    // Sanity check
    if (iter == freeBlocks.end()) {
      // Just use first one
      iter = freeBlocks.begin();
      blockIndex = iter->getBlockIndex();
    }

    // Insert found block to block list
    if (blocks.find(blockIndex) != blocks.end()) {
      panic("Corrupted");
    }

    blocks.emplace(blockIndex, std::move(*iter));

    freeBlocks.erase(iter);
    nFreeBlocks--;
  }
  else {
    printf("GC count is %d IO Count : %ld\n", gcCounter, myIoCount);
    panic("No free block left");
  }

  return blockIndex;
}

uint32_t PageMapping::getLastFreeBlock(
    Bitset &iomap) {  // this is the information about the new fee block..

  if (!bRandomTweak || (lastFreeBlockIOMap & iomap).any()) {
    // Update lastFreeBlockIndex
    lastFreeBlockIndex++;

    if (lastFreeBlockIndex == param.pageCountToMaxPerf) {
      lastFreeBlockIndex = 0;
    }

    lastFreeBlockIOMap = iomap;
  }
  else {
    lastFreeBlockIOMap |= iomap;
  }
  auto it = std::find(erased_block_id.begin(), erased_block_id.end(),
                      lastFreeBlock.at(lastFreeBlockIndex));
  if (it != erased_block_id.end()) {  // if this block is already erased!
    uint32_t erased_block = lastFreeBlock.at(lastFreeBlockIndex);
    lastFreeBlock.at(lastFreeBlockIndex) =
        getFreeBlock(lastFreeBlockIndex);  // get a new block//
    erased_block_id.erase(std::remove(erased_block_id.begin(),
                                      erased_block_id.end(), erased_block),
                          erased_block_id.end());
  }

  auto freeBlock = blocks.find(lastFreeBlock.at(lastFreeBlockIndex));

  // Sanity check
  if (freeBlock == blocks.end()) {
    panic("Corrupted");
  }
  if (freeBlock->second.getNextWritePageIndex() == param.pagesInBlock) {
    lastFreeBlock.at(lastFreeBlockIndex) = getFreeBlock(lastFreeBlockIndex);

    bReclaimMore = true;
  }

  return lastFreeBlock.at(
      lastFreeBlockIndex);  // returns the block id of the last free block.
}

uint32_t PageMapping::getUpdatedLastFreeBlock() {
  uint32_t blockIndexofLeastUsedBlockInFreeBlocks =
      findBlockWithLeastValidDirtyPagesFromVector();  // this will return the id
                                                      // of the leastUsedBlock;

  // Sanity check
  // printf("Value Returned from fun
  // %u\n",blockIndexofLeastUsedBlockInFreeBlocks);

  auto it = std::find(erased_block_id.begin(), erased_block_id.end(),
                      lastFreeBlock.at(blockIndexofLeastUsedBlockInFreeBlocks));
  if (it != erased_block_id.end()) {  // if this block is already erased!
    uint32_t erased_block =
        lastFreeBlock.at(blockIndexofLeastUsedBlockInFreeBlocks);
    lastFreeBlock.at(blockIndexofLeastUsedBlockInFreeBlocks) = getFreeBlock(
        blockIndexofLeastUsedBlockInFreeBlocks);  // get a new block//
    erased_block_id.erase(std::remove(erased_block_id.begin(),
                                      erased_block_id.end(), erased_block),
                          erased_block_id.end());
  }
  auto freeBlock =
      blocks.find(lastFreeBlock.at(blockIndexofLeastUsedBlockInFreeBlocks));

  if (freeBlock == blocks.end()) {
    panic("Corrupted");
  }

  // If current free block is full, get next block
  if (freeBlock->second.getNextWritePageIndex() == param.pagesInBlock) {
    lastFreeBlock.at(blockIndexofLeastUsedBlockInFreeBlocks) =
        getFreeBlock(blockIndexofLeastUsedBlockInFreeBlocks);

    bReclaimMore = true;
  }

  // printf("Returned Block
  // %u\n",lastFreeBlock.at(blockIndexofLeastUsedBlockInFreeBlocks));
  return lastFreeBlock.at(blockIndexofLeastUsedBlockInFreeBlocks);
}

uint32_t PageMapping::findBlockWithLeastValidDirtyPagesFromVector() {
  /*uint32_t returnId = 0;
 // uint32_t leastValidDirtyCount = std::numeric_limits<uint32_t>::max(); //
 Initialize with a high value uint32_t leastValidDirtyCount = 0; for(size_t i =
 0; i < lastFreeBlock.size(); ++i)
  {
    auto block = blocks.find(lastFreeBlock.at(i));
     if (block != blocks.end()) {
          //const Block &blockObj = block->second;

          // Calculate the total of valid pages and dirty pages
          uint32_t validDirtyCount = block->second.getValidPageCount() +
 block->second.getDirtyPageCount() ;

          // Update leastValidDirtyBlock if the current block has fewer valid +
 dirty pages if (validDirtyCount > leastValidDirtyCount) { leastValidDirtyCount
 = validDirtyCount; returnId=i;
          }
      }
  }

  return returnId;// this will be the blockId OF lEAST USED BLOCK..*/
  static uint32_t currentIndex =
      0;  // Keep track of the current index in lastFreeBlock
  static uint32_t currentPageInBlock = 0;

  // If all pages in the current block are written, move to the next block ID in
  // lastFreeBlock
  if (currentPageInBlock >= param.pagesInBlock) {
    currentIndex =
        ((currentIndex + 1) %
         param.pageCountToMaxPerf);  // Move to the next index circularly
    currentPageInBlock = 0;          // Reset page count for the new block
  }

  uint32_t returnId = currentIndex;
  currentPageInBlock++;  // Increment page count for the current block
                         // printf("Id Returned %u\n",returnId);
  return returnId;       // Return the block ID from lastFreeBlock
}

void PageMapping::calculateVictimWeight(
    std::vector<std::pair<uint32_t, float>> &weight, const EVICT_POLICY policy,
    uint64_t tick) {
  float temp;
  // printf("Check if you have entered in the old gc scheme\n");
  weight.reserve(blocks.size());
  // cachedWeight.reserve(blocks.size());

  switch (policy) {
    case POLICY_GREEDY:
    case POLICY_RANDOM:
    case POLICY_DCHOICE:
      for (auto &iter : blocks) {
        if (iter.second.getNextWritePageIndex() != param.pagesInBlock) {
          continue;
        }

        weight.push_back(
            {iter.first,
             iter.second.getValidPageCountRaw()});  // here they have direct
      }
      break;
    case HOTBLOCK:
      for (auto &iter : blocks) {
        if (iter.second.getNextWritePageIndex() != param.pagesInBlock) {
          continue;
        }

        weight.push_back(
            {iter.first,
             iter.second.getBlockAccessCount()});  // here they have direct
      }
      break;
    case POLICY_COST_BENEFIT:
      for (auto &iter : blocks) {
        if (iter.second.getNextWritePageIndex() != param.pagesInBlock) {
          continue;
        }

        temp = (float)(iter.second.getValidPageCountRaw()) / param.pagesInBlock;

        weight.push_back(
            {iter.first,
             temp / ((1 - temp) * (tick - iter.second.getLastAccessedTime()))});
      }

      break;
    default:
      panic("Invalid evict policy");
      ;
  }
}
void PageMapping::calculateVictimCachedWeight(
    std::vector<std::pair<uint32_t, std::pair<uint32_t, uint32_t>>>
        &cachedWeight) {
  // printf

  cachedWeight.reserve(blocks.size());

  for (auto &iter : blocks) {
    if (iter.second.getNextWritePageIndex() != param.pagesInBlock) {
      continue;
    }

    // weight.push_back( {iter.first, iter.second.getValidPageCountRaw()});  //
    // here they have direct
    cachedWeight.push_back(std::make_pair(
        iter.first, std::make_pair(iter.second.getCachedPageCount(),
                                   (iter.second.getValidPageCount()))));
    // cachedWeight.push_back(std::make_pair(iter.first,
    // std::make_pair((iter.second.getValidPageCount()-
    // iter.second.getCachedPageCount()), (iter.second.getValidPageCount()))));
    // //pagesnotcached and valid pages
    // printf("Block Id %u AccCount
    // %u\n",iter.second.getBlockIndex(),iter.second.getBlockAccessCount());
    ftlStats.cachedpagesfoundInAllBlocks += iter.second.getCachedPageCount();
    // HotnessMeter[iter.second.getBlockAccessCount()]++;
  }
  // printf("Size of cachedWeight %ld\n",cachedWeight.size());
}

uint64_t PageMapping::getAccessCountOfLBA(uint64_t lpn) {
  auto iter = globalLBaReuseMap.find(lpn);
  uint64_t temp = 0;
  if (iter != globalLBaReuseMap.end()) {
    temp = iter->second;
  }
  return temp;
}
uint32_t PageMapping::getLPNEvictionFrequency(uint64_t lpn) {
  auto iter = lbaEvictionFrequency.find(lpn);
  uint32_t temp = 0;
  if (iter != lbaEvictionFrequency.end()) {
    temp = iter->second;
  }
  return temp;
}
void PageMapping::findHotAccessBlock(
    std::vector<std::pair<uint32_t, std::pair<uint32_t, uint32_t>>>
        &blockAccessCountVector) {
  blockAccessCountVector.reserve(blocks.size());

  for (auto &iter : blocks) {
    if (iter.second.getNextWritePageIndex() != param.pagesInBlock) {
      continue;
    }

    blockAccessCountVector.push_back(std::make_pair(
        iter.first,
        std::make_pair(
            iter.second.getBlockAccessCount(),
            (iter.second.getCachedPageCount()))));  // here they have direct
  }
}

void PageMapping::callBlockResetFunction(uint64_t &tick) {
  uint64_t elapsedTime = tick - startingTick;
  // printf("ElapsedTime: %lu \n",elapsedTime);

  if (elapsedTime >= 10000000000000) {
    // resetBlockAccesCount();
    startingTick = tick;
  }
}
void PageMapping::selectVictimBlock(
    std::vector<uint32_t> &list,
    uint64_t &tick) {  // this is the list of of items it will take with itself
  static const GC_MODE mode = (GC_MODE)conf.readInt(
      CONFIG_FTL, FTL_GC_MODE);  // some configuration being read
  static const EVICT_POLICY policy =
      (EVICT_POLICY)conf.readInt(CONFIG_FTL, FTL_GC_EVICT_POLICY);
  static uint32_t dChoiceParam =
      conf.readUint(CONFIG_FTL, FTL_GC_D_CHOICE_PARAM);
  uint64_t nBlocks = conf.readUint(
      CONFIG_FTL, FTL_GC_RECLAIM_BLOCK);  // claiming n blocks to free
  std::vector<std::pair<uint32_t, float>> weight;
  std::vector<std::pair<uint32_t, std::pair<uint32_t, uint32_t>>>
      blockAccessCountVector;  // weight of each block
  std::vector<std::pair<uint32_t, std::pair<uint32_t, uint32_t>>>
      cachedWeight;  // ok

  list.clear();

  // Calculate number of blocks to reclaim
  if (mode == GC_MODE_0) {
    // DO NOTHING
  }
  else if (mode == GC_MODE_1) {
    static const float t = conf.readFloat(CONFIG_FTL, FTL_GC_RECLAIM_THRESHOLD);

    nBlocks = param.totalPhysicalBlocks * t - nFreeBlocks;
    // printf("OUt side recliam blocks are ");;
  }
  else {
    panic("Invalid GC mode");
  }

  // reclaim one more if last free block fully used
  if (bReclaimMore) {
    // nBlocks += param.pageCountToMaxPerf;// change is done here

    bReclaimMore = false;
  }

  // Calculate weights of all blocks
  //   // sending the weight vector, policy and the time
  if (policy == CACHED_GC) {
    calculateVictimCachedWeight(
        cachedWeight);  // this should call the cachedGC Algorithm
  }
  else {
    /*if(policy==HOTBLOCK)
    {
    findHotAccessBlock(blockAccessCountVector);// this will call the hot block
    victim Selection algorithm
      }
    else
    {}*/
    if (pageAwareofCache) {
      calculateVictimCachedWeight(cachedWeight);
    }
    else {
      calculateVictimWeight(
          weight, policy,
          tick);  // this will call the basic greedy victim selection
    }
  }

  if (policy == POLICY_RANDOM || policy == POLICY_DCHOICE) {
    uint64_t randomRange =
        policy == POLICY_RANDOM ? nBlocks : dChoiceParam * nBlocks;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> dist(0, weight.size() - 1);
    std::vector<std::pair<uint32_t, float>> selected;
    while (selected.size() < randomRange) {
      uint64_t idx = dist(gen);

      if (weight.at(idx).first < std::numeric_limits<uint32_t>::max()) {
        selected.push_back(weight.at(idx));
        weight.at(idx).first = std::numeric_limits<uint32_t>::max();
      }
    }

    weight = std::move(selected);
  }
  if (policy == CACHED_GC && !(cachedWeight.empty())) {
    nBlocks = MIN(nBlocks, cachedWeight.size());
    for (uint64_t i = 0; i < nBlocks; i++) {
      // list.push_back(cachedWeight.at(i).first);
      // uint32_t victimBlockIndex = findVictimBlock(cachedWeight);
      list.push_back(findVictimBlock(cachedWeight));
    }
  }
  else {  // for other caching schemes

    if (pageAwareofCache) {
      nBlocks = MIN(nBlocks, cachedWeight.size());
      for (uint64_t i = 0; i < nBlocks; i++) {
        // list.push_back(cachedWeight.at(i).first);
        // uint32_t victimBlockIndex = findVictimBlock(cachedWeight);
        list.push_back(findVictimBlock(cachedWeight));
      }
    }
    else {
      std::sort(weight.begin(), weight.end(),
                [](std::pair<uint32_t, float> a, std::pair<uint32_t, float> b)
                    -> bool { return a.second < b.second; });

      nBlocks = MIN(nBlocks, weight.size());
      for (uint64_t i = 0; i < nBlocks; i++) {
        list.push_back(weight.at(i).first);
      }
    }

  }  // elsepart of main
  tick += applyLatency(CPU::FTL__PAGE_MAPPING, CPU::SELECT_VICTIM_BLOCK);
}
uint32_t PageMapping::findVictimBlock(
    std::vector<std::pair<uint32_t, std::pair<uint32_t, uint32_t>>>
        &cachedWeight) {
  /*uint32_t LN_NTBR = cachedWeight[0].second.second -
  cachedWeight[0].second.first; // valid pages minus those pages not cached in
  i,e need to read pages uint32_t Temp_V = cachedWeight[0].second.second;  //
  valid pages uint32_t Temp_B_N =  cachedWeight[0].first;;


  for (size_t i = 1; i < cachedWeight.size(); ++i) {
    auto it = std::find(erased_block_id.begin(), erased_block_id.end(),
  cachedWeight[i].first); if (it == erased_block_id.end()) { continue;
        }
      uint32_t N_NTBR = cachedWeight[i].second.second -
  cachedWeight[i].second.first;  // new need to read pages
          //printf("needtoreed %u and %u\n",N_NTBR,Temp_B_N);
      if (LN_NTBR < N_NTBR) {
           // Do nothing
      } else if (LN_NTBR > N_NTBR) {
          LN_NTBR = N_NTBR;
          Temp_V = cachedWeight[i].second.second;
          Temp_B_N = cachedWeight[i].first;
      } else {
          if (Temp_V < cachedWeight[i].second.second) {
              Temp_V = cachedWeight[i].second.second;
              Temp_B_N = cachedWeight[i].first;;
          }
      }
  }

  return Temp_B_N; // Returning index of the victim block
  auto comparePagesToRead = [](const std::pair<uint32_t, std::pair<uint32_t,
  uint32_t>>& a, const std::pair<uint32_t, std::pair<uint32_t, uint32_t>>& b) {
      uint32_t N_NTBR_a = a.second.second - a.second.first;
      uint32_t N_NTBR_b = b.second.second - b.second.first;

      if (N_NTBR_a == N_NTBR_b) {
          uint32_t Temp_V_a = a.second.second;
          uint32_t Temp_V_b = b.second.second;
          return Temp_V_a < Temp_V_b;
      } else {
          return N_NTBR_a < N_NTBR_b;
      }
  };

  // Use the locally defined comparison function to sort cachedWeight
  std::sort(cachedWeight.begin(), cachedWeight.end(), comparePagesToRead);*/
  std::sort(cachedWeight.begin(), cachedWeight.end(),
            [](const std::pair<uint32_t, std::pair<uint32_t, uint32_t>> &a,
               const std::pair<uint32_t, std::pair<uint32_t, uint32_t>> &b) {
              uint32_t validPagesA = a.second.second;
              uint32_t cachedPagesA = a.second.first;
              uint32_t validPagesB = b.second.second;
              uint32_t cachedPagesB = b.second.first;

              // Sort by minimum valid pages (ascending)
              if (validPagesA != validPagesB) {
                return validPagesA < validPagesB;
              }

              // In the case of a tie, sort by maximum cached pages (descending)
              return cachedPagesA > cachedPagesB;
            });
  return cachedWeight[0].first;
}
SSDInfo PageMapping::getSSDInternalInfo(uint32_t blockId) {
  SSDInfo internal;

  internal.channels = blockId % ssdInternals[0];
  blockId = blockId / ssdInternals[0];
  internal.Chips = blockId % ssdInternals[1];
  blockId = blockId / ssdInternals[1];
  internal.Dies = blockId % ssdInternals[2];
  blockId = blockId / ssdInternals[2];
  internal.Planes = blockId % ssdInternals[3];
  blockId = blockId / ssdInternals[3];
  return internal;
}
void PageMapping::writeToLeastusedBlock(uint64_t lpn, uint64_t &tick) {
  uint64_t beginAt = tick;
  PAL::Request req(param.ioUnitInPage);
  Bitset bit(param.ioUnitInPage);
  auto freeBlock = blocks.find(getUpdatedLastFreeBlock());
  uint32_t newBlockIdx = freeBlock->first;
  for (uint32_t idx = 0; idx < bitsetSize; idx++) {
    if (bit.test(idx)) {
      // Invalidate

      auto mappingList = table.find(lpn);

      if (mappingList == table.end()) {
        panic("Invalid mapping table entry");
      }

      pDRAM->read(&(*mappingList), 8 * param.ioUnitInPage, tick);

      auto &mapping = mappingList->second.at(idx);

      uint32_t newPageIdx = freeBlock->second.getNextWritePageIndex(idx);

      mapping.first = newBlockIdx;
      mapping.second = newPageIdx;

      freeBlock->second.write(newPageIdx, lpn, idx, beginAt);

      // Issue Write
      req.blockIndex = newBlockIdx;
      req.pageIndex = newPageIdx;

      if (bRandomTweak) {
        req.ioFlag.reset();
        req.ioFlag.set(idx);
      }
      else {
        req.ioFlag.set();
      }

      // writeRequests.push_back(req);// this is the write request to the pal;
      pPAL->write(req, beginAt);
      stat.validPageCopies++;
    }
  }
}

void PageMapping::doGarbageCollection(std::vector<uint32_t> &blocksToReclaim,
                                      uint64_t &tick) {
  PAL::Request req(param.ioUnitInPage);
  std::vector<PAL::Request> readRequests;
  std::vector<PAL::Request> writeRequests;
  std::vector<PAL::Request> eraseRequests;
  std::vector<ICL::Request> writeRequests1;
  std::vector<uint64_t> lpns;
  std::vector<uint64_t> lpa_list;
  std::vector<uint64_t> lpnListForCache;
  std::vector<uint64_t> CachedValidLPN;
  Bitset bit(param.ioUnitInPage);
  uint64_t beginAt = tick;  // yeha maine khud se kiya
  uint64_t readFinishedAt = tick;
  uint64_t writeFinishedAt = tick;
  uint64_t eraseFinishedAt = tick;
  victimBlockInfo vbi;
  if (iclCount > SimlationIORequests)
    return;
  // std::exit(0);
  static const EVICT_POLICY policy =
      (EVICT_POLICY)conf.readInt(CONFIG_FTL, FTL_GC_EVICT_POLICY);

  if (blocksToReclaim.size() == 0) {  // if no block to be reclaimed.
    return;
  }
  gcCounter++;

  if (policy == CACHED_GC)  //@CachedGC
  {
    // printf("Tick %lu ICLCount %lu GcCount %d\n",tick,iclCount,gcCounter);
    for (auto &iter : blocksToReclaim) {
      // printf("Indexof ReturnedBlock %u\n",iter);

      auto block = blocks.find(iter);
      // printf("Block: %u GC: %d IO %ld EraseList %ld PagesToMove %lu ICLCount:
      // %ld FreeBlockRatio
      // %f\n",block->second.getBlockIndex(),gcCounter,myIoCount,eraser,(param.pagesInBlock-(block->second.getValidPageCount()-block->second.getCachedPageCount())),iclCount,freeBlockRatio());
      //  printf("VBlock: %u GC: %d VPages: %u AcC %u CPages %u IPages %u
      //  ICLCount %lu Tick %lu PagesToMove
      //  %u\n",block->second.getBlockIndex(),gcCounter,block->second.getValidPageCount(),block->second.getBlockAccessCount(),block->second.getCachedPageCount(),block->second.getDirtyPageCount(),iclCount,tick,(block->second.getValidPageCount()-block->second.getCachedPageCount()));
      if (block == blocks.end()) {
        panic("Invalid block");
      }
      // eraser++;
      //  Copy valid pages to free block
      stat.cachedPageCount += block->second.getCachedPageCount();
      stat.totalValidPagesMovement += block->second.getValidPageCount();

      for (uint32_t pageIndex = 0; pageIndex < param.pagesInBlock;
           pageIndex++) {  // for each page in a block
                           // Valid?
        if (!block->second.isCached(pageIndex) &&
            block->second.isPageValid(
                pageIndex))  /// @@point to note here that if the block is not
                             /// cached
        {
          if (block->second.getPageInfo(pageIndex, lpns, bit)) {
            // believe that you got cached and valid page
            // printf("LPN of:%u is %lu\n",pageIndex,lpns[0]);

            if (!bRandomTweak) {
              bit.set();
            }

            stat.pagesNotCachedButValid++;
            // Issue Read
            req.blockIndex = block->first;
            req.pageIndex = pageIndex;
            req.ioFlag = bit;
            lpa_list.push_back(lpns[0]);

            readRequests.push_back(
                req);  // add it to the read request queue what ever page is
                       // there ..here check the structure//
                       //**************************This is the portion of
                       // GreedyGC********************

            if (blockToBlockMovement) {
              // if((getAccessCountOfLBA(lpns[0]) <= 2 ) ||
              // (getLPNEvictionFrequency(lpns[0]) <= 2))
              if ((getLPNEvictionFrequency(lpns[0]) <= 2)) {
                // Update mapping table
                auto freeBlock = blocks.find(getLastFreeBlock(
                    bit));  // it will return the block which is ready to accept
                            // the IO request.
                // auto freeBlock = blocks.find(getUpdatedLastFreeBlock());
                uint32_t newBlockIdx = freeBlock->first;  // new free block id

                for (uint32_t idx = 0; idx < bitsetSize; idx++) {
                  if (bit.test(idx)) {
                    // Invalidate
                    block->second.invalidate(pageIndex, idx);

                    auto mappingList = table.find(lpns.at(idx));

                    if (mappingList == table.end()) {
                      panic("Invalid mapping table entry");
                    }

                    pDRAM->read(&(*mappingList), 8 * param.ioUnitInPage, tick);

                    auto &mapping = mappingList->second.at(idx);

                    uint32_t newPageIdx =
                        freeBlock->second.getNextWritePageIndex(idx);

                    mapping.first = newBlockIdx;
                    mapping.second = newPageIdx;

                    freeBlock->second.write(newPageIdx, lpns.at(idx), idx,
                                            beginAt);
                    stat.victimToFreeMovements++;
                    // Issue Write
                    req.blockIndex = newBlockIdx;
                    req.pageIndex = newPageIdx;

                    if (bRandomTweak) {
                      req.ioFlag.reset();
                      req.ioFlag.set(idx);
                    }
                    else {
                      req.ioFlag.set();
                    }

                    writeRequests.push_back(
                        req);  // this is the write request to the pal;

                    stat.validPageCopies++;
                  }
                }
              }
              else {
                lpnListForCache.push_back(lpns[0]);
              }

              //********************************EndOfGreedyGCPortion

            }  // end of blockToBlockMovements

            stat.validSuperPageCopies++;
          }
        }
        if (block->second.isCached(pageIndex)) {
          // mynumber++;
          CachedValidLPN.push_back(lpns[0]);  // there will be used for updating
                                              // the dirty bit of each lpn.
          stat.pagesUpdatedDirty++;
        }
      }  // till here it will be the code for the eviction lines
         // printf("size of lpsn is %ld and myNumber is
      // %d\n",lpa_list.size(),mynumber);
      //  Erase block
      req.blockIndex = block->first;
      req.pageIndex = 0;
      req.ioFlag.set();

      eraseRequests.push_back(req);
    }

  }  // end of cachedGC policy
  /*This the garbage collection for other victimSelection Policies*/
  else  // Default Greedy GC
  {
    for (auto &iter : blocksToReclaim) {
      auto block = blocks.find(iter);
      // printf("VBlock: %u GC: %d VPages: %u AcC %u CPages %u IPages %u
      // ICLCount %lu Tick %lu PagesToMove
      // %u\n",block->second.getBlockIndex(),gcCounter,block->second.getValidPageCount(),block->second.getBlockAccessCount(),block->second.getCachedPageCount(),block->second.getDirtyPageCount(),iclCount,tick,(block->second.getValidPageCount()-
      // block->second.getCachedPageCount()));

      if (block == blocks.end()) {
        panic("Invalid block");
      }

      // Copy valid pages to free block
      // int pageIndexer=0;
      stat.cachedPageCount += block->second.getCachedPageCount();
      stat.totalValidPagesMovement += block->second.getValidPageCount();

      for (uint32_t pageIndex = 0; pageIndex < param.pagesInBlock;
           pageIndex++) {  // for each page in the victim block
        // Valid?
        if (block->second.getPageInfo(pageIndex, lpns, bit)) {
          if (block->second.isPageValid(pageIndex)) {
            if (!bRandomTweak) {
              bit.set();
            }

            // Retrive free block

            auto freeBlock = blocks.find(
                getLastFreeBlock(bit));  // it will return the block which is
                                         // ready to accept the IO request.
            // auto freeBlock = blocks.find(getUpdatedLastFreeBlock());

            // printf("block Called %u PageIndex
            // %d\n",freeBlock->second.getBlockIndex(),pageIndexer++);

            // Issue Read
            req.blockIndex = block->first;
            req.pageIndex = pageIndex;
            req.ioFlag = bit;

            readRequests.push_back(
                req);  /// this much of the data i have to read

            // Update mapping table
            uint32_t newBlockIdx = freeBlock->first;  // new free block id

            for (uint32_t idx = 0; idx < bitsetSize; idx++) {
              if (bit.test(idx)) {
                // Invalidate
                block->second.invalidate(pageIndex, idx);

                auto mappingList = table.find(lpns.at(idx));

                if (mappingList == table.end()) {
                  panic("Invalid mapping table entry");
                }

                pDRAM->read(&(*mappingList), 8 * param.ioUnitInPage, tick);

                auto &mapping = mappingList->second.at(idx);

                uint32_t newPageIdx =
                    freeBlock->second.getNextWritePageIndex(idx);

                mapping.first = newBlockIdx;
                mapping.second = newPageIdx;
                auto cmtkey = cmt.find(lpns.at(idx));
                if (cmtkey != cmt.end()) {
                  cmtkey->second.block_id = mapping.first;
                  cmtkey->second.page_Id = mapping.second;
                }

                freeBlock->second.write(newPageIdx, lpns.at(idx), idx, beginAt);

                // Issue Write
                req.blockIndex = newBlockIdx;
                req.pageIndex = newPageIdx;

                if (bRandomTweak) {
                  req.ioFlag.reset();
                  req.ioFlag.set(idx);
                }
                else {
                  req.ioFlag.set();
                }

                writeRequests.push_back(
                    req);  // this is the write request to the pal;

                stat.validPageCopies++;
              }
            }

            stat.validSuperPageCopies++;
          }
          /*else  // for all non valid pages in the
          {
                auto lpnInvalid= table.find(lpns.at(0));
                if(lpnInvalid !=table.end())
                {
                  stat.entreisPresentInmappingtableNotupdated++;
                  auto x = lpnInvalid->second.at(0);
                  if( x.first == iter)
                  {
                   stat.entriesbelongingTothisblock++;
                    //table.erase(lpnInvalid);
                  }
                }
          }*/
        }  // this is check the condition for valid page only
      }    // iterator for

      // Erase block
      req.blockIndex = block->first;
      req.pageIndex = 0;
      req.ioFlag.set();

      eraseRequests.push_back(req);
    }
  }

  // Do actual I/O here
  // This handles PAL2 limitation (SIGSEGV, infinite loop, or so-on)
  if (policy == CACHED_GC) {
    // int k=0;
    SimpleSSD::ICL::Request CacheWriteReq;
    size_t size = readRequests.size();
    pageMoveBucket[size]++;  // totalPagesMovedFromBlockToCache
                             // +=size;//bucketWise entering the number of pages
                             // from blockto Cache

    for (auto &iter : CachedValidLPN) {  // updating dirty Entries in Cache;
      beginAt = tick;
      globalCache->updateDirty(
          iter,
          beginAt);  // this function updates the entries in cache as dirty.
      RemoveEntryFromTable(iter, beginAt);
      writeFinishedAt = MAX(writeFinishedAt, beginAt);
      // tick=writeFinishedAt;
    }
    for (size_t i = 0; i < size; i++) {  // Reading the valid Pages
      auto &iter = readRequests[i];
      beginAt = tick;
      pPAL->read(iter, beginAt);
      readFinishedAt = MAX(readFinishedAt, beginAt);
    }
    if (blockToBlockMovement) {
      for (auto &iter : writeRequests) {
        beginAt = readFinishedAt;
        // totalPagesMovedFromBlockToBlock++;
        pPAL->write(iter, beginAt);

        writeFinishedAt = MAX(writeFinishedAt, beginAt);
      }
    }

    for (auto &iter : eraseRequests) {
      beginAt = readFinishedAt;
      // erase_counter++;
      eraser++;
      erased_block_id.push_back(iter.blockIndex);

      eraseInternal(iter, beginAt);

      eraseFinishedAt = MAX(eraseFinishedAt, beginAt);
    }
    if (blockToBlockMovement) {
      for (size_t i = 0; i < lpnListForCache.size();
           i++) {  // there are those requests that will  be written to the
                   // cache when there is
        // auto& iter = readRequests[i];
        uint64_t lpn = lpnListForCache[i];
        totalPagesMovedFromBlockToCache++;
        // check the access frequency.
        movePagesToCache.push_back(lpn);
        // tick=eraseFinishedAt;
        // beginAt = tick;
        RemoveEntryFromTable(lpn, tick);
      }
    }
    else {
      for (size_t i = 0; i < size;
           i++) {  // there are those requests that will  be written to the
                   // cache when there is
        // auto& iter = readRequests[i];
        uint64_t lpn = lpa_list[i];

        totalPagesMovedFromBlockToCache++;
        // check the access frequency.
        movePagesToCache.push_back(lpn);
        // tick=eraseFinishedAt;
        // beginAt = tick;
        RemoveEntryFromTable(lpn, tick);
      }
    }
    if (movePagesToCache.size()) {
      putPagesInCache = true;
    }
    // printEvictLines();
  }
  else  // else part of cachedGC
  {
    for (auto &iter : readRequests) {
      beginAt = tick;

      pPAL->read(iter, beginAt);

      readFinishedAt = MAX(readFinishedAt, beginAt);
    }
    // std::exit(0);
    //  size_t size = readRequests.size();
    //  printf("Pages toWrite to new block %ld\n",size);
    // pageMoveBucket[size]++;
    // totalPagesMovedFromBlockToBlock +=size;// here it is totalpages moved
    // from victimBlock to FreeBlocks
    for (auto &iter : writeRequests) {
      beginAt = readFinishedAt;
      // totalPagesMovedFromBlockToBlock++;
      pPAL->write(iter, beginAt);

      writeFinishedAt = MAX(writeFinishedAt, beginAt);
    }

    for (auto &iter : eraseRequests) {
      beginAt = readFinishedAt;
      erased_block_id.push_back(iter.blockIndex);
      // eraseMapBucket[(iter.blockIndex % param.pageCountToMaxPerf)]++;

      eraseInternal(iter, beginAt);
      eraser++;
      // printf("Eraser Value %ld\n",eraser);
      eraseFinishedAt = MAX(eraseFinishedAt, beginAt);
    }

  }  // else part of cacheGc
  tick = MAX(writeFinishedAt, eraseFinishedAt);
  tick += applyLatency(CPU::FTL__PAGE_MAPPING, CPU::DO_GARBAGE_COLLECTION);
}
void PageMapping::optimalRepalcementPolicy() {
  if (cmt.size() >= cmtSize) {  // Eviction needed
    std::vector<uint64_t> cmtRemovalEntries;
    // printf("Entries in CM are ..\n");
    /*for (const auto &entry : cmt) {
       auto optMapEntry = optMap.find(entry.first);
        if (optMapEntry != optMap.end() )
        {

      printf("%lu - %lu \t",entry.first,optMapEntry->second[0]);
        }
    }
  printf("\n");*/

    uint64_t lruKey = 0;
    uint64_t maxFirstElement = 0;
    uint64_t currentAccessCount = 0;
    // uint64_t evictionKeyAccessCount=0;
    // Find the least recently used entry
    for (const auto &entry : cmt) {
      uint64_t currentKey = entry.first;
      currentAccessCount = entry.second.cmtEntryAccessCount;
      uint64_t icl = iclCount;
      // Check if the entry exists in the optMap
      // printf("cmtSizeee %ld\n",cmt.size());
      auto optMapEntry = optMap.find(currentKey);
      if (optMapEntry != optMap.end()) {
        // Compare the first element of the vector
        // printf("(LPn %lu - cmt %lu Icl %lu opicl
        // %lu)\t",currentKey,entry.second.iclEntry,iclCount,optMapEntry->second[0]);

        auto &optMapEntryVector = optMapEntry->second;
        optMapEntryVector.erase(
            std::remove_if(optMapEntryVector.begin(), optMapEntryVector.end(),
                           [icl](uint64_t val) { return val <= icl; }),
            optMapEntryVector.end());
        // printf("(LPn %lu - cmt %lu Icl %lu opicl
        // %lu)\t",currentKey,entry.second.iclEntry,iclCount,optMapEntry->second[0]);
        // printf( "\nOptmalSecond %lu %lu %lu optimalvector %lu ICL
        // %lu\n",optMapEntry->second[0],maxFirstElement,currentKey,optMapEntryVector[0],iclCount);

        if (optMapEntry->second.empty()) {
          // cmt.erase(optMapEntry->first);
          // printf("Erased %lu cmtSize %ld\n",optMapEntry->first,cmt.size());
          cmtRemovalEntries.push_back(optMapEntry->first);
          // continue;
        }
        else {
          if (optMapEntry->second[0] > maxFirstElement) {
            maxFirstElement = optMapEntry->second[0];
            lruKey = currentKey;
            // evictionKeyAccessCount=currentAccessCount;
            // cmtRemovalEntries.push_back(lruKey);

            // printf("(LPn %lu - cmt %lu Icl %lu opicl
            // %lu)\t",currentKey,entry.second.iclEntry,iclCount,optMapEntry->second[0]);
          }
        }
        // optMapEntry
      }
    }
    cmtRemovalEntries.push_back(lruKey);
    for (auto entry : cmtRemovalEntries) {
      // cmt.erase(entry);

      // printf("\n");

      evictedCmtEntries[entry] = currentAccessCount;  // victimCache
      // printf("ValueEvicted %lu Size of EvictedMap %ld AccessCountEvictedEntry
      // %u\n",lruKey,evictedCmtEntries.size(),maxFirstElement);
      if (maxFirstElement == 1) {
        cmtStats.entriesWithOneAccessCount++;
      }
      cmtStats.cmtEvictions++;
      // cmtEntryEvictionAccessFrequency[maxFirstElement]++;

      // Remove the first element from the vector in optMap

      cmt.erase(entry);  // Remove LRU entry from the CMT
                         // printf("Eviction %lu Key %lu cmtSize
                         // %ld\n",cmtStats.cmtEvictions,entry,cmt.size());
    }
  }
}

void PageMapping::EvictFromShadowCache() {
  if (shadowwCache.size() >= shadowwCacheSize) {  // Eviction needed
    uint64_t oldestTime = std::numeric_limits<uint64_t>::max();
    uint64_t lruKey = 0;
    // uint32_t keyAccessCount=0;

    // Find the least recently used entry
    for (const auto &entry : shadowwCache) {
      if (entry.second.lastAccessedTime < oldestTime) {
        oldestTime = entry.second.lastAccessedTime;
        lruKey = entry.first;
        // keyAccessCount=entry.second.cmtEntryAccessCount;
      }
    }

    if (lruKey != 0) {
      cmtStats.EvictionsFromShadowCache++;
      shadowwCache.erase(lruKey);  // Remove LRU entry from the CMT
    }
  }
}
void PageMapping::evictEntryFromCMT() {
  if (cmt.size() >= cmtSize) {  // Eviction needed
    uint64_t oldestTime = std::numeric_limits<uint64_t>::max();
    uint64_t lruKey = 0;
    uint32_t keyAccessCount = 0;
    // displayCMt();
    // Find the least recently used entry
    for (const auto &entry : cmt) {
      if (entry.second.lastAccessedTime < oldestTime) {
        oldestTime = entry.second.lastAccessedTime;
        lruKey = entry.first;
        keyAccessCount = entry.second.cmtEntryAccessCount;
      }
    }

    if (lruKey != 0) {
      if (keyAccessCount == 1) {
        evictedCmtEntries[lruKey]++;  // victimCache

        cmtStats.entriesWithOneAccessCount++;
      }

      cmtStats.cmtEvictions++;
      cmtEntryEvictionAccessFrequency[keyAccessCount]++;
      // printf("\nEvictedKey %lu AccessCount %u \n\n",lruKey,keyAccessCount);
      cmt.erase(lruKey);  // Remove LRU entry from the CMT
    }
  }
}

// LeaFTL UTIL
// debug
long long check_for = -9223372036854775807LL - 1LL;
uint64_t correct_pred = 0, wrong_pred = 0;
// int pageIDbits = -1;

std::vector<std::pair<uint64_t, SimpleSSD::FTL::Request>> buffer;
std::vector<std::pair<uint64_t, SimpleSSD::FTL::Request>> rearranged;
// long long table_size;
uint64_t write_count = 0;

// // -------------------------------------------------------
void PageMapping::readInternal(Request &req, uint64_t &tick) {
  PAL::Request palRequest(req);
  uint64_t beginAt;
  uint64_t finishedAt = tick;

  auto mappingList = table.find(req.lpn);

  // -------------------------------------------------------
  if (mappingList != table.end()) {
    // LeaFTL UTIL
    
    auto target = req.lpn;
    auto it = std::find_if(buffer.begin(), buffer.end(), [target](pair<uint64_t, SimpleSSD::FTL::Request> &pair) {
      return pair.first == target;
    });
    if(it != buffer.end()) {
      // outputFile << "LPN Found in buffer" << endl << endl;
      correct_pred++;
    } else {
      auto a = mapping_table.lookup(req.lpn);
      if(std::get<0>(a).size() == 0) {
        return; // failsafe
      }
      auto PPN = std::get<0>(std::get<0>(a)[0]);

      auto LeaFTL_blockID = static_cast<uint32_t>((PPN) / block_size);
      auto LeaFTL_pageID  = static_cast<uint32_t>((PPN) % block_size);

      auto blockID  = mappingList->second.at(0).first;  // block ID from Regular Mapping Scheme
      auto pageID   = mappingList->second.at(0).second;  // Page ID

      if (LeaFTL_blockID == blockID && LeaFTL_pageID == pageID) {
        correct_pred += 1;
      }
      else {
        wrong_pred += 1;
      }

      // OutputFile WRITES
      if((correct_pred + wrong_pred)%1000 == 0) {
        printf("==============================================STATS==============================================\n");
        // printf("%lld Writes Completed", static_cast<long long> (write_count));
        printf("%lld Writes Completed, %lld Reads completed; Summary: \n", static_cast<long long> (write_count), static_cast<long long>(correct_pred + wrong_pred));
        printf("Prediction Accuracy for gamma: %f: %f%%\n", gamma, (double) (100 * correct_pred) / (double)(wrong_pred + correct_pred));
        auto tmp_mp = mapping_table.avg_levels();
        printf("Average number of levels for a frame: %f, and variance: %f\n", tmp_mp.first, tmp_mp.second);
        printf("Regular Table: %f MB, LeaFTL Table: %f MB\n", (double) (table.size() * 8) / (double) (1048576), 
                                                                (double) (mapping_table.memory()) / (double) (1048576));
        printf("Size Reduction: %f%%\n", (1 - (double)((double)(mapping_table.memory()) / (double)(table.size() * 8))) * 100);
      }
    }

    if (bRandomTweak) {
      pDRAM->read(&(*mappingList), 8 * req.ioFlag.count(), tick);
    }
    else {
      pDRAM->read(&(*mappingList), 8, tick);
    }

    for (uint32_t idx = 0; idx < bitsetSize; idx++) {
      if (req.ioFlag.test(idx) || !bRandomTweak) {
        auto &mapping = mappingList->second.at(idx);

        if (mapping.first < param.totalPhysicalBlocks &&
            mapping.second < param.pagesInBlock) {
          palRequest.blockIndex = mapping.first;
          palRequest.pageIndex = mapping.second;

          if (bRandomTweak) {
            palRequest.ioFlag.reset();
            palRequest.ioFlag.set(idx);
          }
          else {
            palRequest.ioFlag.set();
          }

          auto block = blocks.find(palRequest.blockIndex);

          if (block == blocks.end()) {
            panic("Block is not in use");
          }

          beginAt = tick;

          block->second.read(palRequest.pageIndex, idx, beginAt);
          pPAL->read(palRequest, beginAt);

          finishedAt = MAX(finishedAt, beginAt);
        }
      }
    }

    tick = finishedAt;
    tick += applyLatency(CPU::FTL__PAGE_MAPPING, CPU::READ_INTERNAL);
  }
}


// LeaFTL performs writes at block level ----------------------------------
/*
  1. Buffer the writes.
  2. When Buffer size = block_size; sort the writes in-order of LPNs
  3. Perform the writes

  4. If required, compact and promote
*/

uint64_t write_routine = 0;

// Define a custom comparator function
auto comparator =
    [](const std::pair<uint64_t, SimpleSSD::FTL::Request> &a,
        const std::pair<uint64_t, SimpleSSD::FTL::Request> &b) {
      return a.first <
              b.first;  // Compare based on the first element (uint64_t)
    };

auto comparator2 =
    [](const std::pair<int, uint64_t> &a,
        const std::pair<int, uint64_t> &b) {
      return a.second <
              b.second;  // Compare based on the first element (uint64_t)
    };

auto compareByVectorSize = 
    [](const std::pair<int, std::vector<pair<int, uint64_t>>>& a, const std::pair<int, std::vector<pair<int, uint64_t>>>& b) {
        return a.second.size() < 
                b.second.size(); // compare based on vector sizes
    };

// ------------------------------------------------------------------------

void PageMapping::writeInternal(Request &req, uint64_t &tick, bool sendToPAL) {
  
  if (write_count % 1000000 == 0 and write_count != 0) {
    // outputFile << "Compaction" << endl;
    mapping_table.compact();
    mapping_table.promote();
  }
  write_count++;


  PAL::Request palRequest(req);
  std::unordered_map<uint32_t, Block>::iterator block;
  // Table :   std::unordered_map<uint64_t, std::vector<std::pair<uint32_t,
  // uint32_t>>>

  auto mappingList =
      table.find(req.lpn);  // searching the mapping in table (unordered map)

  auto target = req.lpn;

  auto it = std::find_if(buffer.begin(), buffer.end(), [target](pair<uint64_t, SimpleSSD::FTL::Request> &pair) {
    return pair.first == target;
  });
  
  Request tmp = req;
  if(it == buffer.end()){
    buffer.push_back({req.lpn, tmp});  // pushing the request into the buffer
  } else {
    it->second = tmp;
  }
  uint64_t beginAt;
  uint64_t finishedAt = tick;
  bool readBeforeWrite = false;

  if (mappingList != table.end()) {  // mapping found in table
    for (uint32_t idx = 0; idx < bitsetSize; idx++) {
      if (req.ioFlag.test(idx) || !bRandomTweak) {
        auto &mapping = mappingList->second.at(idx);

        if (mapping.first < param.totalPhysicalBlocks &&
            mapping.second < param.pagesInBlock) {
          block = blocks.find(mapping.first);

          // Invalidate current page
          block->second.invalidate(mapping.second, idx);
        }
      }
    }
  }
  else {
    // Create empty mapping
    auto ret = table.emplace(
        req.lpn,  // (Key) uint64_t
        // vector of pairs
        std::vector<std::pair<uint32_t, uint32_t>>(
            bitsetSize, {param.totalPhysicalBlocks, param.pagesInBlock}));

    if (!ret.second) {
      panic("Failed to insert new mapping");
    }

    mappingList = ret.first;
  }
  // table_size = static_cast<long long>(table.size());
  // LeaFTL FUNCT ---------------------------------
  if (static_cast<uint64_t>(buffer.size()) != param.pagesInBlock) {
      return;  // if buffer is not completely filled, data is not flushed to flash
  }



  // base LeaFTL (uses Simple Sort)
  //-----------------------------------------------
  // std::sort(buffer.begin(), buffer.end(), comparator);

  //-----------------------------------------------
  // Improvement over Base LeaFTL
  std::vector<pair<int, uint64_t>> temp = {}, temp2(256, {-1, 0});
  std::map<uint64_t, vector<pair<int, uint64_t>>> mp;
  std::vector<pair<int, vector<pair<int, uint64_t>>>> vec = {};
  std::vector<bool> if_inserted = {};

  for(int i = 0; i<256; i++) {
    // push back the index, and the LPA
    temp.push_back(make_pair(i, buffer[i].first));
  }

  sort(temp.begin(), temp.end(), comparator2);
  for(auto i:temp) {
    mp[i.second/256].push_back(i);
  } temp.clear();


  for(auto i: mp) {
    vec.push_back(i);
    if_inserted.push_back(false);
  }
  std::sort(vec.begin(), vec.end(), compareByVectorSize);
  mp.clear();

  int idx_ = 0;
  for(auto i: vec) {
    int size = i.second.size();
    if((i.second[size-1].second - i.second[0].second + 1) != i.second.size()) {
      // discontinuous vector
      bool inserted = false;
      for(int j = 0; j<=static_cast<int>(256-(i.second[size-1].second - i.second[0].second + 1)); j++) {
        bool can_insert = true;
        for(int k = 0; k<static_cast<int>(i.second.size()); k++) {
          int offset = static_cast<int>(i.second[k].second - i.second[0].second);
          if(temp2[j+offset].first != -1) {
            // not possible to insert as one segment for current j
            can_insert = false;
            break;
          }
        } 
        if(can_insert) { // if the discontinuous vector can be inserted as one segment
          for(int k = 0; k<static_cast<int>(i.second.size()); k++) {
            int offset = static_cast<int>(i.second[k].second - i.second[0].second);
            temp2[j+offset] = i.second[k];
          }          
          inserted = true;
          break;
        }
      } 
        if_inserted[idx_] = inserted;

    } 
    idx_++;
    // continuous vector handled in next loop
  }

  idx_ = 0;
  for(auto i: vec) {
    if(!if_inserted[idx_]) {
      // continuos vector or unplaced discontinuous vectors (also contains the frames with only one writes)
      // MOST OF THE TIMES THIS CODE WOULD BE EXECUTED
      // (find a continuous space in temp2)
      bool inserted = false;
      for(int j = 0; j<=static_cast<int>(256 - i.second.size()); j++) {
        bool found = true;
        for(int k = j; k<static_cast<int>(j + i.second.size()); k++) { // check is empty space is available
          if(temp2[k].first != -1) {
            found = false;
            break;
          }
        }
        if(found) { // update temp2 (empty space found)
          for(int k = j; k<static_cast<int>(j + i.second.size()); k++) {
            temp2[k] = i.second[k-j];
          } inserted = true; break;         
        }
      } if(!inserted) {
        int k = 0;
        for(int j = 0; j<256; j++) {
          if(k == static_cast<int>(i.second.size())) {
            break;
          } if(temp2[j].first == -1) {
            temp2[j] = i.second[k];
            k++;
          }
        }
      }
    } idx_++; 
  }

  for(int i = 0; i<256; i++) {
    rearranged.push_back(buffer[temp2[i].first]);
  }

  buffer = rearranged;
  rearranged.clear();
  if_inserted.clear();

  //-----------------------------------------------

  
  // Write data to free block
  block = blocks.find(getLastFreeBlock(req.ioFlag));
  // long long block_num;
  if (block == blocks.end()) {
    panic("No such block");  // terminate
  }
  std::vector<std::pair<long long, long long>> entries = {};
  int index = 0;
  // uint64_t prev;

  for (auto it : buffer) {  // write the data to a free block
    req = it.second;
    auto mappingList = table.find(it.second.lpn);
    if (sendToPAL) {
      if (bRandomTweak) {
        pDRAM->read(&(*mappingList), 8 * req.ioFlag.count(), tick);
        pDRAM->write(&(*mappingList), 8 * req.ioFlag.count(), tick);
      }
      else {
        pDRAM->read(&(*mappingList), 8, tick);
        pDRAM->write(&(*mappingList), 8, tick);
      }
    }

    if (!bRandomTweak && !req.ioFlag.all()) {
      // We have to read old data
      readBeforeWrite = true;
    }

    for (uint32_t idx = 0; idx < bitsetSize; idx++) {
      if (req.ioFlag.test(idx) || !bRandomTweak) {
        uint32_t pageIndex = block->second.getNextWritePageIndex(idx);
        auto &mapping = mappingList->second.at(idx);

        beginAt = tick;

        block->second.write(pageIndex, req.lpn, idx, beginAt);

        // Read old data if needed (Only executed when bRandomTweak = false)
        // Maybe some other init procedures want to perform 'partial-write'
        // So check sendToPAL variable
        if (readBeforeWrite && sendToPAL) {
          palRequest.blockIndex = mapping.first;
          palRequest.pageIndex = mapping.second;

          // We don't need to read old data
          palRequest.ioFlag = req.ioFlag;
          palRequest.ioFlag.flip();

          pPAL->read(palRequest, beginAt);
        }

        // update mapping to table
        mapping.first = block->first;
        mapping.second = pageIndex;

        long long PPN = mapping.first * param.pagesInBlock + mapping.second;
        entries.push_back(std::make_pair((long long)it.first, PPN));

        if (sendToPAL) {
          palRequest.blockIndex = block->first;
          palRequest.pageIndex = pageIndex;

          if (bRandomTweak) {
            palRequest.ioFlag.reset();
            palRequest.ioFlag.set(idx);
          }
          else {
            palRequest.ioFlag.set();
          }

          pPAL->write(palRequest, beginAt);
        }

        finishedAt = MAX(finishedAt, beginAt);
      }
    }
    // Exclude CPU operation when initializing
    if (sendToPAL) {
      tick = finishedAt;
      tick += applyLatency(CPU::FTL__PAGE_MAPPING, CPU::WRITE_INTERNAL);
    }
    index++;
  }

  write_routine++;
  buffer.clear();

  // outputFile << write_routine << " " << static_cast<double>(mapping_table.memory())/((double)1048576) << " ";

  mapping_table.update(entries);
  // outputFile << "Updated" << endl;
  // GC if needed
  // I assumed that init procedure never invokes GC
  static float gcThreshold = conf.readFloat(CONFIG_FTL, FTL_GC_THRESHOLD_RATIO);

  if (freeBlockRatio() < gcThreshold) {
    if (!sendToPAL) {
      panic("ftl: GC triggered while in initialization");
    }

    std::vector<uint32_t> list;
    uint64_t beginAt = tick;

    selectVictimBlock(list, beginAt);

    debugprint(LOG_FTL_PAGE_MAPPING,
               "GC   | On-demand | %u blocks will be reclaimed", list.size());

    doGarbageCollection(list, beginAt);

    debugprint(LOG_FTL_PAGE_MAPPING,
               "GC   | Done | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")", tick,
               beginAt, beginAt - tick);

    stat.gcCount++;
    stat.reclaimedBlocks += list.size();
  }
}

void PageMapping::printBlockState() {
  for (auto &entry : blocks) {
    uint32_t acc = entry.second.getBlockAccessCount();
    if (acc) {
      printf("AccessCount %u\n", acc);
    }
  }
}
uint64_t PageMapping::getBlockCachedPageCount() {
  uint64_t cachedpages = 0;
  for (auto &entry : blocks) {
    cachedpages += entry.second.getCachedPageCount();
  }
  return cachedpages;
}
void PageMapping ::displayCMt() {
  for (const auto &entry : cmt) {
    std::cout << "Key: " << entry.first
              << " Block ID: " << entry.second.block_id
              << " Page ID: " << entry.second.page_Id
              << " Last Accessed Time: " << entry.second.lastAccessedTime
              << " CMT Entry Access Count: " << entry.second.cmtEntryAccessCount
              << " ICL Entry: " << entry.second.iclEntry << std::endl;
  }
}
void PageMapping::writeEvictedLinesToStorage(
    Request &req, uint64_t &tick, std::vector<PAL::Request> &writeRequests) {
  PAL::Request palRequest(req);
  std::unordered_map<uint32_t, Block>::iterator block;
  Bitset bit(param.ioUnitInPage);
  ;
  auto mappingList =
      table.find(req.lpn);  // mappinglist is the iterator of the table.
  // uint64_t beginAt;
  // uint64_t finishedAt = tick;
  // bool readBeforeWrite = false;
  if (mappingList != table.end()) {  // agr entry mil gayi
    // printf("In write Mapping found for the LPN %lu\n",req.lpn);
    for (uint32_t idx = 0; idx < bitsetSize; idx++) {
      pDRAM->read(&(*mappingList), 8 * param.ioUnitInPage, tick);
      if (req.ioFlag.test(idx) || !bRandomTweak) {
        auto &mapping = mappingList->second.at(idx);
        bit.set();  // set the bit as written in the old.
        if (mapping.first < param.totalPhysicalBlocks &&  // blockid and
            mapping.second < param.pagesInBlock) {        // page id
          // printf("Finding mapping %u\n",mapping.first);
          block = blocks.find(mapping.first);
          if (block != blocks.end()) {
            if (block->second.isCached(mapping.second)) {
              overwrittenPages++;
              // printf("till here are ok %u and %u and cached pages
              // %u\n",mapping.first,mapping.second,block->second.getCachedPageCount());

              block->second.test_decache(mapping.second);
              // printf("PAge Decached\n");
            }
            block->second.invalidate(mapping.second, idx);
          }
        }
        auto freeBlock = blocks.find(getLastFreeBlock(bit));
        uint32_t newBlockIdx = freeBlock->first;
        uint32_t newPageIdx = freeBlock->second.getNextWritePageIndex(idx);
        mapping.first = newBlockIdx;
        mapping.second = newPageIdx;
        freeBlock->second.write(newPageIdx, req.lpn, idx, tick);
        palRequest.blockIndex = newBlockIdx;
        palRequest.pageIndex = newPageIdx;

        if (bRandomTweak) {
          req.ioFlag.reset();
          req.ioFlag.set(idx);
        }
        else {
          req.ioFlag.set();
        }

        writeRequests.push_back(palRequest);
      }  // we have invalidated the old page.
    }
    // printf("That is ok\n");
  }  // if matching is not found meaning that the evictiLine is the write fromt
     // he host..
  else {
  }
}
void PageMapping::writeEvictedDataToFlash(uint64_t &tick) {
  printf("Value of tick %lu\n", tick);
}
void PageMapping::printEvictLines() {
  printf("Contents of evictLIne is :-----\n");
  if (evictLines.size() == 0) {
    printf("Buffer Empty\n");
    return;
  }
  for (const auto &line : evictLines) {
    std::cout << "tag: " << line.tag << ", dirty: " << line.dirty
              << ", valid: " << line.valid << ",Last Accessed"
              << line.lastAccessed << std::endl;
    // Replace the above line with the appropriate member variables you want to
    // print
  }
}

void PageMapping::removeEntriesFromEvictionCache() {
  // printf("Called the evictLines removal\n");
  for (const auto &line : linesToRemove) {
    // Find the matching line in 'evictLines' using the custom comparison
    // function
    auto it = std::find_if(
        evictLines.begin(), evictLines.end(),
        [&](const SimpleSSD::ICL::_Line &l) { return isLineEqual(l, line); });

    if (it != evictLines.end()) {
      tempo--;
      evictLines.erase(it);
    }
  }

  linesToRemove.clear();
}
bool PageMapping::isLineEqual(const SimpleSSD::ICL::_Line &line1,
                              const SimpleSSD::ICL::_Line &line2) {
  return line1.tag == line2.tag;
}
void PageMapping::newFTLRequest(uint64_t lca, uint64_t &tick) {
  SimpleSSD::FTL::Request reqInternal(liner);
  // SimpleSSD::FTL::Request reqInternal;
  uint64_t beginAt;
  uint64_t finishedAt = tick;
  printf("Modified Eviction called for %lu\n", lca);
  debugprint(LOG_ICL_GENERIC_CACHE, "----- | Begin eviction");

  beginAt = tick;
  reqInternal.lpn = lca;
  reqInternal.ioFlag.reset();
  reqInternal.ioFlag.set(0);  // modified evictions needed to be performed
  // printf("Eviction is callied only this number of times %u\n",col);
  // printf()
  pFTL->write(reqInternal,
              beginAt);  // this is the write operation to the nand flash

  finishedAt = MAX(finishedAt, beginAt);
  // tick=finishedAt;
}
void PageMapping::trimInternal(Request &req, uint64_t &tick) {
  auto mappingList = table.find(req.lpn);

  if (mappingList != table.end()) {
    if (bRandomTweak) {
      pDRAM->read(&(*mappingList), 8 * req.ioFlag.count(), tick);
    }
    else {
      pDRAM->read(&(*mappingList), 8, tick);
    }

    // Do trim
    for (uint32_t idx = 0; idx < bitsetSize; idx++) {
      auto &mapping = mappingList->second.at(idx);
      auto block = blocks.find(mapping.first);

      if (block == blocks.end()) {
        panic("Block is not in use");
      }

      block->second.invalidate(mapping.second, idx);
    }

    // Remove mapping
    table.erase(mappingList);

    tick += applyLatency(CPU::FTL__PAGE_MAPPING, CPU::TRIM_INTERNAL);
  }
}

void PageMapping::eraseInternal(PAL::Request &req, uint64_t &tick) {
  static uint64_t threshold =
      conf.readUint(CONFIG_FTL, FTL_BAD_BLOCK_THRESHOLD);
  static const EVICT_POLICY policy =
      (EVICT_POLICY)conf.readInt(CONFIG_FTL, FTL_GC_EVICT_POLICY);
  auto block = blocks.find(req.blockIndex);

  // Sanity checks
  if (block == blocks.end()) {
    panic("No such block");
  }
  if ((policy != CACHED_GC) &&
      (policy != HOTBLOCK))  // this things is changed in the cache..
  {
    if (block->second.getValidPageCount() != 0) {
      // panic("There are valid pages in victim block");
    }
  }

  // Erase block
  block->second.erase();

  pPAL->erase(req, tick);

  // Check erase count
  uint32_t erasedCount = block->second.getEraseCount();

  if (erasedCount < threshold) {
    // Reverse search
    auto iter = freeBlocks.end();

    while (true) {
      iter--;

      if (iter->getEraseCount() <= erasedCount) {
        // emplace: insert before pos
        iter++;

        break;
      }

      if (iter == freeBlocks.begin()) {
        break;
      }
    }

    // Insert block to free block list
    freeBlocks.emplace(iter, std::move(block->second));
    //
    nFreeBlocks++;
    // printf("After Erase FreeBlockRatio %f\n",freeBlockRatio());
  }

  blocks.erase(block);

  tick += applyLatency(CPU::FTL__PAGE_MAPPING, CPU::ERASE_INTERNAL);
}
void PageMapping::RemoveEntryFromTable(uint64_t key_to_remove, uint64_t &tick) {
  auto it = table.find(key_to_remove);
  debugprint(LOG_ICL_GENERIC_CACHE, "EntryUpdated  | Tick %" PRIu64, tick);

  pDRAM->write(&(*it), 8 * param.ioUnitInPage, tick);
  if (it != table.end()) {
    table.erase(it);
  }
}

float PageMapping::calculateWearLeveling() {
  uint64_t totalEraseCnt = 0;
  uint64_t sumOfSquaredEraseCnt = 0;
  uint64_t numOfBlocks = param.totalLogicalBlocks;
  uint64_t eraseCnt;

  for (auto &iter : blocks) {
    eraseCnt = iter.second.getEraseCount();
    totalEraseCnt += eraseCnt;
    sumOfSquaredEraseCnt += eraseCnt * eraseCnt;
  }

  // freeBlocks is sorted
  // Calculate from backward, stop when eraseCnt is zero
  for (auto riter = freeBlocks.rbegin(); riter != freeBlocks.rend(); riter++) {
    eraseCnt = riter->getEraseCount();

    if (eraseCnt == 0) {
      break;
    }

    totalEraseCnt += eraseCnt;
    sumOfSquaredEraseCnt += eraseCnt * eraseCnt;
  }

  if (sumOfSquaredEraseCnt == 0) {
    return -1;  // no meaning of wear-leveling
  }

  return (float)totalEraseCnt * totalEraseCnt /
         (numOfBlocks * sumOfSquaredEraseCnt);
}

void PageMapping::calculateTotalPages(uint64_t &valid, uint64_t &invalid) {
  valid = 0;
  invalid = 0;

  for (auto &iter : blocks) {
    valid += iter.second.getValidPageCount();
    invalid += iter.second.getDirtyPageCount();
  }
}

void PageMapping::getStatList(std::vector<Stats> &list, std::string prefix) {
  Stats temp;

  temp.name = prefix + "page_mapping.gc.count";
  temp.desc = "Total GC count";
  list.push_back(temp);

  temp.name = prefix + "page_mapping.gc.reclaimed_blocks";
  temp.desc = "Total reclaimed blocks in GC";
  list.push_back(temp);

  temp.name = prefix + "page_mapping.gc.superpage_copies";
  temp.desc = "Total copied valid superpages during GC";
  list.push_back(temp);

  temp.name = prefix + "page_mapping.gc.page_copies";
  temp.desc = "Total copied valid pages during GC";
  list.push_back(temp);

  // For the exact definition, see following paper:
  // Li, Yongkun, Patrick PC Lee, and John Lui.
  // "Stochastic modeling of large-scale solid-state storage systems: analysis,
  // design tradeoffs and optimization." ACM SIGMETRICS (2013)
  temp.name = prefix + "page_mapping.wear_leveling";
  temp.desc = "Wear-leveling factor";
  list.push_back(temp);
}

void PageMapping::printFtlStats() {
  printf("TotalFtlReadRequests: %lu\n", ftlStats.TotalFtlReadRequests);
  printf("TotalFtlWriteRequests: %lu\n", ftlStats.TotalFtlWriteRequests);
  printf("pageMappingFoundForReads: %lu\n", ftlStats.pageMappingFoundForReads);
  printf("pageMappingFoundForWrites: %lu\n",
         ftlStats.pageMappingFoundForWrites);
  printf("FreeBlockRationAtEnd: %f\n", ftlStats.FreeBlockRatioValue);
  printf("BlockToBlockMovement %lu\n", stat.victimToFreeMovements);
  printf("CachedPageCount %lu\n", stat.cachedPageCount);
  printf("PagesValidButNotCached %lu\n", stat.pagesNotCachedButValid);
  printf("PagesUpdatedDirty %lu\n", stat.pagesUpdatedDirty);
  printf("totalValidPageMovement %lu\n", stat.totalValidPagesMovement);
  printf("cachedpagesfoundInAllBlocks %u\n",
         ftlStats.cachedpagesfoundInAllBlocks);
  printf("cmtReadRequests %lu\n", cmtStats.cmtReadRequests);
  printf("cmtReadRequestsHits %lu\n", cmtStats.cmtReadRequestsHits);
  printf("cmtWriteRequests %lu\n", cmtStats.cmtWriteRequests);
  printf("cmtWriteRequestsHits %lu\n", cmtStats.cmtWriteRequestsHits);
  printf("cmtEntriesEvictedAgainRequested %lu\n",
         cmtStats.cmtEntriesEvictedAgainRequested);
  printf("deadOnArrivalCounter %lu\n", cmtStats.deadOnArrivalCounter);
  printf("EvictionsFromCmt %lu\n", cmtStats.cmtEvictions);
  printf("EntriesEvictedWithOneAccessCount %lu\n",
         cmtStats.entriesWithOneAccessCount);
  printf("Inconsistant requests %lu\n", ftlStats.inconsistantRequests);
  printf("misPredictionCount %lu \n", cmtStats.misPredictionCount);
  printf("ShadowCacheEviction %lu\n", cmtStats.EvictionsFromShadowCache);
  printf("CmtEntresLoadedInCmt %lu\n", cmtStats.entriesLoadedInCmt);
  if (printDeadOnArrival) {
    printf("DeadPagePercentage \n");
    for (auto i : DeadPagePercentage) {
      printf("%f \t", i);
    }
    printf("\n");
  }
}
void PageMapping::getStatValues(std::vector<double> &values) {
  values.push_back(stat.gcCount);
  values.push_back(stat.reclaimedBlocks);
  values.push_back(stat.validSuperPageCopies);
  values.push_back(stat.validPageCopies);
  values.push_back(calculateWearLeveling());
}

void PageMapping::resetStatValues() {
  memset(&stat, 0, sizeof(stat));
  memset(&ftlStats, 0, sizeof(ftlStats));
  memset(&cmtStats, 0, sizeof(cmtStats));
}

}  // namespace FTL

}  // namespace SimpleSSD
