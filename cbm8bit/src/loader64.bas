!--------------------------------------------------
!- Import of : 
!- c:\src\zimodem\cbm8bit\src\loader64.prg
!- Commodore 64
!--------------------------------------------------
10 IFPEEK(49153)<>54THENLOAD"v-1541",8,1:RUN
20 OPEN5,2,0,CHR$(8):TI$="000000":T1=0
30 PRINT#5,"at&p0r0e0f0":TT=TI+100:GOSUB900:PRINT"...";
40 GOSUB920:PRINT".";:T1=T1+1:IFT1>20THENPRINT"fail.":CLOSE5:END
50 PRINT#5,"at":TT=TI+100:GOSUB900
60 GET#5,A$:GET#5,B$:A$=A$+B$:IFLEFT$(A$,2)<>"ok"ANDLEFT$(A$,2)<>"OK"THEN40
80 GOSUB920
90 PRINT#5,"ats43=2400 +comet64":TT=TI+100:GOSUB900:GOSUB920
100 GET#5,A$:IFA$="E"orA$="e"THENprint"fail.":CLOSE5:END
130 PRINT"you can now load from device#2."
210 PRINT"* reset modem to use other wifi programs."
220 CLOSE5:SYS49152
230 END
900 IFTI<TTTHEN900
910 RETURN
920 GET#5,A$:IFA$<>""THEN920
930 RETURN
