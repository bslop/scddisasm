; -------------------------------------------------------------------------
; Sonic CD Disassembly — Atari Jaguar port (jag_soniccd)
; -------------------------------------------------------------------------
; Jaguar DRAM addresses for everything the Mega CD provided in hardware.
;
; The Jaguar has a 68000, so the Main CPU code is REHOSTED, not rewritten —
; but none of the Mega Drive / Mega CD address space exists on it.  Every
; hardware address in the disassembly is a symbolic EQU in "Main CPU.inc",
; so the whole rebase is this file plus the `if JAGUAR` arms over there.
;
; KEEP IN SYNC WITH jag_soniccd/include/jag_mem_map.inc.  The two sides are
; built by different assemblers and nothing links them, so a mismatch does
; not fail — the platform reads the wrong memory and draws something
; plausible.  tools/check_contract.py compares them on every build.
;
;   $000000-$0003FF  68000 vector table (the Jaguar's, not the Genesis's)
;   $004000-$13FFFF  image base: platform .text/.data/.bss, then the packed
;                    game files (the "disc")
;   $140000-$14FFFF  JAG_WORKRAM   Genesis work RAM            ($FF0000)
;   $150000-$18FFFF  JAG_WORDRAM   Mega CD Word RAM, 2M mode   ($200000)
;   $190000-$1AFFFF  JAG_PRGRAM    Mega CD PRG-RAM bank window ($020000)
;   $1B0000-$1B1FFF  JAG_Z80RAM    Z80 RAM image               ($A00000)
;   $1B2000-$1B2FFF  JAG_IO        I/O + Z80 ctrl + TMSS + YM  ($A1xxxx/$A04xxx)
;   $1B3000-$1B33FF  JAG_VDP       VDP ports + PSG             ($C00000)
;   $1B3400-$1B34FF  JAG_GA        Mega CD gate array          ($A12000)
;   $1B3F00-$1B3FFF  platform diagnostic page + dirty flags
;   $1C0000-$1D17FF  framebuffer, 320x224
;   $1E4000-$1F3FFF  captured Genesis VRAM
;   $1F4000-$1FFFFF  stack (initial SP $200000, top of 2 MB DRAM)
; -------------------------------------------------------------------------

JAG_WORKRAM	EQU	$140000			; Genesis work RAM (64 KB)
JAG_WORDRAM	EQU	$150000			; Word RAM, 2M mode (256 KB)
JAG_PRGRAM	EQU	$190000			; PRG-RAM bank window (128 KB)
JAG_Z80RAM	EQU	$1B0000			; Z80 RAM image (8 KB)
JAG_IO		EQU	$1B2000			; I/O shadow
JAG_VDP		EQU	$1B3000			; VDP shadow
JAG_GA		EQU	$1B3400			; Gate array shadow

; The Mega CD BIOS ROM sits at $000000 on the Main CPU, which is the Jaguar's
; 68000 vector table.  The handful of entry points this game calls are
; provided by the platform layer at the base below instead.
JAG_CDBIOS	EQU	$130000			; BIOS entry stubs

; The platform's call table.  The game is assembled by asm68k and the platform
; by jas, with nothing linking them, so the game calls an ADDRESS rather than a
; symbol.  KEEP IN SYNC WITH include/jag_mem_map.inc (check_contract.py).
; Six bytes per entry, APPEND ONLY.
JAG_PLATCALL	EQU	$131000
_vdp_ctrl_w	EQU	JAG_PLATCALL+$00	; capture a control-port word
_vdp_ctrl_l	EQU	JAG_PLATCALL+$06	; capture a control-port long
_vdp_data_w	EQU	JAG_PLATCALL+$0C	; capture a data-port word
_vdp_data_l	EQU	JAG_PLATCALL+$12	; capture a data-port long
_vdp_scratch	EQU	JAG_PLATCALL+$40	; scratch for a stack-relative source

; Sub-slots inside the I/O shadow.  $A11100/$A11200/$A14000 are far apart in
; Genesis space and would each want their own 4 KB page; they are folded into
; one page here, because nothing reads them as a contiguous block.
JAG_Z80BUS	EQU	JAG_IO+$100		; Z80 bus request  ($A11100)
JAG_Z80RESET	EQU	JAG_IO+$110		; Z80 reset        ($A11200)
JAG_TMSS	EQU	JAG_IO+$120		; TMSS registers   ($A14000)
JAG_YM		EQU	JAG_IO+$200		; YM2612 ports     ($A04000)

; -------------------------------------------------------------------------
