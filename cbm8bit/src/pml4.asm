


* = 4096
        ;.D PML+4.BIN
; PACKET ML +4 BY BO ZIMMERMAN
; UPDATED 2018/08/25 10:14P
        JMP BUF1LIN
        JMP BUFXLIN
        JMP GETPACKET
        JMP CRCP
        JMP BUFAP
        NOP
        NOP
        NOP
NUMX
        bytes 0
CRXFLG
        bytes 0
PEE0
        bytes 0,0
PEE1
        bytes 0,0
PEE2
        bytes 0,0
CRC8
        bytes 0
CRCX
        bytes 0
CRCE
        bytes 0
CRCS
        bytes 0
TIMEOUT
        bytes 0,0
BUFAPX
        bytes 5
DEBUG
        bytes 0,0
SETPSTR
        LDA $2D
        STA $BA
        LDA $2E
        STA $BB
SETPLP
        LDY #$00
        LDA ($BA),Y
        CMP #$50
        BNE SETPNXT
        INY
        LDA ($BA),Y
        CMP #$80
        BEQ SETPDUN
SETPNXT
        LDA $BA
        CLC
        ADC #$07
        STA $BA
        LDA $BB
        ADC #$00
        STA $BB
        LDA $BA
        CMP $2F
        BCC SETPLP
        BNE SETPDUN
        LDA $BA
        CMP $30
        BCC SETPLP
SETPDUN
        RTS
GETPACKET
        JSR IRQSET
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
        STA $BC
        LDA #>PEE0
        STA $BD
        JSR PCKDIG
        BNE PCKERR1
        LDA #<PEE1
        STA $BC
        LDA #>PEE1
        STA $BD
        JSR PCKDIG
        BNE PCKERR1
        LDA #<PEE2
        STA $BC
        LDA #>PEE2
        STA $BD
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
        LDA ($BC),Y
        STA $BA
        INY
        LDA ($BC),Y
        STA $BB
        LDX #$09
DIGLP
        LDY #$00
        LDA ($BC),Y
        CLC
        ADC $BA
        STA ($BC),Y
        INY
        LDA ($BC),Y
        ADC $BB
        STA ($BC),Y
        DEX
        BNE DIGLP
        LDY #$00
        PLA
        SEC
        SBC #48
        CLC
        ADC ($BC),Y
        STA ($BC),Y
        INY
        LDA #$00
        ADC ($BC),Y
        STA ($BC),Y
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
        LDA ($BA),Y
        STA BUF1DX
        LDY #$03
        LDA ($BA),Y
        STA $BC
        LDY #$04
        LDA ($BA),Y
        STA $BD
        LDY #$00
CRCPL1
        CPY BUF1DX
        BCS CRCPDUN
        LDA ($BC),Y
        STA BUF1,Y
        INY
        BNE CRCPL1
CRCPDUN
        JMP DOCRC8
RETIME
        LDA $A4
        STA TIMEOUT
        LDA #$00
        STA TIMEOUT+1
        RTS
CHKTIM
        LDA $A4
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
        JSR IRQSET
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
        LDA $07D1
        CMP $07D2
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
        STA ($BA),Y
        LDY #$03
        LDA #<BUF1
        STA ($BA),Y
        LDY #$04
        LDA #>BUF1
        STA ($BA),Y
        JSR DOCRC8
        JMP $FFCC
BUFXLIN
        JSR IRQSET
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
        JSR IRQSET
        LDX BUFAPX
        JSR $FFC6
        LDY #$00
        TYA
        STA BUF1DX
        STA CRXFLG
BUFALP
        LDA BUFAPX
        CMP #$05
        BNE BUFAL2
        LDA $07D3
        BEQ BUFAX
BUFAL2
        JSR $FFE4
        LDY BUF1DX
        STA BUF1,Y
        LDA BUFAPX
        CMP #$05
        BEQ BUFAP1
BUFAPST
        JSR $FFB7
        CMP #$08
        BEQ BUFAX
        CMP #$42
        BEQ BUFAX
        CMP #$00
        BEQ BUFAP1
        INC BUF1DX
BUFAX
        STA CRXFLG
        JMP BUF1DUN
BUFAP1
        INC BUF1DX
        LDA BUF1DX
        CMP #$FE
        BCC BUFALP
        JMP BUF1DUN
IRQSET
        LDA OLDIRQ
        BEQ IRQSE1
        JMP PREFLOW
IRQSE1
        SEI
        LDA $0314
        STA OLDIRQ
        LDA $0315
        STA OLDIRQ+1
        LDA #<IRQRTN
        STA $0314
        LDA #>IRQRTN
        STA $0315
        CLI
        RTS
IRQRTN
        LDA $9A
        BEQ IRQRT2
        LDA OLDIRQ
        STA $0314
        LDA OLDIRQ+1
        STA $0315
        LDA #$00
        STA OLDIRQ
        JMP $CE0E
IRQRT2
        JSR DOFLOW
        JMP $CE0E
PREFLOW
        LDA #$00
        STA $FC
        STA $FD
DOFLOW
        LDA $07D3
        BNE DOFLO3
        LDA $FD10
        AND #$02
        BNE DOFLOO
        LDA $FD10
        ORA #$02
        STA $FD10
DOFLOO
        RTS
DOFLO3
        CMP #$3C
        BCC DOFLOO
        LDA $FD10
        AND #$02
        BEQ DOFLOO
        LDA $FD10
        AND #$FD
        STA $FD10
        RTS
OLDIRQ
        bytes 0,0
BUFTMR
        bytes 0
BUF1DX
        bytes 0
BUFX
        bytes 0,0
BUF1
        bytes 0
