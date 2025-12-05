; stdlib.inc - Aria Standard Macro Library
; Class system built entirely with NASM-style macros
; No compiler changes needed - pure preprocessing!

;=============================================================================
; CLASS SYSTEM MACROS
;=============================================================================

; CLASS - Define a new class (generates a struct)
; Usage: CLASS ClassName
;
; Example:
;   CLASS Vector3
;       FIELD x, f32
;       FIELD y, f32
;   END_CLASS
;
%macro CLASS 1
    %define __CLASS_NAME__ %1
    
    ; Generate the struct definition
    ; This becomes: const ClassName = struct {
    const %1 = struct {
%endmacro

; FIELD - Add a public field to current class
; Usage: FIELD fieldname, type
;
; Example:
;   FIELD position, Vector3
;
%macro FIELD 2
    %1: %2,
%endmacro

; PRIVATE_FIELD - Add a private field (name mangling)
; Usage: PRIVATE_FIELD fieldname, type
;
; The double-underscore prefix convention signals "internal use only"
; The actual privacy is enforced by naming convention + documentation
;
%macro PRIVATE_FIELD 2
    __%1: %2,
%endmacro

; END_CLASS - Close the class definition
; Usage: END_CLASS
;
%macro END_CLASS 0
    };
    %undef __CLASS_NAME__
%endmacro

;=============================================================================
; METHOD MACROS
;=============================================================================

; METHOD - Define a method for the current class
; Usage: METHOD methodname, return_type
;
; This generates a function named ClassName_methodname
; The first parameter is implicitly *ClassName (the self pointer)
;
; Example:
;   METHOD update, void
;       ; self is the first parameter
;   END_METHOD
;
%macro METHOD 2
    %ifndef __CLASS_NAME__
        %error "METHOD must be used inside a CLASS block"
    %endif
    
    ; Generate function: ClassName_methodName
    func:%{__CLASS_NAME__}_%1 = %2(*%{__CLASS_NAME__}
%endmacro

; METHOD_STATIC - Define a static method (no self parameter)
; Usage: METHOD_STATIC methodname, return_type
;
%macro METHOD_STATIC 2
    %ifndef __CLASS_NAME__
        %error "METHOD_STATIC must be used inside a CLASS block"
    %endif
    
    ; Generate function: ClassName_methodName (no self param)
    func:%{__CLASS_NAME__}_%1 = %2(
%endmacro

; END_METHOD - Just a semantic marker (optional)
%macro END_METHOD 0
    ; No-op, just for readability
%endmacro

;=============================================================================
; MIXIN/TEMPLATE MACROS
;=============================================================================

; MIXIN - Define reusable field groups
; Usage: MIXIN MixinName
;        FIELD x, type
;        END_MIXIN
;
; Then use: APPLY_MIXIN MixinName
;
%macro MIXIN 1
    %push mixin_%1
    %define __MIXIN_NAME__ %1
    %macro MIXIN_%1 0
%endmacro

; END_MIXIN - Close mixin definition
%macro END_MIXIN 0
    %endmacro
    %pop
    %undef __MIXIN_NAME__
%endmacro

; APPLY_MIXIN - Inject mixin fields into current class
; Usage: APPLY_MIXIN MixinName
;
%macro APPLY_MIXIN 1
    MIXIN_%1
%endmacro

;=============================================================================
; HELPER MACROS
;=============================================================================

; SELF - Semantic alias for the first parameter (self pointer)
; Just documentation - not actually needed since first param is implicit
%define SELF self

; CONSTRUCTOR - Semantic marker for initialization method
; Usage: CONSTRUCTOR (parameters)
;
%macro CONSTRUCTOR 0-*
    METHOD init, void
%endmacro

; DESTRUCTOR - Semantic marker for cleanup method
%macro DESTRUCTOR 0
    METHOD destroy, void
%endmacro

;=============================================================================
; INTERFACE/TRAIT SIMULATION
;=============================================================================

; INTERFACE - Document required methods (comment-based contract)
; Usage: INTERFACE InterfaceName
;        REQUIRE_METHOD methodname, signature
;        END_INTERFACE
;
; This is purely documentation - no enforcement (yet)
;
%macro INTERFACE 1
    ; Interface %1 requires the following methods:
    %push interface_%1
%endmacro

%macro REQUIRE_METHOD 2
    ; - %1: %2
%endmacro

%macro END_INTERFACE 0
    %pop
%endmacro

;=============================================================================
; EXAMPLE USAGE
;=============================================================================
;
; CLASS Player
;     FIELD x, f32
;     FIELD y, f32
;     PRIVATE_FIELD health, i32
; END_CLASS
;
; METHOD update, void
;     self.x += 1.0;
; );
;
; This expands to:
;
; const Player = struct {
;     x: f32,
;     y: f32,
;     __health: i32,
; };
;
; func:Player_update = void(*Player) {
;     self.x += 1.0;
; );
;
