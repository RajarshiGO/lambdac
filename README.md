# lambdac

A compiler for Simply Typed Lambda Calculus, written in C++20, targeting
LLVM. It type-checks with full Hindley-Milner inference (no annotations
anywhere — the compiler figures out every type on its own), optimizes
closures at the LLVM IR level, and can either emit IR or JIT-execute your
program in-process.

This started as a learning project to go deep on compiler construction —
lexing through code generation — and ended up somewhere more interesting:
a real type inference engine, a closure optimization pass pair with a
measured performance win, and an execution path that sidesteps a genuine
limitation of how Unix processes report results.

```
let rec fact = \n. if n == 0 then 1 else n * fact (n - 1) in fact 5
```

No type annotations needed. `fact`'s type (`Int -> Int`) is inferred,
not declared.

---

## What's actually in here

- **A real lexer and recursive-descent parser**, with proper operator
  precedence (`1 + 2 * 3` parses the way you'd expect) and both `\` and
  `λ` accepted for lambda, interchangeably.
- **Full Hindley-Milner type inference** (Algorithm W), including
  let-polymorphism — `let id = \x. x in (id 42, id true)` type-checks,
  because each use of `id` gets its own fresh type variables. `let rec`
  is supported too, though recursive functions are necessarily
  monomorphic within their own body (a function can't call itself at two
  different types — that's a known, correct boundary of plain HM, not a
  bug).
- **AST-level beta reduction** with capture-avoiding substitution — the
  classic case where naive substitution silently produces the wrong
  answer is handled correctly, with alpha-renaming when a name collision
  would otherwise occur.
- **Closure conversion**, lifting every lambda to a top-level function
  plus an explicit list of what it captures from its enclosing scope.
  Recursive closures capture themselves — no special-casing needed for
  that, it falls out of the free-variable analysis naturally.
- **LLVM code generation**, with a uniform representation where every
  STLC value (an `Int`, a `Bool`, a closure) is a 64-bit word. This is
  what lets one compiled function serve every type a polymorphic
  function is used at, instead of generating a separate copy per
  call-site type.
- **A custom escape-analysis pass**, built on LLVM's `CaptureTracking`
  infrastructure, that determines which closures never outlive the
  function that created them — and a second pass that promotes those
  from heap (`malloc`) to stack (`alloca`). Measured speedup: **1.88x**,
  from a controlled benchmark (5 interleaved trials, 200K runs each,
  correctness checked before timing). Full writeup in
  [`ESCAPE_ANALYSIS.md`](./ESCAPE_ANALYSIS.md).
- **In-process JIT execution** via LLVM's ORC JIT. Compiled machine code
  is called directly through a native function pointer, rather than run
  as a subprocess — which, as a side effect, also means results aren't
  forced through a Unix process's 8-bit exit code on the way out. (More
  on why that's a real, if secondary, concern below.)
- **A REPL**, because once you have a working JIT, typing expressions
  and seeing their type and value immediately is the easiest way to
  actually use the thing.

---

## Try it

```bash
git clone https://github.com/<you>/lambdac.git
cd lambdac
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

Run a file:

```bash
echo 'let rec fact = \n. if n == 0 then 1 else n * fact (n - 1) in fact 6' > f.stlc
./stlc f.stlc
# 720
```

Or drop into the REPL:

```
$ ./stlc --repl
stlc> let x = 5 in x + 1
6 : Int
stlc> \x. x
<function> : a -> a
stlc> 2 < 3
true : Bool
stlc> :quit
```

Each REPL line is a complete, standalone program — a `let` on one line
doesn't carry over to the next. This was a deliberate scope decision:
building a GHCi-style accumulating session would mean threading a
persistent type environment and incrementally linking JIT modules
without symbol collisions, for a feature a "type something, see its
value" tool doesn't really need.

---

## The language

```ebnf
Expr       ::= LetExpr | LamExpr | IfExpr | BinOpExpr
LetExpr    ::= "let" ["rec"] Identifier "=" Expr "in" Expr
LamExpr    ::= ("\" | "λ") Identifier "." Expr
IfExpr     ::= "if" Expr "then" Expr "else" Expr
BinOpExpr  ::= AppExpr { ("+" | "-" | "*" | "/" | "==" | "<" | ">") AppExpr }
AppExpr    ::= Primary { Primary }
Primary    ::= Identifier | Integer | "true" | "false" | "(" Expr ")"
```

`if`/`then`/`else` is real grammar, not the Church-encoded function
you'll see in some STLC treatments. That's deliberate: this language is
*strict* (arguments are evaluated eagerly), and a Church-encoded `if`
is just an ordinary function call — which would evaluate both branches
unconditionally, including, for a recursive function, the branch that
recurses, regardless of whether the base case was hit. Real `if`
compiles to an actual conditional branch in LLVM IR, so only the taken
branch ever runs. Without this, `fact` above would never terminate.

Types are never written down. `Int`, `Bool`, and function types are all
inferred:

```
$ ./stlc occurs.stlc      # contents: \x. x x
occurs.stlc:1:5: type error: Occurs check failed: type variable a
occurs in a -> b -- infinite type
```

That's Algorithm W correctly rejecting a term that would otherwise
produce an infinite type (`a` unified with `a -> b` produces
`a = a -> b`, substituting into itself forever).

---

## How it fits together

```
source text
  │  Lexer
  ▼
tokens
  │  Parser
  ▼
AST  ──── type-checked by Algorithm W (validation only, doesn't rewrite the tree)
  │  Beta reduction (eliminates immediately-applied lambdas)
  ▼
AST, fewer lambda-applications
  │  Closure conversion (every lambda → top-level function + capture list)
  ▼
functions + closures, nothing LLVM-specific yet
  │  Code generation
  ▼
LLVM IR (unoptimized)
  │  escape analysis → stack promotion → mem2reg
  ▼
LLVM IR (optimized)
  │  either: print it, or JIT-execute it in-process
  ▼
a number
```

Full stage-by-stage walkthrough, with real traced output at every step
(not hand-derived — everything in there was produced by actually
running the compiler), lives in [`ARCHITECTURE.md`](./ARCHITECTURE.md).

---

## On that exit code thing

Worth explaining, since it sounds like a strange problem to go out of
your way to solve.

Unix processes report exactly one thing back to whoever started them
when they finish: an exit code, and that exit code is hard-capped at
8 bits — 0 to 255, full stop, by the OS itself. It's a common,
lightweight pattern to test small compilers and interpreters by running
the compiled output as a subprocess and checking its exit code — this
project's own test suite does exactly that for some cases, via `lli`.
The problem only shows up once a program's real answer is 256 or
higher: the exit code silently wraps (`720` becomes `208`), with no
error, no warning, nothing — so any exit-code-based test has to be
written against the wrong-looking wrapped number, or just kept under
256 and never discover the issue. (`fact 6 = 720` already crosses that
line, which is exactly how this surfaced.)

Running the compiled code in-process — calling it through a native
function pointer rather than launching it as a subprocess — sidesteps
this entirely, since the return value comes straight back from the
function call, before any process boundary forces an 8-bit ceiling onto
it. The CLI still sets a (truncated) exit code too, for anything relying
on traditional `$?`-based scripting, but the real value is always what
gets printed:

```
$ ./stlc f.stlc
720
$ echo $?
208
```

This isn't the main reason the in-process JIT path is worth having —
the bigger win is just not paying subprocess-spawn overhead (`fork`,
`exec`, a fresh address space, dynamic symbol resolution) every time you
want to run compiled code, which matters a lot for something like the
REPL. The exit code behavior is a real, correctly-handled detail that
falls out of that choice, not the reason for it.

---

## Performance

The escape analysis / stack promotion benchmark, run on a workload where
every level of a recursive loop allocates a short-lived, non-escaping
closure:

```
5 trials of 200,000 runs each (loop depth 100):

Unoptimized (heap-allocated closures): 496.02 ms median  (2.480 us/run)
Optimized   (stack-promoted closures): 264.04 ms median  (1.320 us/run)

Speedup: 1.879x
```

Both variants are checked for identical output before any timing
happens — comparing the speed of two programs that don't compute the
same thing would be meaningless. Trials are interleaved
(unoptimized → optimized → unoptimized → …) so any thermal or scheduler
drift gets spread evenly across both conditions instead of biasing
whichever runs first.

---

## Project layout

```
include/        Lexer, Parser, AST, Visitor, Type, Unifier, TypeEnv,
                TypeInfer, FreeVars, BetaReduce, ClosureConvert, CodeGen,
                Passes, and the two custom LLVM passes under passes/
src/            one .cpp per header above, plus main.cpp (the CLI)
tests/          one *_smoke.cpp per pipeline stage — plain executables,
                no test framework, each wired into CMake directly
benchmarks/     closure_alloc_bench.cpp — the measurement behind the
                1.88x figure above
```

Roughly 3,200 lines across `include/` and `src/`, and about 1,100 lines
of tests.

---

## Known limitations

Worth stating plainly rather than discovering by surprise:

- **Recursive functions are monomorphic within their own body.** A
  function can't call itself at two different types in the same
  definition — this is a correct limitation of plain Hindley-Milner
  (polymorphic recursion is undecidable to infer in general), not
  something left unfinished.
- **No loop construct**, and STLC itself has no general recursion —
  `let rec` here is a deliberate, pragmatic extension on top of the
  classical language, added specifically so recursive functions like
  `fact` could be written at all. Pure STLC is strongly normalizing —
  every well-typed program is guaranteed to terminate — and adding
  `let rec` is what trades that guarantee away in exchange for being
  able to write anything useful.
- **The REPL doesn't accumulate state across lines** — see the
  explanation above.

---

## What's next

A few optimizations were scoped out and intentionally not built, since
getting the core pipeline and the escape analysis pass right, with real
benchmarks, mattered more than breadth:

- **Lambda lifting** — closures that capture nothing don't need an
  environment at all; they could become plain top-level functions with
  zero allocation.
- **Uncurrying** — a function that's always fully applied at every call
  site (never partially applied) could be compiled as a single
  multi-argument function instead of a chain of nested closures.
- **Closure hoisting** — a closure created inside a recursive call that
  doesn't depend on anything changing between calls could be allocated
  once rather than repeatedly.

None of these have an LLVM-level equivalent that applies here — they all
depend on knowing something about closures specifically that's already
been thrown away by the time generic LLVM passes see the IR.
