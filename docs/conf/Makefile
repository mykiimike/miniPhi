all: refman.pdf

pdf: refman.pdf

refman.pdf: refman.tex
	pdflatex refman.tex < pdflatex.input
	makeindex refman.idx
	pdflatex refman.tex < pdflatex.input
	latex_count=5 ; \
	while egrep -s 'Rerun (LaTeX|to get cross-references right)' refman.log && [ $$latex_count -gt 0 ] ;\
        do \
          echo "Rerunning latex....." ;\
          pdflatex refman.tex ;\
          latex_count=`expr $$latex_count - 1` ;\
        done

clean:
	$(RM) /s/q *.ps *.dvi *.aux *.toc *.idx *.ind *.ilg *.log *.out refman.pdf
