* = $C800
        ;.D SWIFTDRVR.BIN
        ; KERNAL BAUD RATES
        ; B50=1
        ; B75=2
        ; B110=3
        ; B135=4
        ; B150=5
        ; B300=6
        ; B600=7
        ; B1200=8
        ; B1800=9
        ; B2400=10
        ; B3600=11
        ; B4800=12
        ; B7200=13
        ; B9600=14
        ; B19200=15
        ; B38400=16
        ; B57600=17
        ; B115200=18
        ; B230400=19
;;;;;;;;;;;
BASE = $DE00
DATAPORT = $DE00
STATUS = $DE01
COMMAND = $DE02
CONTROL = $DE03
ESR232 = $DE07
;
RHEAD = 668
RTAIL = 667
RBUFF = $F7
NMINV = $0318
;;;;;;;;;;;
        JMP INIT
; INITCHIP ; NEWVEC
; RECVBYTE
; XMITBYTE
;;;;;;;;;;;
INIT
        SEI
        LDA $031B
        CMP #>DOOPEN
        BEQ NOINIT
        LDY #20
INSAV
        LDA $0319,Y
        STA VECS,Y
        DEY
        BNE INSAV
        LDA $031A
        STA DOOPEN+1
        LDA $031B
        STA DOOPEN+2
        LDA $031C
        STA DOCLOSE+1
        LDA $031D
        STA DOCLOSE+2
        LDA $0326
        STA DOPUT2+1
        LDA $0327
        STA DOPUT2+2
        LDA $032C
        STA DOCLAL+1
        LDA $032D
        STA DOCLAL+2
NOINIT
        LDA #<DOOPEN
        STA $031A
        LDA #>DOOPEN
        STA $031B
        CLI
        RTS
;;;;;;;;;;;
DOOPEN
        JSR $F34A; CALL IOPEN
        PHP
        PHA
        LDA $BA
        CMP #$02
        BEQ DOOP2
        PLA;EARLY EXIT
        PLP
        RTS
DOOP2
        TYA
        PHA
        LDY #$00
        STY RHEAD
        STY RTAIL
        STY RCOUNT
        STY ERRORS
; INTERNAL CLOCK, 8N1, BAUD BELOW
;;DA #>RECVBUFF:;TA RBUFF+1
; INTERNAL CLOCK, 8N1, BAUD BELOW
        STY ESR232
        LDA ($BB),Y
        TAY
        LDA BAUDS,Y
        BPL DOOP3
        LDY #16
        STY CONTROL
        AND #$7F
        STA ESR232
        LDA #$00
DOOP3
        ORA #16;%00010000
        STA CONTROL
; NO PAR, NO ECHO, XMIT INT, RECV INT
        LDA #9; %00001001 ;
        STA COMMAND
        STA ZPXMF
        AND #%11110000; KEEP PARITY/ECHO
        ORA #9;%00001001 ; SET RECV BUF ONLY
        STA ZPXMO
NEWVEC
        SEI
        LDA NMINV+1
        CMP #>NEWNMI
        BEQ NONVEC
        STA OLDNMI+1
        LDA NMINV
        STA OLDNMI
        LDA #<NEWNMI
        STA NMINV
        LDA #>NEWNMI
        STA NMINV+1
NONVEC
        CLI
        LDA #<DOCLOSE
        STA $031C
        LDA #>DOCLOSE
        STA $031D
        LDA #<DOPUT
        STA $0326
        LDA #>DOPUT
        STA $0327
        LDA #>DOCLAL
        STA $032C
        LDA #<DOCLAL
        STA $032D
EOPEN
        JSR UPDCD
        PLA
        TAY
        PLA
        PLP
        RTS
;;;;;;;;;;;;
NEWNMI
        PHA
        TXA
        PHA
        TYA
        PHA
;LDX #%00000011:STX COMMAND ; DISABLE STUFF
        CLD
        LDA STATUS
        AND #8; MASK OUT NON-INDI
        BEQ NREVD
;STA ERRORS
        LDY RTAIL
        LDA DATAPORT
        STA (RBUFF),Y
        INC RTAIL
        INC RCOUNT
NREVD
        NOP;LDA ZPXMF:STA COMMAND
;JMP $FEBC
        JSR $FFE1
        BNE NMOUT
        JMP $FE66
NMOUT
        PLA
        TAY
        PLA
        TAX
        PLA
        RTI
;;;;;;;;;;;;;;;;
SAVBYTE
        bytes 0
UPDCD
        PHP
        PHA
        LDA $DD03
        ORA #$10
        STA $DD03
        LDA STATUS
        AND #64
        BEQ CLRDCD
        LDA $DD01
        ORA #$10
        STA $DD01
        PLA
        PLP
        RTS
CLRDCD
        LDA $DD01
        AND #$EF
        STA $DD01
        PLA
        PLP
        RTS
;;;;;;;;;;;;;;;;;;
DOPUT
        PHA
        LDA $9A
        CMP #$02
        BEQ DOPUT3
        PLA
DOPUT2
        JMP $F1CA
DOPUT3
        LDA STATUS
        AND #16;%00010000
        BEQ DOPUT3
        CLC
        PLA
        STA DATAPORT
        BCC DOPUT4
        LDA #$00
DOPUT4
        RTS
;;;;;;;;;;;;;;;;;;
DOCLOSE
        JSR $F291
        PHP
        PHA
        LDA $BA
        CMP #$02
        BNE NOCLOS
DOCLOS2
        LDA #3; 00000011
        STA COMMAND; DISABLE STUFF
        LDA NMINV+1
        CMP #>NEWNMI
        BNE NOCLOS
        SEI
        LDA OLDNMI
        STA NMINV
        LDA OLDNMI+1
        STA NMINV+1
        TYA
        PHA
        LDY #20
INCLO
        LDA VECS,Y
        STA $0319,Y
        DEY
        BNE INCLO
        PLA
        TAY
        JSR NOINIT; DOOPEN, AND CLI!
NOCLOS
        JSR UPDCD
        PLA
        PLP
        RTS
DOCLAL
        JSR $F32F
        PHP
        PHA
        JMP DOCLOS2
;;;;;;;;;;;;;;;;;;
BAUDS
        BYTE 9,0,0,1,2,2,5,6,7,8,8,9,10,11,12,14,15,130,129,128,0,0,0,0,0,0
VECS
        BYTE 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
OLDNMI
        BYTE 71,254
RCOUNT
        BYTE 0
ERRORS
        BYTE 0
ZPXMO
        BYTE 0
ZPXMF
        BYTE 0
