all: paper.pdf iwp9slides.pdf sc11.pdf 

standard.tex: standard.pic tpics.txt
	pic standard.pic > standard.troff
	groff standard.troff > standard.ps
	open standard.ps

pequalv.ps: pequalv.pic
	pic pequalv.pic > pequalv.troff
	groff pequalv.troff > pequalv.ps
	open pequalv.ps

standard.ps: standard.pic
	pic standard.pic > standard.troff
	groff standard.troff > standard.ps
	open standard.ps
paper.pdf: paper.dvi
	dvipdf paper
paper.dvi: paper.tex all.bib
	latex paper
	bibtex paper
	latex paper
sc11.pdf: sc11.dvi
	dvipdf sc11
sc11.dvi: sc11.tex all.bib standard.tex
	latex sc11
	bibtex sc11
	latex sc11

usenix10.pdf: usenix10.dvi
	dvipdf usenix10
usenix10.dvi: usenix10.tex all.bib
	latex usenix10
	bibtex usenix10
	latex usenix10

eurosys11.pdf: eurosys11.dvi
	dvipdf eurosys11
eurosys11.dvi: eurosys11.tex all.bib
	pdflatex eurosys11
	bibtex eurosys11
	pdflatex eurosys11

iwp9slides.pdf: iwp9slides.tex
	pdflatex iwp9slides.tex
	bibtex iwp9slides
	pdflatex iwp9slides.tex

clean: 
	rm -f paper.dvi paper.pdf paper.log paper.bbl paper.aux paper.blg iwp9slides.pdf
