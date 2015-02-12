doc:
	doxygen > docs/doxylog.log; \
	cp ./docs/conf/Makefile ./docs/latex/; \
	cp ./docs/conf/pdflatex.input ./docs/latex/; \
	cd ./docs/latex/; \
	make; \
	rm refman.pdf; \
	make; \
	cp refman.pdf ../referencesManual.pdf

#all:
#    cd $(PROJECT_PATH)/doc; \
#    $(DOXYGEN_PATH)/doxygen Doxyfile > doxylog.log; \
#    cp $(PROJECT_PATH)/doc/conf/Makefile $(PROJECT_PATH)/doc/latex/Makefile; \
#    cp $(PROJECT_PATH)/doc/conf/pdflatex.input $(PROJECT_PATH)/doc/latex/pdflatex.input; \
#    cd $(PROJECT_PATH)/doc/latex; \
#    $(DOXYGEN_MAKE_PATH)/make; \
#    $(RM) refman.pdf; \
#    $(DOXYGEN_MAKE_PATH)/make; \
#    cp $(PROJECT_PATH)/doc/latex/refman.pdf $(PROJECT_PATH)/doc/$(DOCNAME).pdf

#clean:
#    $(RM) $(PROJECT_PATH)/doc/html/*.*; \
#    $(RM) $(PROJECT_PATH)/doc/latex/*.*

