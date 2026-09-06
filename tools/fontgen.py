#!/usr/bin/env python3
"""Regenerate UN_Orion bitmap headers from embedded Traf Typeface v2.1 raster data.

The embedded data was rasterized from TrafTypeface-Regular.ttf Version 2.100.
Source TTF SHA-256: 7320fdf88753792398172fbbc3432ef1da46d75f0bdcbc982130b162468ace5f
"""
import base64, zlib, sys
from pathlib import Path
FONT_SHA256 = "7320fdf88753792398172fbbc3432ef1da46d75f0bdcbc982130b162468ace5f"
UI = """c-p;Lv2No=5Pj4VTv}R^I=flN3o}JNfTfB9&PB?61OueJLZwS~`GpigzTrM${~*7>54b`=fIx*|XJ#o%q%7yOOZgVBphk<)>~LoG%_xZzb(JmRBuVG-JYHn!Vv#9cah%T6t1Qo#MW#OF@uFB<7dP2Wo)yLNI?rfly0}X6Ol8?TQ?XpcOM1+%=2t3PDm72z^a|g7E#E-5s=;%>ngiFsJ@CUtZJyPPq(-?x@_^VfK5d{ZRD-k$RSQU<XmD-WY_hokM1z^d?16=4!y9Hn%5#mo3Lk#NU5(rV7Ey!f5#kplKO^}JwF)#_Hpnbe2Qoef$|Y{UKyRR16dTCpWy0fPuMbE71!#xYu}8ube2|&Hwc2S;9A;xAgWp{SnN3kqT8@Fz1<CBPQQf1oBeUOc>E6{BdAVi#J?d3wb9gjUMXMdU1Z*HC2L>(CLT-?+U<|Z61_|ja+93R0RJe*1odNH<LFJ-_uANVMw?vjJo^;t~zy+2NsCnIMUh~2E?7f7(q7Agody$3>BElDl3bH~{K~*pn$|n?0{V!Jjer`E{bsu1qgXQoD!8TiVDTkShG^*94Bwxd<p!E*)%vHJ#$vA#H&cpor+T)+w$9C)+&z9{`>jUT`S#H+Q)egovL^R%->9F*^hQ}v|P<BLP`=_&-|Cd})v%Z9WI&$KYuDinM-9^o1%5rJnr9^jjN*t=Fp5_F~4r5(Ks&|nf1~;IXa2rzbNmfWx+<wK)Z@B)6aq}6sTX%#Yw+PDFE%J{rROEB$<bd*-F_aKIY7tf4E%fjzF)TR<a|gpk&#yi|LA*fz-sBM<+~830wEIXx`bx@%wna~#VMgYC>|G(D#Q@R?bKbxQ@o{+39eiwRcQG9cx4hY}?7vceQnNSfctrFJNv;-SKbH+7YD`40ppnA)`Pnn?l4G2VH{Yn!p72=p8ECPxT&W7rqSqyYUhxHe+v3i(X{Dp+x0X8~lJmEQcLq?+Z-Szw@X0~>cT>FB^2M6RR#){2V1)BE6EBj)g6?4wn4<(dg`m1&6|6>kWF!gcE80NY;8QoGYf;VWxZIJ1GqIfzbz_^&*k%*AO4?kK%~rfvJUw{V=b_Cm0=vq#3%T`SgSR)T!+Gx=HKn@-tDPt%%Jk?MqYoD?Ul_1wV?sH44Z0$8!`G$o$rP@Wf&e9uOC$+H<`Bx^Pegw>Y`H0bm*$gs_X?Dul&X}lUgfAeEb$F$PsEyOZPbyOmOpaOvVt-@+TfvCkFclB!cOf6Ic}faEw$>qiFVfs4x2&l2fHTAi+{d7V?06`Gz!>Hd&HhIcL7`nc7mPF(HRi_Hg}7x&A@}orN#GL<%FNx^TXtQY}?Uk%8LP)rsw<~@)_da@VooYwaNG6;mmF(vkjTa9FKjf5s{{6k`w{c9(92D%2zoz7q4tCHkYuOyrKIJV(V-U(#+c0T>_SFgp%M*OtHCmHC|(Hd}J??uR4;=M>yk!j$*^jd4^++BLoj8FUg<#;x>H#CJIL$v`?lmX=#}@-*psv$f9dE4SNUvJZYj!l<6-NFEIZ!`q$G4*KChRUQ=S@o8w}0b^v=1q75fI-}0Mqx4&gGF&Ux+^6RL4L3#Sw9?v2!6B~`w{J_e2kZ#?Kv4*PMEZ09Y_NA<Jz3Ry-M|@07xo6)9HWNHE-)nYZpfvy0n7he&QbW{^mpXzPuKA7BqpYOdqznyS)C#G!v(xT~E{~=yOGs|2_wh4PkuN*;7~uBq9z0NJ4E|4Py=SnZAK>qydL}j$CW8yYYm|Ks!Y0G<81mFXreybi-9PX>e*XFoJ808?"""
DISPLAY = """c-rNfL2l$W5JgGIh_VWI3C3eL2T3reXtb^}$IwEsILN}w+#o(m7~}vsLX=gu+JylGF|1#d$mzBR?zRV@5W#q`yQN~0`YaZUzPWj)-@N($_UmtV-~F(A|Mvac>)YFRKi*tli`TtsOVp0o&qVJ7r1wUM%IWZRHPB79t%g7&1C^0upb+QL`^R~%os8vQ(ocaEMzzMM2GV#M1*TDbjQnkdJMk743n$j<?0$MXwGmHHX2er$A+ub;=Pg|1b8woJpx|`lNyNROZp=alPU4&J7DjOu`US<%7zNrS|Jpp8*;CODC|QeWE416JKCnOR8~Z7}-NMLLet^pkoRi*6!|FZNo6FdO9k_os+l=m5vD_B7+h>Yp-=@{)4O~5W9%F3Av(?C4-cH8(5<8i~bFbJ>_IV2-A+BFXW1`@_5*HEbQgJG$34NipCs_8?@M1QulTBkF8VqhP8bi@9bVNGw)e;Z$NLxLr?K%jHXpcyb1T|Lc&yX1GlR*c$&VrWG-u7nkN5v_Dv}PMY;VAmOl%D$j2w%4qu>4DHmRq>mVo1~d3RDO{`koU*&SWgsH3Xe`8zUPsAI#Y*<?3<Qpg9x6J$E1y%#X;{enODGN3VaTUe^ztCd1~Okw5v<q&xX3w!qfda%_cL<knTb?--M}V%kW&EtkA*Vi99XFz(PxzV?EA_xBU|{Kt$OGN`Qn$Nw5u`QN6{L1m_}YAexJuTSwpmMvLKER9Lq#GIU5o@q}c9nHaNq`(()B!}*_$mh@9g?%9wzHJyV+1#2vYGjfPvdTMTm=jIR5aYr?%K<6qKx+=krUg69-~BnnQ_%vf1+(;nU%18CI(wqC{Fc2Hed+nFhJ!sP<I@_=s<Fo&0jr5sX036>de`U4cj74v3u7Zbtp>!}#l!HzXk2(H!DhOhl5P#_R_D4MqPqB(w1H~T)12KCT<@{s12jLv?7uMUq1$Gl7O7osJ?M6TS8Kty@+0LJ-hL=NB#c@8HmkAN#;n?lI;@|e)l9LbF-NtUtU$PAU!cizuns(M4wmr|PiRFD!fYWXd@{m_3ANp!UstJabHZ3F<4kGd;spMPK@lG*&X@XpU-g-}UgKM3_3FuzeND---bRg4VZVP$TDl77|EEfG87*q#Rv^=pE2YD1m?`MSYHfpDQS)Zcm0zV%n4e6I{_fDz!@>|x_h5%Cw}oU{)ElXL3n_Funb<;DU6r^;vVW+VG*&xMD<&LbK4A2IIo>?Spu?j&7C*N8;A2q?FGiWWz}nu0S5p7w?+5G_ZHTvjlYIT}$YkVHyl%(!Oq@^2EzLweH<tq`N{*DH3a3%5q7__pR8bL!4l~pxsxd8WIr(!vraKQ)ve3b%LzNz_$dTHu24*_JSE$wGC-`NLEzQ$&NIg|cF0}3}!Y+fRtm{)Ph1c{xThqTpDW~A8%Kv`=44(|?+$KNk=B!DllOO^0if*mDE=NAOdM|nOwC&ZyVewG<*FV$w4|m$m)B"""

def write_ui(out, raw):
    adv=raw[:95]; data=raw[95:]
    p=out/'traf_font_22.h'
    with p.open('w') as f:
        f.write('/* Generated from Traf Typeface v2.1 / Version 2.100. */\n#ifndef ORION_TRAF_FONT_22_H\n#define ORION_TRAF_FONT_22_H\n#include <stdint.h>\n#define TRAF_FIRST 32\n#define TRAF_LAST 126\n#define TRAF_W 24\n#define TRAF_H 30\n')
        f.write('static const uint8_t traf_advance[95]={'+','.join(map(str,adv))+'};\n')
        f.write('static const uint32_t traf_rows[95][TRAF_H]={\n')
        off=0
        for _ in range(95):
            vals=[]
            for _ in range(30): vals.append(int.from_bytes(data[off:off+3],'big')); off+=3
            f.write('{'+','.join(f'0x{v:06X}u' for v in vals)+'},\n')
        f.write('};\n#endif\n')

def write_display(out, raw):
    adv=raw[:26]; data=raw[26:]
    p=out/'traf_display_44.h'
    with p.open('w') as f:
        f.write('/* Generated from Traf Typeface v2.1 / Version 2.100. */\n#ifndef ORION_TRAF_DISPLAY_44_H\n#define ORION_TRAF_DISPLAY_44_H\n#include <stdint.h>\n#define TRAF_D_W 48\n#define TRAF_D_H 58\n')
        f.write('static const uint8_t traf_d_advance[26]={'+','.join(map(str,adv))+'};\n')
        f.write('static const uint64_t traf_d_rows[26][TRAF_D_H]={\n')
        off=0
        for _ in range(26):
            vals=[]
            for _ in range(58): vals.append(int.from_bytes(data[off:off+6],'big')); off+=6
            f.write('{'+','.join(f'0x{v:012X}ULL' for v in vals)+'},\n')
        f.write('};\n#endif\n')

def main():
    out=Path(sys.argv[1] if len(sys.argv)>1 else 'build/generated'); out.mkdir(parents=True,exist_ok=True)
    write_ui(out,zlib.decompress(base64.b85decode(UI.encode())))
    write_display(out,zlib.decompress(base64.b85decode(DISPLAY.encode())))
    print(f'Generated Traf v2.1 kernel glyph headers in {out}')
if __name__=='__main__': main()
