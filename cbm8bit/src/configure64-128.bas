!--------------------------------------------------
!- Sunday, May 14, 2017 10:00:50 PM
!- Import of : 
!- c:\src\zimodem\cbm8bit\configure64-128.prg
!- Commodore 64
!--------------------------------------------------
1 REM CONFIGURE64/128  1200B 1.8+
2 REM UPDATED 03/07/2017 07:54P
10 IFPEEK(65532)=61THENPOKE58,254:CLR
15 OPEN5,2,0,CHR$(8):DIMWF$(100):WF=0:P$="ok"
20 CR$=CHR$(13):PRINTCHR$(14);:SY=PEEK(65532):POKE53280,254:POKE53281,246
30 PRINT"{light blue}":IFSY=226THENML=49152:POKE665,73:POKE666,3
40 IFSY=226ANDPEEK(ML)<>76THENCLOSE5:LOAD"pml64.bin",PEEK(186),1:RUN
50 IFSY=61THENML=4864:POKE981,15:P=PEEK(215)AND128:IFP=128THENSYS30643
60 IFSY=61ANDPEEK(ML)<>76THENCLOSE5:LOAD"pml128.bin",PEEK(186),1:RUN
100 REM
110 P$="a"
120 PRINT"{clear}{down*2}CONFIGURE v1.0":PRINT"Requires 64Net WiFi Firmware 1.5+"
130 PRINT"1200 baud version"
140 PRINT"By Bo Zimmerman (bo@zimmers.net)":PRINT:PRINT
197 REM --------------------------------
198 REM GET STARTED                    !
199 REM -------------------------------
200 PH=0:PT=0:MV=ML+18:CR$=CHR$(13)+CHR$(10):QU$=CHR$(34):POKEMV+14,5
202 PRINT "Initializing modem...";:CT=0
203 GET#5,A$:IFA$<>""THEN203
205 PRINT#5,CR$;CR$;"athz0f3e0";CR$;
206 GOSUB900:IFP$<>"ok"THENCT=CT+1:IFCT<10THEN203
207 IFCT<10THENGET#5,A$:IFA$<>""THEN207
208 IFCT<10THENPRINT#5,"athf3e0";CR$;
209 IFCT<10THENGOSUB900:IFP$<>"ok"THENCT=CT+1:GOTO207
210 IFCT=10THENPRINT"Initialization failed.":PRINT"Is your modem set to 1200b?"
220 IFCT=10THENSTOP
300 PRINT:PRINT:GOTO 1000
897 REM --------------------------------
898 REM GET E$ FROM MODEM, OR ERROR    !
899 REM -------------------------------
900 E$="":P$=""
910 SYSML
920 IFE$<>""ANDP$<>E$THENPRINT"{reverse on}{red}Comm error. Expected ";E$;", Got ";P$;"{light blue}{reverse off}"
925 RETURN
997 REM --------------------------------
998 REM THE MAIN LOOP                  !
999 REM -------------------------------
1000 CT=0:CD=0:LC$=""
1010 SYSML+12:PRINT#5,CR$;"ati3";CR$;
1020 GOSUB900:IFP$=""ORP$="ok"ORP$<>LC$THENLC$=P$:CT=CT+1:IFCT<15THEN1010
1030 WI$=P$:PRINT"Current WiFi SSI: ";WI$
1040 CD=0:IFWI$=""THEN2000
1045 GOSUB 1050:GOTO2000
1050 PRINT:PRINT"Testing connection...";
1060 CT=0
1070 SYSML+12:PRINT#5,CR$;"atc";QU$;"www.yahoo.com:80";QU$;CR$;
1080 GOSUB900:CD=1:IFP$="ok"THEN1080
1085 IFLEFT$(P$,8)<>"connect "THENCD=0:GOTO1200
1090 FORI=1TO3:SYSML+12:PRINT#5,CR$;"ath0";CR$;:GOSUB900:NEXTI
1100 PRINT#5,CR$;"ath0";CR$;:GOSUB900
1200 IFCD=0THENPRINT"{reverse on}{red}Fail!{reverse off}{light blue}"
1210 IFCD=1THENPRINT"{reverse on}{light green}Success!{reverse off}{light blue}"
1220 RETURN
2000 SYSML+12:SYSML+12:PRINT:PRINT"Scanning for WiFi hotspots...";
2010 PRINT#5,CR$;"atw";CR$;
2020 GOSUB900:I=1:IFP$=""ORP$="ok"THENPRINT"Done!":PRINT:PRINT:GOTO3000
2030 IFI>=LEN(P$)THEN2100
2040 IFMID$(P$,I,1)=" "THENP$=LEFT$(P$,I-1):GOTO2100
2050 I=I+1:GOTO2030
2100 WF$(WF+1)=P$:WF=WF+1:GOTO2020
3000 PG=1
3100 LP=WF:IFLP>PG+15THENLP=PG+15
3110 FORI=PGTOLP:PRINTSTR$(I)+") ";WF$(I):NEXTI
3120 PRINT:PRINT"Enter X to Quit."
3130 IFLP<WFTHENPRINT"Enter N for Next Page"
3140 IFPG>15THENPRINT"Enter P for Prev Page"
3150 PRINT"Enter a number to Connect to that SSI"
3200 PRINT"? ";:GOSUB5000:A$=P$:IFA$=""THEN3100
3210 IFA$="x"ORA$="X"THENCLOSE5:END
3220 IFA$="n"ORA$="N"THENIFLP<WFTHENPG=PG+15:GOTO3100
3230 IFA$="p"ORA$="P"THENIFPG>15THENPG=PG-15:GOTO3100
3240 A=ASC(MID$(A$,1,1)):IFA<48ORA>57THEN3100
3250 A=VAL(A$):IFA<PG OR A>PG+15 OR A>WFTHEN3100
3260 WI=A:PRINT"Attempt to connect to: ";WF$(A)
3300 PRINT"Enter WiFi Password: ";:GOSUB5000:PA$=P$
3400 SYSML+12:PRINT#5,CR$;"atw";QU$;WF$(WI);",";PA$;QU$;CR$;:GOSUB900
3410 IFP$="error"THENPRINT"{reverse on}{red}Connect Fail!{reverse off}{light blue}":GOTO3100
3415 IFP$="ok"THEN3420
3416 IFP$<>""THEN3400
3417 GOSUB900:GOTO3410
3420 PRINT"{reverse on}{light green}Connect success!{reverse off}{light blue}"
3430 PRINT
3431 TT=TI+300
3432 IFTI<TTTHEN3432
3440 CD=0:GOSUB1050:IFCD=0THEN3100
3450 PRINT"{reverse off}{white}Saving new options..."
3460 SYSML+12:SYSML+12:SYSML+12
3470 PRINT#5,CR$;"atz0&we0";CR$
3480 GOSUB900:IFP$<>"ok"THEN3470
3490 P$="":SYSML+12:IFP$<>""THEN3490
3500 SYSML+12:CLOSE5:RUN
5000 P$=""
5005 PRINT"{reverse on} {reverse off}{left}";
5010 GETA$:IFA$=""THEN5010
5020 IFA$=CHR$(13)THENPRINT" {left}":RETURN
5030 IFA$<>CHR$(20)THENPRINTA$;"{reverse on} {reverse off}{left}";:P$=P$+A$:GOTO5010
5040 IFP$=""THEN5010
5050 P$=LEFT$(P$,LEN(P$)-1):PRINT" {left*2} {left}{reverse on} {reverse off}{left}";:GOTO5010
49999 STOP
50000 OPEN5,2,0,CHR$(8)
50010 GET#5,A$:IFA$<>""THENPRINTA$;
50020 GETA$:IFA$<>""THENPRINT#5,A$;
50030 GOTO 50010
55555 F$="configure64-128":OPEN1,8,15,"s0:"+F$:CLOSE1:SAVE(F$),8:VERIFY(F$),8
