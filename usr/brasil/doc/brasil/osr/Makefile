all: uem-ladis09.ps uem-ladis09.pdf


uem-ladis09.ps: uem-ladis09.dvi
	dvips -t letter -o uem-ladis09.ps uem-ladis09.dvi

uem-ladis09.pdf: uem-ladis09.ps 
	ps2pdf uem-ladis09.ps uem-ladis09.pdf 
	open uem-ladis09.pdf 

uem-ladis09.dvi: uem-ladis09.tex ../brasil.bib
	latex uem-ladis09.tex
	bibtex uem-ladis09
	latex uem-ladis09.tex
	latex uem-ladis09.tex


clean:
	rm -f *~ *.dvi *.log *.aux

distclean:
	rm -f *~ *.dvi *.log *.aux *.bbl *.blg uem-ladis09.ps uem-ladis09.pdf
