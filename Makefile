all: brightnessd


brightnessd: brightnessd.o

.PHONY: all clean
clean:
	-rm -vf brightnessd{,.o}
