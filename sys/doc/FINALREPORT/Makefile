all: finalreport.pdf 

finalreport.pdf: finalreport.tex all.bib ftq.tex commercial.tex os.tex
	pdflatex finalreport
	bibtex finalreport
	pdflatex finalreport

clean: 
	rm -f *.dvi finalreport.pdf *.log *.bbl *.aux *.blg 
