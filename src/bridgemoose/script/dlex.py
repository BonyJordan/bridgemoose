from .ply import lex

def make_lexer():

    token_set = set(["NUMBER", "RPAREN", "WORD", "CARD"])
    states = (
        ('ShapeMode', 'exclusive'),
        ('CardMode', 'exclusive'),
    )
    literals = "!<>+-*/%()?:,=";
    singular_words = set([
        "and",
        "any",
        "c13",
        "east",
        "hascard",
        "north",
        "not",
        "or",
        "shape",
        "south",
        "top2",
        "top3",
        "top4",
        "top5",
        "west",
    ])
    token_set.update([x.upper() for x in singular_words])

    plural_words = set([
        "ace",
        "club",
        "control",
        "diamond",
        "hcp",
        "heart",
        "jack",
        "king",
        "loser",
        "queen",
        "spade",
        "ten",
    ])
    token_set.update([x.upper() for x in plural_words])

    tdefs = {
        "AND"           : "r'&&'",
        "OR"            : "r'\|\|'",
        "LESS_EQUAL"    : "r'<='",
        "GREATER_EQUAL" : "r'>='",
        "EQUAL"         : "r'=='",
        "NOT_EQUAL"     : "r'!='",
    }
    for name, rex in tdefs.items():
        token_set.add(name)
        exec(f"t_{name} = {rex}")

    tsdefs = {
        "PLUS"  : "r'\+'",
        "MINUS"  : "r'-'",
        "COMMA"  : "r','",
        "LPAREN" : "r'\('",
        "PATTERN": "r'[0-9x]{4}'"
    }
    for name, rex in tsdefs.items():
        token_set.add(name)
        exec(f"t_ShapeMode_{name} = {rex}")

    def t_ShapeMode_WORD(t):
        r'(north|south|east|west|any)'
        t.type = t.value.upper()
        return t

    def t_ShapeMode_RPAREN(t):
        r'\)'
        t.lexer.begin('INITIAL')
        return t

    tcdefs = {
        "COMMA"  : "r','",
        "LPAREN" : "r'\('",
    }
    for name, rex in tcdefs.items():
        token_set.add(name)
        exec(f"t_CardMode_{name} = {rex}")

    def t_CardMode_WORD(t):
        r'(north|south|east|west)'
        t.type = t.value.upper()
        return t

    def t_CardMode_RPAREN(t):
        r'\)'
        t.lexer.begin('INITIAL')
        return t

    def t_CardMode_CARD(t):
        r'([23456789TJQKA][CDHS])|([CDHS][23456789TJQKA])'
        if t.value[1] in "CDHS":
            t.value = t.value[1] + t.value[0]
        t.type = CARD
        return t

    def t_NUMBER(t):
        r'\d+'
        t.value = int(t.value)
        return t

    def t_WORD(t):
        r'[A-Za-z_][A-Za-z_0-9]*'
        lower = t.value.lower()

        if lower in singular_words:
            t.type = t.value.upper()
        elif lower in plural_words:
            t.type = t.value.upper()
        elif lower[-1] == "s" and lower[:-1] in plural_words:
            t.type = t.value[:-1].upper()

        if t.type == "SHAPE":
            t.lexer.begin('ShapeMode')
        elif t.type == "HASCARD":
            t.lexer.begin('CardMode')
        return t


    def t_ANY_newline(t):
        r'\n+'
        t.lexer.lineno += len(t.value)

    t_ANY_ignore = ' \t'
    t_ANY_ignore_COMMENT1 = r'\#.*'
    t_ANY_ignore_COMMENT2 = r'//.*'
    t_ANY_ignore_COMMENT3 = r'/\*.*\*/'

    def t_ANY_error(t):
        raise SyntaxError(f"Illegal character {t.value[0]} line {t.lexer.lineno}")


    tokens = list(token_set)
    return lex.lex(), tokens


if __name__ == "__main__":
    import sys
    if len(sys.argv) > 1:
        f = open(sys.argv[1], 'r')
    else:
        f = sys.stdin

    lexer, _ = make_lexer()
    lexer.input(f.read())
    for tok in lexer:
        print(tok)
