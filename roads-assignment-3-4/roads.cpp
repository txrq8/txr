// ===========================================================================
//  Assignments 3 and 4:   The road network graph, and shortest routes
// ===========================================================================
//
//  Assignment 3: the intersections file and the connections file are read
//  and turned into a graph:
//      - one Intersection object for every intersection, holding everything
//        the file says about it plus a vector of pointers to every
//        Connection object that touches it;
//      - one Connection object for every road segment, holding everything
//        the file says about it plus pointers to the two Intersection
//        objects at its ends.
//  The named places are read into a hash table (from assignment 2).  The
//  user picks a named place, the program goes to that place's
//  representative intersection and shows everything about it and about
//  every road segment that touches it.  The user picks a road segment and
//  the program moves along it to the intersection at the other end, and so
//  on until the user has had enough.
//
//  Assignment 4: the user picks two named places, and Dijkstra's algorithm
//  finds the shortest route between their representative intersections.
//  Every intersection on the route is shown, followed by the name, length
//  and direction of the road taken to the next one, and finally the total
//  length of all the road segments followed.
//
//  Files (these are the default names, others may be given on the command
//  line in this order):
//      named-places.txt     intersections.txt     connections.txt
//
//  Compile:   g++ -std=c++11 -O2 -Wall -o roads roads.cpp
//  Run:       ./roads
//        or   ./roads named-places.txt intersections.txt connections.txt
// ===========================================================================

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cctype>

using namespace std;

const double PI = 3.14159265358979323846;

// ===========================================================================
//  Small string helpers
// ===========================================================================

string trim(const string & s)
{ size_t b = 0, e = s.size();
  while (b < e && isspace((unsigned char)s[b]))
    b += 1;
  while (e > b && isspace((unsigned char)s[e - 1]))
    e -= 1;
  return s.substr(b, e - b); }

string upperCase(string s)
{ for (size_t i = 0; i < s.size(); i += 1)
    s[i] = toupper((unsigned char)s[i]);
  return s; }

// lower case, exactly one space between words: used as the hash table key
// so that "new  york" and "New York" are treated as the same name.
string normalise(const string & s)
{ string r;
  bool needSpace = false;
  for (size_t i = 0; i < s.size(); i += 1)
  { unsigned char c = s[i];
    if (isspace(c))
    { if (! r.empty())
        needSpace = true; }
    else
    { if (needSpace)
        r += ' ';
      needSpace = false;
      r += (char)tolower(c); } }
  return r; }

// a number written with a fixed number of digits after the decimal point
string num(double x, int decimals)
{ char buffer[64];
  snprintf(buffer, sizeof(buffer), "%.*f", decimals, x);
  return buffer; }

// a number written as it would appear in the data files: no pointless
// trailing zeros, and no digits lost
string exact(double x)
{ char buffer[64];
  snprintf(buffer, sizeof(buffer), "%.10g", x);
  return buffer; }

string plural(long n, const string & word)
{ char buffer[32];
  snprintf(buffer, sizeof(buffer), "%ld ", n);
  return buffer + word + (n == 1 ? "" : "s"); }

struct Word
{ string text;
  size_t start; };      // where the word starts in the line it came from

vector<Word> splitWords(const string & s)
{ vector<Word> words;
  size_t i = 0, n = s.size();
  while (true)
  { while (i < n && isspace((unsigned char)s[i]))
      i += 1;
    if (i >= n)
      break;
    Word w;
    w.start = i;
    while (i < n && ! isspace((unsigned char)s[i]))
      i += 1;
    w.text = s.substr(w.start, i - w.start);
    words.push_back(w); }
  return words; }

bool isInteger(const string & s)
{ size_t i = 0;
  if (i < s.size() && (s[i] == '-' || s[i] == '+'))
    i += 1;
  if (i == s.size())
    return false;
  for ( ; i < s.size(); i += 1)
    if (! isdigit((unsigned char)s[i]))
      return false;
  return true; }

// true if the whole of s is a number; its value is stored in value.
// only digits . + - e E are allowed, so words like "nan" or "inf" are
// never mistaken for numbers.
bool isNumber(const string & s, double & value)
{ bool sawDigit = false;
  for (size_t i = 0; i < s.size(); i += 1)
  { char c = s[i];
    if (isdigit((unsigned char)c))
      sawDigit = true;
    else if (c != '.' && c != '-' && c != '+' && c != 'e' && c != 'E')
      return false; }
  if (! sawDigit)
    return false;
  char * end;
  value = strtod(s.c_str(), & end);
  return * end == '\0'; }

// reads a line typed by the user, false at end of input
bool readLine(const string & prompt, string & line)
{ cout << prompt << flush;
  if (! getline(cin, line))
  { cout << "\n";
    return false; }
  line = trim(line);
  return true; }

// ===========================================================================
//  Named places and their hash table   (from assignment 2)
// ===========================================================================

struct Place
{ string code, state, name;
  long population;              // -1 if not given
  double area;                  // -1 if not given
  double latitude, longitude;
  long intersection;            // number of its representative intersection
  double distance;              // how far away that intersection is, -1 if not given

  string title() const
  { return name + ", " + state; } };

// named-places lines look like this (fixed width columns):
//   numeric code, state, name, population, area, latitude, longitude,
//   representative intersection, distance to that intersection
// the name may contain spaces, so the numbers are taken from the end of
// the line and everything between the state and them is the name.
bool parsePlace(const string & line, Place & p)
{ size_t i = 0, n = line.size(), s;
  while (i < n && isspace((unsigned char)line[i]))
    i += 1;
  s = i;
  while (i < n && isdigit((unsigned char)line[i]))
    i += 1;
  p.code = line.substr(s, i - s);
  while (i < n && isspace((unsigned char)line[i]))
    i += 1;
  if (i + 1 >= n || ! isalpha((unsigned char)line[i]) || ! isalpha((unsigned char)line[i + 1]))
    return false;
  p.state = upperCase(line.substr(i, 2));
  i += 2;
  if (p.code.empty())           // just in case the code comes after the state
  { while (i < n && isspace((unsigned char)line[i]))
      i += 1;
    s = i;
    while (i < n && isdigit((unsigned char)line[i]))
      i += 1;
    p.code = line.substr(s, i - s); }

  string rest = line.substr(i);
  vector<Word> w = splitWords(rest);
  int nw = w.size(), count = 0;
  vector<double> v(nw);
  while (count < nw && isNumber(w[nw - 1 - count].text, v[nw - 1 - count]))
    count += 1;

  int first;                    // index of the first number used
  double pop = -1, area = -1, lat, lon, inter, dist = -1;
  if (count >= 6)
  { first = nw - 6;
    pop = v[first];     area = v[first + 1];
    lat = v[first + 2]; lon = v[first + 3];
    inter = v[first + 4]; dist = v[first + 5]; }
  else if (count == 5)
  { first = nw - 5;
    pop = v[first];
    if (isInteger(w[nw - 1].text))          // no distance given
    { area = v[first + 1]; lat = v[first + 2]; lon = v[first + 3]; inter = v[first + 4]; }
    else                                    // no area given
    { lat = v[first + 1]; lon = v[first + 2]; inter = v[first + 3]; dist = v[first + 4]; } }
  else if (count == 4)
  { first = nw - 4;
    if (isInteger(w[nw - 1].text))
    { pop = v[first]; lat = v[first + 1]; lon = v[first + 2]; inter = v[first + 3]; }
    else
    { lat = v[first]; lon = v[first + 1]; inter = v[first + 2]; dist = v[first + 3]; } }
  else
    return false;
  if (first == 0)               // there is no name
    return false;

  p.name = trim(rest.substr(0, w[first].start));
  p.population = (long)pop;
  p.area = area;
  if (fabs(lat) > 90 || (lat < 0 && lon > 0))     // longitude came first
  { double t = lat;
    lat = lon;
    lon = t; }
  p.latitude = lat;
  p.longitude = lon;
  p.intersection = (long)inter;
  p.distance = dist;
  return true; }

class PlaceTable
{ protected:
    struct Link
    { string key;
      Place * place;
      Link * next;
      Link(const string & k, Place * p, Link * n): key(k), place(p), next(n) { } };

    vector<Link *> table;
    int count;

    static unsigned int hash(const string & key)          // FNV-1a
    { unsigned int h = 2166136261u;
      for (size_t i = 0; i < key.size(); i += 1)
      { h ^= (unsigned char)key[i];
        h *= 16777619u; }
      return h; }

    void grow()
    { vector<Link *> old = table;
      table.assign(old.size() * 2, (Link *)NULL);
      for (size_t i = 0; i < old.size(); i += 1)
      { Link * L = old[i];
        while (L != NULL)
        { Link * next = L->next;
          unsigned int pos = hash(L->key) % table.size();
          L->next = table[pos];
          table[pos] = L;
          L = next; } } }

  public:
    PlaceTable(): table(1024, (Link *)NULL), count(0) { }

    ~PlaceTable()
    { for (size_t i = 0; i < table.size(); i += 1)
      { Link * L = table[i];
        while (L != NULL)
        { Link * next = L->next;
          delete L;
          L = next; } } }

    void add(Place * p)
    { if (count + 1 > (int)table.size() * 3 / 4)
        grow();
      string key = normalise(p->name);
      unsigned int pos = hash(key) % table.size();
      table[pos] = new Link(key, p, table[pos]);
      count += 1; }

    // every place with exactly this name (ignoring case and extra spaces),
    // in the order they were added
    vector<Place *> find(const string & name) const
    { string key = normalise(name);
      vector<Place *> found;
      for (Link * L = table[hash(key) % table.size()]; L != NULL; L = L->next)
        if (L->key == key)
          found.push_back(L->place);
      for (size_t i = 0, j = found.size(); i + 1 < j; i += 1, j -= 1)
      { Place * t = found[i];
        found[i] = found[j - 1];
        found[j - 1] = t; }
      return found; }

    int size() const
    { return count; }

  private:
    PlaceTable(const PlaceTable &);
    PlaceTable & operator=(const PlaceTable &); };

bool readPlaces(const string & filename, vector<Place *> & places, PlaceTable & table)
{ ifstream fin(filename.c_str());
  if (fin.fail())
  { cerr << "Can not open the named places file \"" << filename << "\"\n";
    return false; }
  string line;
  int bad = 0;
  while (getline(fin, line))
  { if (trim(line).empty())
      continue;
    Place * p = new Place;
    if (parsePlace(line, * p))
    { places.push_back(p);
      table.add(p); }
    else
    { delete p;
      bad += 1; } }
  if (bad > 0)
    cerr << "Warning: " << plural(bad, "line") << " of " << filename << " could not be understood\n";
  return true; }

// ===========================================================================
//  The graph: intersections and connections
// ===========================================================================

struct Connection;

struct Intersection
{ int index;                    // position in RoadMap::intersections
  long number;                  // the number the data files use for it
  double longitude, latitude;
  double distance;              // distance to the nearest named place, -1 if not given
  string state, place;          // that nearest named place
  vector<Connection *> roads;   // every road segment that touches it

  string where() const
  { return "latitude " + exact(latitude) + ", longitude " + exact(longitude); }

  string nearest() const
  { string s = place;
    if (! state.empty())
      s += ", " + state;
    if (distance < 0)
      return "near " + s;
    return exact(distance) + " miles from " + s; } };

struct Connection
{ int index;                    // position in the connections file, from 1
  string name, type;
  Intersection * a, * b;        // the two ends
  double length;                // in miles

  Intersection * otherEnd(const Intersection * from) const
  { return from == a ? b : a; }

  string title() const
  { if (type.empty())
      return name;
    return name + " (type " + type + ")"; } };

// the compass direction from one intersection to another, exactly as the
// assignment describes it
string direction(const Intersection * from, const Intersection * to)
{ double lata = from->latitude * PI / 180, lona = from->longitude * PI / 180;
  double latb = to->latitude * PI / 180, lonb = to->longitude * PI / 180;
  double angle = atan2(lonb - lona, latb - lata) * 180 / PI;
  if (angle > 157.5 || angle < -157.5) return "S";
  if (angle >= 112.5) return "SE";
  if (angle >= 67.5)  return "E";
  if (angle >= 22.5)  return "NE";
  if (angle >= -22.5) return "N";
  if (angle >= -67.5) return "NW";
  if (angle >= -112.5) return "W";
  return "SW"; }

// approximate straight line distance in miles, only used to check that
// the files have been understood correctly
double straightLine(const Intersection * a, const Intersection * b)
{ double milesPerDegree = 69.17;
  double dy = (b->latitude - a->latitude) * milesPerDegree;
  double dx = (b->longitude - a->longitude) * milesPerDegree
              * cos((a->latitude + b->latitude) / 2 * PI / 180);
  return sqrt(dx * dx + dy * dy); }

class RoadMap
{ public:
    vector<Intersection *> intersections;
    vector<Connection *> connections;
    long firstNumber;           // the number the files use for the first intersection
    double consistency;         // fraction of road lengths that fit their end points

    RoadMap(): firstNumber(0), consistency(0) { }

    ~RoadMap()
    { for (size_t i = 0; i < intersections.size(); i += 1)
        delete intersections[i];
      for (size_t i = 0; i < connections.size(); i += 1)
        delete connections[i]; }

    Intersection * find(long number) const
    { long i = number - firstNumber;
      if (i < 0 || i >= (long)intersections.size())
        return NULL;
      return intersections[i]; }

    bool readIntersections(const string & filename);
    bool readConnections(const string & filename);

  private:
    RoadMap(const RoadMap &);
    RoadMap & operator=(const RoadMap &); };

// intersections lines look like this:
//     longitude latitude distance-to-nearest-place state name-of-that-place
// an intersection's number is its position in the file.
bool RoadMap::readIntersections(const string & filename)
{ ifstream fin(filename.c_str());
  if (fin.fail())
  { cerr << "Can not open the intersections file \"" << filename << "\"\n";
    return false; }
  string line;
  int bad = 0, longitudeFirst = 0, latitudeFirst = 0;
  while (getline(fin, line))
  { vector<Word> w = splitWords(line);
    if (w.empty())
      continue;
    double v[4];
    int lead = 0;
    while (lead < (int)w.size() && lead < 4 && isNumber(w[lead].text, v[lead]))
      lead += 1;
    int c = 0;                  // which number is the first coordinate
    bool hasDistance = true;
    if (lead >= 4)
      c = 1;                    // the line starts with its own number
    else if (lead == 3 && isInteger(w[0].text) && ! isInteger(w[1].text))
    { c = 1;
      hasDistance = false; }
    else if (lead == 2)
      hasDistance = false;
    else if (lead < 2)
    { bad += 1;
      continue; }
    int used = c + (hasDistance ? 3 : 2);

    Intersection * x = new Intersection;
    x->index = intersections.size();
    x->longitude = v[c];        // sorted out below if the order is the other way round
    x->latitude = v[c + 1];
    x->distance = hasDistance ? v[c + 2] : -1;
    if (used < (int)w.size())
    { x->state = w[used].text;
      if (used + 1 < (int)w.size())
        x->place = trim(line.substr(w[used + 1].start)); }
    if ((x->longitude < 0 && x->latitude > 0) || fabs(x->longitude) > 90)
      longitudeFirst += 1;
    else if ((x->longitude > 0 && x->latitude < 0) || fabs(x->latitude) > 90)
      latitudeFirst += 1;
    intersections.push_back(x); }

  if (latitudeFirst > longitudeFirst)
    for (size_t i = 0; i < intersections.size(); i += 1)
    { double t = intersections[i]->longitude;
      intersections[i]->longitude = intersections[i]->latitude;
      intersections[i]->latitude = t; }
  if (bad > 0)
    cerr << "Warning: " << plural(bad, "line") << " of " << filename << " could not be understood\n";
  if (intersections.empty())
  { cerr << "There are no intersections in \"" << filename << "\"\n";
    return false; }
  return true; }

struct RawConnection
{ string name, type;
  long a, b;
  double length; };

// connections lines look like this:
//     road-name road-type intersection-number intersection-number length-in-miles
bool parseConnection(const string & line, RawConnection & r)
{ vector<Word> w = splitWords(line);
  int n = w.size();
  vector<double> v(n);
  vector<bool> isNum(n);
  for (int i = 0; i < n; i += 1)
    isNum[i] = isNumber(w[i].text, v[i]);

  // the last run of at least three numbers holds the two ends and the length
  int last = -1;
  for (int i = n - 1; i >= 0; )
  { if (! isNum[i])
    { i -= 1;
      continue; }
    int j = i;
    while (j >= 0 && isNum[j])
      j -= 1;
    if (i - j >= 3)
    { last = i;
      break; }
    i = j; }
  if (last < 0)
    return false;
  int first = last - 2;

  // normally the length is the one with a decimal point
  int lengthAt = last;
  if (isInteger(w[last].text))
    for (int i = first; i <= last; i += 1)
      if (! isInteger(w[i].text))
        lengthAt = i;
  vector<int> ends;
  for (int i = first; i <= last; i += 1)
    if (i != lengthAt)
    { if (! isInteger(w[i].text))
        return false;
      ends.push_back(i); }
  r.a = atol(w[ends[0]].text.c_str());
  r.b = atol(w[ends[1]].text.c_str());
  r.length = fabs(v[lengthAt]);

  // the words before the numbers are the name and the type
  vector<string> before, after;
  for (int i = 0; i < first; i += 1)
    before.push_back(w[i].text);
  for (int i = last + 1; i < n; i += 1)
    after.push_back(w[i].text);
  r.name = "";
  r.type = "";
  if (! after.empty())
  { for (size_t i = 0; i < after.size(); i += 1)
      r.type += (i > 0 ? " " : "") + after[i];
    for (size_t i = 0; i < before.size(); i += 1)
      r.name += (i > 0 ? " " : "") + before[i]; }
  else if (before.size() == 2 && before[0].size() <= 2 && before[1].size() > 2)
  { r.type = before[0];
    r.name = before[1]; }
  else if (before.size() >= 2 && before.back().size() <= 2)
  { r.type = before.back();
    for (size_t i = 0; i + 1 < before.size(); i += 1)
      r.name += (i > 0 ? " " : "") + before[i]; }
  else
    for (size_t i = 0; i < before.size(); i += 1)
      r.name += (i > 0 ? " " : "") + before[i];
  if (r.name.empty())
    r.name = "(unnamed road)";
  return true; }

bool RoadMap::readConnections(const string & filename)
{ ifstream fin(filename.c_str());
  if (fin.fail())
  { cerr << "Can not open the connections file \"" << filename << "\"\n";
    return false; }
  vector<RawConnection> raw;
  string line;
  int bad = 0;
  long lowest = -1, highest = -1;
  while (getline(fin, line))
  { if (trim(line).empty())
      continue;
    RawConnection r;
    if (! parseConnection(line, r))
    { bad += 1;
      continue; }
    long lo = r.a < r.b ? r.a : r.b, hi = r.a < r.b ? r.b : r.a;
    if (raw.empty() || lo < lowest)
      lowest = lo;
    if (raw.empty() || hi > highest)
      highest = hi;
    raw.push_back(r); }
  if (bad > 0)
    cerr << "Warning: " << plural(bad, "line") << " of " << filename << " could not be understood\n";

  // Intersections are numbered by their position in the intersections file.
  // That ought to start from 0, but if the numbers only make sense starting
  // from 1, use 1.  When both are possible, pick the one for which the road
  // lengths agree with the positions of the intersections.
  long n = intersections.size();
  bool zeroOK = lowest >= 0 && highest <= n - 1;
  bool oneOK = lowest >= 1 && highest <= n;
  firstNumber = 0;
  if (oneOK && ! zeroOK)
    firstNumber = 1;
  else if (oneOK && zeroOK)
  { int fits[2] = { 0, 0 };
    for (int base = 0; base <= 1; base += 1)
      for (size_t i = 0; i < raw.size(); i += 1)
        if (straightLine(intersections[raw[i].a - base], intersections[raw[i].b - base])
            <= raw[i].length * 1.1 + 0.1)
          fits[base] += 1;
    if (fits[1] > fits[0])
      firstNumber = 1; }
  for (size_t i = 0; i < intersections.size(); i += 1)
    intersections[i]->number = intersections[i]->index + firstNumber;

  int invalid = 0, fit = 0;
  for (size_t i = 0; i < raw.size(); i += 1)
  { Intersection * a = find(raw[i].a), * b = find(raw[i].b);
    if (a == NULL || b == NULL)
    { invalid += 1;
      continue; }
    Connection * c = new Connection;
    c->index = i + 1;
    c->name = raw[i].name;
    c->type = raw[i].type;
    c->a = a;
    c->b = b;
    c->length = raw[i].length;
    connections.push_back(c);
    a->roads.push_back(c);
    if (b != a)                 // a road that loops back is only listed once
      b->roads.push_back(c);
    if (straightLine(a, b) <= c->length * 1.1 + 0.1)
      fit += 1; }
  if (invalid > 0)
    cerr << "Warning: " << plural(invalid, "road segment") << " in " << filename
         << " refer to intersections that do not exist\n";
  consistency = connections.empty() ? 0 : (double)fit / connections.size();
  return true; }

// ===========================================================================
//  Priority queue for Dijkstra's algorithm: a binary min-heap
// ===========================================================================

class MinHeap
{ public:
    struct Item
    { double key;
      int value; };

  protected:
    vector<Item> items;

  public:
    bool empty() const
    { return items.empty(); }

    void add(double key, int value)
    { Item it;
      it.key = key;
      it.value = value;
      size_t pos = items.size();
      items.push_back(it);
      while (pos > 0)
      { size_t parent = (pos - 1) / 2;
        if (items[parent].key <= key)
          break;
        items[pos] = items[parent];
        pos = parent; }
      items[pos] = it; }

    Item removeSmallest()
    { Item top = items[0], last = items.back();
      items.pop_back();
      size_t n = items.size(), pos = 0;
      if (n > 0)
      { while (true)
        { size_t child = 2 * pos + 1;
          if (child >= n)
            break;
          if (child + 1 < n && items[child + 1].key < items[child].key)
            child += 1;
          if (items[child].key >= last.key)
            break;
          items[pos] = items[child];
          pos = child; }
        items[pos] = last; }
      return top; } };

// Dijkstra's algorithm.  Returns false if there is no route at all.
// Otherwise path is set to the road segments to follow, in order from
// start to end, and total to the sum of their lengths.
bool shortestRoute(const RoadMap & map, Intersection * start, Intersection * end,
                   vector<Connection *> & path, double & total)
{ int n = map.intersections.size();
  vector<double> best(n, -1);              // -1 means not reached yet
  vector<Connection *> via(n, (Connection *)NULL);
  vector<bool> finished(n, false);
  MinHeap heap;
  best[start->index] = 0;
  heap.add(0, start->index);
  while (! heap.empty())
  { int i = heap.removeSmallest().value;
    if (finished[i])
      continue;                 // an out of date entry
    finished[i] = true;
    if (i == end->index)
      break;
    Intersection * here = map.intersections[i];
    for (size_t k = 0; k < here->roads.size(); k += 1)
    { Connection * c = here->roads[k];
      int j = c->otherEnd(here)->index;
      if (finished[j])
        continue;
      double d = best[i] + c->length;
      if (best[j] < 0 || d < best[j])
      { best[j] = d;
        via[j] = c;
        heap.add(d, j); } } }

  if (! finished[end->index])
    return false;
  path.clear();
  for (Intersection * x = end; x != start; )
  { Connection * c = via[x->index];
    path.push_back(c);
    x = c->otherEnd(x); }
  for (size_t i = 0, j = path.size(); i + 1 < j; i += 1, j -= 1)
  { Connection * t = path[i];
    path[i] = path[j - 1];
    path[j - 1] = t; }
  total = best[end->index];
  return true; }

// ===========================================================================
//  Choosing a named place  (the usual way, from assignment 2)
// ===========================================================================

string describe(const Place * p)
{ string s = p->title() + "  (";
  if (p->population >= 0)
    s += "population " + exact(p->population) + ", ";
  return s + "latitude " + exact(p->latitude) + ", longitude " + exact(p->longitude) + ")"; }

vector<Place *> inState(const vector<Place *> & places, const string & state)
{ if (state.empty())
    return places;
  vector<Place *> r;
  for (size_t i = 0; i < places.size(); i += 1)
    if (places[i]->state == state)
      r.push_back(places[i]);
  return r; }

// several places fit what the user typed: they choose one by its number in
// the list or by its state.  NULL if they give up.  When the names only
// start with what was typed there may be very many, so not all are shown.
Place * pickOne(vector<Place *> matches, bool exactMatch)
{ const size_t maxShown = 60;
  while (true)
  { int shown = exactMatch || matches.size() <= maxShown ? matches.size() : maxShown;
    cout << "\nThere are " << matches.size() << " places that match:\n";
    for (int i = 0; i < shown; i += 1)
      cout << "  " << (i + 1 < 10 ? " " : "") << i + 1 << ") " << describe(matches[i]) << "\n";
    if (shown < (int)matches.size())
      cout << "   ... and " << matches.size() - shown
           << " more, give a state or type more of the name to narrow it down\n";
    string s;
    if (! readLine("Which one? Type its number or its state's two letter abbreviation (Enter to go back): ", s)
        || s.empty())
      return NULL;
    if (isInteger(s))
    { int k = atoi(s.c_str());
      if (k >= 1 && k <= shown)
        return matches[k - 1];
      cout << "There is no number " << s << " in the list.\n";
      continue; }
    vector<Place *> narrowed = inState(matches, upperCase(s));
    if (narrowed.empty())
    { cout << "None of them is in \"" << upperCase(s) << "\".\n";
      continue; }
    if (narrowed.size() == 1)
      return narrowed[0];
    matches = narrowed; } }

// asks the user for a named place.  They can type just the name, or the
// name and the state like "Miami, FL" or "Miami FL".  NULL if they give up.
Place * choosePlace(const PlaceTable & table, const vector<Place *> & all, const string & which)
{ while (true)
  { string input;
    cout << "\n";
    if (! readLine("Enter the name of the " + which + " place (or just Enter to go back): ", input)
        || input.empty())
      return NULL;
    string name = input, state;
    size_t comma = input.rfind(',');
    if (comma != string::npos)
    { name = trim(input.substr(0, comma));
      state = upperCase(trim(input.substr(comma + 1))); }
    vector<Place *> matches = inState(table.find(name), state);
    if (matches.empty() && comma == string::npos)
    { size_t space = input.find_last_of(" \t");
      if (space != string::npos)
      { string last = trim(input.substr(space + 1));
        if (last.size() == 2 && isalpha((unsigned char)last[0]) && isalpha((unsigned char)last[1]))
        { matches = inState(table.find(trim(input.substr(0, space))), upperCase(last));
          if (! matches.empty())
          { name = trim(input.substr(0, space));
            state = upperCase(last); } } } }
    bool exactMatch = ! matches.empty();
    if (! exactMatch)
    { // nothing with exactly that name, try names that start with it
      string start = normalise(name);
      for (size_t i = 0; i < all.size() && ! start.empty(); i += 1)
        if (normalise(all[i]->name).compare(0, start.size(), start) == 0
            && (state.empty() || all[i]->state == state))
          matches.push_back(all[i]); }
    if (matches.empty())
    { cout << "Sorry, there is no place called \"" << input << "\".\n";
      continue; }
    if (matches.size() == 1)
    { if (! exactMatch)
        cout << "The only place whose name starts like that is " << matches[0]->title() << ".\n";
      return matches[0]; }
    if (! exactMatch)
      cout << "There is no place called exactly \"" << input << "\".";
    Place * p = pickOne(matches, exactMatch);
    if (p != NULL)
      return p; } }

// ===========================================================================
//  Assignment 3:  exploring the graph
// ===========================================================================

void showPlace(const Place * p)
{ cout << "\n" << p->title() << "\n";
  cout << "   numeric code " << p->code;
  if (p->population >= 0)
    cout << ", population " << p->population;
  if (p->area >= 0)
    cout << ", area " << exact(p->area) << " square miles";
  cout << "\n   latitude " << exact(p->latitude) << ", longitude " << exact(p->longitude) << "\n";
  cout << "   its representative road intersection is #" << p->intersection;
  if (p->distance >= 0)
    cout << ", " << exact(p->distance) << " miles away";
  cout << "\n"; }

// shows a place and returns its representative intersection, or NULL
Intersection * goToPlace(const RoadMap & map, const Place * p)
{ showPlace(p);
  Intersection * x = map.find(p->intersection);
  if (x == NULL)
    cout << "   but there is no intersection #" << p->intersection << " in the road map.\n";
  return x; }

void showIntersection(const Intersection * x)
{ cout << "\n------------------------------------------------------------------------\n";
  cout << "Intersection #" << x->number << "\n";
  cout << "   " << x->where() << "\n";
  cout << "   " << x->nearest() << "\n";
  int n = x->roads.size();
  if (n == 0)
  { cout << "   no road segments touch it\n";
    return; }
  cout << "   " << plural(n, "road segment") << " touch" << (n == 1 ? "es" : "") << " it:\n";
  for (int i = 0; i < n; i += 1)
  { const Connection * c = x->roads[i];
    const Intersection * y = c->otherEnd(x);
    cout << "   " << (i + 1 < 10 ? " " : "") << i + 1 << ") " << c->title()
         << ", " << exact(c->length) << " miles heading " << direction(x, y) << "\n";
    cout << "         to intersection #" << y->number << " at " << y->where() << ",\n";
    cout << "         " << y->nearest() << ", where " << plural(y->roads.size(), "road segment")
         << (y->roads.size() == 1 ? " ends" : " meet") << "\n"; } }

void explore(const RoadMap & map, const PlaceTable & table, const vector<Place *> & all)
{ Place * start = choosePlace(table, all, "starting");
  if (start == NULL)
    return;
  Intersection * here = goToPlace(map, start);
  while (here != NULL)
  { showIntersection(here);
    int n = here->roads.size();
    string prompt;
    if (n == 0)
      prompt = "Type p to choose another place, or q to stop exploring: ";
    else if (n == 1)
      prompt = "Type 1 to follow that road segment, p to choose another place, or q to stop exploring: ";
    else
      prompt = "Which road segment do you want to follow (1 to " + exact(n)
               + ")? Or p to choose another place, q to stop exploring: ";
    while (true)
    { string s;
      if (! readLine(prompt, s) || s == "q" || s == "Q")
        return;
      if (s == "p" || s == "P")
      { Place * p = choosePlace(table, all, "next");
        if (p != NULL)
        { Intersection * x = goToPlace(map, p);
          if (x != NULL)
            here = x; }
        break; }
      if (isInteger(s) && atoi(s.c_str()) >= 1 && atoi(s.c_str()) <= n)
      { Connection * c = here->roads[atoi(s.c_str()) - 1];
        Intersection * next = c->otherEnd(here);
        cout << "\nFollowing " << c->name << " for " << exact(c->length) << " miles heading "
             << direction(here, next) << " ...\n";
        here = next;
        break; }
      cout << "\"" << s << "\" is not one of the choices.\n"; } } }

// ===========================================================================
//  Assignment 4:  shortest routes
// ===========================================================================

void showRoute(const RoadMap & map, const PlaceTable & table, const vector<Place *> & all)
{ Place * from = choosePlace(table, all, "starting");
  if (from == NULL)
    return;
  Place * to = choosePlace(table, all, "destination");
  if (to == NULL)
    return;
  Intersection * start = map.find(from->intersection), * end = map.find(to->intersection);
  if (start == NULL || end == NULL)
  { cout << "Sorry, " << (start == NULL ? from : to)->title()
         << " has no representative intersection in the road map.\n";
    return; }

  cout << "\nShortest route from " << from->title() << " to " << to->title() << "\n";
  cout << "   " << from->title() << " is represented by intersection #" << start->number;
  if (from->distance >= 0)
    cout << " (" << exact(from->distance) << " miles away)";
  cout << "\n   " << to->title() << " is represented by intersection #" << end->number;
  if (to->distance >= 0)
    cout << " (" << exact(to->distance) << " miles away)";
  cout << "\n";

  vector<Connection *> path;
  double total = 0;
  if (! shortestRoute(map, start, end, path, total))
  { cout << "\nThere is no way to get from " << from->title() << " to " << to->title()
         << " by road.\n";
    return; }

  cout << "\n";
  Intersection * here = start;
  double sum = 0;
  for (size_t i = 0; i < path.size(); i += 1)
  { Connection * c = path[i];
    Intersection * next = c->otherEnd(here);
    cout << (i + 1 < 10 ? "   " : i + 1 < 100 ? "  " : i + 1 < 1000 ? " " : "") << i + 1 << ". "
         << "intersection #" << here->number << ", " << here->where() << ", " << here->nearest() << "\n";
    cout << "        take " << c->title() << " for " << exact(c->length) << " miles heading "
         << direction(here, next) << "\n";
    sum += c->length;
    here = next; }
  size_t k = path.size() + 1;
  cout << (k < 10 ? "   " : k < 100 ? "  " : k < 1000 ? " " : "") << k << ". "
       << "intersection #" << here->number << ", " << here->where() << ", " << here->nearest() << "\n";
  cout << "        you have arrived at " << to->title() << "\n";

  // the same route in brief, consecutive segments of the same road joined up
  if (! path.empty())
  { cout << "\nIn brief:\n";
    Intersection * a = start;
    size_t i = 0;
    while (i < path.size())
    { Intersection * b = a;
      double length = 0;
      size_t j = i;
      while (j < path.size() && path[j]->name == path[i]->name)
      { length += path[j]->length;
        b = path[j]->otherEnd(b);
        j += 1; }
      cout << "   " << path[i]->name << " heading " << direction(a, b) << " for "
           << num(length, 2) << " miles\n";
      a = b;
      i = j; } }

  cout << "\nTotal length of the " << plural(path.size(), "road segment") << " followed: "
       << num(sum, 2) << " miles\n"; }

// ===========================================================================

int main(int argc, char * argv[])
{ string placesFile = argc > 1 ? argv[1] : "named-places.txt";
  string intersectionsFile = argc > 2 ? argv[2] : "intersections.txt";
  string connectionsFile = argc > 3 ? argv[3] : "connections.txt";

  RoadMap map;
  vector<Place *> places;
  PlaceTable table;
  if (! map.readIntersections(intersectionsFile) || ! map.readConnections(connectionsFile)
      || ! readPlaces(placesFile, places, table))
    return 1;

  int noIntersection = 0;
  for (size_t i = 0; i < places.size(); i += 1)
    if (map.find(places[i]->intersection) == NULL)
      noIntersection += 1;
  cout << "Read " << plural(table.size(), "named place") << ", "
       << plural(map.intersections.size(), "intersection") << " and "
       << plural(map.connections.size(), "road segment") << ".\n";
  cout << "Intersections are numbered from " << map.firstNumber << "; "
       << num(map.consistency * 100, 1)
       << "% of road segment lengths agree with the positions of their ends.\n";
  if (noIntersection > 0)
    cout << "Warning: " << plural(noIntersection, "named place") << " have no valid intersection.\n";

  while (true)
  { string choice;
    cout << "\n========================================================================\n";
    cout << "  1  explore the roads starting from a named place\n";
    cout << "  2  find the shortest route between two named places\n";
    cout << "  q  quit\n";
    if (! readLine("Your choice: ", choice) || choice == "q" || choice == "Q")
      break;
    if (choice == "1")
      explore(map, table, places);
    else if (choice == "2")
      showRoute(map, table, places);
    else if (! choice.empty())
      cout << "Please type 1, 2 or q.\n"; }

  for (size_t i = 0; i < places.size(); i += 1)
    delete places[i];
  return 0; }
