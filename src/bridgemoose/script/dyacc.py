from .ply import yacc
from . import dlex

def make_parser(tokens):
    def p_binary_op(p):
        """
        nexpr : nexpr "+" nexpr
              | nexpr "-" nexpr
              | nexpr "*" nexpr
              | nexpr "/" nexpr
              | nexpr "%" nexpr
        bexpr : nexpr "<" nexpr
              | nexpr ">" nexpr
              | nexpr LESS_EQUAL nexpr
              | nexpr GREATER_EQUAL nexpr
              | nexpr EQUAL nexpr
              | nexpr NOT_EQUAL nexpr
              | bexpr AND bexpr
              | bexpr OR bexpr
         """
        p[0] = (p[2], p[1], p[3])

    def p_parens(p):
        '''nexpr : "(" nexpr ")"
           bexpr : "(" bexpr ")"'''
        p[0] = p[2]

    def p_number(p):
        '''nexpr : NUMBER'''
        p[0] = p[1]

    def p_variable(p):
        '''bexpr : WORD'''
        p[0] = ("get", p[1])

    def p_unary_op(p):
        """
        bexpr : NOT bexpr
        """
        p[0] = ("not", p[2])

    def p_ternary_op(p):
        """
        nexpr : bexpr "?" nexpr ":" nexpr
        """
        p[0] = ("?", p[1], p[3], p[5])

    def p_compass(p):
        """
        compass : NORTH
                | SOUTH
                | EAST
                | WEST
        """
        p[0] = p[1]

    def p_suit(p):
        """
        suit : CLUB
             | DIAMOND
             | HEART
             | SPADE
        """
        p[0] = p[1]

    def p_hascard_call(p):
        """
        bexpr : HASCARD LPAREN compass COMMA CARD RPAREN
        """
        p[0] = (p[1],p[3],p[5])

    def p_shape_call(p):
        """
        bexpr : SHAPE LPAREN compass COMMA shapelist RPAREN
        """
        p[0] = (p[1],p[3],p[5])

    def p_shapelist_one(p):
        """
        shapelist : PATTERN
        """
        p[0] = ('exact', p[1])

    def p_shapelist_any_one(p):
        "shapelist : ANY PATTERN"
        p[0] = ('any', p[2])

    def p_shapelist_extend(p):
        """shapelist : shapelist PLUS shapelist
                    | shapelist MINUS shapelist"""
        p[0] = (p[2], p[1], p[3])

    def p_fname_12(p):
        """fname12 : HCP
                   | TEN
                   | JACK
                   | QUEEN
                   | KING
                   | ACE
                   | TOP2
                   | TOP3
                   | TOP4
                   | TOP5
                   | C13
                   | CONTROL
                   | LOSER"""
        p[0] = p[1]

    def p_fname_1(p):
        """fname1 : suit"""
        p[0] = p[1]

    def p_function_1_arg(p):
        '''nexpr : fname12 "(" compass ")"
                 | fname1  "(" compass ")"'''
        p[0] = (p[1], p[3])

    def p_function_2_arg(p):
        'nexpr : fname12 "(" compass "," suit ")"'
        p[0] = (p[1], p[3], p[5])

    def p_assignment(p):
        """assignment : WORD "=" nexpr
                      | WORD "=" bexpr"""
        p[0] = ("set", p[1], p[3])

    def p_script(p):
        """script : bexpr
                  | assignment script"""
        if len(p) == 3:
            p[0] = ("script", p[1], p[2])
        else:
            p[0] = ("script", p[1])

    def p_error(p):
        raise SyntaxError(f"Error at line {p.lineno} near '{p.value}'")


    precedence = (
        ('left', '?', ':'),
        ('left', 'OR'),
        ('left', 'AND'),
        ('right', 'NOT'),
        ('left', '+', '-', 'PLUS', 'MINUS'),
        ('left', '*', '/', '%'),
    )
    start = 'script'

    return yacc.yacc()

def fancy_print(tree, indent=0):
    for x in tree:
        if isinstance(x, tuple):
            fancy_print(x, indent+1)
        else:
            print(f"{' '*indent} {x} {type(x)}")

if __name__ == "__main__":
    import sys
    if len(sys.argv) > 1:
        f = open(sys.argv[1], 'r')
    else:
        f = sys.stdin

    lexer, tokens = dlex.make_lexer()
    parser = make_parser(tokens)
    result = parser.parse(f.read())

    fancy_print(result)
    # print(result)
