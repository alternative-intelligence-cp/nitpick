# Aria Programming Manual - Man Pages
# Build system for generating man pages from markdown

.PHONY: all clean install uninstall check test

# Directories
SOURCE_DIR = ../programming_guide
BUILD_DIR = build/man7
SCRIPTS_DIR = scripts
INSTALL_PREFIX ?= /usr/local
MAN_DIR = $(INSTALL_PREFIX)/share/man/man7

# Version
VERSION = 0.2.0

# Tools
PYTHON = python3
MD2MAN = $(SCRIPTS_DIR)/md2man.py
GZIP = gzip
INSTALL = install

# Find all markdown files
SOURCES = $(shell find $(SOURCE_DIR) -name '*.md' ! -name 'README.md')
TARGETS = $(patsubst $(SOURCE_DIR)/%.md,$(BUILD_DIR)/aria-%.7.gz,$(SOURCES))

all: man-pages index

man-pages:
	@echo "==> Generating man pages from markdown..."
	@mkdir -p $(BUILD_DIR)
	@$(PYTHON) $(MD2MAN) $(SOURCE_DIR) $(BUILD_DIR) $(VERSION)
	@echo "==> Man pages generated: $(BUILD_DIR)"

index: man-pages
	@echo "==> Building man page index..."
	@$(PYTHON) $(SCRIPTS_DIR)/build_index.py $(BUILD_DIR)

check:
	@echo "==> Checking man page validity..."
	@for man in $(BUILD_DIR)/*.7.gz; do \
		echo "Checking $$man..."; \
		zcat $$man | groff -man -Tutf8 -z 2>&1 | grep -i error || true; \
	done

test: man-pages
	@echo "==> Testing man page rendering..."
	@echo "Testing random sample..."
	@find $(BUILD_DIR) -name '*.7.gz' | shuf | head -5 | while read man; do \
		echo "---"; \
		echo "Testing: $$man"; \
		man -l $$man | head -20; \
	done

install: all
	@echo "==> Installing man pages to $(MAN_DIR)..."
	@mkdir -p $(MAN_DIR)
	@$(INSTALL) -m 644 $(BUILD_DIR)/*.7.gz $(MAN_DIR)/
	@echo "==> Updating man database..."
	@mandb -q || true
	@echo "==> Man pages installed. Try: man aria-types-int32"

uninstall:
	@echo "==> Removing Aria man pages from $(MAN_DIR)..."
	@rm -f $(MAN_DIR)/aria-*.7.gz
	@echo "==> Updating man database..."
	@mandb -q || true

clean:
	@echo "==> Cleaning build directory..."
	@rm -rf $(BUILD_DIR)
	@rm -f *.7 *.7.gz

stats:
	@echo "==> Man Page Statistics"
	@echo "Total pages: $$(find $(BUILD_DIR) -name '*.7.gz' 2>/dev/null | wc -l)"
	@echo "Total size (compressed): $$(du -sh $(BUILD_DIR) 2>/dev/null | cut -f1)"
	@echo "Categories:"
	@find $(BUILD_DIR) -name '*.7.gz' 2>/dev/null | sed 's/.*aria-\([^-]*\)-.*/\1/' | sort | uniq -c | sort -rn

help:
	@echo "Aria Man Page Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Generate all man pages (default)"
	@echo "  man-pages  - Generate man pages from markdown"
	@echo "  index      - Build searchable index"
	@echo "  check      - Validate man page syntax"
	@echo "  test       - Test rendering of sample pages"
	@echo "  install    - Install to system (requires sudo)"
	@echo "  uninstall  - Remove from system (requires sudo)"
	@echo "  clean      - Remove build artifacts"
	@echo "  stats      - Show statistics"
	@echo ""
	@echo "Variables:"
	@echo "  VERSION        - Man page version (default: $(VERSION))"
	@echo "  INSTALL_PREFIX - Install prefix (default: $(INSTALL_PREFIX))"
