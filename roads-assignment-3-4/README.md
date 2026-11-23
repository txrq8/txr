# Assignments 3 and 4: road network graph and shortest routes

| File | What it is |
| --- | --- |
| `assignment3.cpp` | Assignment 3 on its own: builds the graph and the named-places hash table, then lets you explore the roads from a named place. |
| `assignment4.cpp` | The whole thing: everything in assignment 3, plus shortest routes between two named places with Dijkstra's algorithm. |
| `Assignment_3_Build_the_Graph.pdf` | Write-up for assignment 3: design, sample session, testing, and the full source code. |
| `Assignment_4_Shortest_Routes.pdf` | Write-up for assignment 4: the same, with the shortest-route part added. |
| `tests/` | The test tools used to verify both programs. |

`assignment3.cpp` is exactly `assignment4.cpp` without the priority queue, Dijkstra's algorithm, the route printing and menu option 2.

## Compiling and running

```
g++ -std=c++11 -O2 -Wall -o assignment4 assignment4.cpp
./assignment4                                                 # reads named-places.txt intersections.txt connections.txt
./assignment4 named-places.txt intersections.txt connections.txt   # or name the three files
```

`assignment3.cpp` is built and run the same way. Both use only standard C++. They compile with no warnings as C++98, C++11, C++17 and C++20 with g++ and clang++.

## Results on the course data files

* Every value of all three files is read exactly: 25,376 named places, 29,140 intersections and 47,003 road segments. Each was compared with an independent reader that uses the exact column positions.
* 1,002 shortest routes and 4,600 exploration steps were checked against an independent implementation. All are correct.
* Routes come out close to Google Maps, slightly shorter, because they run between representative intersections and the data has only main roads:

  | Route | Program | Google Maps (approx.) |
  | --- | --- | --- |
  | Miami → Orlando | 219.9 | 235 |
  | New York → Philadelphia | 87.2 | 95 |
  | Chicago → Milwaukee | 86.2 | 92 |
  | Houston → Dallas | 234.0 | 239 |
  | Atlanta → Nashville | 229.2 | 250 |
  | Los Angeles → San Francisco | 355.4 | 382 |

* 18 places in Puerto Rico point to intersections 29140–29145, which are not in the intersections file. The program reports them as having no valid intersection.

## Data files

* `named-places.txt` holds, in order: numeric code, state, name, population, area, latitude, longitude, representative intersection, and the distance to it.
* `intersections.txt` holds, in order: longitude, latitude, distance to the nearest named place, its state, and its name. An intersection's number is its line number.
* `connections.txt` holds, in order: road name (`?` if unnamed), road type, the two intersection numbers, and the length in miles.

The files are written in fixed-width columns, and a value that fills its column can touch the one before it, as in
`35.449578-100.225838`. The programs separate such values, and learn the column positions from the lines where the
fields are clearly separated.

At start-up the program prints two self-checks:

* how many road lengths agree with the positions of their ends;
* how many named places are near the intersection the file gives them.

Both should be near 100%. If either is lower, the program prints a warning that the files may not have been read correctly.

## Tests

* `python3 tests/make_test_data.py DIR [options]` writes data files in the course layout, together with the true graph. Options produce variations: `--touching` for fixed columns with touching fields, `--one-based`, `--lat-first`, `--no-type`, `--type3`, `--crlf`, `--plain-numbers`, `--bom`, `--west` and `--no-south`.
* `python3 tests/check_roads.py ./assignment4 DIR` drives the program through its menus. It checks every route, every exploration step and the place selection against an independent implementation. Add `--assignment3` when testing `assignment3`.
