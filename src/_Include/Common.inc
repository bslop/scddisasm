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
; Nemesis XOR-mode routine adjust — a DISTANCE between two labels
; -------------------------------------------------------------------------
; ☠ A NEW CLASS, and the first one the port has hit that is not about
; pointers at all: code the funnel REWRITES gets LONGER, and the game does
; arithmetic on the distance between labels.
;
; NemDec's decompressor picks its "write one 8-pixel row" routine at run time
; and keeps it in a3.  There are four, in two pairs — to the VDP, and to RAM —
; and each pair is (plain, XOR).  The XOR-mode adjust is written ONCE, in the
; shared NemDecMain, as the distance measured across the VDP pair:
;
;	adda.w	#NemPCD_WriteRowToVDP_XOR-NemPCD_WriteRowToVDP,a3
;
; and then applied to whichever pair a3 belongs to.  On the Mega CD that is
; exact, because both pairs are ten bytes: `move.l d4,(a4)` and
; `move.l d4,(a4)+` are the same size, and the four instructions after them
; are identical.
;
; ⚠ In this port they are NOT the same size.  The VDP write goes through the
; funnel and becomes five instructions — push, marshal, `jsr PC_VDP_STORE_L`,
; pop — so the VDP pair measures $16 while the RAM pair still measures $A.
; Measured in the built R11A__.MMD:
;
;	NemPCD_WriteRowToVDP     $152C68   NemPCD_WriteRowToRAM     $152C96
;	NemPCD_WriteRowToVDP_XOR $152C7E   NemPCD_WriteRowToRAM_XOR $152CA0
;	                delta $16                          delta $A
;
; So a RAM decompression in XOR mode set a3 to $152C96+$16 = $152CAC, which is
; NemDec_BuildCodeTable — and NemPCD_NewRow jumped into the middle of the code
; TABLE BUILDER.  Its NemBCT_ShortCode_Loop then ran with a garbage shift
; count, `d5` came out $FFFF, and 65,536 iterations of
; `move.w d7,(0,a1,d0.w)` smeared $0C61 across $142A00-$15A9FE: all of Genesis
; work RAM, the V-INT vector at $14FD08 with it, and the running level's own
; code in Word RAM.  The next V-INT jumped through $0C610C61 and took an
; address error.  Three seconds into Palmtree Panic, every time.
;
; ⭐ The VDP path is unaffected — its own distance is what the constant
; measures — which is why the title screen never showed it.  Only NemDecToRAM
; in XOR mode is wrong, and only a level uses it.
;
; The fix carries the RIGHT distance from the entry point that chose the pair,
; in d3, which NemDecMain does not touch until after the adjust and which the
; entry movem has already saved.  On the Mega CD the original constant stands.
;
; PARAMETERS:
;	plain - the NemPCD_WriteRowTo* routine a3 was just set to
;	xor   - its XOR-mode twin
; -------------------------------------------------------------------------

NEMXORD macro plain, xor
	if JAGUAR
	move.w	#\xor-\plain,d3
	endif
	endm

; -------------------------------------------------------------------------
; Apply the adjust NEMXORD measured.  See NEMXORD for why this is not just
; the constant the game wrote.
; -------------------------------------------------------------------------

NEMXORA macro
	if JAGUAR
	adda.w	d3,a3
	else
	adda.w	#NemPCD_WriteRowToVDP_XOR-NemPCD_WriteRowToVDP,a3
	endif
	endm

; -------------------------------------------------------------------------
; Compare a work-RAM pointer that was computed 24-bit
; -------------------------------------------------------------------------
; The third face of the same coin as WRAMPTR and WRAMHI, and the quietest,
; because it does not produce a wrong ADDRESS -- it produces a wrong ANSWER:
;
;	addi.l	#objects&$FFFFFF,d0	; slot pointer, masked to 24 bits
;	cmpa.w	d0,a0			; "Is it us?"
;
; `cmpa.w` sign-extends the low word before comparing.  On the Mega CD that is
; the whole trick: the mask drops $FF0000's top byte, and the sign extension of
; $Axxx puts the $FFFF back, so the comparison is exact.  On the Jaguar the
; pointer is $0014Axxx, the sign extension still yields $FFFFAxxx, and the two
; are never equal -- so every "is this object us?" test silently answers NO.
;
; Under JAGUAR the masked value is already the complete pointer (no $FF prefix
; was ever dropped), so the comparison simply widens to .l.  On the Mega CD it
; must stay .w, because there the masked value genuinely is missing the prefix
; that only sign extension restores.
;
; ⚠ Only for operands that are POINTERS.  `cmpa.w #$10,a3` compares against a
; stride and `cmpa.w #0,a4` against zero; both are correct as they stand and
; both would still be correct widened, but widening them says something untrue
; about what the register holds.
;
; PARAMETERS:
;	src  - source operand (dN, aN or #imm)
;	areg - address register holding a live work-RAM pointer
; -------------------------------------------------------------------------

CMPAPTR macro src,areg
	if JAGUAR
		cmpa.l	\src,\areg
	else
		cmpa.w	\src,\areg
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
