;;; aria-mode.el --- Major mode for the Aria programming language -*- lexical-binding: t; -*-

;; Author: Aria Language Project
;; Keywords: languages aria
;; Version: 0.2.0

;;; Commentary:

;; Major mode for editing Aria (.aria) source files.
;;
;; Aria syntax quick-reference:
;;   func:name = ReturnType(params) { pass(val); };
;;   int32:x = 42i32;
;;   use "path/to/file.aria".*;
;;   extern "libc" { func:puts = int32(string:s); }
;;   loop(0i32, 10i32, 1i32) { }   while (cond) { }   till (cond) { }
;;   pass(val)  fail("msg")         ;; return mechanisms
;;   drop(expr) raw(expr) ok(val)   ;; TOS bypass builtins
;;   NIL  NULL  ERR  unknown        ;; language constants

;;; Installation:
;;
;; Add to your Emacs config:
;;   (require 'aria-mode)
;; Or with use-package:
;;   (use-package aria-mode
;;     :mode "\\.aria\\'")
;;
;; For tree-sitter support (Emacs 29+):
;;   (add-to-list 'major-mode-remap-alist '(aria-mode . aria-ts-mode))

;;; Code:

(require 'rx)

;;;; Face definitions

(defgroup aria-mode nil
  "Major mode for the Aria programming language."
  :group 'languages
  :prefix "aria-")

;; We inherit from built-in faces so the mode respects the user's theme.
;; Operators/TOS-builtins get custom faces so they stand out for bug hunting.

(defface aria-tos-builtin-face
  '((t :inherit font-lock-builtin-face :weight bold))
  "Face for TOS bypass builtins: drop, raw, ok."
  :group 'aria-mode)

(defface aria-result-operator-face
  '((t :inherit font-lock-warning-face))
  "Face for Result operators: ? ?! ?? !!!"
  :group 'aria-mode)

(defface aria-typed-suffix-face
  '((t :inherit font-lock-type-face :slant italic))
  "Face for numeric type suffixes: 42i32, 3.14flt64."
  :group 'aria-mode)

;;;; Constants

(defconst aria-mode-integer-types
  '("int8" "int16" "int32" "int64" "int128" "int256" "int512"
    "uint8" "uint16" "uint32" "uint64" "uint128" "uint256" "uint512")
  "Aria integer type names.")

(defconst aria-mode-float-types
  '("flt16" "flt32" "flt64" "flt128")
  "Aria floating-point type names.")

(defconst aria-mode-tbb-types
  '("tbb8" "tbb16" "tbb32" "tbb64" "tbb128" "tbb256")
  "Aria TBB (Twisted Balanced Binary) type names.")

(defconst aria-mode-special-types
  '("fix256" "tfp64" "frac8" "bool" "string" "void" "trit" "tryte" "nit" "nyte")
  "Aria special and balanced-numeric type names.")

(defconst aria-mode-generic-types
  '("Result" "Handle" "arena" "atomic" "simd" "quantum" "complex")
  "Aria generic container type names.")

(defconst aria-mode-all-types
  (append aria-mode-integer-types
          aria-mode-float-types
          aria-mode-tbb-types
          aria-mode-special-types)
  "All non-generic Aria type names.")

(defconst aria-mode-control-keywords
  '("if" "else" "when" "then" "end" "pick" "case" "default"
    "while" "till" "loop" "for" "in" "break" "continue" "fall"
    "defer" "async" "await")
  "Aria control-flow keywords.")

(defconst aria-mode-declaration-keywords
  '("func" "struct" "enum" "trait" "impl" "mod" "type" "const")
  "Aria declaration keywords.")

(defconst aria-mode-module-keywords
  '("use" "extern")
  "Aria module system keywords.")

(defconst aria-mode-other-keywords
  '("pub" "priv" "as" "is" "ref" "mut" "move" "copy"
    "wild" "wildx" "gc" "stack"
    "superpose" "collapse")
  "Aria miscellaneous keywords.")

(defconst aria-mode-return-keywords
  '("pass" "fail")
  "Aria return-mechanism keywords (analogous to 'return' in other languages).")

(defconst aria-mode-tos-builtins
  '("drop" "raw" "ok")
  "Aria TOS (Type OK System) bypass builtins — explicit intent markers.")

(defconst aria-mode-language-constants
  '("true" "false" "NIL" "NULL" "ERR" "unknown")
  "Aria language constants.")

;;;; Syntax helpers

(defun aria-mode--keyword-regexp (words)
  "Return a regexp matching any symbol in WORDS as a whole word."
  (concat "\\_<" (regexp-opt words t) "\\_>"))

(defun aria-mode--type-followed-by-colon-regexp ()
  "Regexp matching 'TYPE:varname' variable declarations.
Group 1 = type name, group 2 = colon, group 3 = variable name."
  (concat "\\_<\\(" (regexp-opt aria-mode-all-types) "\\)\\(:\\)\\([a-zA-Z_][a-zA-Z0-9_]*\\)\\_>"))

;; Typed numeric literal patterns (for bug-hunting: 42i32 vs bare 42)
(defconst aria-mode--integer-suffixes
  "i8\\|i16\\|i32\\|i64\\|i128\\|u8\\|u16\\|u32\\|u64\\|u128\\|tbb8\\|tbb16\\|tbb32\\|tbb64")

(defconst aria-mode--float-suffixes
  "flt32\\|flt64\\|fix256\\|tf")

;;;; Font lock keywords

(defconst aria-mode-font-lock-keywords
  `(
    ;; --- Strings (template strings handled separately via syntax table) ---
    ;; Template strings with &{...} interpolation: `hello &{name}`
    ("`[^`]*`" . font-lock-string-face)

    ;; --- Variable declarations: int32:x ---
    ;; Must come before plain type highlighting to capture both as a unit
    (,(aria-mode--type-followed-by-colon-regexp)
     (1 font-lock-type-face)
     (2 font-lock-punctuation-face)
     (3 font-lock-variable-name-face))

    ;; --- Generic type annotations: Result<T>, Handle<int32> ---
    (,(concat "\\_<\\(" (regexp-opt aria-mode-generic-types) "\\)\\_>\\s-*<")
     1 font-lock-type-face)

    ;; --- Function declarations: func:name = ReturnType(params) ---
    ("\\_<\\(func\\)\\s-*\\(:\\)\\s-*\\([a-zA-Z_][a-zA-Z0-9_]*\\)"
     (1 font-lock-keyword-face)
     (2 font-lock-punctuation-face)
     (3 font-lock-function-name-face))

    ;; --- Return mechanisms (pass/fail are Aria's return equivalent) ---
    (,(aria-mode--keyword-regexp aria-mode-return-keywords)
     . font-lock-keyword-face)

    ;; --- TOS bypass builtins: drop(), raw(), ok() ---
    ;; Only before '(' to distinguish from variable names
    (,(concat "\\_<\\(" (regexp-opt aria-mode-tos-builtins) "\\)\\_>\\s-*(")
     1 'aria-tos-builtin-face)

    ;; --- print / println ---
    ("\\_<\\(print\\(?:ln\\)?\\)\\_>\\s-*(" 1 font-lock-builtin-face)

    ;; --- Control flow keywords ---
    (,(aria-mode--keyword-regexp aria-mode-control-keywords)
     . font-lock-keyword-face)

    ;; --- Declaration keywords ---
    (,(aria-mode--keyword-regexp aria-mode-declaration-keywords)
     . font-lock-keyword-face)

    ;; --- Module keywords: use, extern ---
    (,(aria-mode--keyword-regexp aria-mode-module-keywords)
     . font-lock-preprocessor-face)

    ;; --- Other keywords: pub, wild, wildx, as, is, etc. ---
    (,(aria-mode--keyword-regexp aria-mode-other-keywords)
     . font-lock-keyword-face)

    ;; --- Plain type names (not part of type:name, catches return types etc.) ---
    (,(aria-mode--keyword-regexp aria-mode-all-types)
     . font-lock-type-face)

    ;; --- Language constants: NIL, NULL, ERR, unknown, true, false ---
    (,(aria-mode--keyword-regexp aria-mode-language-constants)
     . font-lock-constant-face)

    ;; --- Result/TOS operators: ?! ?? ? !!! ---
    ;; These are high-value visual markers for bug hunting
    ("?!" . 'aria-result-operator-face)
    ("!!!" . 'aria-result-operator-face)
    ("??" . 'aria-result-operator-face)
    ;; standalone ? (not ?! or ??)
    ("\\(?:[^?!]\\|^\\)\\(\\?\\)\\(?:[^?!]\\|$\\)" 1 'aria-result-operator-face)

    ;; --- Typed integer literals: 42i32, 100u64, 127tbb8 ---
    (,(concat "\\([0-9][0-9_]*\\)\\(" aria-mode--integer-suffixes "\\)\\_>")
     (1 font-lock-constant-face)
     (2 'aria-typed-suffix-face))

    ;; --- Typed float literals: 3.14flt64, 0.5fix256, 1.5tf ---
    (,(concat "\\([0-9][0-9_]*\\.[0-9][0-9_]*\\)\\(" aria-mode--float-suffixes "\\)\\_>")
     (1 font-lock-constant-face)
     (2 'aria-typed-suffix-face))

    ;; --- Balanced ternary literals: 0t1T0 ---
    ("\\b\\(0t[01T]+\\)\\b" 1 font-lock-constant-face)

    ;; --- Balanced nonary literals: 0n4D ---
    ("\\b\\(0n[0-9A-D]+\\)\\b" 1 font-lock-constant-face)

    ;; --- Memory qualifiers: @wild, @gc, @stack, @wildx ---
    ("@\\(wild\\(?:x\\)?\\|gc\\|stack\\)\\b" 1 font-lock-keyword-face)

    ;; --- Compiler intrinsics: @cast, @addr ---
    ("@\\(cast\\|addr\\)\\b" . font-lock-builtin-face)

    ;; --- Struct name after struct: ---
    ("\\_<struct\\_>\\s-*:\\s-*\\([a-zA-Z_][a-zA-Z0-9_]*\\)"
     1 font-lock-type-face)

    ;; --- Function calls (all identifiers before '(') ---
    ("\\([a-zA-Z_][a-zA-Z0-9_]*\\)\\s-*(" 1 font-lock-function-call-face)
    )
  "Font lock keywords for `aria-mode'.")

;;;; Syntax table

(defvar aria-mode-syntax-table
  (let ((st (make-syntax-table)))
    ;; Line comments: //
    (modify-syntax-entry ?/ ". 124b" st)
    (modify-syntax-entry ?* ". 23" st)
    (modify-syntax-entry ?\n "> b" st)
    ;; String delimiters
    (modify-syntax-entry ?\" "\"" st)
    (modify-syntax-entry ?\` "\"" st)   ; backtick = template string
    ;; Brackets
    (modify-syntax-entry ?\( "()" st)
    (modify-syntax-entry ?\) ")(" st)
    (modify-syntax-entry ?\[ "(]" st)
    (modify-syntax-entry ?\] ")[" st)
    (modify-syntax-entry ?\{ "(}" st)
    (modify-syntax-entry ?\} "){" st)
    ;; Punctuation operators
    (modify-syntax-entry ?+ "." st)
    (modify-syntax-entry ?- "." st)
    (modify-syntax-entry ?= "." st)
    (modify-syntax-entry ?< "." st)
    (modify-syntax-entry ?> "." st)
    (modify-syntax-entry ?& "." st)
    (modify-syntax-entry ?| "." st)
    (modify-syntax-entry ?! "." st)
    (modify-syntax-entry ?? "." st)
    (modify-syntax-entry ?% "." st)
    (modify-syntax-entry ?^ "." st)
    (modify-syntax-entry ?~ "." st)
    (modify-syntax-entry ?# "." st)
    (modify-syntax-entry ?$ "." st)
    (modify-syntax-entry ?@ "." st)
    ;; Underscore is word constituent (part of identifiers)
    (modify-syntax-entry ?_ "w" st)
    st)
  "Syntax table for `aria-mode'.")

;;;; Indentation

(defcustom aria-mode-indent-offset 4
  "Number of spaces per indentation level in Aria code."
  :type 'integer
  :group 'aria-mode)

(defun aria-mode-indent-line ()
  "Indent current line as Aria code.
Uses simple brace-counting: indent by one level for each unclosed {."
  (interactive)
  (let ((indent-col 0))
    (save-excursion
      (beginning-of-line)
      (let ((parse-state (syntax-ppss)))
        (setq indent-col (* aria-mode-indent-offset (nth 0 parse-state)))))
    ;; Dedent closing braces to match their opener
    (save-excursion
      (back-to-indentation)
      (when (looking-at "\\s-*[})]")
        (setq indent-col (max 0 (- indent-col aria-mode-indent-offset)))))
    (indent-line-to indent-col)))

;;;; Mode definition

;;;###autoload
(define-derived-mode aria-mode prog-mode "Aria"
  "Major mode for editing Aria programming language source files.

Aria is a systems programming language with:
- TBB (Twisted Balanced Binary) types with ERR sentinel
- Result<T> return values with pass/fail/drop/raw/ok operators
- Blueprint pointer syntax: int32->:ptr  @addr  <-ptr  #pin  $ref
- Typed numeric literals: 42i32, 3.14flt64, 127tbb8
- Balanced numeral systems: 0t1T0 (ternary), 0n4D (nonary)
- loop/while/till loop forms, when/pick pattern matching

Keybindings:
\\{aria-mode-map}"
  :syntax-table aria-mode-syntax-table
  (setq-local font-lock-defaults
              '(aria-mode-font-lock-keywords
                nil   ; no keywords-only
                nil   ; case-sensitive
                nil   ; syntax modifications
                nil)) ; start-of-buffer function
  (setq-local comment-start "// ")
  (setq-local comment-end "")
  (setq-local comment-start-skip "//+\\s-*")
  (setq-local comment-style 'indent)
  (setq-local indent-line-function #'aria-mode-indent-line)
  (setq-local tab-width aria-mode-indent-offset)
  (setq-local indent-tabs-mode nil))

;;;###autoload
(add-to-list 'auto-mode-alist '("\\.aria\\'" . aria-mode))

(provide 'aria-mode)

;;; aria-mode.el ends here
