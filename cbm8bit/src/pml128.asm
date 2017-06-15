        
        
* = 4864
        ; .D PML128.BIN
        ; PACKET ML 128 BY BO ZIMMERMAN
        ; UPDATED 2017/03/04 10:14P
        ;
        JMP BUF1LIN
        JMP BUFXLIN
        JMP GETPACKET
        JMP CRCP
        JMP BUFAP
        NOP
        NOP
        NOP
NUMX
        BYTE 0
CRXFLG
        BYTE 0
PEE0
        BYTE 0,0
PEE1
        BYTE 0,0
PEE2
        BYTE 0,0
CRC8
        BYTE 0
CRCX
        BYTE 0
CRCE
        BYTE 0
CRCS
        BYTE 0
TIMEOUT
        BYTE 0,0
BUFAPX
        BYTE 5
DEBUG
        BYTE 0,0
SETPSTR
        LDA $2F
        STA $FB
        LDA $30
        STA $FC
SETPLP
        LDY #$00
        LDA #$FB
        LDX #$01
        JSR $FF74
        CMP #$50
        BNE SETPNXT
        INY
        LDA #$FB
        LDX #$01
        JSR $FF74
        CMP #$80
        BEQ SETPDUN
SETPNXT
        LDA $FB
        CLC
        ADC #$07
        STA $FB
        LDA $FC
        ADC #$00
        STA $FC
        LDA $FB
        CMP $31
        BCC SETPLP
        BNE SETPDUN
        LDA $FC
        CMP $32
        BCC SETPLP
SETPDUN
        RTS
GETPACKET
        LDX #$0C
PACKCLR
        LDA #$00
        STA NUMX,X
        DEX
        BNE PACKCLR
        STA NUMX
        JSR $FFCC
        LDX #$05
        JSR $FFC6
        JSR BUF1LIN
        LDY #$00
        STY CRC8
PCKLP1
        CPY BUF1DX
        BCC PCKC1
PCKDUN1
        JMP $FFCC
PCKERR1
        LDX #$FF
        STX CRXFLG
        JMP $FFCC
PCKC1
        LDA BUF1,Y
        INY
        CMP #91
        BNE PCKLP1
        LDA BUF1,Y
        INY
        CMP #32
        BNE PCKERR1
        LDA #<PEE0
        STA $FD
        LDA #>PEE0
        STA $FE
        JSR PCKDIG
        BNE PCKERR1
        LDA #<PEE1
        STA $FD
        LDA #>PEE1
        STA $FE
        JSR PCKDIG
        BNE PCKERR1
        LDA #<PEE2
        STA $FD
        LDA #>PEE2
        STA $FE
        JSR PCKDIG
        BNE PCKERR1
        LDA PEE1+1
        BNE PCKERR1
        LDA PEE1
        STA NUMX
        BEQ PCKDUN1
PCKNEWP
        JMP BUFXLIN
PCKDIG
        CPY BUF1DX
        BCC PCKDIG2
DIGERR1
        LDX #$FF
        RTS
PCKDIG2
        LDA BUF1,Y
        INY
PCKDIG3
        CMP #48
        BCC DIGERR1
        CMP #58
        BCS DIGERR1
        TAX
        TYA
        PHA
        TXA
        PHA
        LDY #$00
        LDA ($FD),Y
        STA $FB
        INY
        LDA ($FD),Y
        STA $FC
        LDX #$09
DIGLP
        LDY #$00
        LDA ($FD),Y
        CLC
        ADC $FB
        STA ($FD),Y
        INY
        LDA ($FD),Y
        ADC $FC
        STA ($FD),Y
        DEX
        BNE DIGLP
        LDY #$00
        PLA
        SEC
        SBC #48
        CLC
        ADC ($FD),Y
        STA ($FD),Y
        INY
        LDA #$00
        ADC ($FD),Y
        STA ($FD),Y
        PLA
        TAY
        CPY BUF1DX
        BCS DIGERR2
        LDA BUF1,Y
        INY
        CMP #32
        BNE PCKDIG3
        LDA #$00
        RTS
DIGERR2
        LDA #$FF
        RTS
BUFCLR
        LDA #$00
        TAY
BUFCLP
        STA BUF1,Y
        INY
        BNE BUFCLP
        RTS
DOCRC8
        LDA #$00
        STA CRC8
        LDA #$00
        STA CRCX
DOCRC0
        LDA CRCX
        CMP BUF1DX
        BCC DOCRC1
        RTS
DOCRC1
        TAX
        LDA BUF1,X
        STA CRCE
        LDY #$08
DOCRC2
        LDA CRC8
        EOR CRCE
        AND #$01
        STA CRCS
        LDA CRC8
        CLC
        ROR
        STA CRC8
        LDA CRCS
        BEQ DOCRC3
        LDA CRC8
        EOR #$8C
        STA CRC8
DOCRC3
        LDA CRCE
        CLC
        ROR
        STA CRCE
        DEY
        BNE DOCRC2
        INC CRCX
        BNE DOCRC0
        RTS
CRCP
        JSR SETPSTR
        LDY #$02
        LDA #$FB
        LDX #$01
        JSR $FF74
        STA BUF1DX
        INY
        LDA #$FB
        LDX #$01
        JSR $FF74
        STA $FD
        INY
        LDA #$FB
        LDX #$01
        JSR $FF74
        STA $FE
        LDY #$00
CRCP11
        CPY BUF1DX
        BCS CRCPDUN
        LDA #$FD
        LDX #$01
        JSR $FF74
        STA BUF1,Y
        INY
        BNE CRCP11
CRCPDUN
        JMP DOCRC8
RETIME
        LDA #$00
        TAX
        TAY
        JMP $FFDB
CHKTIM
        JSR $FFDE
        CPX #$03
        BCS TIMBAD
        LDX #$00
        RTS
TIMBAD
        LDX #$FF
        RTS
BUF1LIN
        LDX #$05
        JSR $FFC6
        LDY #$00
        TYA
        STA BUF1DX
        JSR RETIME
BUF1LP
        JSR CHKTIM
        BEQ BUF1L2
        STX CRXFLG
        JMP $FFCC
BUF1L2
        LDA $0A18
        CMP $0A19
        BEQ BUF1LP
        JSR $FFE4
        LDY BUF1DX
        STA BUF1,Y
        CMP #$00
        BEQ BUF1LP
        CMP #$0A
        BEQ BUF1LP
        CMP #$0D
        BEQ BUF1DUN
        JSR RETIME
        INC BUF1DX
        LDA BUF1DX
        CMP #$FE
        BCC BUF1LP
BUF1DUN
        LDA $02B9
        PHA
        JSR SETPSTR
        LDA #$FB
        STA $02B9
        LDA $FB
        CLC
        ADC #$02
        STA PBACK
        LDA $FC
        ADC #$00
        STA PBACK+1
        LDY #$02
        LDX #$01
        LDA BUF1DX
        JSR $FF77
        LDY #$03
        LDX #$01
        LDA #$00
        JSR $FF77
        LDY #$04
        LDX #$01
        LDA #$FE
        JSR $FF77
        LDY #$00
        STY $FB
        LDA #$FE
        STA $FC
BUF1OL1
        CPY BUF1DX
        BCS BUF1OU0
        LDA BUF1,Y
        LDX #$01
        JSR $FF77
        INY
        BNE BUF1OL1
        JMP BUF1OU1
BUF1OU0
        LDX #$01
        LDA PBACK
        JSR $FF77
        LDX #$01
        INY
        LDA PBACK+1
        JSR $FF77
BUF1OU1
        PLA
        STA $02B9
        JSR DOCRC8
        JMP $FFCC
BUFXLIN
        LDA #$00
        STA CRXFLG
        STA BUF1DX
        LDX #$05
        JSR $FFC6
        JSR RETIME
BUFXLP
        JSR $FFE4
        LDY BUF1DX
        STA BUF1,Y
        JSR CHKTIM
        BEQ BUFXEC
        STX CRXFLG
        JMP $FFCC
BUFXEC
        JSR $FFB7
        AND #$0B
        BNE BUFXLP
        JSR RETIME
        LDY BUF1DX
        LDA BUF1,Y
        DEC NUMX
        CMP #$0D
        BNE BUFXNCR
        LDX CRXFLG
        BNE BUFXNCR
        STY CRXFLG
        INC CRXFLG
BUFXNCR
        INC BUF1DX
        LDA BUF1DX
        CMP #$FD
        BCS BUFXDUN
BUF1NI
        LDA NUMX
        BNE BUFXLP
        JMP BUF1DUN
BUFXDUN
        JMP BUF1DUN
BUFAP
        LDX BUFAPX
        JSR $FFC6
        LDY #$00
        TYA
        STA BUF1DX
        STA CRXFLG
BUFALP
        JSR $FFE4
        LDY BUF1DX
        STA BUF1,Y
        JSR $FFB7
        CMP #$00
        BEQ BUFAP1
        INC BUF1DX
        STA CRXFLG
        JMP BUF1DUN
BUFAP1
        INC BUF1DX
        LDA BUF1DX
        CMP #$FE
        BCC BUFALP
        JMP BUF1DUN
PBACK
        BYTE 0,0
BUFTMR
        BYTE 0
BUF1DX
        BYTE 0
BUFX
        BYTE 0,0
BUF1
        BYTE 0
PRINT
        NOP; :F$="PML128.BAS":OPEN1,8,15,"S0:PML128*":SAVEF$,8:VERIFYF$,8
