.PHONY: all build test clean install

all: build

build:
	tree-sitter generate

test: build
	tree-sitter test

parse: build
	tree-sitter parse ../vscode/sample.aria

clean:
	rm -rf node_modules
	rm -rf src/
	rm -f tree-sitter-aria.wasm

install:
	npm install

# Build shared library for Helix/Emacs
lib: build
	tree-sitter build -o libtree-sitter-aria.so

# Install to Neovim
install-nvim: build
	mkdir -p ~/.local/share/nvim/site/pack/tree-sitter/start/tree-sitter-aria
	cp -r . ~/.local/share/nvim/site/pack/tree-sitter/start/tree-sitter-aria/

# Install to Helix
install-helix: lib
	mkdir -p ~/.config/helix/runtime/grammars/sources/aria
	mkdir -p ~/.config/helix/runtime/queries/aria
	cp -r . ~/.config/helix/runtime/grammars/sources/aria/
	cp libtree-sitter-aria.so ~/.config/helix/runtime/grammars/
	cp -r queries/* ~/.config/helix/runtime/queries/aria/
