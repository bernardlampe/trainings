TARGET := main.pdf
SRC := $(TARGET:.pdf=.tex)

.PHONY: all
all: $(TARGET)

$(TARGET): $(SRC)
	pdflatex $^

.PHONY: open
open: $(TARGET)
	evince $(TARGET) &

.PHONY: clean
clean:
	rm -f *.aux *.log $(TARGET)

