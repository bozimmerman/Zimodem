        
        
* = 0
        ; .D TELNETML.BIN
        ; TERMINAL ML 64 BY BO ZIMMERMAN
        ; UPDATED 2017/01/01 07:44P
        ; TERMSETUP LDA $DD01:AND #$10:STA DCDCHK
TERMINAL
        LDX #$05
        JSR $FFC6
        JSR $FFE4
        CMP #$00
        BEQ TERMK
        CMP #$0A
        BEQ TERMK
        JSR $FFD2
        ; TERMK LDA $DD01:AND #$10:BNE TERMK2:LDA DCDCHK:BNE ESCOUT
TERMK
        LDX #$00
        JSR $FFC6
        JSR $FFE4
        CMP #$00
        BEQ TERMINAL
        CMP #$85
        BEQ ESCOUT
        CMP #$1B
        BNE TERMOUT
ESCOUT
        JSR $FFCC
        RTS
TERMOUT
        PHA
        LDX #$05
        JSR $FFC9
        PLA
        JSR $FFD2
        LDX #$00
        JSR $FFC9
        LDX #$FF
        BNE TERMINAL
        ; DCDCHK = $4D
PRINT
        NOP; :F$="TELNETML.BAS":OPEN8,8,15,"S0:TELNETML*":SAVEF$,8:VERIFYF$,8
