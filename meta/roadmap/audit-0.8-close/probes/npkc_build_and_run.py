import os, sys, subprocess, tempfile
ROOT = os.environ["NN"]
sys.path.insert(0, os.path.join(ROOT, "bootstrap", "harness"))
sys.path.insert(0, os.path.join(ROOT, "bootstrap", "generator"))
os.chdir(ROOT)
import harness
tmp = tempfile.mkdtemp()
# build npkrt.o
subprocess.run(["llc","-O0","-filetype=obj","-relocation-model=static",
                os.path.join(ROOT,"bootstrap","runtime","npkrt.ll"),
                "-o", os.path.join(tmp,"npkrt.o")], check=True)
# build npkc from src/main.npk
out = harness.compile_files(harness.group_for(os.path.join(ROOT,"src","main.npk")))
if out.diags:
    print("NPKC BUILD DIAG:", out.diags[0]); sys.exit(2)
base = os.path.join(tmp,"npkc")
open(base+".ll","w").write(out.ir)
subprocess.run(["llc","-O0","-filetype=obj","-relocation-model=static",base+".ll","-o",base+".o"],check=True)
subprocess.run(["ld.lld","-static","-o",base,base+".o",os.path.join(tmp,"npkrt.o")],check=True)
print("npkc built OK ->", base)
# run npkc on the limit program
prog = sys.argv[1]
r = subprocess.run([base, prog], capture_output=True, text=True)
print("=== npkc exit:", r.returncode, "===")
print("=== stderr (diagnostics) ===")
print(r.stderr.strip() or "(none)")
print("=== emitted IR: any check/branch/failsafe? ===")
ir = r.stdout
import re
# show the main function body
m = re.search(r'define .*@main\b.*?\{(.*?)\n\}', ir, re.S)
print(m.group(0) if m else "(no @main found)")
print("=== IR mentions failsafe/trap/limit check? ===")
for kw in ["failsafe","icmp","br ","r_pos","limit"]:
    print(f"  {kw!r}: {ir.count(kw)} occurrences")
