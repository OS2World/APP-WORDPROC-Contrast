
ALL:CONTRAST.EXE CONTRAST.HLP

CONTRAST.HLP:CONTRAST.IPF
        IPFC CONTRAST.IPF

# options: /Alfw recommended model for multi-threaded app
#          /c  compile only
#          /Fs produce source listing file
#          /G2 produce 286 code
#          /Gc function-calling sequence
#          /Gs removes stack probes
#          /Gw PM application
#          /Od disable optimisation
#          /Os optimisation for code size - does best at speed as well
#          /W3 warning level 3
#          /Zi prepare for CodeView
CONTRAST.OBJ: CONTRAST.C CONTRAST.H
        CL /Alfw /c /Fs /G2 /Gc /Gs /Gw /Os /W3 CONTRAST.C

CONTRAST.RES: CONTRAST.RC CONTRAST.ICO CONTRAST.H
        RC -r CONTRAST.RC

# options: /A:16 align on 16 byte boundaries
#          /CO link for CodeView
#          /NOD don't use default libraries
CONTRAST.EXE: CONTRAST.OBJ CONTRAST.DEF CONTRAST.RES
        LINK CONTRAST.OBJ /A:16 ,,, /NOD LLIBCMT.LIB OS2.LIB, CONTRAST.DEF
        RC CONTRAST.RES
