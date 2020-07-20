TESTS=$(wildcard test/*.t)
EXPECTED:=$(TESTS:.t=.expected)

all: lex

.PHONY: test $(EXPECTED) clean

lex: lex.cpp
	g++ -std=c++17 lex.cpp -o lex

test: $(EXPECTED)

$(EXPECTED): %.expected: %.t lex
	@echo testing $<
	@./lex $< | diff -u --color $@ -

clean:
	rm lex
