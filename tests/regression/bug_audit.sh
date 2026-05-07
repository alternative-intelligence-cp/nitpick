#!/bin/bash
# v0.16.0-1 Bug Audit — Test all 13 active bugs from META/ARIA/BUGS
cd /home/randy/Workspace/REPOS/aria
NPKC=build/npkc
DIR=tests/regression
mkdir -p "$DIR"

pass=0; fail=0; skip=0

test_compile_run() {
    local name="$1" file="$2"
    rm -f a.out
    local cout
    cout=$($NPKC "$file" -o a.out 2>&1)
    local crc=$?
    local errors=$(echo "$cout" | grep -c "error:")
    
    if [[ $errors -gt 0 ]]; then
        echo "  COMPILE_ERROR ($errors errors)"
        return 2
    fi
    
    if [[ ! -f a.out ]]; then
        echo "  NO_BINARY"
        return 3
    fi
    
    local rout
    rout=$(timeout 5 ./a.out 2>&1)
    local rrc=$?
    
    if [[ $rrc -eq 139 ]] || [[ $rrc -eq 134 ]] || [[ $rrc -eq 136 ]]; then
        echo "  CRASH (signal $((rrc-128)))"
        return 1
    fi
    
    echo "  EXIT=$rrc"
    return $rrc
}

echo "================================================================"
echo "B1: Non-pub helper from pub function"
echo "================================================================"
cat > "$DIR/b1_test.npk" << 'EOF'
func:helper = int64() { pass 42; };
pub func:caller = int64() { int64:x = raw helper(); pass x; };
func:main = int32() { int64:val = raw caller(); exit 0; };
func:failsafe = int32(tbb32:err) { exit 1; };
EOF
test_compile_run "B1" "$DIR/b1_test.npk"
b1=$?

echo "================================================================"
echo "B2: Large function many if-blocks (20 vars, 20 ifs)"
echo "================================================================"
cat > "$DIR/b2_test.npk" << 'ENDTEST'
func:main = int32() {
    int64:a=1; int64:b=2; int64:c=3; int64:d=4; int64:e=5;
    int64:f=6; int64:g=7; int64:h=8; int64:i=9; int64:j=10;
    int64:k=11; int64:l=12; int64:m=13; int64:n=14; int64:o=15;
    int64:p=16; int64:q=17; int64:r=18; int64:s=19; int64:t=20;
    int64:result=0;
    if (a>0) { result=a+b; }
    if (b>0) { result=result+c; }
    if (c>0) { result=result+d; }
    if (d>0) { result=result+e; }
    if (e>0) { result=result+f; }
    if (f>0) { result=result+g; }
    if (g>0) { result=result+h; }
    if (h>0) { result=result+i; }
    if (i>0) { result=result+j; }
    if (j>0) { result=result+k; }
    if (k>0) { result=result+l; }
    if (l>0) { result=result+m; }
    if (m>0) { result=result+n; }
    if (n>0) { result=result+o; }
    if (o>0) { result=result+p; }
    if (p>0) { result=result+q; }
    if (q>0) { result=result+r; }
    if (r>0) { result=result+s; }
    if (s>0) { result=result+t; }
    if (t>0) { result=result+a; }
    exit 0;
};
func:failsafe = int32(tbb32:err) { exit 1; };
ENDTEST
test_compile_run "B2" "$DIR/b2_test.npk"
b2=$?

echo "================================================================"
echo "B3: Nested module calls (pub→pub cross-module) GC OOM"
echo "================================================================"
cat > "$DIR/b3_mod.npk" << 'EOF'
pub func:mod_func = int64(int64:x) { pass x + 1; };
EOF
cat > "$DIR/b3_test.npk" << 'EOF'
use "tests/regression/b3_mod.npk".*;
func:main = int32() { int64:val = 10; int64:r = raw mod_func(val); exit 0; };
func:failsafe = int32(tbb32:err) { exit 1; };
EOF
test_compile_run "B3" "$DIR/b3_test.npk"
b3=$?

echo "================================================================"
echo "B4: ahget missing key segfault"
echo "================================================================"
cat > "$DIR/b4_test.npk" << 'EOF'
func:failsafe = int32(tbb32:err) { exit 1; };
func:main = int32() {
    int64:ht = ahash(4096);
    ahset(ht, "exists", 100i64);
    int64:found = ahget(ht, "exists");
    int64:missing = ahget(ht, "nonexistent");
    exit 0;
};
EOF
test_compile_run "B4" "$DIR/b4_test.npk"
b4=$?

echo "================================================================"
echo "B5: Extern pointer return corrupts struct fields"
echo "================================================================"
cat > "$DIR/b5_test.npk" << 'EOF'
extern func:malloc = wild int8*(int64:size);
extern func:free = void(wild int8*:ptr);
func:main = int32() {
    int64:sz = 64;
    wild int8->:ptr = malloc(sz);
    free(ptr);
    exit 0;
};
func:failsafe = int32(tbb32:err) { exit 1; };
EOF
test_compile_run "B5" "$DIR/b5_test.npk"
b5=$?

echo "================================================================"
echo "B8: Variables before loop() inaccessible after"
echo "================================================================"
cat > "$DIR/b8_test.npk" << 'EOF'
func:main = int32() {
    int64:x = 10;
    loop(0, 3, 1) {
        x = x + 1;
    }
    if (x > 10) {
        exit 0;
    }
    exit 1;
};
func:failsafe = int32(tbb32:err) { exit 1; };
EOF
test_compile_run "B8" "$DIR/b8_test.npk"
b8=$?

echo "================================================================"
echo "B10: @cast on local func result produces garbage"
echo "================================================================"
cat > "$DIR/b10_test.npk" << 'EOF'
func:get_num = int64() { pass 100; };
func:main = int32() {
    int64:tmp = raw get_num();
    int64:casted = @cast<int64>(tmp);
    if (casted == 100i64) { exit 0; }
    exit 1;
};
func:failsafe = int32(tbb32:err) { exit 1; };
EOF
test_compile_run "B10" "$DIR/b10_test.npk"
b10=$?

echo "================================================================"
echo "B12: fixed with arithmetic evaluates to 0"
echo "================================================================"
cat > "$DIR/b12_test.npk" << 'EOF'
fixed int64:MY_CONST = 5 + 3;
func:main = int32() {
    if (MY_CONST == 8i64) { exit 0; }
    exit 1;
};
func:failsafe = int32(tbb32:err) { exit 1; };
EOF
test_compile_run "B12" "$DIR/b12_test.npk"
b12=$?

echo "================================================================"
echo "B13: Negative constants imported via use are zeroed"
echo "================================================================"
cat > "$DIR/b13_mod.npk" << 'EOF'
pub func:get_neg_val = int64() { int64:v = 0 - 42; pass v; };
EOF
cat > "$DIR/b13_test.npk" << 'EOF'
use "tests/regression/b13_mod.npk".*;
func:main = int32() {
    int64:v = raw get_neg_val();
    int64:expected = 0 - 42;
    if (v == expected) { exit 0; }
    exit 1;
};
func:failsafe = int32(tbb32:err) { exit 1; };
EOF
test_compile_run "B13" "$DIR/b13_test.npk"
b13=$?

echo ""
echo "================================================================"
echo "SUMMARY"
echo "================================================================"
for v in b1 b2 b3 b4 b5 b8 b10 b12 b13; do
    val=${!v}
    status="UNKNOWN"
    [[ $val -eq 0 ]] && status="APPEARS FIXED (exit 0)"
    [[ $val -eq 1 ]] && status="STILL BROKEN (exit 1 / failsafe)"
    [[ $val -eq 2 ]] && status="COMPILE ERROR"
    [[ $val -eq 3 ]] && status="NO BINARY"
    printf "%-5s: %s\n" "$v" "$status"
done
echo ""
echo "(B6 lambda capture, B7 ahset overflow, B9 pipeline, B11 @func_name"
echo " require more nuanced tests — need manual investigation)"

rm -f a.out
