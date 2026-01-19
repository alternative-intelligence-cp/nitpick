#!/bin/bash
# Build script for aria_utils examples

set -e

EXAMPLES_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$EXAMPLES_DIR/../src"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_usage() {
    echo "Usage: ./build.sh <utility|all|clean>"
    echo ""
    echo "Available utilities:"
    echo "  aglob    - Glob pattern matching demo"
    echo "  als      - Directory listing demo"
    echo "  aps      - Process status demo"
    echo "  acp      - File copy demo"
    echo "  amv      - File move demo"
    echo "  depgraph - Dependency graph demo"
    echo "  abc      - Build config parser demo"
    echo "  acat     - File concatenation demo"
    echo "  acurl    - HTTP client demo"
    echo "  adap     - Debug Adapter Protocol demo"
    echo "  astat    - File statistics demo"
    echo "  agrep    - Pattern search demo"
    echo "  atar     - Archive utilities demo"
    echo "  atest    - Test framework demo"
    echo "  ajq      - JSON query demo"
    echo "  all      - Build all examples"
    echo "  clean    - Remove all built examples"
}

build_aglob() {
    echo -e "${YELLOW}Building aglob demo...${NC}"
    g++ -std=c++17 \
        aglob_demo.cpp \
        -I"$SRC_DIR/aglob/include" \
        -L"$SRC_DIR/aglob/build" \
        -Wl,-rpath,"$SRC_DIR/aglob/build" \
        -laglob \
        -lstdc++fs \
        -o aglob_demo
    echo -e "${GREEN}✓ Built aglob_demo${NC}"
}

build_als() {
    echo -e "${YELLOW}Building als demo...${NC}"
    g++ -std=c++17 \
        als_demo.cpp \
        -I"$SRC_DIR/als/include" \
        -L"$SRC_DIR/als/build" \
        -Wl,-rpath,"$SRC_DIR/als/build" \
        -lals \
        -lstdc++fs \
        -o als_demo
    echo -e "${GREEN}✓ Built als_demo${NC}"
}

build_aps() {
    echo -e "${YELLOW}Building aps demo...${NC}"
    g++ -std=c++17 \
        aps_demo.cpp \
        -I"$SRC_DIR/aps/include" \
        -L"$SRC_DIR/aps/build" \
        -Wl,-rpath,"$SRC_DIR/aps/build" \
        -laps \
        -o aps_demo
    echo -e "${GREEN}✓ Built aps_demo${NC}"
}

build_acp() {
    echo -e "${YELLOW}Building acp demo...${NC}"
    g++ -std=c++17 \
        acp_demo.cpp \
        -I"$SRC_DIR/acp/include" \
        -L"$SRC_DIR/acp/build" \
        -Wl,-rpath,"$SRC_DIR/acp/build" \
        -lacp \
        -lstdc++fs \
        -o acp_demo
    echo -e "${GREEN}✓ Built acp_demo${NC}"
}

build_amv() {
    echo -e "${YELLOW}Building amv demo...${NC}"
    g++ -std=c++17 \
        amv_demo.cpp \
        -I"$SRC_DIR/amv/include" \
        -L"$SRC_DIR/amv/build" \
        -Wl,-rpath,"$SRC_DIR/amv/build" \
        -lamv \
        -lstdc++fs \
        -o amv_demo
    echo -e "${GREEN}✓ Built amv_demo${NC}"
}

build_depgraph() {
    echo -e "${YELLOW}Building depgraph demo...${NC}"
    g++ -std=c++17 \
        depgraph_demo.cpp \
        -I"$SRC_DIR/depgraph/include" \
        -L"$SRC_DIR/depgraph/build" \
        -Wl,-rpath,"$SRC_DIR/depgraph/build" \
        -ldepgraph \
        -lpthread \
        -o depgraph_demo
    echo -e "${GREEN}✓ Built depgraph_demo${NC}"
}

build_abc() {
    echo -e "${YELLOW}Building abc demo...${NC}"
    g++ -std=c++17 \
        abc_demo.cpp \
        -I"$SRC_DIR/abc/include" \
        -L"$SRC_DIR/abc/build" \
        -Wl,-rpath,"$SRC_DIR/abc/build" \
        -labc \
        -o abc_demo
    echo -e "${GREEN}✓ Built abc_demo${NC}"
}

build_acat() {
    echo -e "${YELLOW}Building acat demo...${NC}"
    g++ -std=c++17 \
        acat_demo.cpp \
        -I"$SRC_DIR/acat/include" \
        -L"$SRC_DIR/acat/build" \
        -Wl,-rpath,"$SRC_DIR/acat/build" \
        -lacat \
        -o acat_demo
    echo -e "${GREEN}✓ Built acat_demo${NC}"
}

build_acurl() {
    echo -e "${YELLOW}Building acurl demo...${NC}"
    g++ -std=c++17 \
        acurl_demo.cpp \
        -I"$SRC_DIR/acurl/include" \
        -L"$SRC_DIR/acurl/build" \
        -Wl,-rpath,"$SRC_DIR/acurl/build" \
        -lacurl \
        -lcurl \
        -o acurl_demo
    echo -e "${GREEN}✓ Built acurl_demo${NC}"
}

build_adap() {
    echo -e "${YELLOW}Building adap demo...${NC}"
    g++ -std=c++17 \
        adap_demo.cpp \
        -I"$SRC_DIR/adap/include" \
        -L"$SRC_DIR/adap/build" \
        -Wl,-rpath,"$SRC_DIR/adap/build" \
        -ladap \
        -o adap_demo
    echo -e "${GREEN}✓ Built adap_demo${NC}"
}

build_astat() {
    echo -e "${YELLOW}Building astat demo...${NC}"
    g++ -std=c++17 \
        astat_demo.cpp \
        "$SRC_DIR/astat/file_stat.cpp" \
        -I"$SRC_DIR" \
        -o astat_demo
    echo -e "${GREEN}✓ Built astat_demo${NC}"
}

build_agrep() {
    echo -e "${YELLOW}Building agrep demo...${NC}"
    g++ -std=c++17 \
        agrep_demo.cpp \
        "$SRC_DIR/agrep/pattern_search.cpp" \
        -I"$SRC_DIR" \
        -o agrep_demo
    echo -e "${GREEN}✓ Built agrep_demo${NC}"
}

build_atar() {
    echo -e "${YELLOW}Building atar demo...${NC}"
    g++ -std=c++17 \
        atar_demo.cpp \
        "$SRC_DIR/atar/archive.cpp" \
        -I"$SRC_DIR" \
        -o atar_demo
    echo -e "${GREEN}✓ Built atar_demo${NC}"
}

build_atest() {
    echo -e "${YELLOW}Building atest demo...${NC}"
    g++ -std=c++17 \
        atest_demo.cpp \
        "$SRC_DIR/atest/test_framework.cpp" \
        -I"$SRC_DIR" \
        -o atest_demo
    echo -e "${GREEN}✓ Built atest_demo${NC}"
}

build_ajq() {
    echo -e "${YELLOW}Building ajq demo...${NC}"
    g++ -std=c++17 \
        ajq_demo.cpp \
        "$SRC_DIR/ajq/json_query.cpp" \
        -I"$SRC_DIR" \
        -o ajq_demo
    echo -e "${GREEN}✓ Built ajq_demo${NC}"
}

build_ascope() {
    echo -e "${YELLOW}Building ascope demo...${NC}"
    g++ -std=c++17 \
        ascope_demo.cpp \
        "$SRC_DIR/ascope/code_analyzer.cpp" \
        -I"$SRC_DIR" \
        -o ascope_demo
    echo -e "${GREEN}✓ Built ascope_demo${NC}"
}

build_asql() {
    echo -e "${YELLOW}Building asql demo...${NC}"
    g++ -std=c++17 \
        asql_demo.cpp \
        "$SRC_DIR/asql/sql_query.cpp" \
        -I"$SRC_DIR" \
        -lsqlite3 \
        -o asql_demo
    echo -e "${GREEN}✓ Built asql_demo${NC}"
}

build_apic() {
    echo -e "${YELLOW}Building apic demo...${NC}"
    g++ -std=c++17 \
        apic_demo.cpp \
        "$SRC_DIR/apic/media_processor.cpp" \
        -I"$SRC_DIR" \
        -o apic_demo
    echo -e "${GREEN}✓ Built apic_demo${NC}"
}

clean_all() {
    echo -e "${YELLOW}Cleaning examples...${NC}"
    rm -f aglob_demo als_demo aps_demo acp_demo amv_demo depgraph_demo abc_demo acat_demo acurl_demo adap_demo astat_demo agrep_demo atar_demo atest_demo ajq_demo ascope_demo asql_demo apic_demo
    echo -e "${GREEN}✓ Cleaned${NC}"
}

# Main
cd "$EXAMPLES_DIR"

case "${1:-}" in
    aglob)
        build_aglob
        ;;
    als)
        build_als
        ;;
    aps)
        build_aps
        ;;
    acp)
        build_acp
        ;;
    amv)
        build_amv
        ;;
    depgraph)
        build_depgraph
        ;;
    abc)
        build_abc
        ;;
    acat)
        build_acat
        ;;
    acurl)
        build_acurl
        ;;
    adap)
        build_adap
        ;;
    astat)
        build_astat
        ;;
    agrep)
        build_agrep
        ;;
    atar)
        build_atar
        ;;
    atest)
        build_atest
        ;;
    ajq)
        build_ajq
        ;;
    ascope)
        build_ascope
        ;;
    asql)
        build_asql
        ;;
    apic)
        build_apic
        ;;
    all)
        build_aglob
        build_als
        build_aps
        build_acp
        build_amv
        build_depgraph
        build_abc
        build_acat
        build_acurl
        build_adap
        build_astat
        build_agrep
        build_atar
        build_atest
        build_ajq
        build_ascope
        build_asql
        build_apic
        echo -e "${GREEN}Done!${NC}"
        ;;
    clean)
        clean_all
        ;;
    "")
        echo -e "${RED}Error: No utility specified${NC}"
        print_usage
        exit 1
        ;;
    *)
        echo -e "${RED}Error: Unknown utility '$1'${NC}"
        print_usage
        exit 1
        ;;
esac

echo -e "${GREEN}Done!${NC}"
