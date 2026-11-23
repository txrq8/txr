#!/usr/bin/env python3
"""Make synthetic named-places / intersections / connections files in the
same layout as the course files, plus a JSON file holding the true graph so
that check_roads.py can verify the program's answers independently.

usage: make_test_data.py OUTDIR [--rows R] [--cols C] [--seed S] [--places P]
           [--one-based] [--lat-first] [--no-type] [--type3] [--crlf]
           [--west] [--touching]

--touching writes every file in fixed width columns of minimal width with no
separators at all, so a value that fills its column touches its neighbour
(as the assignment 2 text warns: "you can not rely on spaces separating the
data items").
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
p.add_argument("--type3", action="store_true")
p.add_argument("--crlf", action="store_true")
p.add_argument("--west", action="store_true")
p.add_argument("--touching", action="store_true")
p.add_argument("--no-south", action="store_true")
p.add_argument("--plain-numbers", action="store_true", help="write numbers like C++ cout does (no fixed decimals), with some whole-degree coordinates")
p.add_argument("--bom", action="store_true", help="start every file with a UTF-8 byte order mark")
a = p.parse_args()
rnd = random.Random(a.seed)
os.makedirs(a.outdir, exist_ok=True)
base = 1 if a.one_based else 0
eol = "\r\n" if a.crlf else "\n"
if a.touching:
    a.type3 = True

def miles(lat1, lon1, lat2, lon2):
    dy = (lat2 - lat1) * 69.17
    dx = (lon2 - lon1) * 69.17 * math.cos(math.radians((lat1 + lat2) / 2))
    return math.hypot(dx, dy)

# ---- intersections ---------------------------------------------------------
nodes = []          # dicts lon lat dist state place
grid = {}
towns = ["Miami", "Coral Gables", "Hialeah", "Kendall", "Doral", "Homestead",
         "Springfield", "Fort Lauderdale", "Pinecrest", "Sweetwater", "St. Mary's",
         "Winston-Salem", "McAllen"]
lon0 = -120.60 if a.west else -80.60
for r in range(a.rows):
    for c in range(a.cols):
        lat = 25.40 + r * 0.02 + rnd.uniform(-0.006, 0.006)
        lon = lon0 + c * 0.02 + rnd.uniform(-0.006, 0.006)
        grid[r, c] = len(nodes)
        nodes.append(dict(lon=round(lon, 5), lat=round(lat, 5),
                          dist=round(rnd.uniform(10, 40) if (rnd.random() < 0.03 or len(nodes) % 40 == 17)
                                     else rnd.uniform(0, 3), 2),
                          state="FL", place=rnd.choice(towns)))
if a.plain_numbers:                  # some intersections exactly on a whole degree
    for k in range(0, len(nodes), 37):
        nodes[k]["lon"] = float(round(nodes[k]["lon"]))
    nodes[5]["lat"] = float(round(nodes[5]["lat"]))
    for n in nodes:                  # the truth must be what %g writes
        for key in ("lon", "lat", "dist"):
            n[key] = float("%g" % n[key])
# a separate island that no road from the mainland reaches
island = []
for k in range(6):
    island.append(len(nodes))
    nodes.append(dict(lon=round(-157.9 + k * 0.01, 5), lat=round(21.3 + (k % 2) * 0.01, 5),
                      dist=0.5, state="HI", place="Honolulu"))
# another island south of the equator (negative latitude, like American Samoa)
samoa = []
for k in range(0 if a.no_south else 4):
    samoa.append(len(nodes))
    nodes.append(dict(lon=round(-170.70 + k * 0.01, 5), lat=round(-14.28 + (k % 2) * 0.01, 5),
                      dist=1.25, state="AS", place="Pago Pago"))
# an intersection with no roads at all
lonely = len(nodes)
nodes.append(dict(lon=round(lon0 - 0.9, 5), lat=27.0, dist=4.2, state="FL", place="Nowhere Special"))

# ---- connections ------------------------------------------------------------
TYPES = {"I": "L--", "S": "T--", "U": "T--", "C": "P--", "L": "P--", "?": "P--", "F": "F--"}
edges = []          # (name, type, i, j, length)
def add(name, typ, i, j, extra=(1.0, 1.25)):
    L = miles(nodes[i]["lat"], nodes[i]["lon"], nodes[j]["lat"], nodes[j]["lon"])
    L = max(0.01, round(L * rnd.uniform(*extra), 2))
    if rnd.random() < 0.5:
        i, j = j, i
    edges.append((name, TYPES[typ] if a.type3 else typ, i, j, L))

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
    add("Old-Rd-%d" % (10 + k % 5), "L", grid[r, c], grid[r, c + 1], (1.0, 1.6))   # roads have several segments
loop = grid[min(3, a.rows - 1), min(3, a.cols - 1)]
edges.append(("Loop-Rd", TYPES["L"] if a.type3 else "L", loop, loop, 0.4))
for k in range(len(island) - 1):
    add("H-1", "I", island[k], island[k + 1])
for k in range(len(samoa) - 1):
    add("?", "F", samoa[k], samoa[k + 1])
# a few long winding roads, so that the length column has some wide values
for k in range(12):
    r, c = rnd.randrange(a.rows), rnd.randrange(a.cols - 1)
    add("SR-%d" % (900 + k), "S", grid[r, c], grid[r, c + 1], (8.0, 40.0))
rnd.shuffle(edges)

# ---- named places -----------------------------------------------------------
places = []
pool = ["Springfield", "Riverside", "Fairview", "Georgetown", "Salem", "Madison",
        "Clinton", "Franklin", "Greenville", "Bristol", "Ocean View", "Palm Bay",
        "Lake Worth Beach", "St. Petersburg", "Port St. Lucie", "Key Biscayne",
        "Township 12", "Coeur d'Alene", "Winston-Salem", "Ward 4",
        "Llanfair Pwllgwyngyll Gogerych Wyrndrobwll Llant"]          # 48 characters
states = ["FL", "GA", "AL", "TX", "NY", "CA", "OH"]

def place(code, st, name, node, pop=None, area=None):
    spread = 0.2 if (rnd.random() < 0.03 or len(places) % 60 == 7) else 0.01   # a few are far from any road
    lat = round(nodes[node]["lat"] + rnd.uniform(-spread, spread), 6)
    lon = round(nodes[node]["lon"] + rnd.uniform(-spread, spread), 6)
    d = round(miles(lat, lon, nodes[node]["lat"], nodes[node]["lon"]), 2)
    places.append(dict(code=code, state=st, name=name,
                       pop=rnd.randint(50, 9000000) if pop is None else pop,
                       area=round(rnd.uniform(0.1, 9000) if rnd.random() < 0.05 else rnd.uniform(0.1, 80), 4)
                            if area is None else area,
                       lat=lat, lon=lon, inter=node, dist=d))

for k in range(a.places):
    if k < 150:
        name, st = "Town%03d" % k, "FL"          # unique names, easy to test with
    else:
        name = rnd.choice(pool)
        if len(name) < 40 and rnd.random() < 0.4:
            name += " Heights"
        st = rnd.choice(states)
    place("%08d" % (12000000 + k), st, name, rnd.randrange(a.rows * a.cols),
          pop=0 if k % 97 == 5 else None)
# two places with the same name in the same state, and the special cases
place("12099998", "FL", "Twin City", grid[1, 1], 100, 1.0)
place("12099999", "FL", "Twin City", grid[2, 2], 200, 2.0)
place("15000001", "HI", "Island Town", island[2], 5000, 3.0)
if samoa:
    place("60000001", "AS", "Pago Pago", samoa[1], 3656, 0.6)
else:
    # the place still exists (so latitudes in named-places can be negative)
    # but it has no roads of its own: it uses an intersection in Hawaii
    places.append(dict(code="60000001", state="AS", name="Pago Pago", pop=3656, area=0.6,
                       lat=-14.279, lon=-170.700, inter=island[0],
                       dist=round(miles(-14.279, -170.700, nodes[island[0]]["lat"], nodes[island[0]]["lon"]), 2)))
place("12099997", "FL", "Lonely Place", lonely, 7, 0.5)

# ---- write the files ---------------------------------------------------------
def columns(rows, aligns):
    """minimal width columns, no separators; aligns: '<' or '>' per field"""
    widths = [max(len(r[i]) for r in rows) for i in range(len(aligns))]
    out = []
    for r in rows:
        out.append("".join(("%-*s" if al == "<" else "%*s") % (w, v) for v, w, al in zip(r, widths, aligns)).rstrip())
    return out

with open(os.path.join(a.outdir, "intersections.txt"), "w", newline="") as f:
    rows = []
    for n in nodes:
        x, y = (n["lat"], n["lon"]) if a.lat_first else (n["lon"], n["lat"])
        if a.plain_numbers:
            rows.append(["%g" % x, "%g" % y, "%g" % n["dist"], n["state"], n["place"]])
        else:
            rows.append(["%.5f" % x, "%.5f" % y, "%.2f" % n["dist"], n["state"], n["place"]])
    if a.touching:
        lines = columns(rows, ">>><<")
    elif a.plain_numbers:
        lines = [" ".join(r) for r in rows]
    else:
        lines = ["%10s %9s %7s %s %s" % tuple(r) for r in rows]
    f.write(("\ufeff" if a.bom else "") + eol.join(lines) + eol)
with open(os.path.join(a.outdir, "connections.txt"), "w", newline="") as f:
    rows = [[name, typ, str(i + base), str(j + base), "%.2f" % L] for name, typ, i, j, L in edges]
    if a.no_type:
        rows = [[r[0]] + r[2:] for r in rows]
    if a.touching:
        lines = columns(rows, "<<>>>" if not a.no_type else "<>>>")
    else:
        lines = [" ".join(r) for r in rows]
    f.write(("\ufeff" if a.bom else "") + eol.join(lines) + eol)
with open(os.path.join(a.outdir, "named-places.txt"), "w", newline="") as f:
    rows = [[q["code"], q["state"], q["name"], str(q["pop"]), "%.4f" % q["area"], "%.6f" % q["lat"],
             "%.6f" % q["lon"], str(q["inter"] + base), "%.2f" % q["dist"]] for q in places]
    if a.touching:
        for r in rows:
            r[2] = "%-48s" % r[2]                   # the name column is 48 wide
        lines = columns(rows, "<<<>>>>>>")
    else:
        lines = ["%s%s%-50s%10s%12s%12s%13s%8s%8s" % tuple(r) for r in rows]
    f.write(("\ufeff" if a.bom else "") + eol.join(lines) + eol)
with open(os.path.join(a.outdir, "truth.json"), "w") as f:
    if a.no_type:                   # the program can not know the types then
        edges = [(nm, "", i, j, L) for nm, _, i, j, L in edges]
    json.dump(dict(base=base, nodes=nodes, edges=edges, places=places,
                   island=island, lonely=lonely), f)
print("wrote %d intersections, %d connections, %d places to %s"
      % (len(nodes), len(edges), len(places), a.outdir))
