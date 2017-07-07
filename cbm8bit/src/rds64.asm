* = 49152
        ; remote desktop server
        ; v1.0 02/23/2017
        ; for c64net/zimodem v2.7+
        ;
        ; .o
        jmp init; +0
        jmp resmio; +3
        jmp resmio; +6
        jmp resmio; +9
        jmp resmio; +12
        ; ; ; ; ; public regs ; ; ; ; ; ;
stat1s
        byte 0;+15
subst1s
        byte 0;+16
disable
        byte 0;+17
stopflg
        byte 0;+18
pausetm
        byte 0;+19
timeflg
        byte 0;+20
        ; idlhors .byte 0;+21
idlmins
        byte 4;+21
        ; tmohors .byte 0;+23
tmomins
        byte 85;+24
timsbyt
        byte 0,0
timswat
        byte 4
savabyt
        byte 0,0
buf12fl
        byte 0
lastdcd
        byte 0
idlchvt
        byte 0,0
rbuf = $ce00
tbuf = $cf00
        ; ; ; ; ; string table ; ; ; ; ; ;
eight
        byte 8,0
strinit1
        byte 13
        byte "athq0v1x1z0f0e0"
        byte 13,0
strinit2
        byte 13
        byte "ate0n0r0v1f0s0=1s41=1a6400"
        byte 13,0
strok
        byte "ok"
        byte 0
stripis
        byte "server ip addr "
        byte 0
strinit3
        byte 13
        byte "atv0i2"
        byte 13,0
strinit4
        byte " port 6400"
strinit5
        byte 13,0
strinit0
        byte "telnetd 1.0 init."
strinitx
        byte "."
        byte 0
stratha0
        byte "+++"
        byte 0
stratha1
        byte "ath"
        byte 13,0
        ; ; ; ; check methods ; ; ; ; ;
clrclk1
        lda $dc0f
        and #$7f
        sta $dc0f
        lda #$00
        sta $dc0b
        sta $dc0a
        sta $dc09
        sta $dc08
        rts
clrclk2
        lda $dd0f
        and #$7f
        sta $dd0f
        lda #$00
        sta $dd0b
        sta $dd0a
        sta $dd09
        sta $dd08
        rts
chkidl1
        lda $dc0b
chkidl10
        lda $dc0a
        pha
        lda $dc09
        lda $dc08
        pla
        cmp idlmins
        rts
chktmo2
        lda $dd0b
        lda $dd0a
        pha
        lda $dd09
        lda $dd08
        pla
        cmp tmomins
        rts
chkpas1
        lda $dc0b
        lda $dc0a
        lda $dc09
        pha
        lda $dc08
        pla
        cmp #$05
        rts
chkidle
        jmp (idlchvt)
        ; ; ; ; welcome message ; ; ; ; ;
welcome
        tya
        pha
        txa
        pha
        lda $029d
        sta $029e
        ldx #$7b
        ldy #$e4
        jsr outmxp
        lda #$0d
        ldx $029e
        sta tbuf,x
        inc $029e
        ldx #$78
        ldy #$a3
        jsr outmxp
        lda #$0d
        ldx $029e
        sta tbuf,x
        inc $029e
        jsr $f028
        pla
        tax
        pla
        tay
        rts
outmxp
        stx $fb
        sty $fc
        ldy #$00
outmxl
        lda ($fb),y
        beq outmxd
        ldx $029e
        sta tbuf,x
        inc $029e
        iny
        bne outmxl
outmxd
        rts
timein
        lda #<chkidl1
        sta idlchvt
        lda #>chkidl1
        sta idlchvt+1
        jsr clrclk1
        jsr clrclk2
        lda #$00
        sta disable
        sta timeflg
        rts
        ; ; ; ; timeout handlr ; ; ; ; ;
timeout
        tya
        pha
        txa
        pha
        lda timeflg
        bne timeou1
        jsr clrclk1
        jsr clrclk2
        lda #<chkpas1
        sta idlchvt
        lda #>chkpas1
        sta idlchvt+1
        inc timeflg
        inc disable
        jmp timeoudn
timeou1
        cmp #$01
        bne timeou2
        lda #$00
        sta disable
        ldx #<stratha0
        ldy #>stratha0
        jsr outmxp
        jsr $f028
        jsr clrclk1
        inc timeflg
        inc disable
        jmp timeoudn
timeou2
        cmp #$02
        bne timeou3
        ldx #<stratha1
        ldy #>stratha1
        jsr outmxp
        jsr $f028
timeou3
        jsr clrclk1
        lda #$00
        sta disable
        sta timeflg
timeoudn
        pla
        tax
        pla
        tay
        rts
        ; ; ; ; fill input buf ; ; ; ; ;
buffill
        jsr clrbuf1
        ldy #$00
        tya
        sta buf1dx
buffal0
        jsr retims
        lda #$01
        sta timswat
buffal1
        jsr chktims
        bne buffadn
buffal2
        lda $029b
        cmp $029c
        beq buffal1
        lda #$02
        jsr $f1b8
        ldy buf1dx
        sta buf1,y
        cmp #$00
        beq buffal1
        cmp #$0a
        beq buffal1
        inc buf1dx
        jmp buffal0
buffadn
        rts
buffiln
        lda #$02
        jsr $f04d
        jsr buffill
        lda buf1dx
        bne buffil0
buffidn
        lda #$00
        sta buf1dx
        rts
buffil0
        lda buf1,y
        cmp #$0d
        bne buffil3
        ldx #$00
        iny
buffil1
        lda buf1,x
        sta buf1,y
        cpy buf1dx
        beq buffil2
        inx
        iny
        bne buffil1
buffil2
        dec buf1dx
        beq buffidn
        ldy #$00
        beq buffil0
buffil3
        iny
        cpy buf1dx
        beq buffifn
        lda buf1,y
        cmp #$0d
        beq buffifn
        sec
        bcs buffil3
buffifn
        lda #$00
        sta buf1,y
        sty buf1dx
        rts
        ; ; ; ; modem read str ; ; ; ; ;
buflaln
        lda #$02
        jsr $f04d
        jsr buffill
bufladn
        ldy buf1dx
        beq buflabd
        dey
        lda buf1,y
        cmp #$0d
        beq buflad3
        dec buf1dx
        bne bufladn
buflad3
        sty buf1dx
        bne buflad4
buflabd
        ldy #$00
        sta buf1dx
        rts
buflad4
        dey
        lda buf1,y
        cmp #$0d
        bne buflad5
        iny
        bne buflafn
buflad5
        cpy #$00
        beq buflafn
        dey
        sec
        bcs buflad4
buflafn
        ldx #$00
buflaf1
        lda buf1,y
        sta buf1,x
        inx
        iny
        cpy buf1dx
        bne buflaf1
buflaot
        lda #$00
        sta buf1,x
        stx buf1dx
        rts
        ; ; ; ; buffer compare ; ; ; ; ;
bufcmp
        stx $fb
        sty $fc
        ldy #$00
bufcmp1
        lda ($fb),y
        beq bufcmpy
        cmp buf1,y
        bne bufcmpn
        iny
        bne bufcmp1
bufcmpn
        ldy #$ff
bufcmpy
        rts
        ; ; ; ; scn out string ; ; ; ; ;
outsstr
        stx $fb
        sty $fc
        jsr $ffcc
        jmp outmst0
clrbuf1
        ldx #$00
        txa
clrbufl
        sta buf1,x
        dex
        bne clrbufl
        rts
        ; ; ; ; one liners  ; ; ; ; ;
clrrecb
        lda $029c
        sta $029b
        rts
dcdchk
        lda $dd01
        and #$10
        rts
resmio
        jsr $f04f
        jmp $ffcc
        ; ; ; ; mdm out string  ; ; ; ; ;
outmstr
        stx $fb
        sty $fc
        lda #$02
        jsr $efe1
outmst0
        ldy #$00
outmst1
        lda ($fb),y
        beq outmst2
        jsr $ffd2
        iny
        bne outmst1
outmst2
        rts
        ; ; ; ; delay timers  ; ; ; ; ;
retims
        lda $a1
        sta timsbyt
        lda #$04
        sta timswat
        lda #$00
        sta timsbyt+1
        rts
chktims
        lda $a1
        cmp timsbyt
        beq timsgod
chktims2
        sta timsbyt
        inc timsbyt+1
        lda timsbyt+1
        cmp timswat
        bcs timsbad
timsgod
        ldx #$00
        rts
timsbad
        ldx #$ff
        rts
        ; ; ; ; ; init ; ; ; ; ; ;
init
        lda #$00
        sta stat1s
        ldx #<strinit0
        ldy #>strinit0
        jsr outsstr
        lda #$05
        ldx #$02
        ldy #$00
        jsr $ffba
        lda #$01
        ldx #<eight
        ldy #>eight
        jsr $ffbd
        jsr $f409; open=ffc0
        ; check for error
        ; if error, set stat1s, rts
        lda #<rbuf
        sta $f7
        lda #>rbuf
        sta $f8
        lda #<tbuf
        sta $f9
        lda #>tbuf
        sta $fa
        ; baud correction
        lda 678
        bne initb1
        lda #73
        sta 665
        bne initb2
initb1
        lda #43
        sta 665
initb2
        jsr clrrecb
        ldx #<strinitx
        ldy #>strinitx
        jsr outsstr
        ldx #<strinit1
        ldy #>strinit1
        jsr outmstr
        jsr buflaln
        lda buf1dx
        beq initb2
        ldx #<strok
        ldy #>strok
        jsr bufcmp
        bne initb2
        ldx #<strinitx
        ldy #>strinitx
        jsr outsstr
initb3
        jsr clrrecb
        ldx #<strinit2
        ldy #>strinit2
        jsr outmstr
        jsr buflaln
        lda buf1dx
        beq initb3
        ldx #<strok
        ldy #>strok
        jsr bufcmp
        bne initb3
        ldx #<strinitx
        ldy #>strinitx
        jsr outsstr
initb4
        jsr clrrecb
        ldx #<strinit3
        ldy #>strinit3
        jsr outmstr
        jsr buffiln
        lda buf1dx
        beq initb4
        ldx #<strinit5
        ldy #>strinit5
        jsr outsstr
        ldx #<stripis
        ldy #>stripis
        jsr outsstr
        ldx #<buf1
        ldy #>buf1
        jsr outsstr
        ldx #<strinit4
        ldy #>strinit4
        jsr outsstr
        ldy #$00
initl1
        lda $0314,y
        sta savvtr,y
        iny
        cpy #33
        bcc initl1
        sei
        lda #<intrpt
        sta $0314
        lda #>intrpt
        sta $0315
        lda #<brkout
        sta $0316
        lda #>brkout
        sta $0317
        lda #<openot
        sta $031a
        lda #>openot
        sta $031b
        lda #<closet
        sta $031c
        lda #>closet
        sta $031d
        lda #<chkin 
        sta $031e
        lda #<chkin 
        sta $031f
        lda #<chkout
        sta $0320
        lda #>chkout
        sta $0321
        lda #<clrchn
        sta $0322
        lda #>clrchn
        sta $0323
        lda #<chrin
        sta $0324
        lda #>chrin
        sta $0325
        lda #<chrout
        sta $0326
        lda #>chrout
        sta $0327
        ; lda #<getin:sta $032a
        ; lda #>getin:sta $032b
        lda #<stopt
        sta $0328
        lda #>stopt
        sta $0329
        lda #<clall
        sta $032c
        lda #>clall
        sta $032d
        lda #<lload
        sta $0330
        lda #>lload
        sta $0331
        lda #<ssave
        sta $0332
        lda #>ssave
        sta $0333
        ldy #$00
initl2
        lda $0308,y
        sta savbtr,y
        iny
        cpy #$02
        bcc initl2
        lda #<tokein
        sta $0308
        lda #>tokein
        sta $0309
        lda #$73
        sta tokein+1
        lda #$00
        sta tokein+2
        lda savbtr
        clc
        adc #$03
        sta tokenot+1
        lda savbtr+1
        adc #$00
        sta tokenot+2
        ; lda #$7f:sta $dd0d
        ; lda #$80:bit $dd0d
        ; lda #<nmirtn:sta $0318
        ; lda #>nmirtn:sta $0319
        cli
        jsr $f409
        jmp $ffcc
        ; ; ; ; ; tokein 0308 ; ; ; ; ; ;
tokein
        jsr $ffd2
        bne tokein1
tokenot
        jmp $ffd2
tokein1
        pha
        sbc #$80
        bcs tokein2
tokeno
        pla
        cmp #$00
        jmp tokenot
tokein2
        cmp #$1e
        beq tokeinb
        cmp #$17
        beq tokeinb
        jmp tokeno
tokeinb
        pla
        lda #$99
        cmp #$00
        jmp tokenot
        ; ; ; ; ; chrout ffd2 ; ; ; ; ; ;
chrout
        pha
        sta savabyt
        lda $9a
        cmp #$03
        bne chrono
        lda disable
        bne chrono
        jsr dcdchk
        beq chrono
        txa
        pha
        tya
        pha
        lda #<tbuf
        sta $f9
        lda #>tbuf
        sta $fa
        lda savabyt
        sta $9e
        jsr $f014
        pla
        tay
        pla
        tax
chrono
        pla
        jmp (savvtr+18)
        ; ; ; ; ; stopt ff30 ; ; ; ; ; ;
stopt
        lda stopflg
        bne stopone
stoptno
        jmp (savvtr+20)
stopone
        dec stopflg
        lda $91
        and #$7f
        sta savabyt
        cmp savabyt
        php
        jsr $ffcc
        sta $c6
        plp
        rts
        ; ; ; ; ; chrin ffcf ; ; ; ; ; ;
chrin
        lda $99
        bne chrino
        jsr dcdchk
        beq chrino
        ; txa:pha:tya:pha
        ; jsr $f04f
        ; pla:tay:pla:tax
        lda $029b
        cmp $029c
        beq chrino
        lda $c6
        cmp $0289
        bcs chrino
        txa
        pha
        tya
        pha
        sei
        ldx $029c
        lda rbuf,x
        inc $029c
        ldx $c6
        sta $0277,x
        inc $c6
        cli
        pla
        tay
        pla
        tax
chrino
        jmp (savvtr+16)
        ; ; ; ; ; interrupt ; ; ; ; ; ;
intrpt
        lda $dd01
        and #$10
        bne intrpt1
        lda lastdcd
        bne intrkll
        jmp (savvtr)
intrkll
        lda #$00
        sta lastdcd
        lda #$01
        sta stopflg
        lda #$0d
        sta $0277
        lda #$01
        sta $c6
        dec $d020; user is gone, were done
        jmp (savvtr)
intrpt1
        lda lastdcd
        bne intrpt2
        inc lastdcd
        inc $d020
        jsr timein
        lda #$7f
        sta pausetm
        jsr welcome
intrpt2
        jsr chkidle
        bcc intrpt3
intrtmo
        jsr timeout
        jmp intrno
intrpt3
        jsr chktmo2
        bcs intrtmo
        lda timeflg
        bne intrno
        lda pausetm
        beq intrpt9
        dec pausetm
        bne intrno
        lda $029b
        sta $029c
intrpt9
        lda $029b
        cmp $029c
        beq intrno
        lda $c6
        cmp $0289
        bcs intrno
        ldx $029c
        lda rbuf,x
        inc $029c
        ldx $c6
        sta $0277,x
        inc $c6
        cmp #$03
        bne intrptk
        lda #$01
        sta stopflg
intrptk
        jsr clrclk1
intrno
        jmp (savvtr)
        ; ; ; ; ; brkout ; ; ; ; ; ;
brkout
        nop
        jmp (savvtr+2)
        ; ; ; ; ; openot ; ; ; ; ; ;
openot
        nop
        inc disable
        jsr openo1
        pha
        txa
        pha
        tya
        pha
        lda #$00
        sta disable
        jsr $f04f
        pla
        tay
        pla
        tax
        pla
        rts
openo1
        jmp (savvtr+6)
        ; ; ; ; ; closet ; ; ; ; ; ;
closet
        nop
        inc disable
        jsr close1
        pha
        txa
        pha
        tya
        pha
        lda #$00
        sta disable
        jsr $f04f
        pla
        tay
        pla
        tax
        pla
        rts
close1
        jmp (savvtr+8)
        ; ; ; ; ; chkin ; ; ; ; ; ;
chkin
        nop
        jmp (savvtr+10)
        ; ; ; ; ; chkout ; ; ; ; ; ;
chkout
        nop
        jmp (savvtr+12)
        ; ; ; ; ; clrchn ; ; ; ; ; ;
clrchn
        nop
        jsr clrch2
        jmp $f04f
clrch2
        jmp (savvtr+14)
        ; ; ; ; ; getin ; ; ; ; ; ;
getin
        nop
        jmp (savvtr+22)
        ; ; ; ; ; clall ; ; ; ; ; ;
clall
        nop
        jsr clall1
        jmp $f04f
clall1
        jmp (savvtr+24)
        ; ; ; ; ; lload ; ; ; ; ; ;
lload
        inc disable
        jsr lload1
        lda #$00
        sta disable
        jmp $f04f
lload1
        jmp (savvtr+28)
        ; ; ; ; ; ssave ; ; ; ; ; ;
ssave
        inc disable
        jsr ssave1
        lda #$00
        sta disable
        jmp $f04f
ssave1
        jmp (savvtr+30)
nmirtn
        pha
        jmp $ffcc
savvtr
        byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
        byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
savbtr
        byte 0,0,0,0,0,0
buf1dx
        byte 0
buf1
        nop; a page of bytes 4 play
print
        nop; :f$="rds64":open1,8,15,"s0:"+f$+"*":save(f$+".bas"),8
