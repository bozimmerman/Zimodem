!--------------------------------------------------
!- Thursday, April 26, 2018 9:07:31 PM
!- Import of : 
!- c:\src\zimodem\cbm8bit\src\cometmode.prg
!- Commodore 64
!--------------------------------------------------
10 IFPEEK(49153)<>54THENLOAD"v-1541",8,1:RUN
20 OPEN5,2,0,CHR$(8):TI$="000000":T1=0
30 PRINT#5,"at&p0r0e0f0":TT=TI+100:GOSUB900:PRINT"connecting...";
40 GOSUB920:PRINT".";:T1=T1+1:IFT1>20THENPRINT"fail.":CLOSE5:END
50 PRINT#5,"at":TT=TI+100:GOSUB900
60 INPUT#5,A$:IFLEFT$(A$,2)="at"ORLEFT$(A$,2)="AT"THENINPUT#5,A$
70 IFLEFT$(A$,2)<>"ok"ANDLEFT$(A$,2)<>"OK"THEN40
80 Q$=CHR$(34):GOSUB920
90 PRINT#5,"atf0c";Q$;"commodoreserver.com:1541";Q$:TT=TI+400:GOSUB900
100 INPUT#5,A$:IFLEFT$(A$,2)="at"ORLEFT$(A$,2)="AT"THENINPUT#5,A$
110 IFLEFT$(A$,8)<>"connect "ANDLEFT$(A$,8)<>"CONNECT "THEN40
120 PRINT#5,"atb2400o0":TT=TI+100:GOSUB900
130 PRINT"done."
210 PRINT"* reset modem to exit comet mode."
220 CLOSE5:SYS49152
230 END
900 IFTI<TTTHEN900
910 RETURN
920 GET#5,A$:IFA$<>""THEN920
930 RETURN
