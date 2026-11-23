// ===========================================================================
//  Assignment 4:  Shortest routes between named places
//                 (this program also contains all of assignment 3)
// ===========================================================================
//
//  Assignment 3 part: the intersections file and the connections file are
//  read and turned into a graph:
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
//  Assignment 4 part: the user picks two named places, and Dijkstra's
//  algorithm finds the shortest route between their representative
//  intersections.  Every intersection on the route is shown, followed by
//  the name, length and direction of the road taken to the next one, and
//  finally the total length of all the road segments followed.
//
//  Files (these are the default names, others may be given on the command
//  line in this order):
//      named-places.txt     intersections.txt     connections.txt
//
//  Compile:   g++ -std=c++11 -O2 -Wall -o assignment4 assignment4.cpp
//  Run:       ./assignment4
//        or   ./assignment4 named-places.txt intersections.txt connections.txt
// ===========================================================================

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cctype>

using namespace std;

const double PI = 3.14159265358979323846;
const size_t NONE = string::npos;

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
    s[i] = (char)toupper((unsigned char)s[i]);
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

// a number written as it appears in the data files: no pointless trailing
// zeros, and no digits lost
string exact(double x)
{ char buffer[64];
  snprintf(buffer, sizeof(buffer), "%.10g", x);
  return buffer; }

string plural(double n, const string & word)
{ return exact(n) + " " + word + (n == 1 ? "" : "s"); }

string miles(double n)
{ return plural(n, "mile"); }

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

bool isNumberCharacter(char c)
{ return isdigit((unsigned char)c) || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E'; }

// true if the whole of s is a number; its value is stored in value.
// only digits . + - e E are allowed, so words like "nan" or "inf" are
// never mistaken for numbers.
bool isNumber(const string & s, double & value)
{ bool sawDigit = false;
  for (size_t i = 0; i < s.size(); i += 1)
  { if (! isNumberCharacter(s[i]))
      return false;
    if (isdigit((unsigned char)s[i]))
      sawDigit = true; }
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
//  Splitting the lines of the data files into fields
// ===========================================================================
//
//  The data files are written in fixed width columns.  Usually there are
//  spaces between the fields, but a value that fills its whole column
//  touches the one before it, as in "35.449578-100.225838".  Two things
//  deal with that:
//    1. a word made only of number characters is split wherever a sign
//       follows a digit, so that becomes "35.449578" and "-100.225838",
//       and a number followed straight away by letters is split from
//       them, so "1.23FL" becomes "1.23" and "FL";
//    2. the lines in which every field is clearly separated show where each
//       column starts and ends.  If they agree, those column positions are
//       used to read the lines that are not so clear: those with fields
//       stuck together, and any whose numbers had to be split by guessing.

struct Word
{ string text;
  size_t start, end;            // where the word is in its line: [start, end)
  bool doubtful; };             // may really be two fields stuck together

Word makeWord(const string & s, size_t start, size_t end, bool doubtful = false)
{ Word w;
  w.text = s.substr(start, end - start);
  w.start = start;
  w.end = end;
  w.doubtful = doubtful;
  return w; }

// two or more decimal numbers with no space between them, as in
// "-80.1234525.39493" (a longitude and a latitude): they are assumed to
// have the same number of digits after the decimal point.  The split is
// only made if every piece could be a longitude or latitude and none
// starts with a needless 0; anything else is left as it is.  The pieces
// are marked as doubtful.
void splitDecimals(const string & s, size_t start, size_t end, vector<Word> & words)
{ vector<size_t> dots, cuts;
  for (size_t k = start; k < end; k += 1)
    if (s[k] == '.')
      dots.push_back(k);
  bool ok = dots.size() >= 2;
  size_t decimals = ok ? end - dots.back() - 1 : 0;
  for (size_t j = 0; ok && j + 1 < dots.size(); j += 1)
  { size_t cut = dots[j] + decimals + 1;
    if (decimals == 0 || cut >= dots[j + 1])
      ok = false;
    for (size_t k = dots[j] + 1; ok && k < dots[j + 1]; k += 1)
      if (! isdigit((unsigned char)s[k]))
        ok = false;
    if (ok && (fabs(atof(s.substr(cut, dots[j + 1] - cut).c_str())) > 180
               || (s[cut] == '0' && cut + 1 < dots[j + 1])))
      ok = false;
    cuts.push_back(cut); }
  if (ok && fabs(atof(s.substr(start, dots[0] - start).c_str())) > 180)
    ok = false;
  if (ok)
    for (size_t j = 0; j < cuts.size(); j += 1)
    { words.push_back(makeWord(s, start, cuts[j], true));
      start = cuts[j]; }
  words.push_back(makeWord(s, start, end, ok)); }

// a run of number characters that may be several numbers with no space
// between them: split wherever a sign follows a digit, then split joined
// decimals
void splitNumber(const string & s, size_t start, size_t end, vector<Word> & words)
{ size_t from = start;
  for (size_t k = start + 1; k < end; k += 1)
    if ((s[k] == '-' || s[k] == '+') && (isdigit((unsigned char)s[k - 1]) || s[k - 1] == '.'))
    { splitDecimals(s, from, k, words);
      from = k; }
  splitDecimals(s, from, end, words); }

// splits a line into words at the spaces.  For the data files
// (splitNumbers true) numbers that touch each other or touch letters are
// separated as well.
vector<Word> splitWords(const string & s, bool splitNumbers)
{ vector<Word> words;
  size_t i = 0, n = s.size();
  while (true)
  { while (i < n && isspace((unsigned char)s[i]))
      i += 1;
    if (i >= n)
      break;
    size_t start = i;
    bool numeric = true;
    while (i < n && ! isspace((unsigned char)s[i]))
    { if (! isNumberCharacter(s[i]))
        numeric = false;
      i += 1; }
    if (! splitNumbers)
      words.push_back(makeWord(s, start, i));
    else if (numeric)
      splitNumber(s, start, i, words);
    else
    { // numbers followed straight away by letters, as in "1.23FL"
      size_t k = start;
      while (k < i && (isdigit((unsigned char)s[k]) || s[k] == '.' || s[k] == '-' || s[k] == '+'))
        k += 1;
      if (k > start && k < i && isalpha((unsigned char)s[k]) && isdigit((unsigned char)s[k - 1]))
      { splitNumber(s, start, k, words);
        start = k; }
      words.push_back(makeWord(s, start, i)); } }
  return words; }

// true if any of words first ... first+count-1 is doubtful
bool anyDoubtful(const vector<Word> & w, size_t first, size_t count)
{ for (size_t i = first; i < first + count && i < w.size(); i += 1)
    if (w[i].doubtful)
      return true;
  return false; }

// the part of a line from position "from" up to (not including) "to", trimmed.
// NONE for "to" means the end of the line.
string piece(const string & line, size_t from, size_t to)
{ if (from >= line.size())
    return "";
  if (to == NONE || to > line.size())
    to = line.size();
  if (to <= from)
    return "";
  return trim(line.substr(from, to - from)); }

// Learns where the columns of a fixed width file are.  It is given the
// lines in which every field came out separated.  A field that (in most
// of those lines) always ends in the same place is right justified, and
// its column begins where the field before it ends, if that is fixed.  A
// field that always starts in the same place is left justified, and its
// column begins there.  Otherwise a column begins just after the longest
// field before it, as long as that does not run into this one.  The file
// is accepted as having fixed columns only if hardly any of those lines
// contradict the positions.
class Columns
{ protected:
    int fields;
    vector<vector<size_t> > spans;      // for each line: start, end of each field
    vector<size_t> cut, widest;
    vector<bool> numeric;               // field i was always a number

    // the position most lines agree on, and how many agree; NONE if fewer
    // than half do
    static size_t usual(const map<size_t, int> & m, int lines, int & most)
    { size_t best = NONE;
      most = 0;
      for (map<size_t, int>::const_iterator it = m.begin(); it != m.end(); ++it)
        if (it->second > most)
        { most = it->second;
          best = it->first; }
      return most * 2 >= lines ? best : NONE; }

    bool isCut(size_t position) const
    { for (int i = 0; i <= fields; i += 1)
        if (cut[i] == position)
          return true;
      return false; }

    // 1: the fields lie in their columns.  0: a word runs across a column
    // boundary and ends exactly where a column ends, so it is really two
    // fields stuck together.  -1: the line does not follow the columns.
    int check(const vector<size_t> & s) const
    { bool stuck = false;
      for (int j = 0; j < fields; j += 1)
        for (int i = 0; i <= fields; i += 1)
          if (cut[i] != NONE && s[2 * j] < cut[i] && cut[i] < s[2 * j + 1])
          { if (! isCut(s[2 * j + 1]))
              return -1;
            stuck = true; }
      if (stuck)
        return 0;
      for (int i = 0; i < fields; i += 1)
        if ((cut[i] != NONE && s[2 * i] < cut[i]) || (cut[i + 1] != NONE && s[2 * i + 1] > cut[i + 1]))
          return -1;
      return 1; }

    vector<size_t> spanOf(const vector<Word> & w, size_t first) const
    { vector<size_t> s;
      for (int i = 0; i < fields; i += 1)
      { s.push_back(w[first + i].start);
        s.push_back(w[first + i].end); }
      return s; }

  public:
    Columns(int n): fields(n), cut(n + 1, NONE), widest(n, 0), numeric(n, true) { }

    // words first, first+1, ... of w are one line's fields
    void add(const vector<Word> & w, size_t first)
    { spans.push_back(spanOf(w, first));
      for (int i = 0; i < fields; i += 1)
      { double x;
        if (w[first + i].end - w[first + i].start > widest[i])
          widest[i] = w[first + i].end - w[first + i].start;
        if (! isNumber(w[first + i].text, x))
          numeric[i] = false; } }

    // the widest field i has been
    size_t width(int i) const
    { return widest[i]; }

    // Works out cut[i], where field i begins, and cut[i + 1], where it ends.
    // cut[0] is NONE if the first field is right justified, and the last
    // one is NONE if the last field does not always end in the same place.
    // False if the file does not have fixed columns.
    bool find(vector<size_t> & result)
    { int lines = spans.size();
      cut.assign(fields + 1, NONE);
      result = cut;
      if (lines < 2)
        return false;
      vector<map<size_t, int> > starts(fields), ends(fields);
      for (int k = 0; k < lines; k += 1)
        for (int i = 0; i < fields; i += 1)
        { starts[i][spans[k][2 * i]] += 1;
          ends[i][spans[k][2 * i + 1]] += 1; }
      vector<size_t> start(fields), end(fields);
      vector<bool> right(fields);       // right justified
      for (int i = 0; i < fields; i += 1)
      { int cs, ce;
        start[i] = usual(starts[i], lines, cs);
        end[i] = usual(ends[i], lines, ce);
        right[i] = ce > cs || (ce == cs && numeric[i]); }
      if (! right[0])
        cut[0] = start[0];
      for (int i = 1; i < fields; i += 1)
      { if (! right[i])
          cut[i] = start[i];
        else if (right[i - 1] || end[i - 1] != NONE)
          cut[i] = end[i - 1];
        if (cut[i] == NONE)
        { size_t longest = 0, earliest = NONE;
          for (int k = 0; k < lines; k += 1)
          { if (spans[k][2 * i - 1] > longest)
              longest = spans[k][2 * i - 1];
            if (spans[k][2 * i] < earliest)
              earliest = spans[k][2 * i]; }
          if (longest <= earliest)
            cut[i] = longest; }
        if (cut[i] == NONE || (cut[i - 1] != NONE && cut[i] <= cut[i - 1]))
        { cut.assign(fields + 1, NONE);
          return false; } }
      cut[fields] = right[fields - 1] ? end[fields - 1] : NONE;
      int fit = 0, misfit = 0;
      for (int k = 0; k < lines; k += 1)
      { int c = check(spans[k]);
        if (c > 0)
          fit += 1;
        else if (c < 0)
          misfit += 1; }
      if (fit < 2 || misfit * 10 > fit + misfit)      // more than 10% contradict
      { cut.assign(fields + 1, NONE);
        return false; }
      result = cut;
      return true; }

    // true if a line's words really include fields stuck together
    bool stuck(const vector<Word> & w, size_t first) const
    { return cut[1] != NONE && check(spanOf(w, first)) == 0; } };

// How many fields the lines usually have: the largest count that at least
// 5% of the lines have.  Touching fields can only make the count smaller,
// so the largest common one is right even if most lines have touching
// fields.  -1 if there are none.
int usualCount(const map<int, int> & counts, int lines)
{ int best = -1;
  for (map<int, int>::const_iterator it = counts.begin(); it != counts.end(); ++it)
    if (it->second * 20 >= lines && it->second >= 2 && it->first > best)
      best = it->first;
  if (best < 0 && ! counts.empty())
    best = counts.rbegin()->first;
  return best; }

bool readLines(const string & filename, const string & what, vector<string> & lines)
{ ifstream fin(filename.c_str());
  if (fin.fail())
  { cerr << "Can not open the " << what << " file \"" << filename << "\"\n";
    return false; }
  string line;
  while (getline(fin, line))
  { if (lines.empty() && line.compare(0, 3, "\xEF\xBB\xBF") == 0)
      line = line.substr(3);    // a UTF-8 byte order mark
    if (! trim(line).empty())
      lines.push_back(line); }
  if (lines.empty())
  { cerr << "The " << what << " file \"" << filename << "\" is empty\n";
    return false; }
  return true; }

// ===========================================================================
//  Named places and their hash table   (from assignment 2)
// ===========================================================================

struct Place
{ string code, state, name;
  long population;              // -1 if not given
  double area;                  // -1 if not given
  double latitude, longitude;
  long intersection;            // number of its representative intersection, -1 if none
  double distance;              // how far away that intersection is, -1 if not given

  string title() const
  { return name + ", " + state; } };

// Named places lines are: numeric code, state, name, population, area,
// latitude, longitude, representative intersection, distance to it.
// The code and the state come first; this reads them and returns where the
// name starts, or NONE if the line does not start that way.
size_t placeHeader(const string & line, Place & p)
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
    return NONE;
  p.state = upperCase(line.substr(i, 2));
  i += 2;
  if (p.code.empty())           // just in case the code comes after the state
  { while (i < n && isspace((unsigned char)line[i]))
      i += 1;
    s = i;
    while (i < n && isdigit((unsigned char)line[i]))
      i += 1;
    p.code = line.substr(s, i - s); }
  return i; }

// the numbers at the end of a named places line, after position from
vector<Word> trailingNumbers(const string & line, size_t from)
{ vector<Word> w = splitWords(line, true), r;
  double x;
  int k = w.size();
  while (k > 0 && w[k - 1].start >= from && isNumber(w[k - 1].text, x))
    k -= 1;
  for (int i = k; i < (int)w.size(); i += 1)
    r.push_back(w[i]);
  return r; }

// v holds the numbers that follow the name: normally population, area,
// latitude, longitude, intersection, distance.  An empty population is
// allowed (it is unknown).  Values that cannot be right are rejected, so
// that two numbers stuck together are not taken for one.
bool setNumbers(Place & p, const vector<string> & v)
{ int n = v.size();
  vector<double> x(n, -1);
  for (int i = 0; i < n; i += 1)
    if (! isNumber(v[i], x[i]) && ! (i == 0 && n >= 5 && v[i].empty()))
      return false;
  double pop = -1, area = -1, lat, lon, dist = -1;
  int at;                       // which one is the intersection
  if (n == 6)
  { pop = x[0]; area = x[1]; lat = x[2]; lon = x[3]; at = 4; dist = x[5];
    if (! isInteger(v[4]) && isInteger(v[5]))     // distance came first
    { at = 5;
      dist = x[4]; } }
  else if (n == 5 && isInteger(v[4]))             // no distance
  { pop = x[0]; area = x[1]; lat = x[2]; lon = x[3]; at = 4; }
  else if (n == 5)                                // no area
  { pop = x[0]; lat = x[1]; lon = x[2]; at = 3; dist = x[4]; }
  else if (n == 4 && isInteger(v[3]))             // no area, no distance
  { pop = x[0]; lat = x[1]; lon = x[2]; at = 3; }
  else if (n == 4)                                // no population, no area
  { lat = x[0]; lon = x[1]; at = 2; dist = x[3]; }
  else
    return false;
  if (! isInteger(v[at]))
    return false;
  if (fabs(lat) > 90 || (lat < 0 && lon > 0))     // longitude came first
  { double t = lat;
    lat = lon;
    lon = t; }
  if (fabs(lat) > 90 || fabs(lon) > 180 || pop > 1e9 || area > 1e9 || dist > 1e5
      || (pop >= 0 && ! isInteger(v[0])))
    return false;
  p.population = (long)pop;
  p.area = area;
  p.latitude = lat;
  p.longitude = lon;
  p.intersection = x[at] >= 0 && x[at] < 2e9 ? (long)x[at] : -1;     // -1: none
  p.distance = dist;
  return true; }

// reads the name and numbers of a line whose fields are separated by spaces
bool placeFromWords(const string & line, size_t nameStart, const vector<Word> & numbers, Place & p)
{ int count = numbers.size(), use = count < 6 ? count : 6;
  if (use < 4)
    return false;
  int first = count - use;
  vector<string> v;
  for (int i = first; i < count; i += 1)
    v.push_back(numbers[i].text);
  p.name = trim(line.substr(nameStart, numbers[first].start - nameStart));
  return ! p.name.empty() && setNumbers(p, v); }

// reads the name and numbers of a line using the fixed column positions
bool placeFromColumns(const string & line, size_t nameStart, const vector<size_t> & cut,
                      size_t firstWidth, bool firstIsInteger, Place & p)
{ int n = cut.size() - 1;
  vector<string> v(n);
  for (int i = 1; i < n; i += 1)
    v[i] = piece(line, cut[i], cut[i + 1]);
  size_t end = cut[1], start;
  if (end > line.size() || end <= nameStart)
    return false;
  if (cut[0] != NONE)
    start = cut[0];
  else
  { // a right justified number that may touch the name: take the number
    // characters at the end, no more than the column has ever held
    start = end;
    while (start > nameStart && start > end - firstWidth && isspace((unsigned char)line[start - 1]))
      start -= 1;
    size_t last = start;
    while (start > nameStart && last - start < firstWidth
           && (isdigit((unsigned char)line[start - 1])
               || (! firstIsInteger && (line[start - 1] == '.' || line[start - 1] == '-'))))
      start -= 1; }
  if (start < nameStart)
    return false;
  v[0] = piece(line, start, end);
  p.name = piece(line, nameStart, start);
  return ! p.name.empty() && setNumbers(p, v); }

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
{ vector<string> lines;
  if (! readLines(filename, "named places", lines))
    return false;
  int n = lines.size();
  vector<Place> head(n);
  vector<size_t> nameStart(n);
  vector<vector<Word> > numbers(n);
  map<int, int> counts;
  for (int i = 0; i < n; i += 1)
  { nameStart[i] = placeHeader(lines[i], head[i]);
    if (nameStart[i] != NONE)
    { numbers[i] = trailingNumbers(lines[i], nameStart[i]);
      if (numbers[i].size() >= 4)
        counts[numbers[i].size() < 6 ? numbers[i].size() : 6] += 1; } }

  // the lines with all the numbers separated show where the columns are
  int usual = usualCount(counts, n);
  bool fixed = false, firstIsInteger = true;
  vector<size_t> cut;
  size_t firstWidth = 0;
  Columns columns(usual >= 4 ? usual : 4);
  if (usual >= 4)
  {
    for (int i = 0; i < n; i += 1)
      if (nameStart[i] != NONE && (int)numbers[i].size() >= usual)
      { int first = numbers[i].size() - usual;
        columns.add(numbers[i], first);
        if (! isInteger(numbers[i][first].text))
          firstIsInteger = false; }
    fixed = columns.find(cut);
    // the widest the first field (population) ever is: the longest run of
    // digits that ends where its column ends and has a space before it
    firstWidth = columns.width(0);
    for (int i = 0; fixed && cut[0] == NONE && i < n; i += 1)
      if (nameStart[i] != NONE && cut[1] < lines[i].size())
      { size_t k = cut[1];
        while (k > nameStart[i] && isdigit((unsigned char)lines[i][k - 1]))
          k -= 1;
        if (k > nameStart[i] && k < cut[1] && isspace((unsigned char)lines[i][k - 1])
            && cut[1] - k > firstWidth)
          firstWidth = cut[1] - k; } }

  int bad = 0;
  for (int i = 0; i < n; i += 1)
  { Place * p = new Place(head[i]);
    bool ok = false;
    if (nameStart[i] != NONE)
    { // a line whose numbers all came out clearly separated is read word
      // by word; any other by the column positions if they are known
      bool clear = (int)numbers[i].size() >= usual && ! anyDoubtful(numbers[i], 0, numbers[i].size())
                   && ! (fixed && columns.stuck(numbers[i], numbers[i].size() - usual));
      if (clear)
        ok = placeFromWords(lines[i], nameStart[i], numbers[i], * p);
      if (! ok && fixed)
        ok = placeFromColumns(lines[i], nameStart[i], cut, firstWidth, firstIsInteger, * p);
      if (! ok && ! clear)
        ok = placeFromWords(lines[i], nameStart[i], numbers[i], * p); }
    if (ok)
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
      s += (s.empty() ? "" : ", ") + state;
    if (s.empty())
      s = "no named place";
    if (distance < 0)
      return "near " + s;
    return miles(distance) + " from " + s; } };

struct Connection
{ string name, type;            // name is "?" for an unnamed road
  Intersection * a, * b;        // the two ends
  double length;                // in miles

  Intersection * otherEnd(const Intersection * from) const
  { return from == a ? b : a; }

  string roadName() const
  { return name == "?" ? "unnamed road" : name; }

  string title() const
  { if (type.empty())
      return roadName();
    return roadName() + " (type " + type + ")"; } };

// the compass direction from one point to another, exactly as the
// assignment describes it
string direction(double fromLatitude, double fromLongitude, double toLatitude, double toLongitude)
{ double lata = fromLatitude * PI / 180, lona = fromLongitude * PI / 180;
  double latb = toLatitude * PI / 180, lonb = toLongitude * PI / 180;
  double angle = atan2(lonb - lona, latb - lata) * 180 / PI;
  if (angle > 157.5 || angle < -157.5) return "S";
  if (angle >= 112.5) return "SE";
  if (angle >= 67.5)  return "E";
  if (angle >= 22.5)  return "NE";
  if (angle >= -22.5) return "N";
  if (angle >= -67.5) return "NW";
  if (angle >= -112.5) return "W";
  return "SW"; }

string direction(const Intersection * from, const Intersection * to)
{ return direction(from->latitude, from->longitude, to->latitude, to->longitude); }

// approximate straight line distance in miles, only used to check that the
// files have been understood correctly
double straightLine(double lat1, double lon1, double lat2, double lon2)
{ double milesPerDegree = 69.17;
  double dy = (lat2 - lat1) * milesPerDegree;
  double dx = (lon2 - lon1) * milesPerDegree * cos((lat1 + lat2) / 2 * PI / 180);
  return sqrt(dx * dx + dy * dy); }

double straightLine(const Intersection * a, const Intersection * b)
{ return straightLine(a->latitude, a->longitude, b->latitude, b->longitude); }

// splits the end of an intersections line into the state and the place
// name.  The state is a two letter postal code, and may have no space
// after it, as in "FLMiami".
void splitState(const string & text, string & state, string & place)
{ static const string codes =
    " AL AK AZ AR CA CO CT DE DC FL GA HI ID IL IN IA KS KY LA ME MD MA MI MN MS MO MT NE NV NH NJ"
    " NM NY NC ND OH OK OR PA RI SC SD TN TX UT VT VA WA WV WI WY PR GU VI AS MP UM FM MH PW AA AE AP ";
  state = "";
  place = text;
  if (text.size() >= 2 && isupper((unsigned char)text[0]) && isupper((unsigned char)text[1])
      && (text.size() == 2 || isspace((unsigned char)text[2])
          || codes.find(" " + text.substr(0, 2) + " ") != string::npos))
  { state = text.substr(0, 2);
    place = trim(text.substr(2)); } }

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

// Intersections lines are: longitude, latitude, distance to the nearest
// named place, that place's state and name.  An intersection's number is
// its position in the file.
bool RoadMap::readIntersections(const string & filename)
{ vector<string> lines;
  if (! readLines(filename, "intersections", lines))
    return false;
  int n = lines.size();
  vector<vector<Word> > words(n);
  vector<int> lead(n, 0);
  map<int, int> leads;
  for (int i = 0; i < n; i += 1)
  { words[i] = splitWords(lines[i], true);
    double x;
    while (lead[i] < (int)words[i].size() && lead[i] < 4 && isNumber(words[i][lead[i]].text, x))
      lead[i] += 1;
    if (lead[i] >= 2)
      leads[lead[i]] += 1; }

  // how many numbers start each line, decided once for the whole file:
  // normally longitude, latitude and distance, but possibly also the
  // intersection's own number first
  int used = usualCount(leads, n), c = 0;        // c: which number is the first coordinate
  bool hasDistance = true;
  if (used < 2)
  { cerr << "The intersections file \"" << filename << "\" could not be understood\n";
    return false; }
  if (used == 4)
    c = 1;
  else if (used == 3)
  { int numbered = 0, total = 0;
    for (int i = 0; i < n; i += 1)
      if (lead[i] == 3)
      { total += 1;
        if (isInteger(words[i][0].text) && ! isInteger(words[i][1].text))
          numbered += 1; }
    if (numbered * 2 > total)
    { c = 1;
      hasDistance = false; } }
  else
    hasDistance = false;

  // the lines whose numbers are clearly separated show where the columns are
  Columns columns(used);
  for (int i = 0; i < n; i += 1)
    if (lead[i] == used)
      columns.add(words[i], 0);
  vector<size_t> cut;
  bool fixed = columns.find(cut);

  int bad = 0, longitudeFirst = 0, latitudeFirst = 0;
  for (int i = 0; i < n; i += 1)
  { double x[4];
    size_t after = NONE;        // where the state and place name begin
    bool ok = false;
    if (fixed && (lead[i] < used || anyDoubtful(words[i], 0, used) || columns.stuck(words[i], 0))
        && cut[used] != NONE)
    { ok = true;
      for (int k = 0; k < used && ok; k += 1)
        ok = isNumber(piece(lines[i], k == 0 ? 0 : cut[k], cut[k + 1]), x[k]);
      after = cut[used]; }
    if (! ok && lead[i] >= used)
    { ok = true;
      for (int k = 0; k < used; k += 1)
        isNumber(words[i][k].text, x[k]);
      after = words[i][used - 1].end; }
    string state, place;
    if (ok)
      splitState(piece(lines[i], after, NONE), state, place);

    if (! ok && intersections.empty())
    { bad += 1;                 // probably a heading at the top of the file
      continue; }
    Intersection * p = new Intersection;
    p->index = intersections.size();
    if (ok)
    { p->longitude = x[c];      // sorted out below if the order is the other way round
      p->latitude = x[c + 1];
      p->distance = hasDistance ? x[c + 2] : -1;
      p->state = state;
      p->place = place;
      if ((p->longitude < 0 && p->latitude > 0) || fabs(p->longitude) > 90)
        longitudeFirst += 1;
      else if ((p->longitude > 0 && p->latitude < 0) || fabs(p->latitude) > 90)
        latitudeFirst += 1; }
    else
    { // keep a place holder so that the numbering of the rest is not upset
      bad += 1;
      p->longitude = p->latitude = 0;
      p->distance = -1;
      p->place = "(unreadable line in " + filename + ")"; }
    intersections.push_back(p); }

  if (latitudeFirst > longitudeFirst)
    for (size_t i = 0; i < intersections.size(); i += 1)
    { double t = intersections[i]->longitude;
      intersections[i]->longitude = intersections[i]->latitude;
      intersections[i]->latitude = t; }
  if (bad > 0)
    cerr << "Warning: " << plural(bad, "line") << " of " << filename << " could not be understood\n";
  return true; }

struct RawConnection
{ string name, type;
  long a, b;
  double length; };

// Connections lines are: road name, road type, the numbers of the two
// intersections it joins, and its length in miles.  This reads a line whose
// fields are separated by spaces.  fields is set to 5 if it has exactly
// those fields, 4 if it is the same without the type, otherwise 0; w is
// set to its words.
bool connectionFromWords(const string & line, RawConnection & r, int & fields, vector<Word> & w)
{ w = splitWords(line, false);
  fields = 0;
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

  // normally the length is the one with a decimal point, and it comes last
  int lengthAt = last;
  if (isInteger(w[last].text))
    for (int i = first; i <= last; i += 1)
      if (! isInteger(w[i].text))
        lengthAt = i;
  vector<int> ends;
  for (int i = first; i <= last; i += 1)
    if (i != lengthAt)
    { if (! isInteger(w[i].text) || v[i] < 0)
        return false;
      ends.push_back(i); }
  r.a = atol(w[ends[0]].text.c_str());
  r.b = atol(w[ends[1]].text.c_str());
  r.length = fabs(v[lengthAt]);

  // the words before the numbers are the name and then the type
  r.name = "";
  r.type = "";
  for (int i = last + 1; i < n; i += 1)
    r.type += (r.type.empty() ? "" : " ") + w[i].text;
  int nameWords = first;
  if (r.type.empty() && first >= 2)
  { r.type = w[first - 1].text;
    nameWords = first - 1; }
  for (int i = 0; i < nameWords; i += 1)
    r.name += (i > 0 ? " " : "") + w[i].text;
  if (r.name.empty())
    r.name = "?";
  if ((first == 1 || first == 2) && last == n - 1 && lengthAt == last)
    fields = first + 3;
  return true; }

// reads a line using the fixed column positions; fields is 5, or 4 if the
// file has no road types
bool connectionFromColumns(const string & line, const vector<size_t> & cut, int fields, RawConnection & r)
{ int t = fields - 4;           // 1 if there is a type column
  string a = piece(line, cut[1 + t], cut[2 + t]), b = piece(line, cut[2 + t], cut[3 + t]);
  string length = piece(line, cut[3 + t], cut[4 + t]);
  r.name = piece(line, cut[0] == NONE ? 0 : cut[0], cut[1]);
  r.type = t == 1 ? piece(line, cut[1], cut[2]) : "";
  if (r.name.empty() || ! isInteger(a) || ! isInteger(b) || ! isNumber(length, r.length)
      || a[0] == '-' || b[0] == '-')
    return false;
  r.a = atol(a.c_str());
  r.b = atol(b.c_str());
  r.length = fabs(r.length);
  return true; }

bool RoadMap::readConnections(const string & filename)
{ vector<string> lines;
  if (! readLines(filename, "connections", lines))
    return false;
  int n = lines.size();
  vector<RawConnection> parsed(n);
  vector<vector<Word> > words(n);
  vector<bool> ok(n), clear(n);
  vector<int> fields(n);
  int withType = 0, withoutType = 0;
  for (int i = 0; i < n; i += 1)
  { ok[i] = connectionFromWords(lines[i], parsed[i], fields[i], words[i]);
    if (ok[i] && fields[i] == 5)
      withType += 1;
    if (ok[i] && fields[i] == 4)
      withoutType += 1; }
  // the clear lines (all five fields separated, or all four if the file has
  // no road types) show where the columns are
  int shape = withType >= withoutType ? 5 : 4;
  Columns columns(shape);
  for (int i = 0; i < n; i += 1)
  { clear[i] = ok[i] && fields[i] == shape;
    if (clear[i])
      columns.add(words[i], 0); }
  vector<size_t> cut;
  bool fixed = columns.find(cut);

  vector<RawConnection> raw;
  int bad = 0;
  for (int i = 0; i < n; i += 1)
  { RawConnection r;
    bool good = false;
    if (fixed && (! clear[i] || columns.stuck(words[i], 0)))
      good = connectionFromColumns(lines[i], cut, shape, r);
    if (! good && ok[i])
    { r = parsed[i];
      good = true; }
    if (! good)
    { bad += 1;
      continue; }
    raw.push_back(r); }
  if (bad > 0)
    cerr << "Warning: " << plural(bad, "line") << " of " << filename << " could not be understood\n";

  // Intersections are numbered by their position in the intersections file,
  // which ought to start from 0, but might start from 1.  Whichever makes
  // more of the road lengths agree with the positions of the intersections
  // is right.
  long count = intersections.size();
  int fits[2] = { 0, 0 };
  for (int base = 0; base <= 1; base += 1)
    for (size_t i = 0; i < raw.size(); i += 1)
    { long a = raw[i].a - base, b = raw[i].b - base;
      if (a >= 0 && a < count && b >= 0 && b < count
          && straightLine(intersections[a], intersections[b]) <= raw[i].length * 1.1 + 0.1)
        fits[base] += 1; }
  firstNumber = fits[1] > fits[0] ? 1 : 0;
  for (size_t i = 0; i < intersections.size(); i += 1)
    intersections[i]->number = intersections[i]->index + firstNumber;

  int invalid = 0, fit = 0;
  for (size_t i = 0; i < raw.size(); i += 1)
  { Intersection * a = find(raw[i].a), * b = find(raw[i].b);
    if (a == NULL || b == NULL)
    { invalid += 1;
      continue; }
    Connection * c = new Connection;
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
         << (invalid == 1 ? " refers" : " refer") << " to intersections that do not exist\n";
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
bool shortestRoute(const RoadMap & graph, Intersection * start, Intersection * end,
                   vector<Connection *> & path, double & total)
{ int n = graph.intersections.size();
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
    Intersection * here = graph.intersections[i];
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

// every place whose name starts with the given text
vector<Place *> startingWith(const vector<Place *> & all, const string & text, const string & state)
{ vector<Place *> r;
  string start = normalise(text);
  if (start.empty())
    return r;
  for (size_t i = 0; i < all.size(); i += 1)
    if (normalise(all[i]->name).compare(0, start.size(), start) == 0
        && (state.empty() || all[i]->state == state))
      r.push_back(all[i]);
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
           << " more: give a state, or press Enter and type more of the name\n";
    string s;
    if (! readLine("Which one? Type its number or its state's two letter abbreviation (Enter to go back): ", s)
        || s.empty() || s == "q" || s == "Q")
      return NULL;
    if (isInteger(s))
    { int k = s.size() < 9 ? atoi(s.c_str()) : 0;
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
        || input.empty() || input == "q" || input == "Q")
      return NULL;
    string name = input, state;
    size_t comma = input.rfind(',');
    if (comma != string::npos)
    { name = trim(input.substr(0, comma));
      state = upperCase(trim(input.substr(comma + 1))); }
    vector<Place *> matches = inState(table.find(name), state);
    bool exactMatch = ! matches.empty();

    // perhaps it was typed as "Miami FL", without the comma
    string name2, state2;
    size_t space = input.find_last_of(" \t");
    if (comma == string::npos && space != string::npos)
    { string last = trim(input.substr(space + 1));
      if (last.size() == 2 && isalpha((unsigned char)last[0]) && isalpha((unsigned char)last[1]))
      { name2 = trim(input.substr(0, space));
        state2 = upperCase(last); } }
    if (! exactMatch && ! name2.empty())
    { matches = inState(table.find(name2), state2);
      exactMatch = ! matches.empty(); }

    // nothing with exactly that name: try names that start with it
    if (! exactMatch)
      matches = startingWith(all, name, state);
    if (matches.empty() && ! name2.empty())
      matches = startingWith(all, name2, state2);
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
  if (p->intersection < 0)
  { cout << "   it has no representative road intersection\n";
    return; }
  cout << "   its representative road intersection is #" << p->intersection;
  if (p->distance >= 0)
    cout << ", " << miles(p->distance) << " away";
  cout << "\n"; }

// shows a place and returns its representative intersection, or NULL
Intersection * goToPlace(const RoadMap & graph, const Place * p)
{ showPlace(p);
  Intersection * x = graph.find(p->intersection);
  if (x == NULL && p->intersection >= 0)
    cout << "   but there is no intersection #" << p->intersection << " in the road map\n";
  if (x == NULL)
    cout << "So there are no roads to explore from " << p->title() << ".\n";
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
         << ", " << miles(c->length) << (y == x ? " looping back here" : " heading " + direction(x, y)) << "\n";
    cout << "         to intersection #" << y->number << " at " << y->where() << ",\n";
    cout << "         " << y->nearest() << ", where " << plural(y->roads.size(), "road segment")
         << (y->roads.size() == 1 ? " ends" : " meet") << "\n"; } }

void explore(const RoadMap & graph, const PlaceTable & table, const vector<Place *> & all)
{ Intersection * here = NULL;
  while (here == NULL)
  { Place * start = choosePlace(table, all, "starting");
    if (start == NULL)
      return;
    here = goToPlace(graph, start); }
  while (true)
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
      if (s.empty())
        continue;
      if (s == "p" || s == "P")
      { Place * p = choosePlace(table, all, "next");
        if (p != NULL)
        { Intersection * x = goToPlace(graph, p);
          if (x != NULL)
            here = x; }
        break; }
      int k = isInteger(s) && s.size() < 9 ? atoi(s.c_str()) : 0;
      if (k >= 1 && k <= n)
      { Connection * c = here->roads[k - 1];
        Intersection * next = c->otherEnd(here);
        cout << "\nFollowing " << c->roadName() << " for " << miles(c->length)
             << (next == here ? " looping back" : " heading " + direction(here, next)) << " ...\n";
        here = next;
        break; }
      cout << "\"" << s << "\" is not one of the choices.\n"; } } }

// ===========================================================================
//  Assignment 4:  shortest routes
// ===========================================================================

void showRoute(const RoadMap & graph, const PlaceTable & table, const vector<Place *> & all)
{ Place * from = choosePlace(table, all, "starting");
  if (from == NULL)
    return;
  Intersection * start = graph.find(from->intersection);
  if (start == NULL)
  { cout << "Sorry, " << from->title() << " has no representative intersection in the road map.\n";
    return; }
  Place * to = choosePlace(table, all, "destination");
  if (to == NULL)
    return;
  Intersection * end = graph.find(to->intersection);
  if (end == NULL)
  { cout << "Sorry, " << to->title() << " has no representative intersection in the road map.\n";
    return; }

  cout << "\nShortest route from " << from->title() << " to " << to->title() << "\n";
  cout << "   " << from->title() << " is represented by intersection #" << start->number;
  if (from->distance >= 0)
    cout << " (" << miles(from->distance) << " away)";
  cout << "\n   " << to->title() << " is represented by intersection #" << end->number;
  if (to->distance >= 0)
    cout << " (" << miles(to->distance) << " away)";
  cout << "\n";

  vector<Connection *> path;
  double total = 0;
  if (! shortestRoute(graph, start, end, path, total))
  { cout << "\nThere is no way to get from " << from->title() << " to " << to->title()
         << " by road.\n";
    return; }

  cout << "\n";
  Intersection * here = start;
  for (size_t i = 0; i < path.size(); i += 1)
  { Connection * c = path[i];
    Intersection * next = c->otherEnd(here);
    cout << (i + 1 < 10 ? "   " : i + 1 < 100 ? "  " : i + 1 < 1000 ? " " : "") << i + 1 << ". "
         << "intersection #" << here->number << ", " << here->where() << ", " << here->nearest() << "\n";
    cout << "        take " << c->title() << " for " << miles(c->length) << " heading "
         << direction(here, next) << "\n";
    here = next; }
  size_t k = path.size() + 1;
  cout << (k < 10 ? "   " : k < 100 ? "  " : k < 1000 ? " " : "") << k << ". "
       << "intersection #" << here->number << ", " << here->where() << ", " << here->nearest() << "\n";
  cout << "        you have arrived at " << to->title() << "\n";

  // the same route in brief: consecutive segments of the same named road
  // are joined up (unnamed roads are never joined)
  if (! path.empty())
  { cout << "\nIn brief:\n";
    Intersection * a = start;
    size_t i = 0;
    while (i < path.size())
    { Intersection * b = a;
      double length = 0;
      size_t j = i;
      do
      { length += path[j]->length;
        b = path[j]->otherEnd(b);
        j += 1; }
      while (j < path.size() && path[j]->name == path[i]->name && path[i]->name != "?");
      cout << "   " << path[i]->roadName() << " heading " << direction(a, b) << " for "
           << num(length, 2) << " miles\n";
      a = b;
      i = j; } }

  cout << "\nTotal length of the " << plural(path.size(), "road segment") << " followed: "
       << num(total, 2) << " miles\n"; }

// ===========================================================================

int main(int argc, char * argv[])
{ string placesFile = argc > 1 ? argv[1] : "named-places.txt";
  string intersectionsFile = argc > 2 ? argv[2] : "intersections.txt";
  string connectionsFile = argc > 3 ? argv[3] : "connections.txt";

  RoadMap graph;
  vector<Place *> places;
  PlaceTable table;
  if (! graph.readIntersections(intersectionsFile) || ! graph.readConnections(connectionsFile)
      || ! readPlaces(placesFile, places, table))
    return 1;

  // checks that the files have been understood: named places should be
  // about as far from their representative intersection as the file says
  int noIntersection = 0, near = 0;
  for (size_t i = 0; i < places.size(); i += 1)
  { const Place * p = places[i];
    const Intersection * x = graph.find(p->intersection);
    if (x == NULL)
      noIntersection += 1;
    else if (straightLine(p->latitude, p->longitude, x->latitude, x->longitude)
             <= (p->distance < 0 ? 0 : p->distance) * 1.1 + 1)
      near += 1; }
  cout << "Read " << plural(table.size(), "named place") << ", "
       << plural(graph.intersections.size(), "intersection") << " and "
       << plural(graph.connections.size(), "road segment") << ".\n";
  cout << "Intersections are numbered from " << graph.firstNumber << ".\n";
  cout << "Checks: " << num(graph.consistency * 100, 1)
       << "% of road segment lengths agree with the positions of their ends,\n"
       << "        " << num(places.empty() ? 0 : 100.0 * near / places.size(), 1)
       << "% of named places are as close to their intersection as the file says.\n";
  if (graph.consistency < 0.95 || (! places.empty() && near < 0.95 * places.size()))
    cerr << "Warning: these checks are low, so the data files may not have been read correctly.\n";
  if (noIntersection > 0)
    cerr << "Warning: " << plural(noIntersection, "named place")
         << (noIntersection == 1 ? " has" : " have") << " no valid intersection.\n";

  while (true)
  { string choice;
    cout << "\n========================================================================\n";
    cout << "  1  explore the roads starting from a named place\n";
    cout << "  2  find the shortest route between two named places\n";
    cout << "  q  quit\n";
    if (! readLine("Your choice: ", choice) || choice == "q" || choice == "Q")
      break;
    if (choice == "1")
      explore(graph, table, places);
    else if (choice == "2")
      showRoute(graph, table, places);
    else if (! choice.empty())
      cout << "Please type 1, 2 or q.\n"; }

  for (size_t i = 0; i < places.size(); i += 1)
    delete places[i];
  return 0; }
