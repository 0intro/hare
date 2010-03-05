all: exec.ps exec.pdf


exec.ps: exec.dvi
	dvips -t letter -o exec.ps exec.dvi

exec.pdf: exec.ps 
	ps2pdf exec.ps exec.pdf

exec.dvi: exec.tex ../brasil.bib
	latex exec.tex
#	bibtex exec
	latex exec.tex
#	bibtex exec
	latex exec.tex

exec.html: exec.dvi
	latex2html -split 0 -ps_images exec.tex

clean:
	rm -f *~ *.dvi *.log *.aux

distclean:
	rm -f *~ *.dvi *.log *.aux exec.ps exec.pdf
