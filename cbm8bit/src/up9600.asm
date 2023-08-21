        
        
* = $C800
        ; UP9600 KERNAL ADAPTER
        ; ORIGINALLY BY DANIAL DALLMAN
        ; ADAPTED 2017 FOR KERNAL BY BO ZIMMERMAN
        ; MODIFIED ON 2/14/2017 1:26A
        ; .D @0:UP9600.BIN
        ;  PROVIDED FUNCTIONS
        JMP INIT
        JMP INSTALL; INSTALL AND (PROBE FOR) UP9600 (C=ERROR)
        JMP ENABLE; (RE-)ENABLE INTERFACE
        JMP DISABLE; DISABLE INTERFACE (EG. FOR FLOPPY ACCESSES)
        ;  RSOUT AND RSIN BOTH MODIFY A AND X REGISTER
        JMP RSOUT; PUT BYTE TO RS232 (BLOCKING)
        JMP RSIN; READ BYTE FROM RS232 (C=TRYAGAIN)
JIFFIES = $A2; LOWEST BYTE OF SYSTEM'S JIFFIE COUNTER
ORIGIRQ = $EA31; (MUST INCEASE JIFFIE-COUNTER !)
ORIGNMI = $FE47
NMIVECT = 792
IRQVECT = 788
WRSPTR = 670; WRITE-POINTER INTO SEND BUFFER
RDSPTR = 669; READ-POINTER INTO SEND BUFFER
WRRPTR = 667; WRITE-POINTER INTO RECEIVE BUFFER
RDRPTR = 668; READ-POINTER INTO RECEIVE BUFFER
        ;  STATIC VARIABLES
STIME
        BYTE 0;; COPY OF $A2=JIFFIES TO DETECT TIMEOUTS
OUTSTAT = 169
UPFLAG
        BYTE 0
SAVBYTE
        BYTE 0,0,0,0
        ; JIFFIES .BYTE 0
RECPTR = 247; RECBUF = $CB00;; 247 - 248
SNDPTR = 249; SNDBUF = $CC00;; 249 - 250
        ; 
        ; 
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
        LDA #<DOCHKIN
        STA $031E
        LDA #>DOCHKIN
        STA $031F
        LDA #<DOCHKOUT
        STA $0320
        LDA #>DOCHKOUT
        STA $0321
        LDA #<DOCHRIN
        STA $0324
        LDA #>DOCHRIN
        STA $0325
        LDA #<DOGETIN
        STA $032A
        LDA #>DOGETIN
        STA $032B
        LDA #<DOPUT
        STA $0326
        LDA #>DOPUT
        STA $0327
        CLI
        RTS
        ;  *******************************
DOOPEN
        PHA
        TYA
        PHA
        JSR DISABLE
        JSR $F34A; CALL IOPEN
        LDY #$00
        LDA $BA
        CMP #$02
        BNE EOPEN
        LDY #$00
         LDA ($BB),Y
        CMP #$0C
        BCC EOPEN
        LDA #$01
        STA UPFLAG
        JSR INSTALL
EOPEN
        PLA
        TAY
        PLA
        LDX #$00
        CLC
        RTS
        ;  *******************************
DOCHRIN
        LDA UPFLAG
        BEQ NOCHRIN
        LDA $99
        CMP #$02
        BEQ DOGET2
NOCHRIN
        JMP $F157
DOGETIN
        LDA UPFLAG
        BEQ NOGETIN
        LDA $99
        CMP #$02
        BEQ DOGET2
NOGETIN
        JMP $F13E
DOGET2
        TYA
        PHA
        TXA
        PHA
        LDA #$00
        STA SAVBYTE
        STA $0297
        JSR RSIN
        BCC DOGOTIN
        PLA
        TAX
        PLA
        TAY
        LDA #$08
        STA $0297
        LDA #$00
        CLC
        RTS
DOGOTIN
        STA SAVBYTE
DOGET4
        PLA
        TAX
        PLA
        TAY
        LDA SAVBYTE
        CLC
        RTS
        ;  *******************************
DOPUT
        PHA
        LDA UPFLAG
        BEQ NOPUT1
        LDA $9A
        CMP #$02
        BEQ DOPUT2
NOPUT1
        PLA
        JMP $F1CA
DOPUT2
        CLC
        PLA
        JSR RSOUT
        LDA #$00
        STA $0297
        CLC
DOPUT4
        RTS
        ;  *******************************
        ; ;;;;;;;;;;;;;;;;
DOCLOSE
        PHA
        JSR DISABLE
        JSR $F314
        BEQ DOCLO2
        PLA
        CLC
        RTS
DOCLO2
        JSR $F31F; SET BA
        LDA $BA
        CMP #$02
        BEQ DOCLO4
DOCLO3
        PLA
        JMP $F291
DOCLO4
        LDA #$00
        STA UPFLAG
        PLA
        LDX #$00
        CLC
        RTS
        ;  *******************************
DOCHKIN
        PHA
        LDA UPFLAG
        BNE DOCHKI1
        PLA
        JMP $F20E
DOCHKI1
        PLA
        JSR $F30F
        BEQ DOCHKI2
        JMP $F701
DOCHKI2
        JSR $F31F
        LDA $BA
        CMP #$02
        BEQ DOCHKI4
        CMP #$04
        BCC NOCHKIN
DOCHKI3
        JSR DISABLE
        JMP NOCHKIN
DOCHKI4
        STA $99
        JSR ENABLE
        CLC
        RTS
NOCHKIN
        JMP $F219
        ; 
DOCHKOUT
        PHA
        LDA UPFLAG
        BNE DOCHKO1
        PLA
        JMP $F250
DOCHKO1
        PLA
        JSR $F30F
        BEQ DOCHKO2
        JMP $F701
DOCHKO2
        JSR $F31F
        LDA $BA
        CMP #$02
        BEQ DOCHKO4
        CMP #$04
        BCC NOCHKOUT
DOCHKO3
        JSR DISABLE
        JMP NOCHKOUT
DOCHKO4
        STA $9A
        JSR ENABLE
        CLC
        RTS
NOCHKOUT
        JMP $F25B
        ;  *******************************
NMIDOBIT
        PHA
        BIT $DD0D; CHECK BIT 7 (STARTBIT PRINT)
        BPL NMIDOBI2; NO STARTBIT RECEIVED, THEN SKIP
        LDA #$13
        STA $DD0F; START TIMER B (FORCED RELOAD, SIGNAL AT PB7)
        STA $DD0D; DISABLE TIMER AND FLAG INTERRUPTS
        LDA #<NMIBYTRY; ON NEXT NMI CALL NMIBYTRY
        STA NMIVECT; (TRIGGERED BY SDR FULL)
        LDA #>NMIBYTRY
        STA NMIVECT+1
NMIDOBI2
        PLA; IGNORE, IF NMI WAS TRIGGERED BY RESTORE-KEY
        RTI
        ; 
NMIBYTRY
        PHA
        BIT $DD0D; CHECK BIT 7 (SDR FULL PRINT)
        BPL NMIDOBI2; SDR NOT FULL, THEN SKIP (EG. RESTORE-KEY)
        LDA #$92
        STA $DD0F; STOP TIMER B (KEEP SIGNALLING AT PB7!)
        STA $DD0D; ENABLE FLAG (AND TIMER) INTERRUPTS
        LDA #<NMIDOBIT; ON NEXT NMI CALL NMIDOBIT
        STA NMIVECT; (TRIGGERED BY A STARTBIT)
        LDA #>NMIDOBIT
        STA NMIVECT+1
        TXA
        PHA
        TYA
        PHA
        LDA $DD0C; READ SDR (BIT0=DATABIT7,...,BIT7=DATABIT0)
        CMP #128; MOVE BIT7 INTO CARRY-FLAG
        AND #127
        TAX
        LDA REVTAB,X; READ DATABITS 1-7 FROM LOOKUP TABLE
        ADC #0; ADD DATABIT0
        LDY WRRPTR; AND WRITE IT INTO THE RECEIVE BUFFER
        STA (RECPTR),Y
        INY
        STY WRRPTR
        SEC;;START BUFFER FULL CHK
        TYA
        SBC RDRPTR
        CMP #200
        BCC NMIBYTR2
        LDA $DD01;; MORE THAN 200 BYTES IN THE RECEIVE BUFFER
        AND #$FD;; THEN DISABLE RTS
        STA $DD01
NMIBYTR2
        PLA
        TAY
        PLA
        TAX
        PLA
        RTI
        ;  *******************************************************
        ;  IRQ PART
NEWIRQ
        LDA $DC0D
NEWIRQ1
        LSR; READ IRQ-MASK
        LSR; MOVE BIT1 INTO CARRY-FLAG (TIMER B - FLAG)
        AND #$02; TEST BIT3 (SDR - FLAG)
        BEQ NEWIRQ3; SDR NOT EMPTY, THEN SKIP THE FIRST PART
        LDX OUTSTAT
        BEQ NEWIRQ2; SKIP, IF WE'RE NOT WAITING FOR AN EMPTY SDR
        DEX
        STX OUTSTAT
NEWIRQ2
        BCC NEWIRQ6
NEWIRQ3
        CLI
        JSR $FFEA
        LDA $CC
        BNE NEWIRQ5
        DEC $CD
        BNE NEWIRQ5
        LDA #$14
        STA $CD
        LDY $D3
        LSR $CF
        LDX $0287
        LDA ($D1),Y
        BCS NEWIRQ4
        INC $CF
        STA $CE
        JSR $EA24
        LDA ($F3),Y
        STA $0287
        LDX $0286
        LDA $CE
NEWIRQ4
        EOR #$80
        JSR $EA1C
NEWIRQ5
        JSR $EA87
NEWIRQ6
        JMP $EA81
        ;  *******************************
        ;  GET BYTE FROM SERIAL INTERFACE
RSIN
        LDY RDRPTR
        CPY WRRPTR
        BEQ RSIN3; SKIP (EMPTY BUFFER, RETURN WITH CARRY SET)
        LDA (RECPTR),Y
        INY
        STY RDRPTR
        PHA;;BEGIN BUFFER EMPTYING CHK
        TYA
        SEC
        SBC WRRPTR
        CMP #206;;256-50
        BCC RSIN2
        LDA #2
        ORA $DD01
        STA $DD01;; ENABLE RTS
        CLC
RSIN2
        PLA
RSIN3
        RTS
        ;  ******************************
        ;  PUT BYTE TO SERIAL INTERFACE
RSOUT
        PHA
        STA $9E
        CMP #$80
        AND #$7F
        STX $A8
        STY $A7
        TAX
        JSR RSOUTX
RSOUT3
        LDA REVTAB,X
        ADC #$00
        LSR
        SEI
        STA $DC0C
        LDA #$02
        STA OUTSTAT
        ROR
        ORA #$7F
        STA $DC0C
        CLI
        LDX $A8
        LDY $A7
        PLA
        RTS
RSOUTX
        CLI
        LDA #$FD
        STA $A2
RSOUTX2
        LDA OUTSTAT
        BEQ RSOUTX3
        BIT $A2
        BMI RSOUTX2
RSOUTX3
        JMP $F490
        ;  ******************************
        ;  INSTALL (AND PROBE FOR) SERIAL INTERFACE
        ;  RETURN WITH CARRY SET IF THERE WAS AN ERROR
INSTERR
        CLI
        SEC
        RTS
INSTALL
        SEI
        LDA IRQVECT
        CMP #<ORIGIRQ
        BNE INSTERR; IRQ-VECTOR ALREADY CHANGED
        LDA IRQVECT+1
        CMP #>ORIGIRQ
        BNE INSTERR; IRQ-VECTOR ALREADY CHANGED
        LDA NMIVECT
        CMP #<ORIGNMI
        BNE INSTERR; NMI-VECTOR ALREADY CHANGED
        LDA NMIVECT+1
        CMP #>ORIGNMI
        BNE INSTERR; NMI-VECTOR ALREADY CHANGED
        LDY #0
        STY WRSPTR
        STY RDSPTR
        STY WRRPTR
        STY RDRPTR
        ;  PROBE FOR RS232 INTERFACE
        CLI
        LDA #$7F
        STA $DD0D; DISABLE ALL NMIS
        LDA #$80
        STA $DD03; PB7 USED AS OUTPUT
        STA $DD0E; STOP TIMERA
        STA $DD0F; STOP TIMERB
        BIT $DD0D; CLEAR PENDING INTERRUPTS
        LDX #8
INSTALL2
        STX $DD01; TOGGLE TXD
        STA $DD01; AND LOOK IF IT TRIGGERS AN
        DEX; SHIFT-REGISTER INTERRUPT
        BNE INSTALL2
        LDA $DD0D; CHECK FOR BIT3 (SDR-FLAG)
        AND #8
        BEQ INSTERR; NO INTERFACE DETECTED
        ;  GENERATE LOOKUP TABLE
        LDX #0
INSTALL3
        STX OUTSTAT; OUTSTAT USED AS TEMPORARY VARIABLE
        LDY #8
INSTALL4
        ASL OUTSTAT
        ROR
        DEY
        BNE INSTALL4
        STA REVTAB,X
        INX
        BPL INSTALL3
        ;  ******************************
        ;  ENABLE SERIAL INTERFACE (IRQ+NMI)
ENABLE
        PHA
        TXA
        PHA
        TYA
        PHA
        LDA IRQVECT
        CMP #<NEWIRQ
        BNE ENABL2
        LDA IRQVECT+1
        CMP #>NEWIRQ
        BNE ENABL2
        PLA
        TAY
        PLA
        TAX
        PLA
        RTS
ENABL2
        SEI
        LDX #<NEWIRQ; INSTALL NEW IRQ-HANDLER
        LDY #>NEWIRQ
        STX IRQVECT
        STY IRQVECT+1
        LDX #<NMIDOBIT; INSTALL NEW NMI-HANDLER
        LDY #>NMIDOBIT
        STX NMIVECT
        STY NMIVECT+1
        LDX $2A6; PAL OR NTSC VERSION PRINT
        LDA ILOTAB,X; (KEYSCAN INTERRUPT ONCE EVERY 1/64 SECOND)
        STA $DC06; (SORRY THIS WILL BREAK CODE, THAT USES
        LDA IHITAB,X; THE TI$ - VARIABLE)
        STA $DC07; START VALUE FOR TIMER B (OF CIA1)
        TXA
        ASL
        EOR #$33; ** TIME CONSTANT FOR SENDER **
        LDX #0; 51 OR 55 DEPENDING ON PAL/NTSC VERSION
        STA $DC04; START VALUE FOR TIMERA (OF CIA1)
        STX $DC05; (TIME IS AROUND 1/(2*BAUDRATE) )
        ASL; ** TIME CONSTANT FOR RECEIVER **
        ORA #1; 103 OR 111 DEPENDING ON PAL/NTSC VERSION
        STA $DD06; START VALUE FOR TIMERB (OF CIA2)
        STX $DD07; (TIME IS AROUND 1/BAUDRATE )
        LDA #$41; START TIMERA OF CIA1, SP1 USED AS OUTPUT
        STA $DC0E; GENERATES THE SENDER'S BIT CLOCK
        LDA #1
        STA OUTSTAT
        STA $DC0D; DISABLE TIMERA (CIA1) INTERRUPT
        STA $DC0F; START TIMERB OF CIA1 (GENERATES KEYSCAN IRQ)
        LDA #$92; STOP TIMERB OF CIA2 (ENABLE SIGNAL AT PB7)
        STA $DD0F
        LDA #$98
        BIT $DD0D; CLEAR PENDING NMIS
        STA $DD0D; ENABLE NMI (SDR AND FLAG) (CIA2)
        LDA #$8A
        STA $DC0D; ENABLE IRQ (TIMERB AND SDR) (CIA1)
        LDA #$FF
        STA $DD01; PB0-7 DEFAULT TO 1
        STA $DC0C; SP1 DEFAULTS TO 1
        SEC
        LDA WRRPTR
        SBC RDRPTR
        CMP #200
        BCS ENABLE2;; DON'T ENABLE RTS IF REC-BUFFER IS FULL
        LDA #2;; ENABLE RTS
        STA $DD03;; (THE RTS LINE IS THE ONLY OUTPUT)
ENABLE2
        CLI
        PLA
        TAY
        PLA
        TAX
        PLA
        RTS
        ;  TABLE OF TIMER VALUES FOR PAL AND NTSC VERSION
ILOTAB
        BYTE 149,37
        ; 
IHITAB
        BYTE 66,64
        ;  *******************************************************
        ;  DISABLE SERIAL INTERFACE
DISABLE
        PHA
        TXA
        PHA
        TYA
        PHA
        LDA IRQVECT
        CMP #<NEWIRQ
        BNE NODIS
        LDA IRQVECT+1
        CMP #>NEWIRQ
        BEQ DISABL2
NODIS
        PLA
        TAY
        PLA
        TAX
        PLA
        RTS
DISABL2
        SEI
        LDA $DD01; DISABLE RTS
        AND #$FD
        STA $DD01
        LDA #$7F
        STA $DD0D; DISABLE ALL CIA INTERRUPTS
        STA $DC0D
        LDA #$41; QUICK (AND DIRTY) HACK TO SWITCH BACK
        STA $DC05; TO THE DEFAULT CIA1 CONFIGURATION
        LDA #$81
        STA $DC0D; ENABLE TIMER1 (THIS IS DEFAULT)
        LDA #<ORIGIRQ; RESTORE OLD IRQ-HANDLER
        STA IRQVECT
        LDA #>ORIGIRQ
        STA IRQVECT+1
        LDA #<ORIGNMI; RESTORE OLD NMI-HANDLER
        STA NMIVECT
        LDA #>ORIGNMI
        STA NMIVECT+1
        CLI
        PLA
        TAY
        PLA
        TAX
        PLA
        RTS
        ; 
        ; 
REVTAB
        ; .BUF 128
PRINT
        NOP; :F$="UP9600.BAS":OPEN1,8,15,"S0:UP9600*":CLOSE1:SAVEF$,8
