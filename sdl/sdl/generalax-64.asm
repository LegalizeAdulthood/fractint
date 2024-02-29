;       Generic assembler routines that have very little at all
;       to do with fractals.
;

; ---- 32-bit Multiply/Divide Routines (includes 16-bit emulation)
;
;       multiply()
;       divide()
;

DEFAULT REL

;%use smartalign
%include "xfract_a-64.inc"

; external functions
;
; external data
CEXTERN overflow   ;:DWORD  4 - int

section .bss
align  16

sign:                      ; sign flag goes here
        resb   INTSZ

section .text

; =======================================================
;
;       32-bit integer multiply routine with an 'n'-bit shift.
;       Overflow condition returns 0x7fffffffh with overflow = 1;
;
;       long x, y, z, multiply();
;       int n;
;
;       z = multiply(x,y,n)
;

CGLOBAL    multiply ;    x:qword, y:qword, n:dword
;                        in RDI,  in RSI,  in EDX
multiply:
align 16

        mov     ecx, edx                ; set up the shift
        mov     rax, rdi                ; load X into EAX
        imul    rsi                     ; do the multiply with Y
        cmp     ecx, 32                 ; ugly klooge:  check for 32-bit shift
        jb      short fastm1            ;  < 32 bits:  no problem
fastm0:
        shrd    rax, rdx, 16     ; max shift is 31, so use 16 twice
        sar       rdx, 16            ; shrd doesn't shift rdx, so shift rdx preserving the sign
        shrd    rax, rdx, 16
        sar       rdx, 16
        sub     ecx, 32             ;  >= 32 bits:  done with manual shift
        cmp     ecx, 32            ; check again for 32-bit shift
        jae       short fastm0
fastm1: shrd    rax, rdx, cl       ; shift down 'n' bits
        js        fastm3      ; is sign flag set?
        sar     rdx, cl
        jne     overmf      ; something left in rdx, we overflowed
        ret
fastm3: sar     rdx, cl
        inc    rdx
        jne    overmf
        ret
overmf:
        mov     rax,07fffffffh          ; overflow value
        mov     dword [overflow],1      ; flag overflow
        ret

;multiply        endp


; =======================================================
;
;       32-bit integer divide routine with an 'n'-bit shift.
;       Overflow condition returns 0x7fffffffh with overflow = 1;
;
;       long x, y, z, divide();
;       int n;
;
;       z = divide(x,y,n);      /* z = x / y; */
;

CGLOBAL    divide   ;    x:qword, y:qword, n:dword
;                        in RDI,  in RSI,  in EDX
divide:
align 16

        mov     ecx, 64                 ; set up to shift result
        sub     ecx, edx                ; (64-bitshift) to the right (RDX -> RAX)

        mov     rax, 0                  ; clear out the low-order bits
        mov     rdx, rdi                ; load X into RDX (shifts to RDX:RAX)
;        mov     rbx, rsi                ; leave Y in RSI (else must push/pop RBX)

        mov     dword [sign], 0         ; clear out the sign flag
        cmp     rdx, 0                  ; is X negative?
        jge     short divides1          ;  nope
        not     dword [sign]            ;  yup.  flip signs
        neg     rax
        neg     rdx                     ;   ...
divides1:
        cmp     rsi, 0                  ; is Y negative?
        jg      short divides2          ;  nope
        jl      short setsign1
        jmp     overd1                  ; don't divide by zero, set overflow
setsign1:
        not     dword [sign]            ;  yup.  flip signs
        neg     rsi                     ;   ...
divides2:
        shrd    rax, rdx, cl            ; move bits into RAX
        shr     rdx, cl                 ; move bits out of RDX

fastd2:
        cmp     rdx, rsi                ; umm, will the divide blow out?
        jae     overd1                  ;  yup.  better skip it.
        div     rsi                     ; do the divide
        cmp     rax, 0                  ; did the sign flip?
        jl      overd1                  ;  then we overflowed
        cmp     dword [sign], 0         ; is the sign reversed?
        je      short divides3          ;  nope
        neg     rax                     ; flip the sign
divides3:
        jmp     dividereturn            ; back to common code

overd1:
        mov     rax, 07fffffffh         ; overflow value
        mov     dword [overflow], 1     ; flag overflow

dividereturn:                           ; that's all, folks!
        ret
;divide          endp

