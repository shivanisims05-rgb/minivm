#!/usr/bin/env python3
"""Local web UI for mbc.   Run:  python3 server.py   then open http://localhost:8000

Only the Python standard library is used. Every number, token, tree and
instruction shown in the browser comes from the real ./mbc binary; this file
just calls it and packages the output as JSON.
"""
import json, os, re, struct, subprocess, sys, tempfile
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = os.path.dirname(os.path.abspath(__file__))
MBC = os.path.join(ROOT, "mbc")
EX = os.path.join(ROOT, "examples")
PORT = int(os.environ.get("PORT", "8000"))
MAX_NAME = 64            # keep in sync with src/common.h
TRACE_LIMIT = 30000          # do not trace programs that execute more instructions than this

GROUPS = [
    ("Basics", ["hello", "expr", "demo", "factorial", "fibonacci", "gcd", "sum", "primes", "compare", "scopes", "calls", "recursion"]),
    ("New language features", ["modulo", "logic", "loops", "breaks", "deadcode", "showcase"]),
    ("Benchmarks (large)", ["bench_fib", "bench_loop", "bench_perfect"]),
]


def run(args, timeout=30):
    try:
        p = subprocess.run([MBC] + args, capture_output=True, text=True, timeout=timeout)
        return {"out": p.stdout, "err": p.stderr, "rc": p.returncode}
    except subprocess.TimeoutExpired:
        return {"out": "", "err": "timed out", "rc": -1}


def examples():
    present = {f[:-3] for f in os.listdir(EX) if f.endswith(".mc")}
    out, seen = [], set()
    for group, names in GROUPS:
        for n in names:
            if n in present:
                out.append({"name": n, "group": group}); seen.add(n)
    for n in sorted(present - seen):
        out.append({"name": n, "group": "Other"})
    return out


def write_source(d, source):
    path = os.path.join(d, "prog.mc")
    with open(path, "w") as f:
        f.write(source)
    return path


def clean(msg, path):
    return msg.replace(path + ":", "line ").replace(path, "prog.mc").strip()


def stat(s, key):
    m = re.search(key + r": (\d+)", s)
    return int(m.group(1)) if m else None


def analyse(source, opt):
    with tempfile.TemporaryDirectory() as d:
        path = write_source(d, source)
        res = {}
        # compile-time phases first: if one fails, report it and stop
        for key, cmd in (("tokens", ["tokens"]), ("ast", ["ast"]), ("ast_opt", ["ast", "-O"]),
                         ("bytecode_plain", ["dis"]), ("bytecode_opt", ["dis", "-O"])):
            r = run(cmd + [path])
            if r["rc"] != 0:
                phase = {"tokens": "tokens", "ast": "ast", "ast_opt": "ast",
                         "bytecode_plain": "bytecode", "bytecode_opt": "bytecode"}[key]
                res["error"] = {"phase": phase, "message": clean(r["err"] or r["out"], path)}
                return res
            res[key] = r["out"]

        flags = ["-O"] if opt else []
        res["opt"] = bool(opt)
        res["bytecode"] = res["bytecode_opt"] if opt else res["bytecode_plain"]

        plain = run(["run", path, "--stats"])
        folded = run(["run", path, "--stats", "-O"])
        res["output_plain"], res["stats_plain"] = plain["out"], plain["err"].strip()
        res["output_opt"], res["stats_opt"] = folded["out"], folded["err"].strip()
        res["rc_plain"], res["rc_opt"] = plain["rc"], folded["rc"]
        sel = folded if opt else plain
        res["output"] = sel["out"]
        res["stats"] = sel["err"].strip()
        if sel["rc"] != 0:
            res["runtime_error"] = clean(sel["err"], path)

        n_instr = stat(res["stats"], "instructions executed")
        if n_instr is None:
            # the run failed before the stats line; fall back to a bounded trace
            n_instr = 0
        if n_instr <= TRACE_LIMIT:
            res["trace"] = run(["run", path, "--trace"] + flags)["out"]
        else:
            res["trace"] = ""
            res["trace_skipped"] = n_instr

        it = run(["interp", path])
        res["interp"] = it["out"]
        res["rc_interp"] = it["rc"]
        res["interp_error"] = clean(it["err"], path) if it["rc"] != 0 else ""
        res["match_interp"] = (it["out"] == plain["out"]) and ((it["rc"] != 0) == (plain["rc"] != 0))
        res["match_opt"] = (plain["out"] == folded["out"]) and ((plain["rc"] != 0) == (folded["rc"] != 0))
        return res


def field_map(raw):
    """Describe the .mbc layout so the page can colour the hex dump."""
    fields = []
    if len(raw) < 12:
        return fields
    n = struct.unpack_from("<i", raw, 4)[0]
    fields.append({"name": "magic", "start": 0, "end": 4})
    fields.append({"name": "code length", "start": 4, "end": 8})
    code_end = 8 + 4 * n
    fields.append({"name": "code", "start": 8, "end": code_end})
    if len(raw) >= code_end + 4:
        nf = struct.unpack_from("<i", raw, code_end)[0]
        fields.append({"name": "function count", "start": code_end, "end": code_end + 4})
        pos = code_end + 4
        entry = MAX_NAME + 4  # name field + address (see bc_save in src/bytecode.c)
        for i in range(max(0, min(nf, 64))):
            fields.append({"name": "function table", "start": pos, "end": min(pos + entry, len(raw))})
            pos += entry
    return fields


def mbc_roundtrip(source, opt, mode):
    with tempfile.TemporaryDirectory() as d:
        path = write_source(d, source)
        out = os.path.join(d, "prog.mbc")
        c = run(["compile", path, "-o", out] + (["-O"] if opt else []))
        if c["rc"] != 0:
            return {"error": clean(c["err"] or c["out"], path)}
        with open(out, "rb") as f:
            raw = bytearray(f.read())
        res = {"size": len(raw), "compile_msg": c["out"].replace(out, "prog.mbc").strip()}
        res["fields"] = field_map(bytes(raw))
        note = ""
        if mode == "opcode":
            struct.pack_into("<i", raw, 8, 77)
            note = "The first instruction word was overwritten with 77, which is not a valid opcode."
        elif mode == "truncate":
            raw = raw[: len(raw) // 2]
            note = "The file was cut in half."
        elif mode == "magic":
            raw[0:4] = b"XXXX"
            note = "The magic number MBC1 was replaced with XXXX."
        with open(out, "wb") as f:
            f.write(raw)
        res["note"] = note
        res["bytes"] = list(raw[:640])
        res["truncated_dump"] = len(raw) > 640
        res["mode"] = mode
        e = run(["exec", out, "--stats"])
        res["exec_out"] = e["out"]
        res["exec_err"] = e["err"].replace(out, "prog.mbc").strip()
        res["exec_rc"] = e["rc"]
        return res


def parse_bench(text):
    r = {}
    m = re.search(r"bytecode VM\s*:\s*([\d.]+) s per run\s*\((\d+) instructions\)", text)
    if m:
        r["vm_s"], r["instructions"] = float(m.group(1)), int(m.group(2))
    m = re.search(r"tree-walking interp\s*:\s*([\d.]+) s per run\s*\((\d+) function calls\)", text)
    if m:
        r["interp_s"], r["calls"] = float(m.group(1)), int(m.group(2))
    return r


def bench(source, opt, n):
    with tempfile.TemporaryDirectory() as d:
        path = write_source(d, source)
        flags = ["-O"] if opt else []
        probe = run(["bench", path, "-n", "1"] + flags, timeout=60)
        if probe["rc"] != 0:
            return {"error": clean(probe["err"] or probe["out"], path)}
        p = parse_bench(probe["out"])
        if "vm_s" not in p:
            return {"error": "could not read benchmark output"}
        slow = max(p["vm_s"], p.get("interp_s", 0))
        reps = n if n else (1 if slow > 1 else 3 if slow > 0.2 else 20 if slow > 0.01 else 400)
        r = run(["bench", path, "-n", str(reps)] + flags, timeout=120)
        q = parse_bench(r["out"])
        q["reps"] = reps
        q["text"] = r["out"].replace(path, "prog.mc").strip()
        return q


def run_tests():
    p = subprocess.run(["sh", os.path.join(ROOT, "tests", "run_tests.sh")], capture_output=True, text=True,
                       timeout=300, cwd=ROOT)
    section, items = "", []
    passed = failed = 0
    for line in p.stdout.splitlines():
        m = re.match(r"== (.*) ==", line)
        if m:
            section = m.group(1)
            continue
        m = re.match(r"\s+(ok|FAIL)\s+(.*)", line)
        if m:
            items.append({"section": section, "ok": m.group(1) == "ok", "name": m.group(2).strip()})
            continue
        m = re.match(r"passed: (\d+)\s+failed: (\d+)", line)
        if m:
            passed, failed = int(m.group(1)), int(m.group(2))
    return {"items": items, "passed": passed, "failed": failed}


def rand_expr(rng, depth, vars_):
    if depth <= 0 or rng.random() < 0.25:
        if vars_ and rng.random() < 0.5:
            return rng.choice(vars_)
        return str(rng.randint(-9, 20))
    k = rng.random()
    a, b = rand_expr(rng, depth - 1, vars_), rand_expr(rng, depth - 1, vars_)
    if k < 0.12:
        return "!(%s)" % a
    if k < 0.2:
        return "-(%s)" % a
    op = rng.choice(["+", "-", "*", "/", "%", "<", ">", "<=", ">=", "==", "!=", "&&", "||"])
    return "(%s %s %s)" % (a, op, b)


def rand_program(rng):
    lines = ["int main() {", "    int a = %d;" % rng.randint(-5, 9), "    int b = %d;" % rng.randint(1, 9)]
    for _ in range(rng.randint(2, 5)):
        k = rng.random()
        e = rand_expr(rng, 3, ["a", "b"])
        if k < 0.4:
            lines.append("    print(%s);" % e)
        elif k < 0.6:
            lines.append("    a = %s;" % e)
        elif k < 0.8:
            lines.append("    if (%s) { print(1); } else { print(0); }" % e)
        else:
            lines.append("    for (int i = 0; i < %d; i = i + 1) { b = b + %s; }" % (rng.randint(1, 6), rand_expr(rng, 2, ["a", "i"])))
    lines += ["    print(a);", "    print(b);", "    return 0;", "}"]
    return "\n".join(lines) + "\n"


def fuzz(n_prog=60, n_files=60):
    import random
    rng = random.Random()
    mism = 0
    first = ""
    with tempfile.TemporaryDirectory() as d:
        path = os.path.join(d, "r.mc")
        for _ in range(n_prog):
            src = rand_program(rng)
            with open(path, "w") as f:
                f.write(src)
            a, b, c = run(["run", path]), run(["run", path, "-O"]), run(["interp", path])
            same = a["out"] == b["out"] == c["out"] and (a["rc"] != 0) == (b["rc"] != 0) == (c["rc"] != 0)
            if not same:
                mism += 1
                first = first or src
        # random bytecode files: the VM must reject or run them, never crash
        crashes = 0
        good = os.path.join(d, "g.mbc")
        with open(path, "w") as f:
            f.write("int main() { print(1 + 2); return 0; }\n")
        run(["compile", path, "-o", good])
        with open(good, "rb") as f:
            base = bytearray(f.read())
        bad = os.path.join(d, "b.mbc")
        for _ in range(n_files):
            raw = bytearray(base)
            for _ in range(rng.randint(1, 6)):
                raw[rng.randrange(len(raw))] = rng.randrange(256)
            if rng.random() < 0.3:
                raw = raw[: rng.randrange(1, len(raw))]
            with open(bad, "wb") as f:
                f.write(raw)
            try:
                p = subprocess.run([MBC, "exec", bad], capture_output=True, timeout=10)
                if p.returncode < 0 or p.returncode > 100:
                    crashes += 1
            except subprocess.TimeoutExpired:
                pass  # an infinite loop in a mutated program is legal, not a crash
    return {"programs": n_prog, "mismatches": mism, "files": n_files, "crashes": crashes, "example": first}


class H(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def send(self, code, body, ctype="application/json"):
        data = body if isinstance(body, bytes) else body.encode()
        self.send_response(code)
        self.send_header("Content-Type", ctype + "; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        if self.path in ("/", "/index.html"):
            with open(os.path.join(ROOT, "web", "index.html"), "rb") as f:
                return self.send(200, f.read(), "text/html")
        if self.path == "/api/examples":
            return self.send(200, json.dumps(examples()))
        if self.path.startswith("/api/example/"):
            name = os.path.basename(self.path[len("/api/example/"):])
            p = os.path.join(EX, name + ".mc")
            if os.path.isfile(p):
                with open(p) as f:
                    return self.send(200, f.read(), "text/plain")
        self.send(404, "not found", "text/plain")

    def do_POST(self):
        n = int(self.headers.get("Content-Length", 0))
        if n > 200000:
            return self.send(413, "too big", "text/plain")
        try:
            req = json.loads(self.rfile.read(n) or b"{}")
            src, opt = req.get("source", ""), bool(req.get("opt"))
            if self.path == "/api/run":
                return self.send(200, json.dumps(analyse(src, opt)))
            if self.path == "/api/mbc":
                mode = req.get("mode", "none")
                mode = mode if mode in ("none", "opcode", "truncate", "magic") else "none"
                return self.send(200, json.dumps(mbc_roundtrip(src, opt, mode)))
            if self.path == "/api/bench":
                return self.send(200, json.dumps(bench(src, opt, int(req.get("n") or 0))))
            if self.path == "/api/fuzz":
                return self.send(200, json.dumps(fuzz()))
            if self.path == "/api/tests":
                return self.send(200, json.dumps(run_tests()))
        except Exception as e:  # keep the demo alive whatever the input
            return self.send(500, json.dumps({"error": "server error: %s" % e}))
        self.send(404, "not found", "text/plain")


if __name__ == "__main__":
    if not os.path.exists(MBC):
        sys.exit("build first:  make")
    print(f"mbc web demo running at http://localhost:{PORT}   (Ctrl+C to stop)")
    try:
        ThreadingHTTPServer(("127.0.0.1", PORT), H).serve_forever()
    except KeyboardInterrupt:
        print()
