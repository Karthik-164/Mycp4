#!/usr/bin/env python3
"""misra_report.py <raw.txt> - summarise MISRA C:2012 findings per rule and file.
Rule titles are short paraphrases (the official MISRA text is copyrighted)."""
import collections, re, sys

# rule: (category, topic, disposition)
RULES = {
 "7.1":  ("Required", "octal constants (file modes such as 0644)", "fix: use S_I* names or a named constant"),
 "8.9":  ("Advisory", "object could have block/file scope instead", "fix: move/limit scope"),
 "10.1": ("Required", "operand of inappropriate essential type", "fix: explicit casts"),
 "10.3": ("Required", "assignment to narrower/different essential type", "fix: explicit casts"),
 "10.4": ("Required", "operands of different essential type in one expression", "fix: explicit casts"),
 "10.8": ("Required", "cast widens/changes essential category of an expression", "fix: cast earlier"),
 "11.5": ("Advisory", "void* converted to object pointer", "deviation: malloc / read-write buffers"),
 "12.1": ("Advisory", "implicit operator precedence", "fix: add parentheses"),
 "12.3": ("Advisory", "comma used (multi-declarator lines)", "fix: one declaration per line"),
 "13.4": ("Advisory", "assignment used as a value (getopt/readdir/pop loops)", "fix: split statements"),
 "14.1": ("Required", "floating-point loop control variable (format_size)", "review"),
 "14.4": ("Required", "controlling expression not boolean", "fix: explicit comparison"),
 "15.1": ("Advisory", "goto used", "deviation: single 'cleanup:' label pattern"),
 "15.4": ("Advisory", "more than one break in a loop", "review"),
 "15.5": ("Advisory", "more than one exit point in a function", "deviation: goto-cleanup / early return"),
 "15.6": ("Required", "loop/if body not a compound statement", "fix: add braces"),
 "15.7": ("Required", "else-if chain without a final else", "fix: add else"),
 "17.1": ("Required", "<stdarg.h> features used", "deviation: printf-style logger"),
 "17.2": ("Required", "recursion", "deviation: directory walk; depth bounded by the file system"),
 "17.7": ("Required", "return value of a function ignored", "fix: cast to (void) or check"),
 "18.4": ("Advisory", "pointer arithmetic (+ / -)", "deviation: buffer offsets in read/write loops"),
 "19.2": ("Advisory", "union type used (glibc pthread_mutex_t)", "deviation: POSIX threads API"),
 "21.3": ("Required", "malloc/free (<stdlib.h>) used", "deviation: dynamic buffers are the point"),
 "21.6": ("Required", "standard I/O (<stdio.h>) used", "deviation: console output is a feature"),
 "21.10":("Required", "<time.h> functions used", "deviation: timestamps in log"),
 "21.16":("Required", "memcmp on buffers not of unsigned char type", "fix: use unsigned char buffers"),
}

rows = [re.match(r"(.+?):(\d+): ([\d.]+)$", l).groups() for l in open(sys.argv[1]) if l.strip()]
by_rule = collections.Counter(r for _, _, r in rows)
by_file = collections.Counter(f for f, _, _ in rows)

def key(r): return tuple(int(x) for x in r.split("."))
print(f"MISRA C:2012 (cppcheck addon) - {len(rows)} findings in project files\n")
print(f"{'Rule':<6}{'Category':<10}{'Count':>6}  Topic  ->  disposition")
print("-" * 100)
cat_total = collections.Counter()
for r in sorted(by_rule, key=key):
    cat, topic, disp = RULES.get(r, ("?", "(no description)", "review"))
    cat_total[cat] += by_rule[r]
    print(f"{r:<6}{cat:<10}{by_rule[r]:>6}  {topic}  ->  {disp}")
print("-" * 100)
print("By category: " + ", ".join(f"{k}: {v}" for k, v in cat_total.items()))
fix = sum(by_rule[r] for r in by_rule if RULES.get(r, ("", "", ""))[2].startswith("fix"))
dev = sum(by_rule[r] for r in by_rule if RULES.get(r, ("", "", ""))[2].startswith("deviation"))
print(f"Mechanically fixable: {fix}   Justified deviations (documented): {dev}   Other: {len(rows)-fix-dev}")
print("\nPer file:")
for f, n in sorted(by_file.items()):
    print(f"  {f:<22}{n:>4}")
