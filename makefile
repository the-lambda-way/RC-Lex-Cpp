GCC=/usr/local/gcc-10.1.0/bin/g++-10.1
INCLUDES=/usr/local/gcc-10.1.0/include/c++/10.1.0/
TESTS=$(wildcard test/*.t)
EXPECTED:=$(TESTS:.t=.expected)

all: lex

.PHONY: test $(EXPECTED) clean

lex: lex.cpp
	$(GCC) -std=c++20 -ggdb -I$(INCLUDES) lex.cpp -o lex

test: $(EXPECTED)

$(EXPECTED): %.expected: %.t lex
	@echo testing $<
	@./lex $< | diff -u --color $@ -

clean:
	rm lex
