# Assignments 3 and 4: road graph exploration and shortest routes

`roads.cpp` is the whole solution (assignments 3 and 4 together). It needs only standard C++11.

```
g++ -std=c++11 -O2 -Wall -o roads roads.cpp
./roads                                   # reads named-places.txt intersections.txt connections.txt
./roads named-places.txt intersections.txt connections.txt   # or name the files yourself
```

Put the three data files from the course module in the same folder, or pass their paths in that order.

## What it does

* **Assignment 3.** It builds the graph. There is one `Intersection` object per intersection, with a
  `vector<Connection *> roads` of every segment that touches it, and one `Connection` object per road
  segment, with pointers `a` and `b` to the intersections at its two ends. The named places go into a
  hash table (`PlaceTable`, chaining with FNV-1a hashing, grows itself). Menu option **1** asks for a
  place, goes to its representative intersection and lists every road segment there: the road's name
  and type, its length, the direction it heads, and the intersection at the other end (its number,
  position, nearest named place and how many roads meet there). Type a segment's number to move along
  it, `p` to jump to another place, or `q` to stop.
* **Assignment 4.** Menu option **2** asks for two places and runs Dijkstra's algorithm with a binary
  min-heap. It prints every intersection on the route, then the name, length and direction of the
  road taken to the next one, then a short summary and the total length of all the segments followed.

Places can be typed as `Miami`, `Miami, FL` or `Miami FL`. Case does not matter. If several places
share the name, they are listed and you pick one by number or by state. If no name matches exactly,
the program lists the places whose names start with what you typed.

Direction follows the assignment exactly: `atan2(lonb - lona, latb - lata)`, converted to degrees and
put into one of the 8 compass sectors.

## File layouts the program understands

* `intersections.txt`: `longitude latitude distance state place-name`, one intersection per line.
  An intersection's number is its position in the file.
* `connections.txt`: `road-name road-type intersection-A intersection-B length-in-miles`
* `named-places.txt`: `numeric-code state name population area latitude longitude
  representative-intersection distance`. The name may contain spaces.

The reader also copes with small variations: latitude before longitude, no road type, numbering from
1 instead of 0, and Windows line endings. When it starts, it reports what it read and how many road
lengths agree with the positions of their ends. That number should be close to 100%. If it is low,
the files were not understood correctly.

## Tests (`tests/`)

* `make_test_data.py OUTDIR [options]` writes synthetic data files in the same layouts, plus a
  `truth.json` holding the real graph.
* `check_roads.py ./roads OUTDIR` drives the program and checks every answer independently:
  * every route is checked against a separate Dijkstra, which is itself cross-checked with
    Bellman-Ford;
  * every step must be a real road segment with the right name, length and direction;
  * the total must be correct;
  * every intersection shown while exploring must list exactly the right road segments;
  * choosing places by name, by state and by number is tested;
  * the special cases are tested: unreachable places, a place with no roads, and the same place
    twice.
