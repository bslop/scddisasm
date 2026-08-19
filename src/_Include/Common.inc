; -------------------------------------------------------------------------
; Sonic CD Disassembly
; -------------------------------------------------------------------------
; Common definitions
; -------------------------------------------------------------------------

; -------------------------------------------------------------------------
; Regions
; -------------------------------------------------------------------------

JAPAN		EQU	0
USA		EQU	1
EUROPE		EQU	2

; -------------------------------------------------------------------------
; Align
; -------------------------------------------------------------------------
; PARAMETERS:
;	bound - Size boundary
;	value - (OPTIONAL) Value to pad with
; -------------------------------------------------------------------------

ALIGN macro bound, value
	local	pad
pad	=	((\bound)-((*)%(\bound)))%(\bound)
	if narg>1
		dcb.b	pad,\value
	else
		dcb.b	pad,0
	endif
	endm

; -------------------------------------------------------------------------
; Rebase a work-RAM pointer just reloaded with movea.w
; -------------------------------------------------------------------------
; The game stores work-RAM pointers as 16-bit words (`move.w a0,oParent(a1)`)
; and reloads them with `movea.w`, which sign-extends.  On the Mega Drive that
; round-trip is exact -- the pointers live in $FF8000-$FFFFFF and the sign
; extension puts the $FFFF back.  On the Atari Jaguar it still puts $FFFF back,
; and there is no memory there, so the pointer has to be mapped into the
; rebased work RAM.
;
; WORKRAM+$10000 is the exact restoration and it is exact for a reason: every
; variable those words can point at lives in the upper 32 KB of the Genesis
; map, so the sign extension is always $FFFFwwww, and adding base+$10000 gives
; base|wwww in full 32-bit arithmetic.  (An earlier WORKRAM-$FF0000 landed on
; the same address only because the 68000 truncates to 24 bits -- correct here,
; but resting on the bus width rather than on the arithmetic.)  Neither movea
; nor adda touches CCR, so a following Bcc still tests what it used to.
; Form and rationale from jaguar-shared, JAGUAR_PORTING_NOTES.md, jag_s3k.
;
; PARAMETERS:
;	reg - Address register just loaded with movea.w
; -------------------------------------------------------------------------

WRAMPTR macro reg
	if JAGUAR
		adda.l	#(WORKRAM+$10000)&$FFFFFFFF,\reg
	endif
	endm

; -------------------------------------------------------------------------
; Prime a data register with the work-RAM base, for a 16-bit splice
; -------------------------------------------------------------------------
; The other half of the same story as WRAMPTR, and the one that does NOT go
; through movea.w: the game primes a data register with $FFFFFFFF, moves a
; 16-bit field into its low word, and uses the result as a full pointer --
;
;	moveq	#$FFFFFFFF,d0
;	move.w	oAddr(a0),d0		; $FFFFFFFF -> $FFFFwwww
;	movea.l	d0,a1
;	jmp	(a1)
;
; On the Mega CD that is exact, because the program itself runs from work RAM
; at $FFxxxx and the top half is a constant.  On the Jaguar the top half is
; not $FFFF, so the splice has to start from the rebased base instead.  Work
; RAM is 64 KB and 64 KB-aligned, so base|wwww is exactly base+wwww and the
; word field needs no adjustment -- only the register it lands in.
;
; ⚠ It must be followed IMMEDIATELY by the move.w that fills the low word.
; Unlike WRAMPTR this form does touch CCR (moveq sets N, move.l #base clears
; it), which is safe only because the splice sets the flags again straight
; after; do not put a Bcc between them.
;
; PARAMETERS:
;	reg - Data register about to receive a 16-bit work-RAM field
; -------------------------------------------------------------------------

WRAMHI macro reg
	if JAGUAR
		move.l	#WORKRAM,\reg
	else
		moveq	#$FFFFFFFF,\reg
	endif
	endm

; -------------------------------------------------------------------------
; Pad RS to even address
; -------------------------------------------------------------------------

RSEVEN macros
	rs.b	__rs&1

; -------------------------------------------------------------------------
; Generate repeated RS structure entries
; -------------------------------------------------------------------------
; PARAMETERS:
;	name  - Entry name base
;	count - Number of entries
;	size  - Size of entry
; -------------------------------------------------------------------------

RSRPT macro name, count, size
	local cnt
cnt	=	0
	rept	\count
\name\\$cnt	rs.\0	\size
cnt		=	cnt+1
	endr
	endm

; -------------------------------------------------------------------------
; Store string with static size
; -------------------------------------------------------------------------
; PARAMETERS:
;	len - Length of string
;	str - String to store
; -------------------------------------------------------------------------

STRSZ macro len, str
	local	len2, str2
	if strlen(\str)>(\len)
len2		=	\len
str2		SUBSTR	1,\len,\str
	else
len2		= 	strlen(\str)
str2		EQUS	\str
	endif
	dc.b	"\str2"
	dcb.b	\len-len2, " "
	endm

; -------------------------------------------------------------------------
; Store number with static number of digits
; -------------------------------------------------------------------------
; PARAMETERS:
;	digits - Number of digits
;	num    - Number to store
; -------------------------------------------------------------------------

NUMSTR macro digits, num
	local	num2, digits2, mask
num2	=	\num
digits2	=	1
mask	=	10
	while	(num2<>0)&(digits2<(\digits))
num2		=	num2/10
mask		=	mask*10
digits2		=	digits2+1
	endw
num2	=	(\num)%mask
	dcb.b	(\digits)-strlen("\#num2"), "0"
	dc.b	"\#num2"
	endm

; -------------------------------------------------------------------------
; Store month string
; -------------------------------------------------------------------------

MNTHSTR macro month
	local	mthstr
mthstr	SUBSTR	1+((\month)*3), 3+((\month)*3), &
	"JANFEBMARAPRMAYJUNJULAUGSEPOCTNOVDEC"
	dc.b	"\mthstr"
	endm

; -------------------------------------------------------------------------
