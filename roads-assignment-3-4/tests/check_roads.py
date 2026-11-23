#!/usr/bin/env python3
"""Drives the roads program and checks every answer against an independent
Python implementation (its own Dijkstra, cross-checked with Bellman-Ford,
and its own direction calculation written from the assignment text).

usage: check_roads.py PROGRAM DATADIR [--routes N] [--walks N] [--steps N]
"""
import argparse, heapq, json, math, os, random, re, select, subprocess, sys

ap = argparse.ArgumentParser()
ap.add_argument("program")
ap.add_argument("datadir")
ap.add_argument("--routes", type=int, default=300)
ap.add_argument("--walks", type=int, default=20)
ap.add_argument("--steps", type=int, default=60)
ap.add_argument("--seed", type=int, default=7)
ap.add_argument("--bellman", type=int, default=5)
ap.add_argument("--assignment3", action="store_true", help="the program has no shortest route option")
args = ap.parse_args()
rnd = random.Random(args.seed)
T = json.load(open(os.path.join(args.datadir, "truth.json")))
nodes, edges, places, base = T["nodes"], T["edges"], T["places"], T["base"]
N = len(nodes)
files = [os.path.join(args.datadir, f) for f in
         ("named-places.txt", "intersections.txt", "connections.txt")]
failures = 0

def fail(msg):
    global failures
    failures += 1
    if failures <= 25:
        print("FAIL:", msg)

# ---------------------------------------------------------------- the truth
adj = [[] for _ in range(N)]          # (other, name, type, length)
for name, typ, i, j, L in edges:
    adj[i].append((j, name, typ, L))
    if j != i:
        adj[j].append((i, name, typ, L))

def dijkstra(s):
    d = [math.inf] * N
    d[s] = 0
    h = [(0, s)]
    while h:
        du, u = heapq.heappop(h)
        if du > d[u]:
            continue
        for v, _, _, L in adj[u]:
            if du + L < d[v]:
                d[v] = du + L
                heapq.heappush(h, (d[v], v))
    return d

def bellman_ford(s):
    d = [math.inf] * N
    d[s] = 0
    for _ in range(N):
        changed = False
        for _, _, i, j, L in edges:
            if d[i] + L < d[j]: d[j] = d[i] + L; changed = True
            if d[j] + L < d[i]: d[i] = d[j] + L; changed = True
        if not changed:
            break
    return d

for _ in range(args.bellman):          # make sure the reference itself is right
    s = rnd.randrange(N)
    d1, d2 = dijkstra(s), bellman_ford(s)
    if any(abs(x - y) > 1e-9 for x, y in zip(d1, d2) if x != math.inf or y != math.inf):
        fail("reference Dijkstra disagrees with Bellman-Ford")

def direction(a, b):
    """written straight from the assignment text"""
    lata, lona = math.radians(nodes[a]["lat"]), math.radians(nodes[a]["lon"])
    latb, lonb = math.radians(nodes[b]["lat"]), math.radians(nodes[b]["lon"])
    deg = math.degrees(math.atan2(lonb - lona, latb - lata))
    if 22.5 <= deg < 67.5: return "NE"
    if 67.5 <= deg < 112.5: return "E"
    if 112.5 <= deg <= 157.5: return "SE"
    if deg > 157.5 or deg < -157.5: return "S"
    if -157.5 <= deg < -112.5: return "SW"
    if -112.5 <= deg < -67.5: return "W"
    if -67.5 <= deg < -22.5: return "NW"
    return "N"

def g(x):                              # how the program prints file numbers
    s = "%.10g" % x
    return s

def miles(x):
    return "%s mile%s" % (g(x), "" if x == 1 else "s")

def nearest(n):
    v = nodes[n]
    return "%s from %s, %s" % (miles(v["dist"]), v["place"], v["state"])

def title(name, typ):
    name = "unnamed road" if name == "?" else name
    return name if typ == "" else "%s (type %s)" % (name, typ)

# ------------------------------------------------------------- the program
class Program:
    def __init__(self):
        self.p = subprocess.Popen([args.program] + files, stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=0)
    def until(self, *endings):
        buf = bytearray()
        while True:
            ready, _, _ = select.select([self.p.stdout], [], [], 30)
            if not ready:
                raise RuntimeError("no expected prompt after 30 s; output so far:\n" + buf.decode()[-1500:])
            c = self.p.stdout.read(1)
            if not c:
                raise RuntimeError("program ended; output so far:\n" + buf.decode())
            buf += c
            if any(buf.endswith(e.encode()) for e in endings):
                return buf.decode()
    def send(self, s):
        self.p.stdin.write((s + "\n").encode())
        self.p.stdin.flush()
    def close(self):
        self.p.stdin.close()
        rest = self.p.stdout.read().decode()
        err = self.p.stderr.read().decode()
        code = self.p.wait()
        return code, rest, err

MENU = "Your choice: "
PLACE = "(or just Enter to go back): "
PICK = "(Enter to go back): "
EXPLORE = "q to stop exploring: "

prog = Program()
head = prog.until(MENU)
m = re.search(r"Read (\d+) named places, (\d+) intersections and (\d+) road segments", head)
if not m or (int(m.group(1)), int(m.group(2)), int(m.group(3))) != (len(places), N, len(edges)):
    fail("load summary wrong: " + head)
if ("numbered from %d." % base) not in head:
    fail("numbering base not detected: " + head)
mc = re.search(r"Checks: (\S+)% of road segment lengths agree.*\n\s+(\S+)% of named places are as close", head)
if not mc or float(mc.group(1)) < 99.0 or float(mc.group(2)) < 99.0:
    fail("self checks too low: " + head)
if "Warning" in head or "could not" in head:
    fail("warnings while loading: " + head)
for ln in head.strip().splitlines()[:4]:
    print(ln)

# ------------------------------------------------------------ route checks
unique = [k for k, q in enumerate(places) if re.fullmatch(r"Town\d{3}", q["name"])]
line_re = re.compile(r"^\s*(\d+)\. intersection #(\d+), latitude (\S+), longitude (\S+), (.*)$")
take_re = re.compile(r"^\s+take (.*) for (\S+) miles? heading (\S+)$")

def check_route(a, b, text):
    pa, pb = places[a], places[b]
    sa, sb = pa["inter"], pb["inter"]
    d = dijkstra(sa)[sb]
    if d == math.inf:
        if "There is no way to get from" not in text:
            fail("route %s -> %s should be impossible" % (pa["name"], pb["name"]))
        return
    lines = text.splitlines()
    steps, takes = [], []
    for ln in lines:
        mm = line_re.match(ln)
        if mm: steps.append(mm)
        tt = take_re.match(ln)
        if tt: takes.append(tt)
    if not steps or len(steps) != len(takes) + 1:
        fail("route %s -> %s badly formed" % (pa["name"], pb["name"]))
        return
    path = [int(s.group(2)) - base for s in steps]
    if path[0] != sa or path[-1] != sb:
        fail("route does not start/end at the representative intersections")
    for k, s in enumerate(steps):
        n = path[k]
        if not (0 <= n < N):
            fail("bad intersection number"); return
        if int(s.group(1)) != k + 1:
            fail("step numbering")
        if float(s.group(3)) != nodes[n]["lat"] or float(s.group(4)) != nodes[n]["lon"]:
            fail("coordinates of #%d wrong" % (n + base))
        if s.group(5) != nearest(n):
            fail("description of #%d wrong: %r" % (n + base, s.group(5)))
    total = 0.0
    for k, t in enumerate(takes):
        u, v = path[k], path[k + 1]
        L = float(t.group(2))
        if not any(w == v and title(nm, ty) == t.group(1) and Lw == L for w, nm, ty, Lw in adj[u]):
            fail("step %d: no road %r of length %s from #%d to #%d" % (k + 1, t.group(1), L, u + base, v + base))
        if t.group(3) != direction(u, v):
            fail("step %d: direction %s should be %s" % (k + 1, t.group(3), direction(u, v)))
        total += L
    mt = re.search(r"Total length of the (\d+) road segments? followed: (\S+) miles", text)
    if not mt:
        fail("no total"); return
    if int(mt.group(1)) != len(takes):
        fail("segment count wrong")
    if abs(float(mt.group(2)) - total) > 0.0051:
        fail("total %s is not the sum %.4f" % (mt.group(2), total))
    if abs(float(mt.group(2)) - d) > 0.0051:
        fail("route %s -> %s: total %s but shortest is %.4f" % (pa["name"], pb["name"], mt.group(2), d))
    # the brief version must add up too
    brief = [float(x) for x in re.findall(r"^   .+? heading \S+ for (\S+) miles$", text, re.M)]
    if takes and abs(sum(brief) - total) > 0.01 * len(brief) + 0.0051:
        fail("brief summary does not add up")

def ask_route(a, b):
    prog.send("2"); prog.until(PLACE)
    prog.send("%s, %s" % (places[a]["name"], places[a]["state"])); prog.until(PLACE)
    prog.send("%s, %s" % (places[b]["name"], places[b]["state"]))
    return prog.until(MENU)

routes = 0
for _ in range(0 if args.assignment3 else args.routes):
    a, b = rnd.choice(unique), rnd.choice(unique)
    check_route(a, b, ask_route(a, b)); routes += 1
# special cases: same place, an island, a place with no roads
special = {q["name"]: k for k, q in enumerate(places)}
for a, b in [] if args.assignment3 else [(unique[0], unique[0]), (special["Island Town"], unique[1]),
             (unique[2], special["Island Town"]), (special["Lonely Place"], unique[3])]:
    check_route(a, b, ask_route(a, b)); routes += 1
print("checked %d routes" % routes)

# --------------------------------------------------------- explore checks
blk_head = re.compile(r"Intersection #(\d+)\n   latitude (\S+), longitude (\S+)\n   (.*)\n")
item_re = re.compile(r"^ +(\d+)\) (.*), (\S+) miles? (?:heading (\S+)|looping back here)\n"
                     r"\s+to intersection #(\d+) at latitude (\S+), longitude (\S+),\n"
                     r"\s+(.*), where (\d+) road segments? (?:meet|ends)$", re.M)

def check_block(text):
    h = list(blk_head.finditer(text))[-1]
    n = int(h.group(1)) - base
    v = nodes[n]
    if float(h.group(2)) != v["lat"] or float(h.group(3)) != v["lon"] or h.group(4) != nearest(n):
        fail("intersection #%d details wrong" % (n + base))
    items = list(item_re.finditer(text[h.start():]))
    shown = sorted((it.group(2), float(it.group(3)), it.group(4) or "LOOP", int(it.group(5)) - base)
                   for it in items)
    truth = sorted((title(nm, ty), L, "LOOP" if w == n else direction(n, w), w) for w, nm, ty, L in adj[n])
    if shown != truth:
        fail("roads at #%d:\n  shown %s\n  truth %s" % (n + base, shown, truth))
    for it in items:
        w = int(it.group(5)) - base
        if (float(it.group(6)), float(it.group(7))) != (nodes[w]["lat"], nodes[w]["lon"]) \
           or it.group(8) != nearest(w) or int(it.group(9)) != len(adj[w]):
            fail("description of the far end #%d wrong" % (w + base))
    return n, [int(it.group(5)) - base for it in items]

visited = 0
for _ in range(args.walks):
    k = rnd.choice(unique)
    prog.send("1"); prog.until(PLACE)
    prog.send("%s, %s" % (places[k]["name"], places[k]["state"]))
    text = prog.until(EXPLORE)
    if "representative road intersection is #%d" % (places[k]["inter"] + base) not in text:
        fail("did not go to the representative intersection")
    n, ends = check_block(text)
    if n != places[k]["inter"]:
        fail("started at the wrong intersection")
    for _ in range(args.steps):
        if not ends:
            break
        c = rnd.randrange(len(ends))
        prog.send(str(c + 1))
        text = prog.until(EXPLORE)
        n2, ends2 = check_block(text)
        if n2 != ends[c]:
            fail("moved to #%d instead of #%d" % (n2 + base, ends[c] + base))
        n, ends = n2, ends2
        visited += 1
    prog.send("q"); prog.until(MENU)
print("checked %d exploration moves" % visited)

# ------------------------------------------------- choosing places by name
def choose(inputs, expect_code):
    if not inputs:
        return
    prog.send("1"); prog.until(PLACE)
    out = ""
    for s in inputs:
        prog.send(s)
        out = prog.until(PLACE, PICK, EXPLORE)
    if not out.endswith(EXPLORE):
        fail("choosing with %r did not reach a place" % (inputs,))
        prog.send(""); prog.send(""); prog.until(MENU); return
    if expect_code is None:          # only check that some place was reached
        prog.send("q"); prog.until(MENU); return
    q = [p for p in places if p["code"] == expect_code][0]
    if "\n%s, %s\n   numeric code %s," % (q["name"], q["state"], expect_code) not in out:
        fail("choosing with %r gave the wrong place:\n%s" % (inputs, out[:300]))
    prog.send("q"); prog.until(MENU)

t0 = places[unique[0]]
choose([t0["name"].upper()], t0["code"])                         # case does not matter
choose(["  %s  " % t0["name"].lower()], t0["code"])
choose(["%s %s" % (t0["name"], t0["state"].lower())], t0["code"])  # "Name ST"
choose(["Twin City", "2"], [p for p in places if p["name"] == "Twin City"][1]["code"])
choose(["Twin City", "fl", "1"], [p for p in places if p["name"] == "Twin City"][0]["code"])
choose(["Twin", "1"], [p for p in places if p["name"] == "Twin City"][0]["code"])  # prefix
choose(["Island"], "15000001")                                    # unique prefix
choose(["nonexistent place", "Lonely Place"], "12099997")
choose(["Town00 FL", "3"], places[unique[2]]["code"])                  # prefix and state, no comma
choose(["Town01, FL", "fl", "4"], places[unique[13]]["code"])          # prefix, state again, number
longest = [p for p in places if p["name"].startswith("Llanfair")]
if longest:                      # a name that fills its whole column
    same = [p for p in longest if p["state"] == longest[0]["state"]]
    choose(["%s, %s" % (longest[0]["name"], longest[0]["state"])] + (["1"] if len(same) > 1 else []),
           same[0]["code"])
for name in ("Springfield", "Palm Bay Heights", "St. Petersburg"):
    group = [p for p in places if p["name"] == name]
    if not group:
        continue
    st = group[-1]["state"]
    same = [p for p in group if p["state"] == st]
    extra = ([st] if len(group) > 1 else []) + ([str(len(same))] if len(same) > 1 else [])
    choose([name] + extra, same[-1]["code"])
    choose(["%s, %s" % (name, st)] + ([str(len(same))] if len(same) > 1 else []), same[-1]["code"])
print("checked choosing places by name")

# invalid input in the menus must not break anything
prog.send("7"); txt = prog.until(MENU)
if ("Please type 1 or q." if args.assignment3 else "Please type 1, 2 or q.") not in txt:
    fail("menu does not reject a bad choice properly")
if args.assignment3:
    prog.send("2"); txt = prog.until(MENU)
    if "Please type 1 or q." not in txt:
        fail("assignment 3 program should not offer option 2")
prog.send("1"); prog.until(PLACE); prog.send("q"); prog.until(MENU)     # q at a place prompt goes back
prog.send("1"); prog.until(PLACE); prog.send("Twin City"); prog.until(PICK)
prog.send("4294967297"); txt = prog.until(PICK)
if "There is no number" not in txt:
    fail("place list accepted 4294967297")
prog.send("q"); prog.until(PLACE)
prog.send(""); prog.until(MENU)
prog.send("1"); prog.until(PLACE); prog.send(t0["name"] + ", " + t0["state"]); prog.until(EXPLORE)
prog.send("999"); prog.until(EXPLORE); prog.send("abc"); prog.until(EXPLORE)
for huge in ("4294967297", "-4294967295", "99999999999999999999", "0", "-1", "+1x"):
    prog.send(huge); txt = prog.until(EXPLORE)
    if "is not one of the choices" not in txt:
        fail("explore accepted %r" % huge)
prog.send("p"); prog.until(PLACE); prog.send(""); prog.until(EXPLORE)
prog.send("q"); prog.until(MENU)
prog.send("1"); prog.until(PLACE); prog.send("Lonely Place"); txt = prog.until(EXPLORE)
if "no road segments touch it" not in txt:
    fail("intersection without roads not reported")
prog.send("q"); prog.until(MENU)
code, rest, err = prog.close()     # end of input at the menu
if code != 0:
    fail("exit code %d" % code)
if err.strip():
    fail("unexpected messages on stderr: " + err)

print("\n%s: %d failures" % ("PASSED" if failures == 0 else "FAILED", failures))
sys.exit(1 if failures else 0)
