!--------------------------------------------------
!- Wednesday, June 14, 2017 1:32:23 AM
!- Import of : 
!- z:\unsorted\64net\irc64-128.prg
!- Commodore 64
!--------------------------------------------------
1 REM IRC64/128  1200B 1.8+
2 REM UPDATED 06/13/2017 02:58P
10 POKE254,PEEK(186):IFPEEK(254)<8THENPOKE254,8
12 IFPEEK(65532)=61THENPOKE58,254:CLR
15 OPEN5,2,0,CHR$(8):DIMPP$(25):P$="ok":POKE186,PEEK(254)
20 CR$=CHR$(13):PRINTCHR$(14);:SY=PEEK(65532):POKE53280,254:POKE53281,246
30 PRINT"{light blue}":IFSY=226THENML=49152:POKE665,73:POKE666,3
40 IFSY=226ANDPEEK(ML+1)<>209THENCLOSE5:LOAD"pml64.bin",PEEK(186),1:RUN
50 IFSY=61THENML=4864:POKE981,15:P=PEEK(215)AND128:IFP=128THENSYS30643
60 IFSY=61ANDPEEK(ML)<>76THENCLOSE5:LOAD"pml128.bin",PEEK(254),1:RUN
100 REM GET THE BAUD RATE
110 P$="a"
120 PRINT"{clear}{down*2}IRC CHAT v1.0":PRINT"Requires 64Net WiFi firmware 1.8+"
130 PRINT"1200 baud version"
140 PRINT"By Bo Zimmerman (bo@zimmers.net)":PRINT:PRINT
197 REM --------------------------------
198 REM GET STARTED                    !
199 REM -------------------------------
200 UN=PEEK(254):IP$="":CR$=CHR$(13)+CHR$(10)
201 PH=0:PT=0:MV=ML+18
202 PRINT "Initializing modem..."
203 GET#5,A$:IFA$<>""THEN203
205 PRINT#5,CHR$(19);CR$;"athz0f0e0";CR$;:GOSUB900:IFP$<>"ok"THEN203
208 GET#5,A$:IFA$<>""THEN208
210 PRINT#5,CR$;"ate0n0r0v1q0";CR$;
220 GOSUB900:IFP$<>"ok"THEN208
230 PRINT#5,"ate0v1x1f3q0s40=250";CR$;CHR$(19);
240 GOSUB900:IFP$<>"ok"THENPRINT"Zimodem init failed: ";R$:STOP
300 REM GET THE SERVER
310 PRINT:PRINT"{down}Some servers:"
320 PRINT"irc.nlnog.net port 6667, #c-64"
325 PRINT"irc.freenode.net port 6667, #c64"
330 PRINT"irc.us.ircnet.net port 6667, #c-64"
350 SE$="":PRINT:PRINT"What is your IRC server host":GOSUB5000:SE$=P$
360 IFSE$=""THENPRINT"I guess you're done then":CLOSE5:STOP
370 PRINT"What is the port":GOSUB5000:PO$=P$:IFVAL(PO$)<=0THENSE$="":GOTO360
380 SE$=SE$+":"+PO$
390 GET#5,A$:IFA$<>""THEN390
400 REM MAKE THE CONNECTION
410 QU$=CHR$(34):PRINT#5,"ath&d13&m13&m10cp";QU$;SE$;QU$
420 GOSUB900
430 IFLEFT$(P$,8)<>"connect "THENPRINT"Unable to connect to ";SE$;":";P$:GOTO300
435 P=VAL(MID$(P$,8))
440 PRINT:PRINT"What is your nickname";:GOSUB5000:NI$=P$:N1=LEN(NI$)
450 IFNI$=""THENPRINT"I guess not.":STOP
460 P$="NICK "+NI$:GOSUB600:IFE$<>"ok"THENSTOP
470 P$="USER guest 0 * :Joe Anonymous":GOSUB600:IFE$<>"ok"THENSTOP
490 PRINT"{light green}{reverse on}Connected, wait for MOTD. ? for help{reverse off}{light blue}"
500 GOTO 1000: REM GO START MAIN LP
597 REM --------------------------------
598 REM TRANSMIT P$ TO THE OPEN SOCKET !
599 REM -------------------------------
600 IFLEN(P$)>0ANDASC(RIGHT$(P$,1))=10THENP$=LEFT$(P$,LEN(P$)-1)
605 OP$=P$:SYSML+9:C8$=MID$(STR$(PEEK(MV+8)),2)
610 PRINT#5,"ats42=";C8$;"t";QU$;P$;QU$
620 E$="ok":SYSML:IFP$<>"ok"THENP$=OP$:PRINT"xmit fail";CC$:GOTO600
630 RETURN
697 REM --------------------------------
698 REM GET NEXT FROM SOCKET INTO PP   !
699 REM -------------------------------
700 GOSUB930:IFP0<>PANDP0>0THENPRINT"Unexpected packet id: ";P0;"/";P:STOP
710 IFP0=0THENRETURN
720 PP$(PE)=P$:PE=PE+1:IFPE>=25THENPE=0
790 P$="":RETURN
797 REM --------------------------------
798 REM GET P$ FROM SOCKET P           !
799 REM -------------------------------
800 E=0:P$="":IFPH>=25THENPH=0
810 IFPH<>PETHENP$=PP$(PH):PH=PH+1:RETURN
820 GOSUB700:IFP0=0THENE=1:RETURN:REM FAIL
830 IFPH<>PETHENP$=PP$(PH):PH=PH+1:RETURN
840 E=1:RETURN
897 REM --------------------------------
898 REM GET E$ FROM MODEM, OR ERROR    !
899 REM -------------------------------
900 E$=""
910 SYSML
920 IFE$<>""ANDP$<>E$THENPRINT"{reverse on}{red}Comm error. Expected ";E$;", Got ";P$;"{light blue}{reverse off}"
925 RETURN
927 REM --------------------------------
928 REM GET PACKET HEADER, SETS P0,P1,P2, RETURNS P0=0 IF NADA
929 REM -------------------------------
930 SYSML+12:E=0:P0=0:P1=0:P2=0:PR=0:I=0:I0=0:I1=0:CR=0:P4=0:P5=0:C8=0
940 P$="":PRINT#5,CHR$(17);
945 SYSML+6:P0=PEEK(MV+2):P1=PEEK(MV+4):P2=PEEK(MV+6)
950 PL=PEEK(MV+0):CR=PEEK(MV+1):C8=PEEK(MV+8)
960 IFP0>0ANDP2<>C8THENPRINT#5,"atl0":GOTO945
970 IFP1=0THENP$=""
980 IFCR=255THENE=1:P$="":P0=0:P1=0:P2=0:PL=0:RETURN
990 RETURN
995 PRINT"Expected ";E$;", got ";A$:STOP
997 REM --------------------------------
998 REM THE MAIN LOOP                  !
999 REM -------------------------------
1000 GETA$:IFA$<>""THEN3000
1020 GOSUB800:IFP$=""THEN1000
1030 MS$="":MC$="":MA$="":I=1:LP$=P$
1040 IFMID$(P$,1,1)<>":"THEN1100
1050 IFI>LEN(P$)THEN1100
1060 IFMID$(P$,I,1)<>" "THENI=I+1:GOTO1050
1070 MS$=MID$(P$,1,I-1):P$=MID$(P$,I+1)
1100 I=1:IFP$=""THEN1000
1110 IFI>LEN(P$)THEN1200
1120 IFMID$(P$,I,1)<>" "THENI=I+1:GOTO1110
1130 MC$=MID$(P$,1,I-1):P$=MID$(P$,I+1)
1200 MA$=P$:IFMC$=""THEN1000
1210 A=ASC(MID$(MC$,1,I)):IFA<48ORA>57THEN1300
1220 IFLEN(MC$)>1ANDMID$(MC$,1,1)="0"THENMC$=MID$(MC$,2):GOTO1220
1230 C=VAL(MC$):IFC>430ANDC<460THENPRINT"Bad Nick given(";C;") : "+MA$:GOTO440
1240 IFC>400THENPRINT"Error: "+MA$:GOTO1000
1250 IFLEN(MA$)>N1+1ANDMID$(MA$,1,N1)=NI$THENMA$=MID$(MA$,LEN(NI$)+2)
1260 PRINT MA$:GOTO1000
1300 REM CMDS
1310 IFMC$="PING"THEN1400
1320 IFMC$="MODE"THEN1000
1330 IFMC$="PRIVMSG"THEN2000
1340 IFMC$="VERSION"THEN2100
1350 IFMC$="QUIT"THEN2200
1360 IFMC$="JOIN"THEN2300
1370 IFMC$="PONG"THEN1000
1390 REM
1397 PRINTLP$:GOTO1000
1398 PRINT"Unknown: '"+MC$+"' '"+MA$+"'"
1399 GOTO1000
1400 REM ...
1410 OM$=MA$:P$="PONG :"+MA$:GOSUB600:E$="":GOTO1000
2000 I=1:IFMA$=""THEN1000
2010 IFMID$(MA$,I,2)=" :"THEN2050
2020 I=I+1:GOTO2010
2050 GOSUB2900
2090 PRINT"{white}{reverse on}";MS$;"{reverse off}{light blue}: ";MID$(MA$,I+2):GOTO1000
2100 PRINT"{reverse on}";LP$;"{reverse off}":GOTO1000
2200 GOSUB2900:PRINT"{pink}{reverse on}";MS$;" has left the channel.{reverse off}{light blue}":GOTO1000
2300 GOSUB2900:PRINT"{light green}{reverse on}";MS$;" has joined the channel.{reverse off}{light blue}":GOTO1000
2900 II=2
2910 IFII>LEN(MS$)THENII=LEN(MS$):GOTO2950
2920 IFMID$(MS$,II,1)="!"THENII=II-1:GOTO2950
2930 II=II+1:GOTO2910
2950 MS$=MID$(MS$,1,II):RETURN
3000 IFA$=CHR$(13)THEN1000
3010 PRINT"{reverse on}{pink}Stream paused. Enter ? for help.{reverse off}"
3020 IFOM$<>""THENP$="PING "+OM$:GOSUB600
3090 PRINT"{light blue}>{reverse on} {reverse off}{left}";:IN$="":IT=TI+1000:GOTO3200
3100 IFTI>ITTHENPRINT"{reverse on}{red}Cancelled{reverse off}{pink}":PRINT:GOTO1000
3150 GETA$:IFA$=""THEN3100
3170 IT=TI+1000
3200 IFA$=CHR$(13)THEN3300
3210 IFA$<>CHR$(20)ORLEN(IN$)=0THEN3230
3220 IN$=LEFT$(IN$,LEN(IN$)-1):PRINT" {left*2} {left}{reverse on} {reverse off}{left}";:GOTO3100
3230 A=ASC(A$):IFA<32OR(A>95ANDA<193)ORA>218THEN3100
3240 IN$=IN$+A$:PRINTA$;"{reverse on} {reverse off}{left}";:GOTO3100
3300 IFIN$=""THENPRINT" ":PRINT:GOTO1000
3305 IFIN$="?"THENPRINT" ":GOTO3400
3310 IFMID$(IN$,1,1)="/"THENIN$=MID$(IN$,2):GOTO3500
3320 IFCC$=""THENPRINT:PRINT"{reverse on}{red}No Channel Selected! Try ?{reverse off}{light blue}":GOTO1000
3330 P$="PRIVMSG "+CC$+" :"+IN$
3340 GOSUB600:PRINT" ":PRINT"{white}{reverse on}";NI$;"{reverse off}{light blue}: ";IN$
3350 E$="":PRINT:GOTO1000
3400 PRINT"{light blue}{reverse off}------------------------------"
3410 PRINT"{reverse on}{purple}/join #c-64{reverse off} {light blue}Change channels."
3420 PRINT"{reverse on}{purple}/quit{reverse off} {light blue}Logout and exit."
3430 REMPRINT"{reverse on}{purple}/list{reverse off} {light blue}List channels."
3440 REMPRINT"{reverse on}{purple}/who mask*{reverse off} {light blue}User info."
3480 PRINT"{reverse on}{purple}Anything else{reverse off} {light blue}Send message"
3490 PRINT"{light blue}{reverse off}------------------------------":GOTO1000
3500 X=0:FORI=2TOLEN(IN$):IFX=0ANDMID$(IN$,I,1)=" "THENX=I
3510 NEXT:A$="":IFX>1THENA$=MID$(IN$,X+1):IN$=MID$(IN$,1,X-1)
3520 PRINT
3530 IFIN$="join"THEN4000
3540 IFIN$="quit"THENP$="QUIT :":GOSUB600:CLOSE5:END
3550 IFIN$="list"THENP$="LIST":GOSUB600:GOTO1000
3560 IFIN$="who"THENP$="WHO :"+AA$:GOSUB600:GOTO1000
3999 PRINT"{reverse on}{red}Unknown Command: ";IN$;". Try ?{reverse off}{light blue}":GOTO1000
4000 PRINT"Joining ";QU$;A$;QU$:AA$=A$
4100 P$="JOIN :"+AA$:GOSUB600
4110 IFP$<>E$THEN4100
4120 E$="":CC$=AA$:GOTO1000
5000 PRINT"? {reverse on} {reverse off}{left}";:P$=""
5010 GETA$:IFA$=""THEN5010
5020 IFA$=CHR$(13)THENPRINT" {left}":RETURN
5030 IFA$<>CHR$(20)THENPRINTA$;"{reverse on} {reverse off}{left}";:P$=P$+A$:GOTO5010
5040 IFP$=""THEN5010
5050 P$=LEFT$(P$,LEN(P$)-1):PRINT" {left*2} {left}{reverse on} {reverse off}{left}";:GOTO5010
50000 OPEN5,2,0,CHR$(8)
50010 GET#5,A$:IFA$<>""THENPRINTA$;
50020 GETA$:IFA$<>""THENPRINT#5,A$;
50030 GOTO 50010
55555 F$="irc64-128":OPEN1,8,15,"s0:"+F$:CLOSE1:SAVEF$,8:VERIFYF$,8
