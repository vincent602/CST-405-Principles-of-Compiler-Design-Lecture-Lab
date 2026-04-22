CC = gcc
LEX = flex
YACC = bison
CFLAGS = -g -Wall

TARGET = minicompiler
OBJS = lex.yy.o parser.tab.o main.o ast.o symtab.o codegen.o tac.o semantic.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

lex.yy.c: scanner.l parser.tab.h
	$(LEX) scanner.l

parser.tab.c parser.tab.h: parser.y
	$(YACC) -d parser.y

lex.yy.o: lex.yy.c
	$(CC) $(CFLAGS) -c lex.yy.c

parser.tab.o: parser.tab.c
	$(CC) $(CFLAGS) -c parser.tab.c

main.o: main.c ast.h codegen.h tac.h semantic.h
	$(CC) $(CFLAGS) -c main.c

ast.o: ast.c ast.h
	$(CC) $(CFLAGS) -c ast.c

symtab.o: symtab.c symtab.h
	$(CC) $(CFLAGS) -c symtab.c

codegen.o: codegen.c codegen.h ast.h symtab.h
	$(CC) $(CFLAGS) -c codegen.c

tac.o: tac.c tac.h ast.h
	$(CC) $(CFLAGS) -c tac.c

semantic.o: semantic.c semantic.h ast.h symtab.h
	$(CC) $(CFLAGS) -c semantic.c

clean:
	rm -f $(TARGET) $(OBJS) lex.yy.c parser.tab.c parser.tab.h *.s *.tac

test: $(TARGET)
	./$(TARGET) test.c test.s
	@echo "\n=== Generated MIPS Code ==="
	@cat test.s

test-switch: $(TARGET)
	./$(TARGET) test_switch.cm test_switch.s > test_switch.compile.out 2>&1
	@echo "\n=== test_switch.spim.out ==="
	@if command -v spim >/dev/null 2>&1; then \
		spim -file test_switch.s > test_switch.spim.out 2>&1; \
		cat test_switch.spim.out; \
	else \
		echo "spim not found; generated test_switch.s only"; \
	fi

project6: $(TARGET)
	./$(TARGET) project6.cm project6.s

benchmark: $(TARGET)
	./project6_benchmark.sh

.PHONY: all clean test test-switch project6 benchmark
