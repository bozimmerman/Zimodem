        
        
* = 49152
        ; .D SWIFTDRVR.BIN
        ; BAUD RATES
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
        ; ;;;;;;;;;
BASE = $DE00
DATAPORT = $DE00
STATUS = $DE01
COMMAND = $DE02
CONTROL = $DE03
        ;
RHEAD = $A7
RTAIL = $A8
RBUFF = $F7
RCOUNT = $B4
ERRORS = $B5
ZPXMO = $B6
ZPXMF = $BD
NMINV = $0318
        ; ;;;;;;;;;
        JMP INIT
        ; INITCHIP ; NEWVEC
        ; RECVBYTE
        ; XMITBYTE
        ; ;;;;;;;;;
INIT
        SEI
        LDA #<DOOPEN
        STA $031A
        LDA #>DOOPEN
        STA $031B
        LDA #<DOCLOSE
        STA $031C
        LDA #>DOCLOSE
        STA $031D
        LDA #<DOCHRIN
        STA $0324
        LDA #<DOGETIN
        STA $032A
        LDA #>DOCHRIN
        STA $0325
        LDA #>DOGETIN
        STA $032B
        LDA #<DOPUT
        STA $0326
        LDA #>DOPUT
        STA $0327
        CLI
        RTS
        ; ;;;;;;;;;
DOOPEN
        JSR $F34A; CALL IOPEN
        PHA
        TYA
        PHA
        LDA $BA
        CMP #$02
        BEQ DOOP2
        PLA; EARLY EXIT
        TAY
        PLA
        RTS
DOOP2
        NOP
        LDA #$00
        STA RHEAD
        STA RTAIL
        STA RCOUNT
        STA ERRORS
        ; LDA #<RECVBUFF:;TA RBUFF
        ; DA #>RECVBUFF:;TA RBUFF+1
        ; INTERNAL CLOCK, 8N1, BAUD BELOW
        LDY #$00
        LDA ($BB),Y
        ORA #%00010000
        STA CONTROL
        ; NO PAR, NO ECHO, NO XMIT INT, YES RECV INT
        LDA #%00001001 ;
        STA COMMAND
        STA ZPXMF
        AND #%11110000 ; KEEP PARITY/ECHO
        ORA #%00001001 ; SET RECV BUF ONLY
        STA ZPXMO
NEWVEC
        SEI
        LDA #<NEWNMI
        STA NMINV
        LDA #>NEWNMI
        STA NMINV+1
        CLI
EOPEN
        PLA
        TAY
        PLA
        RTS
        LDX #$00
        RTS
        ; ;;;;;;;;;;
NEWNMI
        NOP
        PHA
        TXA
        PHA
        TYA
        PHA
        LDA STATUS
        LDX #%00000011
        STX COMMAND ; DISABLE STUFF
        STA ERRORS
        AND #%00001000 ; MASK OUT NON-INDI
        BEQ NREVD
        LDA DATAPORT
        LDY RTAIL
        STA (RBUFF),Y
        INC RTAIL
        INC RCOUNT
        LDA ZPXMF
        STA COMMAND
NREVD
        PLA
        TAY
        PLA
        TAX
        PLA
        RTI
        ; ;;;;;;;;;;;;;;
SAVBYTE
        ; .BYTE 0
DOCHRIN
        LDA $99
        CMP #$02
        BEQ DOGET2
        JMP $F157
DOGETIN
        LDA $99
        CMP #$02
        BEQ DOGET2
        JMP $F13E
DOGET2
        TYA
        PHA
        TXA
        PHA
        LDA #$00
        STA SAVBYTE
        LDA RHEAD
        CMP RTAIL
        BEQ DOGET4
DOGET3
        TAY
        LDA (RBUFF),Y
        INC RHEAD
        STA SAVBYTE
DOGET4
        PLA
        TAX
        PLA
        TAY
        LDA SAVBYTE
        CLC
        RTS
        ; ;;;;;;;;;;;;;;;;
DOPUT
        PHA
        LDA $9A
        CMP #$02
        BEQ DOPUT2
        PLA
        JMP $F1CA
DOPUT2
        LDA STATUS
        AND #%00010000
        BEQ DOPUT2
        CLC
        PLA
        STA DATAPORT
        BCC DOPUT3
        LDA #$00
DOPUT3
        RTS
        ; ;;;;;;;;;;;;;;;;
DOCLOSE
        PHA
        JSR $F314
        BEQ DOCLO2
        PLA
        CLC
        RTS
DOCLO2
        JSR $F31F ; SET BA
        LDA $BA
        CMP #$02
        BEQ DOCLO3
        PLA
        JMP $F291
DOCLO3
        PLA
        JSR $F291
        LDX #%00000011
        STX COMMAND ; DISABLE STUFF
        SEI
        LDA #$47
        STA NMINV
        LDA #$FE
        STA NMINV+1
        CLI
        LDX #$00
        RTS
        ; ;;;;;;;;;;;;;;;;
PRINT
        NOP; :OPEN1,8,15,"S0:SWIFTDRVR*":CLOSE1:SAVE"SWIFTDRVR",8
