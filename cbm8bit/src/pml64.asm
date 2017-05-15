        
        
* = 49152
        ; .D PML64.BIN
        ; PACKET ML 64 BY BO ZIMMERMAN
        ; UPDATED 2017/03/03 10:14P
        JMP BUF1LIN
        JMP BUFXLIN
        JMP GETPACKET
        JMP CRCP
        JMP BUFAP
        NOP
        NOP
        NOP
NUMX
        ; .BYTE 0
CRXFLG
        ; .BYTE 0
PEE0
        ; .BYTE 0 0
PEE1
        ; .BYTE 0 0
PEE2
        ; .BYTE 0 0
CRC8
        ; .BYTE 0
CRCX
        ; .BYTE 0
CRCE
        ; .BYTE 0
CRCS
        ; .BYTE 0
TIMEOUT
        ; .BYTE 0 0
BUFAPX
        ; .BYTE 5
DEBUG
        ; .BYTE 0 0
SETPSTR
        LDA $2D
        STA $FB
        LDA $2E
        STA $FC
SETPLP
        LDY #$00
        LDA ($FB),Y
        CMP #$50
        BNE SETPNXT
        INY
        LDA ($FB),Y
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
        CMP $2F
        BCC SETPLP
        BNE SETPDUN
        LDA $FC
        CMP $30
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
        LDA ($FB),Y
        STA BUF1DX
        LDY #$03
        LDA ($FB),Y
        STA $FD
        LDY #$04
        LDA ($FB),Y
        STA $FE
        LDY #$00
CRCPL1
        CPY BUF1DX
        BCS CRCPDUN
        LDA ($FD),Y
        STA BUF1,Y
        INY
        BNE CRCPL1
CRCPDUN
        JMP DOCRC8
RETIME
        LDA $A1
        STA TIMEOUT
        LDA #$00
        STA TIMEOUT+1
        RTS
CHKTIM
        LDA $A1
        CMP TIMEOUT
        BNE CHKTIM2
        LDX #$00
        RTS
CHKTIM2
        STA TIMEOUT
        INC TIMEOUT+1
        LDA TIMEOUT+1
        CMP #$04
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
        LDA $029B
        CMP $029C
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
        JSR SETPSTR
        LDY #$02
        LDA BUF1DX
        STA ($FB),Y
        LDY #$03
        LDA #<BUF1
        STA ($FB),Y
        LDY #$04
        LDA #>BUF1
        STA ($FB),Y
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
        STA CRXFLG
        JMP BUF1DUN
BUFAP1
        INC BUF1DX
        LDA BUF1DX
        CMP #$FE
        BCC BUFALP
        JMP BUF1DUN
BUFTMR
        ; .BYTE 0
BUF1DX
        ; .BYTE 0
BUFX
        ; .BYTE 0 0
BUF1
        ; .BYTE 0
PRINT
        NOP; :F$="PML64.BAS":OPEN8,8,15,"S0:PML64*":SAVEF$,8:VERIFYF$,8
