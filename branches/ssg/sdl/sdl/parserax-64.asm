;   PAGE          ,132

;    Name: PARSERA.ASM
;  Author: Chuck Ebbert  CompuServe [76306,1226]
;                         internet: 76306.1226@compuserve.com

; Fast floating-point routines for Fractint.

;   (c) Copyright 1992-1998 Chuck Ebbert.  All rights reserved.

; This program is an assembler version of the C 'execution engine' part
;    of Mark Peterson's FRACTINT Formula Parser.  Many of the operator
;    functions were copied from Mark's code in the files FPU087.ASM
;    and FPU387.ASM.  The basic operator functions are assembler versions
;    of the code in PARSER.C.  Many 'combined' operator functions were
;    added to the program as well.

; As of 31 Decmember 1993 this is also an in-memory compiler.  The code
;    generator is in PARSERFP.C.  Define the variable COMPILER to
;    build the compiler, otherwise the interpreter will be built.
;    COMPILER must also be #defined in PARSERFP.C to build compiler.

; 5 Dec 1999 bug fix: removed loop unroll because the modified return
;    address was skipping over functions that needed to execute.

; 1 Dec 1998 speed improvements:
;    Functions Clr2, LT2, LodLT2, LTE2, LodLTE2, GT2, LodGT2,
;    LodGTE2, AndClr2, OrClr2, LodLTEAnd2 were modified to alter
;    their return address on the stack, forcing an exit from
;    the evaluation loop.  This allows the code to run
;    faster because it doesn't have to test for loop end.
;    The loop was also unrolled one time for further gains.

; ---------------------------------------------------------------------------

;    This code may be freely distributed and used in non-commercial
;    programs provided the author is credited either during program
;    execution or in the documentation, and this copyright notice
;    is left intact.  Sale of this code, or its use in any commercial
;    product requires permission from the author.  Nominal distribution
;    and handling fees may be charged by shareware and freeware
;    distributors.


;             Date      Init   Change description
;
;           5 Dec 1999   JCO   Removed loop unroll
;           1 Dec 1998   CAE   Speed improvements
;           5 Apr 1998   JCO   Added LoadRealPwr (0,0) domain check
;           6 Mar 1997   TIW   Added if/else support
;           7 Aug 1996   TIW   Added scrnpix constant
;           4 Aug 1996   TIW   Added whitesq variable support
;          30 Jun 1996   TIW   Added floor, ceil, trunc, and round functions
;           7 Mar 1995   TIW   Added PWR (0,0) domain check
;          21 Feb 1995   TIW   Shortened ATanh/ATan for MASM 6 compatibility
;          21 Feb 1995   CAE   Changes ATan and ATanh

;          15 Feb 1995   CAE   Added safety tests to macros.
;                              Changed fStkASin, etc. to work with compiler.
;                              Added fwait to Sto2 function for safety.

;           8 Feb 1995   CAE   Removed transparent3d code.
;                              Added inversion support (compiler untested.)

;           8 Jan 1995   JCO   Added fStkASin, fStkASinh, fStkACos, fStkACosh,
;                              fStkATan, fStkATanh, fStkSqrt, fStkCAbs.

;          31 Dec 1994   JCO   Made changes to keep code in line with C code.
;                              Not necessary, since code isn't called.  Will
;                              make it easier to make it run later.  Added
;                              old <- z to end of fform_per_pixel to match
;                              C code.

;          30 Dec 1993   CAE   Compiler is working
;                              Changed EXIT_OPER -> ret in 3 operator fns
;                              Added safety test for fn size in macros

;          12 Dec 1993   CAE   Compiler additions

;           4 Dec 1993   CAE   SinhCosh function accuracy improved
;                              Added LoadImagAdd/Sub/Mul

;          19 Nov 1993   CAE   Revised macros for compiler mode.

;          10 Nov 1993   CAE   Changed Exp function for more accuracy.

;          06 Nov 93     CAE   Added 'LodRealPwr', 'One', 'ORClr2', 'Sqr3'.
;                              Revised Pwr function to use regs vs. memory.
;                              Changed many functions to 'included' type.

;          31 Oct 93     CAE   Added 'Dbl' function.

;          09 Oct 1993   CAE   Changed SinhCosh to use wider range of 387.
;                              Most FNINITs changed to FINIT.
;                              Loop logic revised slightly.
;                              Separated code from parserfp.c's codeseg.
;                              Added fStkStoClr2, fStkZero and fStkIdent.
;                              New 'pseudo calctype' fn. fFormulaX added.

;          12 Jul 1993   CAE   Moved BadFormula to PARSER.C.

DEFAULT REL

%include "xfract_a-64.inc"

ARGSZ              equ 2*DBLSZ        ; size of complex arg
;;;ARGSZ              equ 32              ; size of hypercomplex arg
PDDING             equ 4
CPFX               equ PTRSZ+INTSZ+PDDING ; size of constarg prefix (ptr + int)
CARG               equ CPFX+ARGSZ      ; size of constarg
LASTSQR            equ CARG*4+CPFX     ; offset of lastsqr from start of v
WHITESQ            equ CARG*9+CPFX     ; offset of whitesq from start of v
SCRNPIX            equ CARG*10+CPFX    ; offset of scrnpix from start of v
JCSZ               equ 3*LNGSZ+2*(INTSZ+PDDING) ; size of jump_control structure (3 long + 2 int)

; ---------------------------------------------------------------------------
; Pop a number of scalars from the FPU stack.
; Generate as many 'fcompp' instr.'s as possible.
; Then a 'fstp st(0)' if needed.
%macro POP_STK 1
;   NumToPop        = %1 >> 1
%assign NumToPop  (%1 >> 1)
%if (NumToPop >= 1 )
  %rep (NumToPop)    ;NumToPop
      fcompp
  %endrep
%endif
;   NumToPop        = %1 - ( NumToPop << 1 )
%assign NumToPop  (%1 - ( (%1 >> 1) << 1 ))
%if (NumToPop = 1)                 ;NumToPop
      fstp         st0
%endif
%endmacro

; Uncomment the following line to enable compiler code generation.
;   Note: I removed all the compiler code when I ported over to NASM.  JCO  1/12/2002
;   Note: I restored the compiler code, but it doesn't work.  JCO  7/26/2019
; %define  COMPILER

; ---------------------------------------------------------------------------
; Generate beginning code for operator fn.
%macro BEGN_OPER 1
; %1 = OperName
%ifndef COMPILER
;; only align when no compiler
   align           16
%endif

;; always generate public and begin of proc (before fixups)
   CGLOBAL          fStk%1
;_fStk&OperName     proc near
fStk%1:

%ifdef COMPILER
;; generate the fixups for compiler
;; size of fn. | 8000h to mark it as an OPER instead of an INCL  CAE 27Dec93
      dw           Size_%1 OR 8000h
;; pointer to the start of actual code                      CAE 19Dec93
      dq           Code_%1
;;    addr of fn to include (undefined if Incl_%1==255 below)
      dq           IAddr_%1
;; offset of x fixup or 255 if none
      db           XFixup_%1
;; offset of y fixup or 255 if none
      db           YFixup_%1
;; offset of included(called) fn or 255 if none
      db           Incl_%1
%endif

;; added label for code begin point                              CAE 25Nov93
Code_%1:

%endmacro

; ---------------------------------------------------------------------------
%macro  END_OPER 1
; %1 = OperName
; Generate end of operator fn. code.

%ifndef COMPILER
;; gen a return instr.
      ret

%else

;; gen a jump label
End_%1:

;; generate zero for fixups not generated during fn.

%ifndef          Incl_%1
;; No included operator. Generate 255 offset, 0 address.          CAE 19Nov93
   Incl_%1     EQU 255
   IAddr_%1    EQU 0
%endif

%ifndef          XFixup_%1
   XFixup_%1   EQU 255
%endif

%ifndef          YFixup_%1
   YFixup_%1   EQU 255
%endif

%endif

;; Always gen size of fn (subtract size of header here)
Size_%1     EQU $ - Code_%1
;; Make sure fn is of legal size                                  CAE 30DEC93
;;   .errnz          (Size_%1 > 127)
%if (Size_%1 > 127)
%error  Size_%1 > 127
%endif
;; and end of procedure.
;; _fStk%1
%endmacro

; ---------------------------------------------------------------------------
%macro BEGN_INCL 1
;  %1 = OperName
;; Generate beginning code for 'included' operator fn.
;; No fixups allowed in one of these functions.

;; Safety test: generate an equate here so the INCL_OPER          CAE 15Feb95
;;    macro can test to see if this really is includable.
;;Is_Incl_%1  EQU 1
%define Is_Incl_%1 1

;; Don't bother with align in compiler mode.
%ifndef          COMPILER
   align           16
%endif

;; Generate public (incl fns. can be called directly) and begin of proc.
   CGLOBAL          fStk%1
;_fStk&OperName     proc near
fStk%1:

%ifdef           COMPILER
;; Size of included fn.  changed to word                          CAE 27Dec93
      dw           Size_%1
%endif

;; added label for code begin point                               CAE 25Nov93
Code_%1:

%endmacro

; ---------------------------------------------------------------------------
; Generate end of 'included' operator fn. code.
%macro END_INCL 1
;  %1 = OperName

%ifndef          COMPILER
;; generate return
      ret
%else
;; generate label for jump to end of fn.
End_%1:
%endif

;; always generate actual size of fn. (subtract hdr. size)
   Size_%1  EQU $ - Code_%1
;; Make sure fn is of legal size                                  CAE 30DEC93
;;   .errnz          (Size_%1 > 127)
%if (Size_%1 > 127)
%error  Size_%1 > 127
%endif
;; always generate end-of-proc
;_fStk&OperName     endp
%endmacro

; ---------------------------------------------------------------------------
; 'Include' a function inside another one
%macro INCL_OPER 2
;  %1 =CallingOper, %2 = OperToIncl

;; Make sure the included fn was defined with the BEGN_INCL macro.
   %ifndef          Is_Incl_%2                         ;  CAE 15Feb95
   %error        fStk%1 included function fStk%2 was not defined with BEGN_INCL macro
   %endif

;; Gen equate for offset of include in outer fn.
;; Always generate this to prevent >1 include even when not       CAE 15FEB95
;;    building the compiler.
%%Incl_%1  EQU $ - Code_%1

%ifdef           COMPILER
;; Address of included fn.
IAddr_%1 EQU fStk%2
;; Gen 1 1-byte placeholder for the included fn to make codegen easier
      db           0ffH
%else

;; Generate a call to the included fn.
      sub     rsp, 8             ; need rsp on 16-byte boundary
      call         fStk%2
      add     rsp, 8

%endif

%endmacro

; ---------------------------------------------------------------------------
; Exit early from an operator function.
%macro EXIT_OPER 1
;  %1 = FnToExit

%ifdef           COMPILER
;; jump to end of operator fn
      jmp          short End_%1
%else

;; return to caller
      ret
%endif

%endmacro

; ---------------------------------------------------------------------------
; Generate an FPU instruction and a fixup.
; AddrToFix is = X or Y
%macro FIXUP 3
;  %1 = OperName, %2 = InstrToFix, %3 = Addr

%ifdef           COMPILER

;; Generate a fixup as an offset from start of fn.
;; Fixup is two bytes into the instruction, thus the '+ 2'.
;; This may not be true for all instructions.
%ifidni          %3, X
   XFixup_%1   EQU $ - Code_%1 + 2
%else
;; assume fixup is for y
   YFixup_%1   EQU $ - Code_%1 + 2
%endif
;; Generate a load, store or whatever of any convenient value.
      %2  QWORD [fLastOp]
%else

%ifidni          %3, X
;; Gen load of X using RSI.
      %2  QWORD [rsi]
%else
;; Assume fixup is for y, use RSI+DBLSZ.
      %2  QWORD [rsi + DBLSZ]
%endif
%endif

%endmacro

; ---------------------------------------------------------------------------
; Align 16 if no compiler.
%macro PARSALIGN 0
%ifndef  COMPILER
   align           16
%endif
%endmacro

; CAE added macros for common operations Feb 1995

%macro GEN_SQR0 0
;; square the stack top, don't save magnitude in lastsqr          CAE 15FEB95
      fld          st0                 ; x x y
      fld          st0                 ; x x x y
      fmul         st0,st3             ; xy x x y
      fadd         st0,st0             ; 2xy x x y
      fxch         st3                 ; y x x 2xy
      fadd         st2,st0             ; y x x+y 2xy
      fsubp        st1,st0             ; x-y x+y 2xy
      fmulp        st1,st0             ; xx-yy 2xy
%endmacro

%macro GEN_SQRT 0              ;                           CAE 15Feb95
   ; can use a max of 2 regs
      fld          st1                 ; y x y
      fld          st1                 ; x y x y
      fpatan                           ; atan x y
      fdiv         qword [_2_]         ; theta=atan/2 x y
      fsincos                          ; cos sin x y
      fxch         st3                 ; y sin x cos
      fmul         st0,st0             ; yy sin x cos
      fxch         st2                 ; x sin yy cos
      fmul         st0,st0             ; xx sin yy cos
      faddp        st2,st0             ; sin xx+yy cos
      fxch         st2                 ; cos xx+yy sin
      fxch                             ; xx+yy cos sin
      fsqrt                            ; sqrt(xx+yy) cos sin
      fsqrt                            ; mag=sqrt(sqrt(xx+yy)) cos sin
      fmul         st2,st0             ; mag cos mag*sin
      fmulp        st1,st0             ; mag*cos mag*sin
%endmacro

; ---------------------------------------------------------------------------
; CAE 1 Dec 1998 added macro

%macro ALTER_RET_ADDR 0
;;;      mov          WORD PTR [sp],offset past_loop
      mov          qword [rsp], qword past_loop
%endmacro

; ---------------------------------------------------------------------------
; external functions
   CEXTERN           invertz2

; ---------------------------------------------------------------------------
; external data
   CEXTERN           invert          ;:DWORD - int
   CEXTERN           maxit           ;:QWORD - long
   CEXTERN           inside          ;:DWORD - int
;   CEXTERN           outside         ;:DWORD - int
   CEXTERN           coloriter       ;:QWORD - long
;   CEXTERN           kbdcount        ;:DWORD - int   ; keyboard counter
;   CEXTERN           dotmode         ;:DWORD - int
   CEXTERN           PointFive       ;:QWORD - double
   CEXTERN           _1_             ;:QWORD - double
   CEXTERN           _2_             ;:QWORD - double
   CEXTERN           infinity        ;:QWORD - double
   CEXTERN           LastOp          ;:DWORD - int (unsigned)
   CEXTERN           LastInitOp      ;:QWORD - long
   CEXTERN           InitOpPtr       ;:QWORD - long
   CEXTERN           InitStoPtr      ;:QWORD - long
   CEXTERN           InitLodPtr      ;:QWORD - long
   CEXTERN           s               ;:QWORD - array of pointers to Arg
   CEXTERN           dy1             ;:QWORD - pointer to double
   CEXTERN           dx1             ;:QWORD - pointer to double
   CEXTERN           dy0             ;:QWORD - pointer to double
   CEXTERN           dx0             ;:QWORD - pointer to double
   CEXTERN           new             ;:QWORD - DBL x 2
   CEXTERN           old             ;:QWORD - DBL x 2
   CEXTERN           overflow        ;:DWORD - int
   CEXTERN           save_release    ;:DWORD - int
   CEXTERN           col             ;:DWORD - int
   CEXTERN           row             ;:DWORD - int
   CEXTERN           Arg1            ;:QWORD - (pointer to DBL x 2)
   CEXTERN           Arg2            ;:QWORD - (pointer to DBL x 2)
   CEXTERN           pfls            ;:QWORD - (pointer) function, operand
   CEXTERN           v               ;:QWORD - pointer to Const Args array
   CEXTERN           ldcheck         ;:DWORD - int
   CEXTERN           jump_index      ;:DWORD - int
   CEXTERN           InitJumpIndex   ;:DWORD - int
   CEXTERN           jump_control    ;:QWORD - array of pointers to JUMP_CONTROL_ST
   CEXTERN           delxx           ;:QWORD - DBL
   CEXTERN           delxx2          ;:QWORD - DBL
   CEXTERN           delyy           ;:QWORD - DBL
   CEXTERN           delyy2          ;:QWORD - DBL
   CEXTERN           xxmin           ;:QWORD - double
   CEXTERN           yymax           ;:QWORD - double
   CEXTERN           use_grid        ;:DWORD - int

; ---------------------------------------------------------------------------

; local storage area

section .bss
align  16

fLastOp:    ;    offset of lastop here
      resb  PTRSZ
PtrToZ:     ;     offset of z
      resb  PTRSZ

; ---------------------------------------------------------------------------
; Operator Functions follow.
; ---------------------------------------------------------------------------

; NOTE: None of these operator functions may change any registers but
;       rax and rsi.  The exceptions are those functions that update
;       the current values of the 'status' regs as needed.

;  On entry to these functions:
;   FPU stack is used as the evaluation stack.
;         The FPU stack can overflow into memory.  Accuracy is not lost but
;         calculations are slower.
;;;   es -> DGROUP
;;;   ds -> parser data
;   rcx -> lastop
;   rdx == orbit counter (in fFormulaX)
;   rdi -> stack overflow area, used by push and pull functions and as
;         a temporary storage area
;   rbx -> current operator, operand pair
;    [rbx] = operator function address, i.e. addr. of current '_fStkXXX'
;    [rbx+PTRSZ] = operand pointer or zero if no operand
;   rsi = operand pointer (loaded from [rbx+PTRSZ] before call of operator fn.)

; New rules Feb 1993:
;  1. No EXIT_OPER before an INCL_OPER
;     (no jumps can be made past an included function.)
;  2. No included fn may include another, or have any fixups.
;  3. Only one included fn. allowed per 'normal' fn.


; --------------------------------------------------------------------------

section .text

; --------------------------------------------------------------------------
; Included functions must be before any fns that include them.
; --------------------------------------------------------------------------
     BEGN_INCL Log                 ; Log
   ; From FPU387.ASM
   ; Log is called by Pwr and is also called directly.
      ftst
      fstsw        ax
      sahf
      jnz          short .NotBothZero
      fxch                             ; y x
      ftst
      fstsw        ax
      sahf
      fxch                             ; x y
      jnz          short .NotBothZero
      POP_STK 2                        ; clear two numbers
      fldz
      fldz
      mov          rax, 1              ; domain error (1 in rax)
      EXIT_OPER    Log                 ; return (0,0)
   PARSALIGN
.NotBothZero:
      xor          rax,rax             ; no domain error (0 in rax)
      fld          st1                 ; y x y
      fld          st1                 ; x y x y
      fpatan                           ; z.y x y
      fxch         st2                 ; y x z.y
      fmul         st0,st0             ; yy x z.y
      fxch                             ; x yy z.y
      fmul         st0,st0             ; xx yy z.y
      fadd                             ; mod z.y
      fldln2                           ; ln2, mod, z.y
      fmul         qword [PointFive]   ; ln2/2, mod, z.y
      fxch                             ; mod, ln2/2, z.y
      fyl2x                            ; z.x, z.y
   END_INCL Log
; --------------------------------------------------------------------------
   BEGN_INCL       SinhCosh            ; Included fn, Sinh, Cosh of st
   ; From FPU087.ASM with mods to use less registers & for 387.
   ; Mod for 387-only after Fractint v18.                        CAE 09OCT93
   ; NOTE: Full 80-bit accuracy is *NOT* maintained in this function!
   ;       Only 1 additional register can be used here.
   ; Changed fn so that rounding errors are less.                CAE 04DEC93
      fstcw       word [Arg2]         ; use arg2 to hold CW
      fwait
      fldln2                           ; ln(2) x
      fdivp        st1,st0             ; x/ln(2), start the fdivr instr.
      mov          ax, word [Arg2]     ; Now do some integer instr.'s
      push         rax                 ; Save control word on stack
      or           ax,0000110000000000b
      mov          word [Arg2],ax
      fld          st0                 ; x/ln(2), x/ln(2)
      fldcw        word [Arg2]         ; Now set control to round toward zero
   ; Chop toward zero rounding applies now                        CAE 4DEC93
      frndint                          ; int = integer(x/ln(2)), x/ln(2)
      pop          rax                 ; restore old CW to AX
      mov          word [Arg2],ax      ; ...then move it to Arg2
      fldcw        word [Arg2]         ; Restore control word from Arg2
   ; Normal rounding is in effect again                           CAE 4DEC93
      fxch                             ; x/ln(2), int
      fsub         st0,st1             ; -1 < rem < 1.0, int
      f2xm1                            ; 2**rem-1, int
      fadd         qword [_1_]         ; 2**rem, int
      fscale                           ; e**x, int
      fstp         st1                 ; e**x
      fld          st0                 ; e**x, e**x
      fmul         qword [PointFive]   ; e^x/2 e^x
      fstp         QWORD [rdi]         ; e^x  use overflow stk for temp here
      fdivr        qword [PointFive]   ; e**-x/2
      fld          st0                 ; e**-x/2, e**-x/2
      fadd         QWORD [rdi]         ; coshx, e**-x/2
      fxch                             ; e^-x/2, coshx
      fsubr        QWORD [rdi]         ; sinhx, coshx (fsubr pending)
   END_INCL        SinhCosh
; --------------------------------------------------------------------------
   BEGN_INCL       Ident               ; Ident                   CAE 09OCT93
   END_INCL        Ident
; --------------------------------------------------------------------------
   BEGN_INCL       Sqr3                ; Sqr3                    CAE 06NOV93
      fmul         st0,st0             ; Magnitude/sqr of a real# on st
   END_INCL        Sqr3                ; x^2 0 ...
; --------------------------------------------------------------------------
   BEGN_INCL       Conj                ; Complex conjugate
      fxch                             ; y x ...
      fchs                             ; -y x ...
      fxch                             ; x -y ...
   END_INCL        Conj
; --------------------------------------------------------------------------
   BEGN_INCL       Conj2               ; Complex conjugate (uses a reg)
      fldz                             ; 0 x y ...               CAE 20Nov93
      fsubrp       st2,st0             ; x -y ...
   END_INCL        Conj2
; --------------------------------------------------------------------------
   BEGN_INCL       Real                ; Real
      fstp         st1                 ; x ...
      fldz                             ; 0 x ...
      fxch                             ; x 0 ...
   END_INCL        Real
; --------------------------------------------------------------------------
   BEGN_INCL       RealFlip            ; Real, flip combined.
      fstp         st1                 ; y=x ...
      fldz                             ; x=0 y ...
   END_INCL        RealFlip
; --------------------------------------------------------------------------
   BEGN_INCL       Add                 ; Add
      faddp        st2,st0             ; Arg2->d.x += Arg1->d.x;
      faddp        st2,st0             ; Arg2->d.y += Arg1->d.y;
   END_INCL        Add
; --------------------------------------------------------------------------
   BEGN_INCL       Sub                 ; Subtract
      fsubp        st2,st0             ; Arg2->d.x -= Arg1->d.x;
      fsubp        st2,st0             ; Arg2->d.y -= Arg1->d.y;
   END_INCL        Sub
; --------------------------------------------------------------------------
   BEGN_OPER       LodRealAdd          ; Load, Real, Add combined
      FIXUP        LodRealAdd, fadd, X ; Add x-value from memory
   END_OPER        LodRealAdd
; --------------------------------------------------------------------------
   BEGN_OPER       LodRealSub          ; Load, Real, Subtract combined
      FIXUP        LodRealSub, fsub, X ; (fsub qword ptr X)
   END_OPER        LodRealSub
; --------------------------------------------------------------------------
   BEGN_OPER       LodImagAdd          ; Load, Imag, Add combined CAE 4DEC93
      FIXUP        LodImagAdd, fadd, Y ; Add y-value from memory
   END_OPER        LodImagAdd
; --------------------------------------------------------------------------
   BEGN_OPER       LodImagSub          ; Load, Imag, Sub combined CAE 4DEC93
      FIXUP        LodImagSub, fsub, Y ; (fsub qword ptr Y)
   END_OPER        LodImagSub
; --------------------------------------------------------------------------
   BEGN_INCL       Real2               ; Real value (fast version)
      fldz                             ; 0 x y ... (uses a reg)
      fstp         st2                 ; x 0 ...
   END_INCL        Real2
; --------------------------------------------------------------------------
   BEGN_OPER       Lod                 ; Load
      FIXUP        Lod, fld, Y         ; y ...
      FIXUP        Lod, fld, X         ; x y ...
   END_OPER        Lod
; --------------------------------------------------------------------------
   BEGN_INCL       Clr1                ; Clear stack
      finit                            ; changed from fninit     CAE 09OCT93
   END_INCL        Clr1
; --------------------------------------------------------------------------
   BEGN_INCL       Imag                ; Imaginary value
      POP_STK      1                   ; y
      fldz                             ; 0 y
      fxch                             ; x=y 0
   END_INCL        Imag
; --------------------------------------------------------------------------
   BEGN_INCL       ImagFlip            ; Imaginary value, flip combined
      POP_STK      1                   ; y ...
      fldz                             ; x=0 y ...
   END_INCL        ImagFlip
; --------------------------------------------------------------------------
   BEGN_INCL Abs                       ; Absolute value
      fxch
      fabs
      fxch
      fabs
   END_INCL Abs
; --------------------------------------------------------------------------
   BEGN_OPER       LodRealMul          ; Load, Real, Multiply
      FIXUP        LodRealMul, fld, X  ; y.x x.x x.y
      fmul         st2,st0             ; y.x x.x z.y
      fmul                             ; z.x z.y
   END_OPER        LodRealMul
; --------------------------------------------------------------------------
   BEGN_OPER       LodImagMul          ; Load, Imag, Multiply     CAE 4DEC93
      FIXUP        LodImagMul, fld, Y  ; y.y x.x x.y
      fmul         st2,st0             ; y.y x.x z.y
      fmul                             ; z.x z.y
   END_OPER        LodImagMul
; --------------------------------------------------------------------------
   BEGN_INCL       Neg                 ; Negative
      fxch
      fchs                             ; Arg1->d.y = -Arg1->d.y;
      fxch
      fchs
   END_INCL        Neg
; --------------------------------------------------------------------------
   BEGN_OPER       EndInit             ; End of initialization expr.
%ifndef            COMPILER            ; this instr not needed   CAE 30DEC93
      mov          qword [LastInitOp],rbx      ; LastInitOp=OpPtr
%endif
      mov          eax, dword [jump_index]     ; InitJumpIndex=jump_index TIW 06Mar97
      mov          dword [InitJumpIndex],eax
      finit                            ; changed from fninit     CAE 09OCT93
   END_OPER        EndInit
; --------------------------------------------------------------------------
   BEGN_OPER       StoClr1             ; Store, clear FPU
      FIXUP        StoClr1, fstp, X    ; y ...
      FIXUP        StoClr1, fst, Y     ; y ...
      finit                            ; use finit, not fninit
   END_OPER        StoClr1
; --------------------------------------------------------------------------
   BEGN_OPER       StoClr2             ; Store, clear FPU        CAE 09OCT93
      FIXUP        StoClr2, fstp, X    ; y
      FIXUP        StoClr2, fstp, Y    ; <empty> (store pending)
   END_OPER        StoClr2
; --------------------------------------------------------------------------
   BEGN_OPER       Sto                 ; Store, leave on ST
   ; Revised to do store first, then exchange.                   CAE 10NOV93
      FIXUP        Sto, fst, X
      fxch                             ; y x ...
      FIXUP        Sto, fst, Y
      fxch                             ; x y ...
   END_OPER        Sto
; --------------------------------------------------------------------------
   BEGN_OPER       Sto2                ; Store, leave on ST (uses a reg)
      fld          st1                 ; y x y
      FIXUP        Sto2, fstp, Y       ; x y
      FIXUP        Sto2, fst, X
      fwait                            ; CAE added fwait for safety 15Feb95
   END_OPER        Sto2
; --------------------------------------------------------------------------
   BEGN_OPER       LodReal             ; Load a real
      fldz                             ; 0 ...
      FIXUP        LodReal, fld, X     ; x 0 ...
   END_OPER        LodReal
; --------------------------------------------------------------------------
   BEGN_OPER       LodRealC            ; Load real const
      fldz                             ; y=0 ...
      FIXUP        LodRealC, fld, X    ; x 0 ...
   END_OPER        LodRealC
; --------------------------------------------------------------------------
   BEGN_OPER       LodRealFlip         ; Load real, flip
      FIXUP        LodRealFlip, fld, X ; y=x ...
      fldz                             ; x=0 y ...
   END_OPER        LodRealFlip
; --------------------------------------------------------------------------
   BEGN_OPER       LodRealAbs          ; Load real, abs
      fldz                             ; 0 ...
      FIXUP        LodRealAbs, fld, X  ; x 0 ...
      fabs                             ; x=abs(x) 0 ...
   END_OPER        LodRealAbs
; --------------------------------------------------------------------------
   BEGN_INCL       Flip                ; Exchange real, imag
      fxch                             ; x=y y=x ...
   END_INCL        Flip
; --------------------------------------------------------------------------
   BEGN_OPER       LodImag             ; Load, imaginary
      fldz                             ; 0 ...
      FIXUP        LodImag, fld, Y     ; x=y 0
   END_OPER        LodImag
; --------------------------------------------------------------------------
   BEGN_OPER       LodImagFlip         ; Load, imaginary, flip
      FIXUP        LodImagFlip, fld, Y ; y ...
      fldz                             ; 0 y ...
   END_OPER        LodImagFlip
; --------------------------------------------------------------------------
   BEGN_OPER       LodImagAbs          ; Load, imaginary, absolute value
      fldz                             ; 0 ...
      FIXUP        LodImagAbs, fld, Y  ; x=y 0 ...
      fabs                             ; x=abs(y) 0 ...
   END_OPER        LodImagAbs
; --------------------------------------------------------------------------
   BEGN_OPER       LodConj             ; Load, conjugate
      FIXUP        LodConj, fld, Y     ; y ...
      fchs                             ; y=-y ...
      FIXUP        LodConj, fld, X     ; x y ...
   END_OPER        LodConj
; --------------------------------------------------------------------------
   BEGN_OPER       LodAdd              ; Load, Add (uses a reg)
      FIXUP        LodAdd, fadd, X
      FIXUP        LodAdd, fld, Y
      faddp        st2,st0
   END_OPER        LodAdd
; --------------------------------------------------------------------------
   BEGN_OPER       LodSub              ; Load, Subtract (uses a reg)
      FIXUP        LodSub, fsub, X
      FIXUP        LodSub, fld, Y
      fsubp        st2,st0
   END_OPER        LodSub
; --------------------------------------------------------------------------
   BEGN_OPER       StoDup              ; Store, duplicate top operand
      FIXUP        StoDup, fst, X      ; x y
      fld          st1                 ; y x y
      FIXUP        StoDup, fst, Y      ; y x y
      fld          st1                 ; x y x y
   END_OPER        StoDup
; --------------------------------------------------------------------------
   BEGN_OPER       StoDbl              ; Store, double (uses a reg)
      FIXUP        StoDbl, fst, X      ; x y (store x)
      fadd         st0,st0             ; 2x y
      fld          st1                 ; y 2x y
      FIXUP        StoDbl, fst, Y      ; y 2x y (store y)
      faddp        st2,st0             ; 2x 2y
   END_OPER        StoDbl
; --------------------------------------------------------------------------
   BEGN_INCL       Zero                ; Zero                    CAE 09OCT93
      POP_STK      2                   ; ...
      fldz                             ; 0 ...
      fldz                             ; 0 0 ...
   END_INCL        Zero
; --------------------------------------------------------------------------
   BEGN_INCL       One                 ; One                     CAE 06NOV93
      POP_STK      2                   ; ...
      fldz                             ; 0 ...
      fld1                             ; 1 0 ...
   END_INCL        One
; --------------------------------------------------------------------------
   BEGN_OPER       LodSubMod           ; Load, Subtract, Mod
      FIXUP        LodSubMod, fsub, X  ; x.x-y.x  x.y  ...
      fmul         st0,st0             ; sqr(x.x-y.x) x.y ...
      fldz                             ; 0 sqrx x.y ...
      fxch         st2                 ; x.y sqrx 0 ...
      FIXUP        LodSubMod, fsub, Y  ; x.y-y.y sqrx 0 ...
      fmul         st0,st0             ; sqry sqrx 0 ...
      fadd                             ; mod 0
   END_OPER        LodSubMod
; --------------------------------------------------------------------------
   BEGN_INCL       Sqr                 ; Square, save magnitude in LastSqr
      fld          st0                 ; x x y
      fmul         st1,st0             ; x x*x y
      fmul         st0,st2             ; xy xx y
      mov          rsi, qword [v]      ; rsi -> variables
      fadd         st0,st0             ; 2xy xx y
      fxch         st2                 ; y xx 2xy
      fmul         st0,st0             ; yy xx 2xy
      fld          st1                 ; xx yy xx 2xy
      fadd         st0,st1             ; xx+yy yy xx 2xy
      fstp         QWORD [rsi + LASTSQR] ; yy xx 2xy
      fsubp        st1,st0             ; xx-yy 2xy
   END_INCL        Sqr
; --------------------------------------------------------------------------
   BEGN_INCL       Sqr0                ; Square, don't save magnitude
       GEN_SQR0
   END_INCL        Sqr0
; --------------------------------------------------------------------------
   BEGN_INCL       Mul                 ; Multiply
   ; From FPU087.ASM
      fld          st1                 ; y.y, y.x, y.y, x.x, x.y
      fmul         st0,st4             ; y.y*x.y, y.x. y.y, x.x, x.y
      fld          st1                 ; y.x, y.y*x.y, y.x, y.y, x.x, x.y
      fmul         st0,st4             ; y.x*x.x,y.y*x.y,y.x y.y,x.x,x.y
      fsubr                            ; newx=y.x*x.x-y.y*x.y,y.x,y.y,x.x,x.y
      fxch         st3                 ; x.x, y.x, y.y, newx, x.y
      fmulp        st2,st0             ; y.x, y.y*x.x, newx, x.y
      fmulp        st3,st0             ; y.y*x.x, newx, y.x*x.y
      faddp        st2,st0             ; newx newy = y.x*x.y + x.x*y.y
   END_INCL        Mul
; --------------------------------------------------------------------------
   BEGN_OPER       LodMul              ; Load, Multiply
   ; This is just load followed by multiply but it saves a fn. call
   ;    and also allows optimizer enhancements.
      FIXUP        LodMul, fld, Y      ; y.y x.x x.y
      FIXUP        LodMul, fld, X      ; y.x y.y x.x x.y
      fld          st1                 ; y.y, y.x, y.y, x.x, x.y
      fmul         st0,st4             ; y.y*x.y, y.x. y.y, x.x, x.y
      fld          st1                 ; y.x, y.y*x.y, y.x, y.y, x.x, x.y
      fmul         st0, st4            ; y.x*x.x, y.y*x.y, y.x, y.y, x.x, x.y
      fsubr                            ; newx=y.x*x.x-y.y*x.y,y.x,y.y,x.x,x.y
      fxch         st3                 ; x.x, y.x, y.y, newx, x.y
      fmulp        st2, st0            ; y.x, y.y*x.x, newx, x.y
      fmulp        st3, st0            ; y.y*x.x, newx, y.x*x.y
      faddp        st2, st0            ; newx newy = y.x*x.y + x.x*y.y
   END_OPER        LodMul
; --------------------------------------------------------------------------
   BEGN_INCL       Div                 ; Divide
   ; From FPU087.ASM with speedups
      fld          st1                 ; y.y,y.x,y.y,x.x,x.y
      fmul         st0,st0             ; y.y*y.y,y.x,y.y,x.x,x.y
      fld          st1                 ; y.x,y.y*y.y,y.x,y.y,x.x,x.y
      fmul         st0,st0             ; y.x*y.x,y.y*y.y,y.x,y.y,x.x,x.y
      fadd                             ; mod,y.x,y.y,x.x,x.y
      ftst
      fstsw       ax
      sahf
      jz           short .DivNotOk
                                       ; can't do this divide until now
      fdiv         st1,st0             ; mod,y.x=y.x/mod,y.y,x.x,x.y
      fdivp        st2,st0             ; y.x,y.y=y.y/mod,x.x,x.y
      fld          st1                 ; y.y,y.x,y.y,x.x,x.y
      fmul         st0,st4             ; y.y*x.y,y.x,y.y,x.x,x.y
      fld          st1                 ; y.x,y.y*x.y,y.x,y.y,x.x,x.y
      fmul         st0,st4             ; y.x*x.x,y.y*x.y,y.x,y.y,x.x,x.y
      fadd                             ; y.x*x.x+y.y*x.y,y.x,y.y,x.x,x.y
      fxch         st3                 ; x.x,y.x,y.y,newx,x.y
      fmulp        st2,st0             ; y.x,y.y*x.x,newx,x.y
      fmulp        st3,st0             ; x.x*y.y,newx,y.x*x.y
      fsubp        st2,st0             ; newx,newy
      EXIT_OPER    Div
.DivNotOk:
      POP_STK      5                   ; clear 5 from stack (!)
      fld          qword [infinity]    ; return a very large number
      fld          st0
      mov          eax, dword [save_release]
      cmp          eax,1920
      jle          short .oldwayD
      mov          dword [overflow], 1
.oldwayD:
   END_INCL        Div
; --------------------------------------------------------------------------
   BEGN_INCL       Recip               ; Reciprocal
   ; From FPU087.ASM
      fld          st1                 ; y, x, y
      fmul         st0,st0             ; y*y, x, y
      fld          st1                 ; x, y*y, x, y
      fmul         st0,st0             ; x*x, y*y, x, y
      fadd                             ; mod, x, y
      ftst
      fstsw       ax
      sahf
      jz           short .RecipNotOk
      fdiv         st1,st0             ; mod, newx=x/mod, y
      fchs                             ; -mod newx y
      fdivp        st2,st0             ; newx, newy=y/-mod
      EXIT_OPER    Recip
.RecipNotOk:
      POP_STK      3                   ; clear three from stack
      fld          qword [infinity]    ; return a very large number
      fld          st0
      mov          eax, dword [save_release]
      cmp          eax,1920
      jle          short .oldwayR
      mov          dword [overflow], 1
.oldwayR:
   END_INCL        Recip
; --------------------------------------------------------------------------
   BEGN_OPER       StoSqr              ; Sto, Square, save magnitude
      fld          st0                 ; x x y
      FIXUP        StoSqr, fst, X      ;   "   (store x)
      fmul         st1,st0             ; x x*x y
      fmul         st0,st2             ; xy xx y
      fadd         st0,st0             ; 2xy xx y
      fxch         st2                 ; y xx 2xy
      FIXUP        StoSqr, fst, Y      ;    "     (store y)
      fmul         st0,st0             ; yy xx 2xy
   ; It is now safe to overlay rsi here
      mov          rsi, qword [v]      ; rsi -> variables
      fld          st1                 ; xx yy xx 2xy
      fadd         st0,st1             ; xx+yy yy xx 2xy
      fstp         QWORD [rsi + LASTSQR] ; yy xx 2xy
      fsubp        st1,st0             ; xx-yy 2xy
   END_OPER        StoSqr
; --------------------------------------------------------------------------
   BEGN_OPER       StoSqr0             ; Sto, Square, don't save magnitude
      fld          st0                 ; x x y
      FIXUP        StoSqr0, fst, X     ; store x
      fld          st0                 ; x x x y
      fmul         st0,st3             ; xy x x y
      fadd         st0,st0             ; 2xy x x y
      fxch         st3                 ; y x x 2xy
      FIXUP        StoSqr0, fst, Y     ; store y
      fadd         st2,st0             ; y x x+y 2xy
      fsubp        st1,st0             ; x-y x+y 2xy
      fmulp        st1,st0             ; xx-yy 2xy
   END_OPER        StoSqr0
; --------------------------------------------------------------------------
   BEGN_INCL       Mod2                ; Modulus (uses a reg)
      fmul         st0,st0             ; xx y
      fldz                             ; 0 xx y
      fxch         st2                 ; y xx 0
      fmul         st0,st0             ; yy xx 0
      fadd                             ; mod 0
   END_INCL        Mod2
; --------------------------------------------------------------------------
   BEGN_OPER       LodMod2             ; Load, Modulus (uses a reg)
      fldz                             ; 0 ...
      FIXUP        LodMod2, fld, X     ; x 0 ...
      fmul         st0,st0             ; xx 0
      FIXUP        LodMod2, fld, Y     ; y xx 0
      fmul         st0,st0             ; yy xx 0
      fadd                             ; mod 0
   END_OPER        LodMod2
; --------------------------------------------------------------------------
   BEGN_OPER       StoMod2             ; Store, Modulus (uses a reg)
      FIXUP        StoMod2, fst, X     ; x y
      fmul         st0,st0             ; xx y
      fldz                             ; 0 xx y
      fxch         st2                 ; y xx 0
      FIXUP        StoMod2, fst, Y     ; y xx 0
      fmul         st0,st0             ; yy xx 0
      fadd                             ; mod 0
   END_OPER        StoMod2
; --------------------------------------------------------------------------
   BEGN_OPER       Clr2                ; Test ST, clear FPU
      ftst
      fstsw       ax
                                       ;                      CAE 1 Dec 1998
      ALTER_RET_ADDR                   ; change return address on stack

      fninit                           ; fstsw will complete first
;      and          ah, 01000000b    ; return 1 if zf=1
      and          rax, 4000h    ; return 1 if zf=1
      shr          rax,14              ; RAX will be returned by fFormula()
   END_OPER        Clr2
; --------------------------------------------------------------------------
   BEGN_OPER       PLodAdd             ; Load, Add (uses no regs)
      fxch                             ; y x
      FIXUP        PLodAdd, fadd, Y    ; add y from memory
      fxch                             ; x y
      FIXUP        PLodAdd, fadd, X    ; add x, overlap execution
   END_OPER        PLodAdd
; --------------------------------------------------------------------------
   BEGN_OPER       PLodSub             ; Load, Subtract (uses no regs)
      fxch
      FIXUP        PLodSub, fsub, Y    ; sub y from memory
      fxch                             ; x y
      FIXUP        PLodSub, fsub, X    ; sub x, overlap execution
   END_OPER        PLodSub
; --------------------------------------------------------------------------
   BEGN_OPER       LodDup              ; Load, duplicate
      FIXUP        LodDup, fld, Y      ; y ...
      FIXUP        LodDup, fld, X      ; x y ...
      fld          st1                 ; y x y ...
      fld          st1                 ; x y x y ...
   END_OPER        LodDup
; --------------------------------------------------------------------------
   BEGN_OPER       LodSqr              ; Load, square (no save lastsqr)
      FIXUP        LodSqr, fld, Y      ; y ...
      fld          st0                 ; y y ...
      fadd         st1,st0             ; y 2y ...
      fld          st0                 ; y y 2y
      FIXUP        LodSqr, fld, X      ; x y y 2y ...
      fmul         st3,st0             ; x y y 2xy ...
      fadd         st2,st0             ; x y X+y 2xy ...
      fsubrp       st1,st0             ; x-y x+y 2xy ...
      fmul                             ; xx-yy 2xy ...
   END_OPER        LodSqr
; --------------------------------------------------------------------------
   BEGN_OPER       LodSqr2             ; Load, square (save lastsqr)
      FIXUP        LodSqr2, fld, Y     ; y ...
      fld          st0                 ; y y ...
      fadd         st1,st0             ; y 2y ...
      fmul         st0,st0             ; yy 2y ...
      FIXUP        LodSqr2, fld, X     ; x yy 2y ...
      fmul         st2,st0             ; x yy 2xy ...
      mov          rsi, qword [v]      ; put address of v in rsi
      fmul         st0,st0             ; xx yy 2xy ...
      fld          st0                 ; xx xx yy 2xy
      fadd         st0,st2             ; mod xx yy 2xy
      fstp         QWORD [rsi + LASTSQR] ; xx yy 2xy ... (save lastsqr)
      fsubrp       st1,st0             ; xx-yy 2xy ...
   END_OPER        LodSqr2
; --------------------------------------------------------------------------
   BEGN_OPER       LodDbl              ; Load, double
      FIXUP        LodDbl, fld, Y      ; load y
      fadd         st0,st0             ; double it
      FIXUP        LodDbl, fld, X      ; same for x
      fadd         st0,st0
   END_OPER        LodDbl
; --------------------------------------------------------------------------
   BEGN_INCL       Dbl                 ; Double                  CAE 31OCT93
      fxch                             ; y x ...
      fadd         st0,st0             ; 2y x ...
      fxch                             ; x 2y ...
      fadd         st0,st0             ; 2x 2y ...
   END_INCL        Dbl
; --------------------------------------------------------------------------
   BEGN_INCL       Mod                 ; Modulus (uses no regs)
      fmul         st0,st0             ; x*x y
      fxch                             ; y x*x
      fmul         st0,st0             ; y*y x*x
      fadd                             ; mod
      fldz                             ; 0 mod
      fxch                             ; mod 0
   END_INCL        Mod
; --------------------------------------------------------------------------
; The following code was 'discovered' by experimentation.  The Intel manuals
;   really don't help much in writing this kind of code.
; --------------------------------------------------------------------------
   BEGN_INCL       Push2               ; Push stack down from 8 to 6
      fdecstp                            ; roll the stack
      fdecstp                            ; ...
      fstp         tword [rdi]         ; store x on overflow stack
      fstp         tword [rdi + 10]  ; and y (ten bytes each)
      add          rdi, 20              ; adjust rdi
   END_INCL        Push2
; --------------------------------------------------------------------------
   BEGN_INCL       Pull2        ; Pull stack up from 2 to 4
      fld          tword [rdi - 10]  ; oldy x y
      sub          rdi, 20             ; adjust rdi now
      fxch         st2                  ; y x oldy
      fld          tword [rdi]         ; oldx y x oldy
      fxch         st2                  ; x y oldx oldy
   END_INCL        Pull2
; --------------------------------------------------------------------------
   BEGN_INCL       Push4               ; Push stack down from 8 to 4
      fdecstp                          ; roll the stack four times
      fdecstp
      fdecstp
      fdecstp
      fstp         tword [rdi + 20]         ; save the bottom four numbers
      fstp         tword [rdi + 30]         ; save full precision on overflow
      fstp         tword [rdi]
      fstp         tword [rdi + 10]
      add          rdi, 40         ; adjust rdi
   END_INCL        Push4
; --------------------------------------------------------------------------
   BEGN_INCL       Push2a              ; Push stack down from 6 to 4
      fdecstp                          ; roll the stack 4 times
      fdecstp
      fdecstp
      fdecstp
      fstp         tword [rdi]         ; save only two numbers
      fstp         tword [rdi + 10]
      add          rdi, 20
      fincstp                          ; roll back 2 times
      fincstp
   END_INCL        Push2a
; --------------------------------------------------------------------------
; End of stack overflow/underflow code.
; --------------------------------------------------------------------------
   BEGN_INCL       Exp                 ; Exponent
   ; From FPU387.ASM with mods to use less registers.
   ; Modified to preserve 80-bit accuracy.                      CAE 10NOV93
      fldln2                           ; ln2 x y
      fdivp        st1,st0             ; x/ln2 y
      fstp         tword [rdi]         ; y
      fsincos                          ; cosy, siny
      fld1                             ; 1 cos sin
      fld          tword [rdi]         ; x/ln2 1 cos sin
      fprem                            ; prem, 1, cos, sin
      f2xm1                            ; e**prem-1, 1, cos, sin
      fadd                             ; e**prem, cos, sin
      fld          tword [rdi]         ; x.x/ln2, e**prem, cos, sin
      fxch                             ; e**prem, x.x/ln2, cos, sin
      fscale                           ; e**x.x, x.x/ln2, cos, sin
      fstp         st1                 ; e**x.x, cos, sin
      fmul         st2,st0             ; e**x.x, cos, z.y
      fmul                             ; z.x, z.y
   END_INCL        Exp
; --------------------------------------------------------------------------
   BEGN_OPER       Pwr                 ; Power
   ; First exchange the top two complex numbers.
      fxch         st2                 ; x.x y.y y.x x.y
      fxch                             ; y.y x.x y.x x.y
      fxch         st3                 ; x.y x.x y.x y.y
      fxch                             ; x.x x.y y.x y.y
   ; Now take the log of the # on st.
      INCL_OPER    Pwr, Log            ; l.x l.y y.x y.y
      cmp          ax,1                ; log domain error?
      jne          short .domainok     ; nope
      test         dword [ldcheck], 1  ; user wants old pwr?
      jnz          short .domainok     ; yup
      POP_STK      4                   ; clear stack completely
      fldz                             ; 0
      fldz                             ; 0 0
      EXIT_OPER    Pwr                 ; return (0,0)
   PARSALIGN
.domainok:
   ; Inline multiply function from FPU087.ASM instead of include.
      fld          st1                 ; y.y y.x y.y x.x x.y
      fmul         st0,st4             ; y.y*x.y y.x y.y x.x x.y
      fld          st1                 ; y.x y.y*x.y y.x y.y x.x x.y
      fmul         st0,st4             ; y.x*x.x y.y*x.y y.x y.y x.x x.y
      fsubr                            ; newx=y.x*x.x-y.y*x.y y.x y.y x.x x.y
      fxch         st3                 ; x.x y.x y.y newx x.y
      fmulp        st2,st0             ; y.x y.y*x.x newx x.y
      fmulp        st3,st0             ; y.y*x.x newx y.x*x.y
      faddp        st2,st0             ; newx newy=y.x*x.y+x.x*y.y
   ; Exp function from FPU387.ASM.  4 regs are free here.
   ; Modified to use the regs instead of memory.                 CAE 06NOV93
      fldln2                           ; ln2 x y
      fdiv                             ; x/ln2 y
      fxch                             ; y x/ln2
      fsincos                          ; cosy, siny, x/ln2
      fxch                             ; sin, cos, x/ln2
      fxch         st2                 ; x/ln2, cos, sin
      fld1                             ; 1, x/ln2, cos, sin
      fld          st1                 ; x/ln2, 1, x/ln2, cos, sin
      fprem                            ; prem, 1, x/ln2, cos, sin
      f2xm1                            ; e**prem-1, 1, x/ln2, cos, sin
      fadd                             ; e**prem, x/ln2, cos, sin
      fscale                           ; e**x.x, x.x/ln2, cos, sin
      fstp         st1                 ; e**x.x, cos, sin
      fmul         st2,st0             ; e**x.x, cos, z.y
      fmul                             ; z.x, z.y
   END_OPER        Pwr
; --------------------------------------------------------------------------
   BEGN_OPER       LodRealPwr          ; lod, real, power         CAE 6NOV93
   ; First take the log of the # on st.
      INCL_OPER    LodRealPwr, Log     ; l.x l.y
      cmp          ax,1                ; log domain error?
      jne          short .domainok2    ; nope
      cmp          dword [ldcheck], 1  ; user wants old lodrealpwr?
      je           short .domainok2    ; yup
      POP_STK      2                   ; clear stack completely
      fldz                             ; 0
      fldz                             ; 0 0
      EXIT_OPER    LodRealPwr          ; return (0,0)
   PARSALIGN
.domainok2:
   ; Inline multiply by a real.
      FIXUP        LodRealPwr, fld, X  ; y.x, x.x, x.y
      fmul         st2,st0             ; y.x, x.x, z.y
      fmulp        st1,st0             ; z.x z.y
   ; Exp function from FPU387.ASM.  4 regs are free here, so use them.
      fldln2                           ; ln2 x y
      fdiv                             ; x/ln2 y
      fxch                             ; y x/ln2
      fsincos                          ; cosy, siny, x/ln2
      fxch                             ; sin, cos, x/ln2
      fxch         st2                 ; x/ln2, cos, sin
      fld1                             ; 1, x/ln2, cos, sin
      fld          st1                 ; x/ln2, 1, x/ln2, cos, sin
      fprem                            ; prem, 1, x/ln2, cos, sin
      f2xm1                            ; e**prem-1, 1, x/ln2, cos, sin
      fadd                             ; e**prem, x/ln2, cos, sin
      fscale                           ; e**x.x, x.x/ln2, cos, sin
      fstp         st1                 ; e**x.x, cos, sin
      fmul         st2,st0             ; e**x.x, cos, z.y
      fmul                             ; z.x, z.y
   END_OPER        LodRealPwr
; --------------------------------------------------------------------------
   BEGN_OPER       Cosh                ; Cosh
      INCL_OPER    Cosh, SinhCosh      ; sinhx coshx y
      fxch         st2                 ; y coshx sinhx
      fsincos                          ; cosy siny coshx sinhx
      fmulp        st2,st0             ; siny x=cosy*coshx sinhx
      fmulp        st2,st0             ; x y=sinhx*siny
   END_OPER        Cosh
; --------------------------------------------------------------------------
   BEGN_OPER       Sinh                ; Sinh
      INCL_OPER    Sinh, SinhCosh      ; sinhx coshx y
      fxch         st2                 ; y coshx sinhx
      fsincos                          ; cosy siny coshx sinhx
      fmulp        st3,st0             ; siny coshx x=sinhx*cosy
      fmulp        st1,st0             ; y=coshx*siny x
      fxch                             ; x y
   END_OPER        Sinh
; --------------------------------------------------------------------------
   BEGN_OPER       Sin                 ; Sin
      fsincos                          ; cosx sinx y
      fxch         st2                 ; y sinx cosx
      INCL_OPER    Sin, SinhCosh       ; sinhy coshy sinx cosx
      fmulp        st3,st0             ; coshy sinx y=cosx*sinhy
      fmulp        st1,st0             ; x=sinx*coshy y
   END_OPER        Sin
; --------------------------------------------------------------------------
   BEGN_OPER       Cos                 ; Cos
      fsincos                          ; cosx sinx y
      fxch         st2                 ; y sinx cosx
      INCL_OPER    Cos, SinhCosh       ; sinhy coshy sinx cosx
      fchs                             ; -sinhy coshy sinx cosx
      fmulp        st2,st0             ; coshy y=-sinhy*sinx cosx
      fmulp        st2,st0             ; y x=cosx*coshy
      fxch                             ; x y
   END_OPER        Cos
; --------------------------------------------------------------------------
   BEGN_OPER       CosXX               ; CosXX
      fsincos                          ; cosx sinx y
      fxch         st2                 ; y sinx cosx
      INCL_OPER    CosXX, SinhCosh     ; sinhy coshy sinx cosx
      ; note missing fchs here
      fmulp        st2,st0             ; coshy y=sinhy*sinx cosx
      fmulp        st2,st0             ; y x=cosx*coshy
      fxch                             ; x y
   END_OPER        CosXX
; --------------------------------------------------------------------------
   BEGN_OPER       Tan                 ; Tan
      fadd         st0,st0             ; 2x y
      fsincos                          ; cos2x sin2x y
      fxch         st2                 ; y sin2x cos2x
      fadd         st0,st0             ; 2y sin2x cos2x
      INCL_OPER    Tan, SinhCosh       ; sinh2y cosh2y sin2x cos2x
      fxch                             ; cosh2y sinh2y sin2x cos2x
      faddp        st3,st0             ; sinhy sinx denom=cos2x+cosh2y
      fld          st2                 ; denom sinh2y sin2x denom
      fdivp        st2,st0             ; sinh2y x=sin2x/denom denom
      fdivrp       st2,st0             ; x y=sinh2y/denom
   END_OPER        Tan
; --------------------------------------------------------------------------
   BEGN_OPER       CoTan               ; CoTan
      fadd         st0,st0             ; 2x y
      fsincos                          ; cos2x sin2x y
      fxch         st2                 ; y sin2x cos2x
      fadd         st0,st0             ; 2y sin2x cos2x
      INCL_OPER    CoTan, SinhCosh     ; sinh2y cosh2y sin2x cos2x
      fxch                             ; cosh2y sinh2y sin2x cos2x
      fsubrp       st3,st0             ; sinh2y sin2x denom=cosh2y-cos2x
      fld          st2                 ; denom sinh2y sin2x denom
      fdivp        st2,st0             ; sinh2y x=sin2x/denom denom
      fchs                             ; -sinh2y x denom
      fdivrp       st2,st0             ; x y=-sinh2y/denom
   END_OPER        CoTan
; --------------------------------------------------------------------------
   BEGN_OPER       Tanh                ; Tanh
      fadd         st0,st0             ; 2x y
      INCL_OPER    Tanh, SinhCosh      ; sinh2x cosh2x y
      fxch         st2                 ; y cosh2x sinh2x
      fadd         st0,st0             ; 2y cosh2x sinh2x
      fsincos                          ; cos2y sin2y cosh2x sinh2x
      faddp        st2,st0             ; sin2y denom=cos2y+cosh2x sinh2x
      fxch                             ; denom sin2y sinh2x
      fdiv         st1,st0             ; denom y=sin2y/denom sinh2x
      fdivp        st2,st0             ; y x=sinh2x/denom
      fxch                             ; x y
   END_OPER        Tanh
; --------------------------------------------------------------------------
   BEGN_OPER       CoTanh              ; CoTanh
      fadd         st0,st0             ; 2x y
      INCL_OPER    CoTanh, SinhCosh    ; sinh2x cosh2x y
      fxch         st2                 ; y cosh2x sinh2x
      fadd         st0,st0             ; 2y cosh2x sinh2x
      fsincos                          ; cos2y sin2y cosh2x sinh2x
      fsubp        st2,st0             ; sin2y denom=cosh2x-cos2y sinh2x
      fchs                             ; -sin2y denom sinh2x
      fxch                             ; denom -sin2y sinh2x
      fdiv         st1,st0             ; denom y=-sin2y/denom sinh2x
      fdivp        st2,st0             ; y x=sinh2x/denom
      fxch                             ; x y
   END_OPER CoTanh
; --------------------------------------------------------------------------
; JCO added Sqrt .. CAbs for version 19.
; CAE updated them 15Feb94 to work with compiler mode.
; --------------------------------------------------------------------------
   BEGN_OPER       Sqrt                ; Sqrt
      GEN_SQRT
   END_OPER Sqrt
; --------------------------------------------------------------------------
   BEGN_OPER       ASin                ; ArcSin
      fld          st1                 ; y x y
      fld          st1                 ; x y x y
      GEN_SQR0                         ; tz1.x tz1.y x y
      fxch         st1                 ; tz1.y tz1.x x y
      fchs                             ; -tz1.y tz1.x x y
      fxch         st1                 ; tz1.x -tz1.y x y
      fsubr        qword [_1_]         ; 1-tz1.x -tz1.y x y
      GEN_SQRT                         ; tz1.x tz1.y x y
      fsubrp       st3,st0             ; tz1.y x tz1.x-y
      fadd                             ; tz1.y+x tz1.x-y
      fxch         st1                 ; tz1.x-y tz1.y+x
      INCL_OPER    ASin, Log           ; l.x l.y
      fchs                             ; -l.x l.y
      fxch         st1                 ; l.y -l.x ;; rz = (-i)*l
   END_OPER ASin
; --------------------------------------------------------------------------
   BEGN_OPER       ACos                ; ArcCos
      fld          st1                 ; y x y
      fld          st1                 ; x y x y
      GEN_SQR0                         ; tz1.x tz1.y x y
      fsub         qword [_1_]         ; tz1.x-1 tz1.y x y
      GEN_SQRT                         ; tz.x tz.y x y
      faddp        st2,st0             ; tz.y tz.x+x y
      faddp        st2,st0             ; tz.x+x tz.y+y
      INCL_OPER    ACos, Log           ; l.x l.y
      fchs                             ; -l.x l.y
      fxch         st1                 ; l.y -l.x ;; rz = (-i)*l
   END_OPER ACos
; --------------------------------------------------------------------------
   BEGN_OPER       ASinh               ; ArcSinh
      fld          st1                 ; y x y
      fld          st1                 ; x y x y
      GEN_SQR0                         ; tz1.x tz1.y x y
      fadd         qword [_1_]         ; tz1.x+1 tz1.y x y
      GEN_SQRT                         ; tz.x tz.y x y
      faddp        st2,st0             ; tz.y tz.x+x y
      faddp        st2,st0             ; tz.x+x tz.y+y
      INCL_OPER    ASinh, Log          ; l.x l.y
   END_OPER ASinh
; --------------------------------------------------------------------------
   BEGN_OPER       ACosh               ; ArcCosh
      fld          st1                 ; y x y
      fld          st1                 ; x y x y
      GEN_SQR0                         ; tz1.x tz1.y x y
      fsub         qword [_1_]         ; tz1.x+1 tz1.y x y
      GEN_SQRT                         ; tz.x tz.y x y
      faddp        st2,st0             ; tz.y tz.x+x y
      faddp        st2,st0             ; tz.x+x tz.y+y
      INCL_OPER    ACosh, Log          ; l.x l.y
   END_OPER ACosh
; --------------------------------------------------------------------------
   BEGN_OPER       ATanh               ; ArcTanh
      fld          st1                 ; y x y
      fchs                             ; -y x y
      fld          st1                 ; x -y x y
      fld1                             ; 1 x -y x y
      fadd         st3,st0             ; 1 x -y 1+x y
      fsubr                            ; 1-x -y 1+x y
      INCL_OPER    ATanh, Div          ; d.x d.y
   ; From FPU387.ASM
      ftst
      fstsw        ax
;      sahf
;      jnz          short .ATanh_NotBothZero
      and          rax, 4000h   ; 0 if 0 flag not set
      jz           short .ATanh_NotBothZero
      fxch                             ; y x
      ftst
      fstsw        ax
;      sahf
      and          rax, 4000h   ; 0 if 0 flag not set
      fxch                             ; x y
;      jnz          short .ATanh_NotBothZero
      jz           short .ATanh_NotBothZero
      POP_STK      2                   ; clear two numbers
      fldz
      fldz
      jmp          short .End_Log_ATanh ; return (0,0)
   PARSALIGN
.ATanh_NotBothZero:
      fld          st1                 ; y x y
      fld          st1                 ; x y x y
      fpatan                           ; z.y x y
      fxch         st2                 ; y x z.y
      fmul         st0,st0             ; yy x z.y
      fxch                             ; x yy z.y
      fmul         st0,st0             ; xx yy z.y
      fadd                             ; mod z.y
      fldln2                           ; ln2, mod, z.y
      fmul         qword [PointFive]   ; ln2/2, mod, z.y
      fxch                             ; mod, ln2/2, z.y
      fyl2x                            ; z.x, z.y
.End_Log_ATanh:
      fld          qword [PointFive]   ; .5 l.x l.y
      fmul         st1,st0             ; .5 l.x/2 l.y
      fmulp        st2,st0             ; l.x/2 l.y/2
   END_OPER ATanh
; --------------------------------------------------------------------------
   BEGN_OPER       ATan                ; ArcTan
      fxch                             ; y x
      fld          st1                 ; x y x
      fchs                             ; -x y x
      fxch         st2                 ; x y -x
      fld          st1                 ; y x y -x
      fld1                             ; 1 y x y -x
      fadd         st3,st0             ; 1 y x 1+y -x
      fsubr                            ; 1-y x 1+y -x
      INCL_OPER    ATan, Div           ; d.x d.y
   ; CAE put log fn inline 15Feb95
      ftst
      fstsw        ax
;      sahf
;      jnz          short .ATan_NotBothZero
      and          rax, 4000h   ; 0 if 0 flag not set
      jz           short .ATan_NotBothZero
      fxch                             ; y x
      ftst
      fstsw        ax
;      sahf
      and          rax, 4000h   ; 0 if 0 flag not set
      fxch                             ; x y
;      jnz          short .ATan_NotBothZero
      jz          short .ATan_NotBothZero
      POP_STK      2                   ; clear two numbers
      fldz
      fldz
      jmp          short .End_Log_ATan ; return (0,0)
   PARSALIGN
.ATan_NotBothZero:
      fld          st1                 ; y x y
      fld          st1                 ; x y x y
      fpatan                           ; z.y x y
      fxch         st2                 ; y x z.y
      fmul         st0,st0             ; yy x z.y
      fxch                             ; x yy z.y
      fmul         st0,st0             ; xx yy z.y
      fadd                             ; mod z.y
      fldln2                           ; ln2, mod, z.y
      fmul         qword [PointFive]   ; ln2/2, mod, z.y
      fxch                             ; mod, ln2/2, z.y
      fyl2x                            ; z.x, z.y
.End_Log_ATan:
      fld          qword [PointFive]   ; .5 l.x l.y
      fmul         st1,st0             ; .5 z.y=l.x/2 l.y
      fmulp        st2,st0             ; z.y l.y/2
      fxch                             ; l.y/2 z.y
      fchs                             ; z.x=-l.y/2 z.y
   END_OPER ATan
; --------------------------------------------------------------------------
   BEGN_OPER       CAbs                ; Complex Absolute Value
      fmul         st0,st0             ; x*x y
      fxch                             ; y x*x
      fmul         st0,st0             ; y*y x*x
      fadd                             ; y*y+x*x
      fsqrt                            ; mag=sqrt(yy+xx)
      fldz                             ; 0 mag
      fxch                             ; mag 0
   END_OPER CAbs
; --------------------------------------------------------------------------
; End of new functions.                                          CAE 15Feb95
; --------------------------------------------------------------------------
   BEGN_OPER       Floor               ; Complex floor
      fstcw        word [Arg2]         ; use arg2 to hold CW
      fwait
      mov          ax,word [Arg2]      ; Now do some integer instr.'s
      push         rax                 ; Save control word on stack
      and          ax,1111001111111111b
      or           ax,0000010000000000b
      mov          word [Arg2],ax
      fldcw        word [Arg2]         ; Now set control to round toward -inf
   ; Chop toward negative infinity applies now
      frndint                          ; floor(x) y
      fxch                             ; y floor(x)
      frndint                          ; floor(y) floor(x)
      fxch                             ; floor(x) floor(y)
      pop          rax                 ; restore old CW to AX
      mov          word [Arg2],ax      ; ...then move it to Arg2
      fldcw        word [Arg2]         ; Restore control word from Arg2
   ; Normal rounding is in effect again
   END_OPER        Floor
; --------------------------------------------------------------------------
   BEGN_OPER       Ceil                ; Complex ceiling
      fstcw        word [Arg2]         ; use arg2 to hold CW
      fwait
      mov          ax,word [Arg2]      ; Now do some integer instr.'s
      push         rax                 ; Save control word on stack
      and          ax,1111001111111111b
      or           ax,0000100000000000b
      mov          word [Arg2],ax
      fldcw        word [Arg2]         ; Now set control to round toward +inf
   ; Chop toward positive infinity applies now
      frndint                          ; ceil(x) y
      fxch                             ; y ceil(x)
      frndint                          ; ceil(y) ceil(x)
      fxch                             ; ceil(x) ceil(y)
      pop          rax                 ; restore old CW to AX
      mov          word [Arg2],ax      ; ...then move it to Arg2
      fldcw        word [Arg2]         ; Restore control word from Arg2
   ; Normal rounding is in effect again
   END_OPER        Ceil
; --------------------------------------------------------------------------
   BEGN_OPER       Trunc               ; Complex truncation
      fstcw        word [Arg2]         ; use arg2 to hold CW
      fwait
      mov          ax,word [Arg2]      ; Now do some integer instr.'s
      push         rax                 ; Save control word on stack
      or           ax,0000110000000000b
      mov          word [Arg2],ax
      fldcw        word [Arg2]         ; Now set control to round toward zero
   ; Chop toward zero rounding applies now
      frndint                          ; trunc(x) y
      fxch                             ; y trunc(x)
      frndint                          ; trunc(y) trunc(x)
      fxch                             ; trunc(x) trunc(y)
      pop          rax                 ; restore old CW to AX
      mov          word [Arg2],ax      ; ...then move it to Arg2
      fldcw        word [Arg2]         ; Restore control word from Arg2
   ; Normal rounding is in effect again
   END_OPER        Trunc
; --------------------------------------------------------------------------
   BEGN_OPER       Round               ; Complex round to nearest
      fstcw        word [Arg2]         ; use arg2 to hold CW
      fwait
      mov          ax,word [Arg2]      ; Now do some integer instr.'s
      push         rax                 ; Save control word on stack
      and          ax,1111001111111111b
      or           ax,0000010000000000b
      mov          word [Arg2],ax
      fldcw        word [Arg2]         ; Now set control to round toward -inf
   ; Round toward negative infinity applies now
      fadd         qword [PointFive]   ; x+.5  y
      frndint                          ; round(x) y
      fxch                             ; y round(x)
      fadd         qword [PointFive]   ; y+.5 round(x)
      frndint                          ; round(y) round(x)
      fxch                             ; round(x) round(y)
      pop          rax                 ; restore old CW to AX
      mov          word [Arg2],ax      ; ...then move it to Arg2
      fldcw        word [Arg2]         ; Restore control word from Arg2
   ; Normal rounding is in effect again
   END_OPER        Round
; --------------------------------------------------------------------------
; End of new functions.                                          TIW 30Jun96
; --------------------------------------------------------------------------
   BEGN_INCL       Jump                ;
;      mov          rax,JCSZ           ; rax = sizeof(jump control struct)
;      imul         dword [jump_index] ; address of jump_control[jump_index]
      imul         eax, [jump_index], JCSZ   ; upper 32 bits of rax set to zero
      lea          rbx, [jump_control]
      add          rbx, rax
      mov          eax, dword [rbx+INTSZ+PDDING+3*LNGSZ] ; jump_index = DestJumpIndex
      mov          rbx, qword [rbx+INTSZ+PDDING]         ; rbx = JumpOpPtr
      mov          dword [jump_index],eax
      add          rbx, qword [pfls]   ;
   END_INCL        Jump                ;
; --------------------------------------------------------------------------
   BEGN_OPER       JumpOnTrue          ;
      ftst                             ; test Arg1.x
      fstsw       ax
;;      sahf
      and          rax, 4000h                ; zero flag is inverted, result is 0 if(Arg1.x != 0)
      jnz           short .NotTrue      ; if(Arg1.x == 0)
      INCL_OPER    JumpOnTrue, Jump    ; call Jump
      jmp          short .EndJumpOnTrue
.NotTrue:
      add          dword [jump_index], 1      ; else jump_index++
.EndJumpOnTrue:
   END_OPER        JumpOnTrue          ;
; --------------------------------------------------------------------------
   BEGN_OPER       JumpOnFalse         ;
      ftst                             ; test Arg1.x
      fstsw       ax
;;      sahf
      and          rax, 4000h                ; zero flag is inverted, result is 0 if(Arg1.x != 0)
      jz          short .True         ; if(Arg1.x != 0)
      INCL_OPER    JumpOnFalse, Jump
      jmp          short .EndJumpOnFalse
.True:
      add          dword [jump_index], 1      ; else jump_index++
.EndJumpOnFalse:
   END_OPER        JumpOnFalse         ;
; --------------------------------------------------------------------------
   BEGN_OPER       JumpLabel           ;
      add          dword [jump_index], 1      ; jump_index++
   END_OPER        JumpLabel           ;
; --------------------------------------------------------------------------
; End of new functions.                                          TIW 09Mar97
; --------------------------------------------------------------------------
   BEGN_OPER       LT                  ; <
   ; Arg2->d.x = (double)(Arg2->d.x < Arg1->d.x);
      fcomip       st2                 ; y.y, x.x, x.y, comp arg1 to arg2
;      fstsw        ax
      POP_STK      3
;      sahf
      fldz                             ; 0 (Arg2->d.y = 0.0;)
      jbe          short .LTfalse      ; jump if arg1 <= arg2
      fld1                             ; 1 0 (return arg2 < arg1)
      EXIT_OPER    LT
.LTfalse:
      fldz                             ; 0 0
   END_OPER        LT
; --------------------------------------------------------------------------
   BEGN_OPER       LT2                 ; LT, set AX, clear FPU
   ; returns !(Arg2->d.x < Arg1->d.x) in ax
      xor          rax, rax
      fcomi         st2                 ; compare arg1, arg2
;      fstsw       ax
                                       ;                      CAE 1 Dec 1998
      ALTER_RET_ADDR                   ; change return address on stack

      fninit
;      sahf
      setbe        al                  ; return (Arg1 <= Arg2) in RAX
;      xor          ah,ah
   END_OPER        LT2
; --------------------------------------------------------------------------
   BEGN_OPER       LodLT               ; load, LT
   ; return (1,0) on stack if arg2 < arg1
      FIXUP        LodLT, fcomp, X     ; compare arg2 to arg1, pop st
      fnstsw        ax                  ; y ...
      POP_STK      1                   ; ...
;      sahf
      and         rax, 100h       ; check if carry flag set
      fldz                             ; 0 ...
;      jae          short .LodLTfalse   ; jump when arg2 >= arg1
      jz          short .LodLTfalse   ; jump when arg2 >= arg1
      fld1                             ; 1 0 ...
      EXIT_OPER    LodLT
.LodLTfalse:
      fldz                             ; 0 0 ...
   END_OPER        LodLT
; --------------------------------------------------------------------------
   BEGN_OPER       LodLT2              ; Lod, LT, set AX, clear FPU
   ; returns !(Arg2->d.x < Arg1->d.x) in ax
;      xor          rax, rax
      FIXUP        LodLT2, fcom, X     ; compare arg2, arg1
      fnstsw        ax
                                       ;                      CAE 1 Dec 1998
      ALTER_RET_ADDR                   ; change return address on stack

      fninit                           ; clear fpu
;      sahf
;      setae        al                  ; set al when arg2 >= arg1
;      xor          ah,ah               ; clear ah
      and         rax, 100h       ; check if carry flag set
      jnz           .LodLT2false
      mov         rax, 1
      EXIT_OPER    LodLT2
.LodLT2false:
      xor           rax, rax
   END_OPER        LodLT2              ; ret 0 in ax for true, 1 for false
; --------------------------------------------------------------------------
   BEGN_OPER       LodLTMul            ; Lod, LT, Multiply (needs 4 on stack)
   ; for '<expr> * ( <expr> < <var> )'
   ; return number on stack if arg2 < arg1
      FIXUP        LodLTMul, fcomp, X  ; comp Arg2 to Arg1, pop st
      fnstsw        ax                  ; save status
      POP_STK      1                   ; clear 1 from stack
;      sahf
      and         rax, 100h       ; check if carry flag set
;      jae          short .LodLTMulfalse ; jump if arg2 >= arg1
      jz            short .LodLTMulfalse ; jump if arg2 >= arg1
      EXIT_OPER    LodLTMul            ; return value on st
   PARSALIGN
.LodLTMulfalse:
      POP_STK      2                   ; return (0,0)
      fldz
      fldz
   END_OPER        LodLTMul
; --------------------------------------------------------------------------
   BEGN_OPER       GT                  ; >
   ; Arg2->d.x = (double)(Arg2->d.x > Arg1->d.x);
      fcomip      st2                 ; compare arg1, arg2
;      fstsw        ax
      POP_STK      3
;      sahf
      fldz                             ; 0 (Arg2->d.y = 0.0;)
      jae          short .GTfalse      ; jump if Arg1 >= Arg2
      fld1                             ; 1 0, return arg2 > arg1
      EXIT_OPER    GT
.GTfalse:
      fldz                             ; 0 0
   END_OPER        GT
; --------------------------------------------------------------------------
   BEGN_OPER       GT2                 ; GT, set AX, clear FPU
   ; returns !(Arg2->d.x > Arg1->d.x) in ax
      xor          rax, rax
      fcomi         st2                 ; compare arg1, arg2
;      fstsw       ax
                                       ;                      CAE 1 Dec 1998
      ALTER_RET_ADDR                   ; change return address on stack

      fninit
;      sahf
      setae        al                  ; return (Arg1 >= Arg2) in RAX
;      xor          ah,ah
   END_OPER        GT2
; --------------------------------------------------------------------------
   BEGN_OPER       LodGT               ; load, GT
   ; return (1,0) on stack if arg2 > arg1
      FIXUP        LodGT, fcomp, X     ; compare arg2 to arg1, pop st
      fnstsw        ax                  ; y ...
      POP_STK      1                   ; ...
;      sahf
      and         ax, 4100h       ; check if carry flag or zero flag set
      fldz                             ; 0 ...
;      jbe          short .LodGTfalse   ; jump when arg2 <= arg1
      jnz          short .LodGTfalse   ; jump when arg2 <= arg1
      fld1                             ; 1 0 ...
      EXIT_OPER    LodGT
.LodGTfalse:
      fldz                             ; 0 0 ...
   END_OPER        LodGT
; --------------------------------------------------------------------------
   BEGN_OPER       LodGT2              ; Lod, GT, set AX, clear FPU
   ; returns !(Arg2->d.x > Arg1->d.x) in AX
;      xor          rax, rax
      FIXUP        LodGT2, fcom, X     ; compare arg2, arg1
      fnstsw        ax
                                       ;                      CAE 1 Dec 1998
      ALTER_RET_ADDR                   ; change return address on stack

      fninit                           ; clear fpu
;      sahf
;     setbe        al                  ; set al when arg2 <= arg1
;     xor          ah,ah               ; clear ah
      and         rax, 4100h       ; check if carry or zero flag set
      jz            .LodGT2false
      mov         rax, 1
      EXIT_OPER    LodGT2
.LodGT2false:
      xor           rax, rax
   END_OPER        LodGT2              ; ret 0 in ax for true, 1 for false
; --------------------------------------------------------------------------
   BEGN_OPER       LTE                 ; <=
   ; Arg2->d.x = (double)(Arg2->d.x <= Arg1->d.x);
      fcomip      st2                 ; y x y, comp Arg1 to Arg2
;      fstsw        ax                  ; save status now
      POP_STK      3
      fldz                             ; 0 (Arg2->d.y = 0.0;)
;      sahf
      jb           short .LTEfalse     ; jump if arg1 > arg2
      fld1                             ; 1 0, ret arg2 <= arg1
      EXIT_OPER    LTE
.LTEfalse:
      fldz                             ; 0 0
   END_OPER        LTE
; --------------------------------------------------------------------------
   BEGN_OPER       LTE2                ; LTE, test ST, clear
   ; return !(Arg2->d.x <= Arg1->d.x) in AX
      xor          rax, rax
      fcomi         st2                 ; comp Arg1 to Arg2
;      fstsw       ax
                                       ;                      CAE 1 Dec 1998
      ALTER_RET_ADDR                   ; change return address on stack

      fninit                           ; clear stack
;      and         ah,1        ; mask cf
;      shr          ax,8               ; ax=1 when arg1 < arg1
      setb        al                 ; rax=1 when arg1 < arg1
   END_OPER        LTE2                ; return (Arg1 < Arg2),
; --------------------------------------------------------------------------
   BEGN_OPER       LodLTE              ; load, LTE
   ; return (1,0) on stack if arg2 <= arg1
      FIXUP        LodLTE, fcomp, X    ; compare arg2 to arg1, pop st
      fnstsw        ax                  ; y ...
      POP_STK      1                   ; ...
;      sahf
      and            rax, 4100h       ; check if carry or zero flag set
      fldz                             ; 0 ...
;      ja           short .LodLTEfalse  ; jump when arg2 > arg1
      jz           short .LodLTEfalse  ; jump when arg2 > arg1
      fld1                             ; 1 0 ...
      EXIT_OPER    LodLTE
.LodLTEfalse:
      fldz                             ; 0 0 ...
   END_OPER        LodLTE
; --------------------------------------------------------------------------
   BEGN_OPER       LodLTE2             ; Load, LTE, test ST, clear
   ; return !(Arg2->d.x <= Arg1->d.x) in AX
;      xor          rax, rax
      FIXUP        LodLTE2, fcom, X    ; comp Arg2 to Arg1
      fnstsw        ax
                                       ;                      CAE 1 Dec 1998
      ALTER_RET_ADDR                   ; change return address on stack

      fninit
;      sahf
;      seta         al
;      xor          ah,ah               ; ax=1 for expr. false
      and          rax, 4100h       ; check if carry and zero flag set
      jnz             .LodLTE2false
      mov         rax, 1
      EXIT_OPER    LodLTE2
.LodLTE2false:
      xor           rax, rax
   END_OPER        LodLTE2             ; return (Arg2 > Arg1)
; --------------------------------------------------------------------------
   BEGN_OPER       LodLTEMul           ; Lod, LTE, Multiply (needs 4 on stk)
   ; for '<expr> * ( <expr> <= <var> )'
   ; return number on stack if arg2 <= arg1
      FIXUP        LodLTEMul, fcomp, X ; comp Arg2 to Arg1, pop st
      fnstsw        ax                  ; save status
      POP_STK      1                   ; clear 1 from stack
;      sahf
      and          rax, 4100h       ; check if carry and zero flag set
;      ja           short .LodLTEMulfalse ; jump if arg2 > arg1
      jz           short .LodLTEMulfalse ; jump if arg2 > arg1
      EXIT_OPER    LodLTEMul           ; return value on st
   PARSALIGN
.LodLTEMulfalse:
      POP_STK      2                   ; return (0,0)
      fldz
      fldz
   END_OPER        LodLTEMul
; --------------------------------------------------------------------------
   BEGN_OPER       LodLTEAnd2          ; Load, LTE, AND, test ST, clear
   ; this is for 'expression && (expression <= value)'
   ; stack has {arg2.x arg2.y logical.x junk} on entry (arg1 in memory)
   ; Arg2->d.x = (double)(Arg2->d.x <= Arg1->d.x);
;      xor          rax, rax
      FIXUP        LodLTEAnd2, fcom, X ; comp Arg2 to Arg1
      fnstsw        ax
                                       ;                      CAE 1 Dec 1998
      ALTER_RET_ADDR                   ; change return address on stack

;      sahf
      and          rax, 4100h       ; check if carry and zero flag set
      fxch         st2                 ; logical.x arg2.y arg2.x junk ...
;      ja           .LTEA2RFalse        ; right side is false, Arg2 > Arg1
      jz           .LTEA2RFalse        ; right side is false, Arg2 > Arg1
      ftst                             ; now see if left side of expr is true
      fnstsw        ax
;      sahf
      and          rax, 4000h       ; check if zero flag set
      fninit                           ; clear fpu
;      jz           .LTEA2LFalse        ; jump if left side of && is false
      jnz          .LTEA2LFalse        ; jump if left side of && is false (zf is inverted)
      xor          rax,rax             ; return zero in rax for expr true
      ret                              ; changed EXIT_OPER->ret  CAE 30DEC93
.LTEA2RFalse:
      fninit
.LTEA2LFalse:
      mov          rax,1               ; return rax=1 for condition false
   END_OPER        LodLTEAnd2
; --------------------------------------------------------------------------
   BEGN_OPER      GTE                 ; >=
   ; Arg2->d.x = (double)(Arg2->d.x >= Arg1->d.x);
      fcomip      st2                 ; y x y (compare arg1,arg2)
;      fstsw        ax
      POP_STK      3                   ; clear 3 from stk
;      sahf
      fldz                             ; 0 (Arg2->d.y = 0.0;)
      ja           short .GTEfalse     ; jmp if arg1 > arg2
      fld1                             ; 1 0 (return arg2 >= arg1 on stack)
      EXIT_OPER    GTE
.GTEfalse:
      fldz                             ; 0 0
   END_OPER        GTE
; --------------------------------------------------------------------------
   BEGN_OPER       LodGTE              ; load, GTE
   ; return (1,0) on stack if arg2 >= arg1
      FIXUP        LodGTE, fcomp, X    ; compare arg2 to arg1, pop st
      fnstsw        ax                  ; y ...
      POP_STK      1                   ; ...
      fldz                             ; 0 ...
;      sahf
      and         rax, 100h       ; check if carry flag set
;      jb           short .LodGTEfalse  ; jump when arg2 < arg1
      jnz          short .LodGTEfalse  ; jump when arg2 < arg1
      fld1                             ; 1 0 ...
      EXIT_OPER    LodGTE
.LodGTEfalse:
      fldz                             ; 0 0 ...
   END_OPER        LodGTE
; --------------------------------------------------------------------------
   BEGN_OPER       LodGTE2             ; Lod, GTE, set AX, clear FPU
   ; return !(Arg2->d.x >= Arg1->d.x) in AX
;      xor          rax, rax
      FIXUP        LodGTE2, fcom, X    ; compare arg2, arg1
      fnstsw        ax
                                       ;                      CAE 1 Dec 1998
      ALTER_RET_ADDR                   ; change return address on stack

      fninit                           ; clear fpu
      and          rax,100h      ; mask cf
      shr          rax,8               ; shift it (RAX = 1 when arg2 < arg1)
   END_OPER        LodGTE2             ; ret 0 in ax for true, 1 for false
; --------------------------------------------------------------------------
   BEGN_OPER       EQ                  ; ==
   ; Arg2->d.x = (double)(Arg2->d.x == Arg1->d.x);
      fcomip      st2                 ; compare arg1, arg2
;      fstsw        ax
      POP_STK      3
;      sahf
      fldz                             ; 0 (Arg2->d.y = 0.0;)
      jne          short .EQfalse      ; jmp if arg1 != arg2
      fld1                             ; 1 0 (ret arg2 == arg1)
      EXIT_OPER    EQ
.EQfalse:
      fldz
   END_OPER       EQ
; --------------------------------------------------------------------------
   BEGN_OPER       LodEQ               ; load, EQ
   ; return (1,0) on stack if arg2 == arg1
      FIXUP        LodEQ, fcomp, X     ; compare arg2 to arg1, pop st
      fnstsw        ax                  ; y ...
      POP_STK      1                   ; ...
      fldz                             ; 0 ...
;      sahf
      and          rax, 4000h       ; check if zero flag set
;      jne          short .LodEQfalse   ; jump when arg2 != arg1
      jz           short .LodEQfalse   ; jump when arg2 != arg1 (zf is inverted)
      fld1                             ; 1 0 ... (return arg2 == arg1)
      EXIT_OPER    LodEQ
.LodEQfalse:
      fldz                             ; 0 0 ...
   END_OPER        LodEQ
; --------------------------------------------------------------------------
   BEGN_OPER       NE                  ; !=
   ; Arg2->d.x = (double)(Arg2->d.x != Arg1->d.x);
      fcomip      st2                 ; compare arg1,arg2
;      fstsw        ax
      POP_STK      3
;      sahf
      fldz
      je           short .NEfalse      ; jmp if arg1 == arg2
      fld1                             ; ret arg2 != arg1
      EXIT_OPER    NE
.NEfalse:
      fldz
   END_OPER        NE
; --------------------------------------------------------------------------
   BEGN_OPER       LodNE               ; load, NE
   ; return (1,0) on stack if arg2 != arg1
      FIXUP        LodNE, fcomp, X     ; compare arg2 to arg1, pop st
      fnstsw        ax                  ; y ...
      POP_STK      1                   ; ...
      fldz                             ; 0 ...
;      sahf
      and          rax, 4000h       ; check if zero flag set
;      je           short .LodNEfalse   ; jump when arg2 == arg1
      jnz          short .LodNEfalse   ; jump when arg2 == arg1 (zf is inverted)
   ; CAE changed above 'jne' to 'je'                              9 MAR 1993
      fld1                             ; 1 0 ...
      EXIT_OPER    LodNE
.LodNEfalse:
      fldz                             ; 0 0 ...
   END_OPER        LodNE
; --------------------------------------------------------------------------
   BEGN_OPER       OR                  ; Or
   ; Arg2->d.x = (double)(Arg2->d.x || Arg1->d.x);
      ftst                             ; a1.x a1.y a2.x a2.y ...
      fnstsw        ax
;      sahf
      and        rax, 4000h
      POP_STK      2                   ; a2.x a2.y ...
;      jnz          short .Arg1True
      jz          short .Arg1True     ; zero flag is inverted
      ftst
      fnstsw        ax
;      sahf
      and        rax, 4000h
      POP_STK      2                   ; ...
      fldz                             ; 0 ...
;      jz           short .NoneTrue
      jnz          short .NoneTrue     ; zero flag is inverted
      fld1                             ; 1 0 ...
      EXIT_OPER    OR
   PARSALIGN
.Arg1True:
      POP_STK      2                   ; ...
      fldz                             ; 0 ...
      fld1                             ; 1 0 ...
      EXIT_OPER    OR
.NoneTrue:                             ; 0 ...
      fldz                             ; 0 0 ...
   END_OPER        OR
; --------------------------------------------------------------------------
   BEGN_OPER      AND                 ; And
   ; Arg2->d.x = (double)(Arg2->d.x && Arg1->d.x);
      ftst                             ; a1.x a1.y a2.x a2.y ...
      fnstsw        ax
;      sahf
      and        rax, 4000h
      POP_STK      2                   ; a2.x a2.y ...
;      jz           short .Arg1False
      jnz           short .Arg1False     ; zero flag is inverted
      ftst
      fnstsw        ax
;      sahf
      and        rax, 4000h
      POP_STK      2                   ; ...
      fldz                             ; 0 ...
;      jz           short .Arg2False
      jnz           short .Arg2False     ; zero flag is inverted
      fld1                             ; 1 0 ...
      EXIT_OPER    AND
   PARSALIGN
.Arg1False:
      POP_STK      2                   ; ...
      fldz                             ; 0 ...
.Arg2False:
      fldz                             ; 0 0 ...
   END_OPER       AND
; --------------------------------------------------------------------------
   BEGN_OPER       ANDClr2             ; And, test ST, clear FPU
   ; for bailouts using <condition> && <condition>
   ;  Arg2->d.x = (double)(Arg2->d.x && Arg1->d.x);
   ;  Returns !(Arg1 && Arg2) in ax
;      xor          rax, rax
      ftst                             ; y.x y.y x.x x.y
      fnstsw        ax
                                       ;                      CAE 1 Dec 1998
      ALTER_RET_ADDR                   ; change return address on stack

;      sahf
      and        rax, 4000h
;      jz           short .Arg1False2
      jnz           short .Arg1False2     ; zero flag is inverted
      fxch         st2                 ; x.x y.y y.x x.y
      ftst
      fnstsw        ax
;      sahf
      and        rax, 4000h
      fninit
;      jz           short .Arg2False2
      jnz           short .Arg2False2     ; zero flag is inverted
.BothTrue2:
      xor          rax, rax
      ret                              ; changed EXIT_OPER->ret  CAE 30DEC93
.Arg1False2:
      fninit
.Arg2False2:
      mov          rax,1
   END_OPER        ANDClr2
; --------------------------------------------------------------------------
   BEGN_OPER       ORClr2           ; Or, test ST, clear FPU      CAE 6NOV93
   ; for bailouts using <condition> || <condition>
   ;  Arg2->d.x = (double)(Arg2->d.x || Arg1->d.x);
   ;  Returns !(Arg1 || Arg2) in ax
;      xor          rax, rax
      ftst                             ; y.x y.y x.x x.y
      fstsw        ax
                                       ;                      CAE 1 Dec 1998
      ALTER_RET_ADDR                   ; change return address on stack

;      sahf
      and        rax, 4000h
;      jnz          short .ORArg1True
      jz          short .ORArg1True     ; zero flag is inverted
      fxch         st2                 ; x.x y.y y.x x.y
      ftst
      fstsw        ax
;      sahf
      and        rax, 4000h
      fninit
;      jnz          short .ORArg2True
      jz          short .ORArg2True     ; zero flag is inverted
.ORNeitherTrue:
      mov          rax,1
      ret                              ; changed EXIT_OPER->ret  CAE 30DEC93
.ORArg1True:
      fninit
.ORArg2True:
      xor          rax, rax
   END_OPER        ORClr2

; --------------------------------------------------------------------------
;   assume          ds:DGROUP, es:nothing
; --------------------------------------------------------------------------

%ifndef  COMPILER

; --------------------------------------------------------------------------
; called once per image
; --------------------------------------------------------------------------
   CGLOBAL          Img_Setup
   align           16
   ; Changed to FAR, FRAME/UNFRAME added by CAE 09OCT93
Img_Setup:
      FRAME        rsi,rdi

      xor          rdi, rdi
      mov          rsi, [pfls]          ; rsi = &pfls[0]
      mov          edi, [LastOp]  ; load index of lastop, upper 32 bits of rdi set to zero
      sub          rdi, 1               ; flastop now points at last operator
      ; above added by CAE 09OCT93 because of loop logic changes

      shl          rdi, 4               ; convert to offset, 2xPTRSZ = x16
      add          rdi, rsi             ; rdi = offset lastop
      mov          qword [fLastOp], rdi ; save value of flastop
      mov          rax, qword [v]       ; build a ptr to Z
      add          rax, 3*CARG+CPFX
      mov          qword [PtrToZ], rax  ; and save it
      UNFRAME      rsi,rdi
      ret
;_Img_Setup         endp
; --------------------------------------------------------------------------
;  Hybrid orbitcalc/per-pixel routine (tested, but not implemented.)
;
;  To implement, stick the following code in calcfrac.c around line 2000,
;     just before the line that says "while (++coloriter < maxit)".
; --------------------------------------------------------------------------
;  if (curfractalspecific->orbitcalc == fFormula  /* 387 parser  */
;        && periodicitycheck == 0
;        && !show_orbit
;        && inside >= -5
;        && attractors == 0
;        && !distest ){
;     fFormulaX();  /* orbit till done  */
;  } else
; --------------------------------------------------------------------------
   CGLOBAL          fFormulaX          ;                         CAE 09OCT93
   align           16
fFormulaX:    ;     proc far
      push         rsi
      push         rdi
      UNFRAME      rsi,rdi
      mov          eax, [InitJumpIndex]
      mov          [jump_index],eax
      mov          rdx,[maxit]         ; rdx holds coloriter during loop
      mov          [coloriter],rdx     ; set coloriter to maxit
;      mov          eax,eds            ; save ds in ax
;      lds          cx,word [_fLastOp]         ; ds:cx -> one past last token
      mov          rcx, qword [fLastOp]      ; rcx -> one past last token
;      mov          es,ax              ; es -> DGROUP
;   assume          es:DGROUP, ds:nothing ; swap es, ds before any fn. calls
      jmp          short skipfirst     ; skip bailout test first time
   align           16
outer_loop:
      or           rax,rax               ; did bailout occur?
      jnz          short doneloop      ; yes, exit
skipfirst:
;      dec          rdx                ; ++coloriter
      sub          rdx,1
      jle          short doneloop      ; yes, exit because of maxiter
      mov          rbx, qword [InitOpPtr]       ; bx -> one before first token
      lea          rdi, [s]            ; reset stk overflow ptr
   align           16
inner_loop2:
      cmp          rbx,rcx             ; time to quit yet?
      jae          short outer_loop    ; yes, bx points to last function
      add          rbx,PTRSZ*2         ; point to next pointer pair
      push         qword inner_loop2   ; do this first
      mov          rsi, qword [rbx+PTRSZ]    ; set rsi to operand pointer
      jmp          QWORD [rbx]         ; jmp to operator fn
   align           16
doneloop:
   ; NOTE: rdx must be preserved here.
      mov          rsi, qword [PtrToZ]  ; rsi -> z
      lea          rdi, [new]           ; rdi -> new
      mov          rcx, 2               ; get ready to move 2 qwords
      rep          movsq                ; new = z
      pop         rdi
      pop         rsi
;      mov          ds,ax               ; restore ds before return
;   assume          ds:DGROUP, es:nothing
      sub          qword [coloriter],rdx      ; now put new coloriter back from rdx
      ret
;_fFormulaX         endp
; --------------------------------------------------------------------------
;       orbitcalc function follows
; --------------------------------------------------------------------------
   CGLOBAL          fFormula
   align           16
fFormula:      ;    proc far
      push         rbx
      push         rdi                 ; don't build a frame here
      lea          rdi, [s]            ; reset this for stk overflow area
      mov          rbx, qword [InitOpPtr]       ; rbx -> one before first token
      mov          eax, [InitJumpIndex]
      mov          [jump_index], eax
      mov          rcx, [fLastOp]      ; rcx -> last token
      push         rsi

   align           16
inner_loop:                            ; new loop             CAE 1 Dec 1998
      mov          rsi, qword [rbx + PTRSZ]
      call         qword [rbx]         ; (*opptr)()
      add          rbx, PTRSZ*2        ; ++opptr
      jmp          short inner_loop

   align           16
past_loop:
   ; NOTE: RAX was set by the last operator fn called.
      mov          rsi, qword [PtrToZ] ; rsi -> z
      lea          rdi, [new]          ; rdi -> new
      mov          rcx, 2              ; get ready to move 2 qwords
      rep          movsq               ; new = z

      pop          rsi
      pop          rdi                 ; restore rsi, rdi, rbx
      pop          rbx
      ret                              ; return AX unmodified
;_fFormula          endp
; --------------------------------------------------------------------------
   CGLOBAL          fform_per_pixel    ; called once per pixel
   align           16
fform_per_pixel:   ; proc far
;;  Add rax to FRAME as a dummy to get RSP adjustment out of pixel_loop
      FRAME        rsi, rdi, rbx, rax
   ;    if((row+col)&1)
      mov          eax, dword [row]    ; eax = row
      mov          edx, dword [col]    ; edx = col
      add          eax, edx            ; eax = row+col
      and          eax, 1              ; eax = (row+col)&1
      mov          rbx, qword [v]      ; load pointer to constants
      test         eax, eax            ; zero?
      je           checker_is_0
   ;      v[9].a.d.x = 1.0;            ; not zero, set whitesq.x=1.0
      fld1                             ; constant 1.0 to ST
      fstp         QWORD [rbx + WHITESQ]         ; copy ST to whitesq.x
      jmp          checker_is_1
checker_is_0:                          ; is zero, set whitesq to (0,0)
   ;      v[9].a.d.x= 0.0;
      fldz                             ; load constant zero to ST
      fstp         QWORD [rbx + WHITESQ]         ; copy ST to whitesq.x
checker_is_1:
      fldz
      fstp         QWORD [rbx + WHITESQ+ DBLSZ]
   ;    v[10].a.d.x = (double)col;
;      mov          rbx, qword [v]     ; load pointer to constants
      fild         dword [col]          ; ST  = col
      fstp         QWORD [rbx + SCRNPIX]    ; scrnpix.x = col
   ;    v[10].a.d.y = (double)row;
      fild         dword [row]              ; ST  = row
      fstp         QWORD [rbx + SCRNPIX + DBLSZ]         ; scrnpix.y = row
      mov          dword [jump_index], 0        ;jump_index = 0
      cmp          dword [invert], 0            ; inversion support added
      je           skip_invert          ;                        CAE 08FEB95

      lea          rdi, [old]           ; long passed in rdi
      sub          rsp, 8              ; need rsp on 16-byte boundary after next
      push         rdi                  ; save address for data transfer
      call         qword invertz2       ; invertz2(&old)
      pop          rsi                  ; put &old into rsi
      add          rsp, 8
      ; now copy old to v[0].a.d
      mov          rdi, qword [v]       ; rsi already points to old
      add          rdi, CPFX            ; make rdi point to v[0].a.d
      mov          rcx, 2               ; get ready to move 2 qwords
      rep          movsq                ; v[0] = old

      jmp          after_load
skip_invert:
      cmp          dword [use_grid], 0          ; inversion support added
      je           skip_grid
   ;   v[0].a.d.x = dx0[col]+dShiftx;
      xor          rax, rax
      mov          eax, dword [col]
      shl          rax, 3            ; x8 - size of double
      mov          rbx, [dx0]
      add          rbx, rax
      fld          QWORD [rbx]
      xor          rax, rax
      mov          eax, dword [row]
      shl          rax, 3
      mov          rbx, [dx1]
      add          rbx, rax
      fadd         QWORD [rbx]
      mov          rbx, qword [v]      ; load pointer to constants
      fstp         QWORD [rbx + CPFX]
   ;  v[0].a.d.y = dy0[row]+dShifty;
      xor          rax, rax
      mov          eax, dword [row]
      shl          rax, 3
      mov          rbx, [dy0]
      add          rbx, rax
      fld          QWORD [rbx]
      xor          rax, rax
      mov          eax, dword [col]
      shl          rax, 3
      mov          rbx, [dy1]
      add          rbx, rax
      fadd         QWORD [rbx]
      mov          rbx, qword [v]      ; load pointer to constants
      fstp         QWORD [rbx + CPFX + DBLSZ]
      jmp          after_load
skip_grid:
   ;  v[0].a.d.x = (double)(xxmin + col*delxx + row*delxx2);
      fild         DWORD [row]
      fld          QWORD [delxx2]
      fmulp        st1, st0
      fild         DWORD [col]
      fld          QWORD [delxx]
      fmulp        st1, st0
      faddp        st1, st0
      fadd         QWORD [xxmin]
      mov          rbx, qword [v]      ; rbx = &v
      fstp         QWORD [rbx + CPFX]
;      fwait
   ;  v[0].a.d.y = (double)(yymax - row*delyy - col*delyy2)
      fild         DWORD [row]
      fld          QWORD [delyy]
      fmulp        st1, st0
      fsubr        QWORD [yymax]
      fild         DWORD [col]
      fld          QWORD [delyy2]
      fmulp        st1, st0
      fsubp        st1, st0
      fstp         QWORD [rbx + CPFX + DBLSZ]
after_load:
      lea          rdi, [s]               ; rdi points to stack overflow area
      mov          rbx, qword [pfls]      ; rbx -> pfls
      mov          rcx, qword [fLastOp]   ; rcx = offset &f[LastOp]
      cmp          qword [LastInitOp], 0
      je           short skip_initloop    ; no operators to do here
      mov          [LastInitOp], rcx      ; lastinitop=lastop
      jmp          short pixel_loop
   align           16
pixel_loop:
      mov          rsi, qword [rbx + PTRSZ]  ; get address of load or store
      call         qword [rbx]         ; (*opptr)()
      add          rbx, PTRSZ*2        ; ++opptr
      cmp          rbx, [LastInitOp]
      jb           short pixel_loop
skip_initloop:
      mov          rsi, qword [PtrToZ] ; rsi -> z
      lea          rdi, [old]          ; rdi -> old
      mov          rcx, 2              ; get ready to move 2 qwords
      rep          movsq               ; old = z

;   ; subtract removed     CAE 1 Dec 1998
      mov          qword [InitOpPtr], rbx      ; InitOptPtr = OpPtr;
      UNFRAME      rsi, rdi, rbx, rax
      xor          rax, rax
      ret
;_fform_per_pixel   endp
; --------------------------------------------------------------------------

%else  ; COMPILER

; --------------------------------------------------------------------------
; . . . and now for the real fun!
; --------------------------------------------------------------------------
   CGLOBAL          Img_Setup
   align           16
Img_Setup:
      mov          rax, qword [v]        ; build a ptr to Z
      add          rax, 3*CARG+CPFX
      mov          qword [PtrToZ], rax   ; and save it
      ret
;Img_Setup         endp
; --------------------------------------------------------------------------
;  Hybrid orbitcalc/per-pixel routine.
; --------------------------------------------------------------------------
   CGLOBAL          fFormulaX
   align           16
fFormulaX:
      FRAME        rsi, rdi
      mov          rdx,[maxit]         ; edx holds coloriter during loop
      mov          [coloriter], rdx    ; set coloriter to maxit
      jmp          short skipfirst     ; skip bailout test first time
   align           16
outer_loop:
      or           rax,rax             ; did bailout occur?
      jnz          short doneloop      ; yes, exit
skipfirst:
;      dec          rdx                 ; ++coloriter, was maxiter reached?
      sub          rdx, 1
      jle          short doneloop      ; yes, exit because of maxiter
      push         outer_loop
      lea          rdi, qword [s]      ; reset this for stk overflow area
      jmp          compiled_fn_2       ; call the compiled code
doneloop:
   ; NOTE: rdx must be preserved here.
      mov          rsi, qword [PtrToZ] ; rsi -> z
      lea          rdi, [new]          ; rdi -> new
      mov          rcx, 2
      rep          movsq               ; new = z
      UNFRAME      rsi, rdi
      sub          [coloriter],rdx     ; now put new coloriter back from rdx

      ret
;fFormulaX         endp
; --------------------------------------------------------------------------
;       orbitcalc function follows
; --------------------------------------------------------------------------
   CGLOBAL          fFormula
   align           16
fFormula:        ;  proc far
      push         rdi                 ; don't build a frame here
      lea          rdi, qword [s]      ; reset this for stk overflow area
      push         rsi                 ; compiled_fn modifies rsi
      sub          rsp, 8             ; need rsp on 16-byte boundary
      call         compiled_fn_2       ; call the compiled code
      add          rsp, 8
   ; NOTE: RAX was set by the compiled code and must be preserved here.
      mov          rsi, qword [PtrToZ] ; rsi -> z
      lea          rdi, qword [new]    ; rdi -> new
      mov          rcx, 2              ; get ready to move 2 qwords
      rep          movsq               ; new = z
      pop          rsi
      pop          rdi                  ; restore rsi, rdi
      ret                              ; return RAX unmodified
;fFormula          endp
; --------------------------------------------------------------------------
   CGLOBAL         fform_per_pixel     ; called once per pixel
   align           16
fform_per_pixel:    ; proc far
      FRAME        rsi, rdi
      cmp          invert,0            ; inversion support added
      je           skip_invert         ;                        CAE 08FEB95
      lea          rdi, [old]          ; double passed in rdi 
      sub          rsp, 8             ; need rsp on 16-byte boundary
      push         rdi                 ; save address for data transfer
      call         qword invertz2      ; invertz2(&old)
      pop          rsi                 ; put &old into rsi
      add          rsp, 8
      ; now copy old to v[0].a.d
      mov          rdi, qword [v]      ; rsi already points to old
      add          rdi, CPFX           ; make rdi point to v[0].a.d
      mov          rcx, 2              ; get ready to move 2 qwords
      rep          movsq               ; v[0] = old
      jmp          after_load
skip_invert:
      cmp          dword [use_grid], 0 ; inversion support added
      je           skip_grid  
   ;   v[0].a.d.x = dx0[col]+dShiftx;
      xor          rax, rax
      mov          eax, [col]
      shl          rax, 3              ; x8 - size of double
      mov          rbx, [dx0]
      add          rbx, rax
      fld          QWORD [rbx]
      mov          eax, dword [row]
      shl          rax, 3
      mov          rbx, [dx1]
      add          rbx, rax
      fadd         QWORD [rbx]
      mov          rbx, qword [v]      ; load pointer to constants
      fstp         QWORD [rbx + CPFX]
   ;  v[0].a.d.y = dy0[row]+dShifty;
      xor          rax, rax
      mov          eax, dword [row]
      shl          rax, 3
      mov          rbx, [dy0]
      add          rbx, rax
      fld          QWORD [rbx]
      mov          eax, dword [col]
      shl          rax, 3
      mov          rbx, [dy1]
      add          rbx, rax
      fadd         QWORD [rbx]
      mov          rbx, qword [v]      ; load pointer to constants
      fstp         QWORD [rbx + CPFX + DBLSZ]
      jmp          after_load
skip_grid:
   ;  v[0].a.d.x = (double)(xxmin + col*delxx + row*delxx2); 
      fild         DWORD [row]
      fld          QWORD [delxx2]
      fmulp        st1, st0
      fild         DWORD [col]
      fld          QWORD [delxx]
      fmulp        st1, st0
      faddp        st1, st0
      fadd         QWORD [xxmin]
      mov          rbx, qword [v]      ; rbx = &v
      fstp         QWORD [rbx + CPFX]
;      fwait
   ;  v[0].a.d.y = (double)(yymax - row*delyy - col*delyy2)
      fild         DWORD [row]
      fld          QWORD [delyy]
      fmulp        st1, st0
      fsubr        QWORD [yymax]
      fild         DWORD [col]
      fld          QWORD [delyy2]
      fmulp        st1, st0
      fsubp        st1, st0
      fstp         QWORD [rbx + CPFX + DBLSZ]
after_load:
      lea          rdi, [s]            ; rdi points to stack overflow area

      call         compiled_fn_1       ; call compiled code

      UNFRAME      rdi, rsi
      xor          rax, rax
      ret

;fform_per_pixel   endp

   align           16
   CGLOBAL         compiled_fn_1
compiled_fn_1:    ; proc near
      ret                             ; compiled code will be put here
      resw         1023
;compiled_fn_1     endp

   align           16
   CGLOBAL          compiled_fn_2
compiled_fn_2:    ; proc near
      ret                             ; ...and here
      resw         1023
;compiled_fn_2     endp
; --------------------------------------------------------------------------

%endif  ; COMPILER

; --------------------------------------------------------------------------
