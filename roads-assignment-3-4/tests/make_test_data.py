#!/usr/bin/env python3
"""Make synthetic named-places / intersections / connections files in the
same layout as the course files, plus a JSON file holding the true graph so
that check_roads.py can verify the program's answers independently.

usage: make_test_data.py OUTDIR [--rows R] [--cols C] [--seed S]
                                [--one-based] [--lat-first] [--no-type] [--crlf]
"""
import argparse, json, math, os, random

p = argparse.ArgumentParser()
p.add_argument("outdir")
p.add_argument("--rows", type=int, default=30)
p.add_argument("--cols", type=int, default=30)
p.add_argument("--seed", type=int, default=1)
p.add_argument("--places", type=int, default=400)
p.add_argument("--one-based", action="store_true")
p.add_argument("--lat-first", action="store_true")
p.add_argument("--no-type", action="store_true")
p.add_argument("--crlf", action="store_true")
a = p.parse_args()
rnd = random.Random(a.seed)
os.makedirs(a.outdir, exist_ok=True)
base = 1 if a.one_based else 0
eol = "\r\n" if a.crlf else "\n"

def miles(p, q):
    dy = (q[1] - p[1]) * 69.17
    dx = (q[0] - p[0]) * 69.17 * math.cos(math.radians((p[1] + q[1]) / 2))
    return math.hypot(dx, dy)

# ---- intersections: (lon, lat) --------------------------------------------
nodes = []          # dicts lon lat dist state place
grid = {}
towns = ["Miami", "Coral Gables", "Hialeah", "Kendall", "Doral", "Homestead",
         "Springfield", "Fort Lauderdale", "Pinecrest", "Sweetwater"]
for r in range(a.rows):
    for c in range(a.cols):
        lat = 25.40 + r * 0.02 + rnd.uniform(-0.006, 0.006)
        lon = -80.60 + c * 0.02 + rnd.uniform(-0.006, 0.006)
        grid[r, c] = len(nodes)
        nodes.append(dict(lon=round(lon, 5), lat=round(lat, 5),
                          dist=round(rnd.uniform(0, 3), 2), state="FL",
                          place=rnd.choice(towns)))
# a separate island that no road from the mainland reaches
island = []
for k in range(6):
    island.append(len(nodes))
    nodes.append(dict(lon=round(-157.9 + k * 0.01, 5), lat=round(21.3 + (k % 2) * 0.01, 5),
                      dist=0.5, state="HI", place="Honolulu"))
# an intersection with no roads at all
lonely = len(nodes)
nodes.append(dict(lon=-81.5, lat=27.0, dist=4.2, state="FL", place="Nowhere Special"))

# ---- connections ----------------------------------------------------------
edges = []          # (name, type, i, j, length)
def add(name, typ, i, j, extra=(1.0, 1.25)):
    L = miles((nodes[i]["lon"], nodes[i]["lat"]), (nodes[j]["lon"], nodes[j]["lat"]))
    L = max(0.01, round(L * rnd.uniform(*extra), 2))
    if rnd.random() < 0.5:
        i, j = j, i
    edges.append((name, typ, i, j, L))

for r in range(a.rows):
    for c in range(a.cols):
        if c + 1 < a.cols and rnd.random() < 0.85:
            add("SR-%d" % (800 + r) if r % 5 else "I-%d" % (10 + r), "S" if r % 5 else "I",
                grid[r, c], grid[r, c + 1])
        if r + 1 < a.rows and rnd.random() < 0.85:
            add("US-%d" % (c + 1) if c % 3 else "?", "U" if c % 3 else "?",
                grid[r, c], grid[r + 1, c])
        if r + 1 < a.rows and c + 1 < a.cols and rnd.random() < 0.08:
            add("CR-%d" % rnd.randint(1, 99), "C", grid[r, c], grid[r + 1, c + 1])
# parallel duplicates with different lengths, and a self loop
for k in range(20):
    r, c = rnd.randrange(a.rows), rnd.randrange(a.cols - 1)
    add("Old-Rd-%d" % k, "L", grid[r, c], grid[r, c + 1], (1.0, 1.6))
loop = grid[min(3, a.rows - 1), min(3, a.cols - 1)]
edges.append(("Loop-Rd", "L", loop, loop, 0.4))
for k in range(len(island) - 1):
    add("H-1", "I", island[k], island[k + 1])
rnd.shuffle(edges)

# ---- named places ---------------------------------------------------------
places = []
pool = ["Springfield", "Riverside", "Fairview", "Georgetown", "Salem", "Madison",
        "Clinton", "Franklin", "Greenville", "Bristol", "Ocean View", "Palm Bay",
        "Lake Worth Beach", "St. Petersburg", "Port St. Lucie", "Key Biscayne"]
states = ["FL", "GA", "AL", "TX", "NY", "CA", "OH"]
used = set()
for k in range(a.places):
    if k < 150:
        name = "Town%03d" % k           # unique names, easy to test with
        st = "FL"
    else:
        name = rnd.choice(pool) + ("" if rnd.random() < 0.6 else " Heights")
        st = rnd.choice(states)
    node = rnd.randrange(a.rows * a.cols)
    places.append(dict(code="%08d" % (12000000 + k), state=st, name=name,
                       pop=rnd.randint(50, 900000), area=round(rnd.uniform(0.1, 80), 4),
                       lat=round(nodes[node]["lat"] + rnd.uniform(-0.01, 0.01), 6),
                       lon=round(nodes[node]["lon"] + rnd.uniform(-0.01, 0.01), 6),
                       inter=node, dist=round(rnd.uniform(0, 2), 2)))
# two places with the same name in the same state, and the special cases
places.append(dict(code="12099998", state="FL", name="Twin City", pop=100, area=1.0,
                   lat=25.5, lon=-80.5, inter=grid[1, 1], dist=0.1))
places.append(dict(code="12099999", state="FL", name="Twin City", pop=200, area=2.0,
                   lat=25.6, lon=-80.4, inter=grid[2, 2], dist=0.2))
places.append(dict(code="15000001", state="HI", name="Island Town", pop=5000, area=3.0,
                   lat=21.3, lon=-157.9, inter=island[2], dist=0.3))
places.append(dict(code="12099997", state="FL", name="Lonely Place", pop=7, area=0.5,
                   lat=27.0, lon=-81.5, inter=lonely, dist=0.0))

# ---- write the files -------------------------------------------------------
with open(os.path.join(a.outdir, "intersections.txt"), "w", newline="") as f:
    for n in nodes:
        x, y = (n["lat"], n["lon"]) if a.lat_first else (n["lon"], n["lat"])
        f.write("%10.5f %9.5f %7.2f %s %s%s" % (x, y, n["dist"], n["state"], n["place"], eol))
with open(os.path.join(a.outdir, "connections.txt"), "w", newline="") as f:
    for name, typ, i, j, L in edges:
        if a.no_type:
            f.write("%s %d %d %.2f%s" % (name, i + base, j + base, L, eol))
        else:
            f.write("%s %s %d %d %.2f%s" % (name, typ, i + base, j + base, L, eol))
with open(os.path.join(a.outdir, "named-places.txt"), "w", newline="") as f:
    for q in places:
        f.write("%s%s%-50s%10d%12.4f%12.6f%13.6f%8d%8.2f%s" % (
            q["code"], q["state"], q["name"], q["pop"], q["area"], q["lat"], q["lon"],
            q["inter"] + base, q["dist"], eol))
with open(os.path.join(a.outdir, "truth.json"), "w") as f:
    if a.no_type:                   # the program can not know the types then
        edges = [(nm, "", i, j, L) for nm, _, i, j, L in edges]
    json.dump(dict(base=base, nodes=nodes, edges=edges, places=places,
                   island=island, lonely=lonely), f)
print("wrote %d intersections, %d connections, %d places to %s"
      % (len(nodes), len(edges), len(places), a.outdir))
