all: html ps dvi info

html: bsp.html bspint.html

ps: bsp.ps bspint.ps

dvi: bsp.dvi bspint.dvi

info: bsp.info bspint.info

%.info: %.texinfo
	@rm -f $@ $@-[0-9] $@-[0-9][0-9]
	makeinfo $<

%.dvi: %.texinfo
	-rm -f $*.ps
	-tex $<
	-texindex $*.??
	-tex $<
	-rm -f $*.ps $*.aux $*.cp* $*.fn* $*.ky* \
		$*.log $*.pg* $*.toc $*.tp* $*.vr*

%.ps: %.dvi
	dvips $< -o $@


%.html: %.texinfo
	texi2html $<

clean:
	-rm -f *.aux *.cp* *.fn* *.ky* \
		*.log *.pg* *.toc *.tp* *.vr* *.dvi *.ps
	-rm -f *.html *.info*
