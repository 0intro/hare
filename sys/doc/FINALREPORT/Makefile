all: finalreport.pdf 

activering.pdf: activering.pic
	pic activering.pic > activering.troff
	groff activering.troff > activering.ps
	ps2pdf activering.ps activering.pdf
	open activering.pdf

finalreport.pdf: finalreport.tex all.bib ftq.tex commercial.tex os.tex activering.pdf
	pdflatex finalreport
	bibtex finalreport
	pdflatex finalreport

clean: 
	rm -f *.dvi finalreport.pdf *.log *.bbl *.aux *.blg 
