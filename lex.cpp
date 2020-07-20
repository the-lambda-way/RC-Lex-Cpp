// An implementation of the Rosetta Code Lexical Analyzer in C++
// http://rosettacode.org/wiki/Compiler/lexical_analyzer

#include <concepts>
#include <fstream>       // file_to_string, string_to_file
#include <functional>    // std::invoke
#include <iomanip>       // std::setw
#include <ios>           // std::left
#include <iostream>
#include <map>           // keywords
#include <ranges>
#include <sstream>
#include <string>
#include <variant>       // TokenVal
#include <vector>

using namespace std;


// =====================================================================================================================
// Machinery
// =====================================================================================================================

// ---------------------------------------------------------
// Input / Output
// ---------------------------------------------------------
string file_to_string (const string& path)
{
    // Open file
    ifstream file {path, ios::in | ios::binary | ios::ate};
    if (!file)   throw (errno);

    // Allocate string memory
    string contents;
    contents.resize(file.tellg());

    // Read file contents into string
    file.seekg(0);
    file.read(contents.data(), contents.size());

    return contents;
}


void string_to_file (const string& path, const string& contents)
{
    ofstream file {path, ios::out | ios::binary};
    if (!file)    throw (errno);

    file.write(contents.data(), contents.size());
}


template <class F>
    requires invocable<F, string> && same_as<string, invoke_result_t<F, string>>
void with_IO (string source, string destination, F f)
{
    string input;

    if (source == "stdin")    getline(cin, input);
    else                      input = file_to_string(source);

    string output = invoke(f, input);

    if (destination == "stdout")    cout << output;
    else                            string_to_file(destination, output);
}


// Add escaped newlines and backslashs back in for printing
string sanitize (string s)
{
    for (int i = 0; i < s.size(); ++i)
    {
        if      (s[i] == '\n')    s.replace(i++, 1, "\\n");
        else if (s[i] == '\\')    s.replace(i++, 1, "\\\\");
    }

    return s;
}


// ---------------------------------------------------------
// Scanner
// ---------------------------------------------------------
class Scanner
{
public:
    const char* pos;
    int         line = 1;
    int         col  = 1;


    Scanner (const char* source) : pos {source} {}


    inline char peek ()    { return *pos; }


    char next ()
    {
        advance();
        return peek();
    }


    void advance ()
    {
        if (*pos == '\n')    { ++line; col = 1; }
        else                 ++col;

        ++pos;
    }
}; // class Scanner


// =====================================================================================================================
// Tokens
// =====================================================================================================================
enum class TokenName
{
    OP_MULTIPLY, OP_DIVIDE, OP_MOD, OP_ADD, OP_SUBTRACT, OP_NEGATE,
    OP_LESS, OP_LESSEQUAL, OP_GREATER, OP_GREATEREQUAL, OP_EQUAL, OP_NOTEQUAL,
    OP_NOT, OP_ASSIGN, OP_AND, OP_OR,
    LEFTPAREN, RIGHTPAREN, LEFTBRACE, RIGHTBRACE, SEMICOLON, COMMA,
    KEYWORD_IF, KEYWORD_ELSE, KEYWORD_WHILE, KEYWORD_PRINT, KEYWORD_PUTC,
    IDENTIFIER, INTEGER, STRING,
    END_OF_INPUT, ERROR
};

using TokenVal = variant<int, string>;

struct Token
{
    TokenName name;
    TokenVal  value;
    int       line;
    int       column;
};


const char* to_cstring (TokenName name)
{
    static constexpr const char* s[] =
    {
        "Op_multiply", "Op_divide", "Op_mod", "Op_add", "Op_subtract", "Op_negate",
        "Op_less", "Op_lessequal", "Op_greater", "Op_greaterequal", "Op_equal", "Op_notequal",
        "Op_not", "Op_assign", "Op_and", "Op_or",
        "LeftParen", "RightParen", "LeftBrace", "RightBrace", "Semicolon", "Comma",
        "Keyword_if", "Keyword_else", "Keyword_while", "Keyword_print", "Keyword_putc"
    };

    return s[static_cast<int>(name)];
}

string to_string (TokenVal v)
{
    return holds_alternative<int>(v) ? to_string(get<int>(v)) : get<string>(v);
}

string to_string (Token t)
{
    ostringstream out;
    out << setw(2) << t.line << "   " << setw(2) << t.column  << "   ";

    switch (t.name)
    {
        case (TokenName::STRING) :
            out << "String            \"" << sanitize(to_string(t.value)) << "\"\n"; break;
        case (TokenName::INTEGER)      : out << "Integer           " << left << to_string(t.value) << '\n'; break;
        case (TokenName::IDENTIFIER)   : out << "Identifier        "         << to_string(t.value) << '\n'; break;
        case (TokenName::ERROR)        : out << "Error             "         << to_string(t.value) << '\n'; break;
        case (TokenName::END_OF_INPUT) : out << "End_of_input\n"; break;
        default                        : out << to_cstring(t.name) << '\n';
    }

    return out.str();
}


string list_tokens (const ranges::range auto& tokens)
{
    string s = "Location  Token name        Value\n"
               "--------------------------------------\n";

    for (auto t : tokens)    s += to_string(t);
    return s;
}


// =====================================================================================================================
// Lexer
// =====================================================================================================================

// Should follow C++ paradigms, i.e. an object-oriented lexer, not a monadic parser combinator
// Should reflect the C implementation, but improve upon it, and remain approachable and didactic
// Should be declarative as feasible
// Compound patterns can call functions, symbols should use case statements




class Lexer
{
public:
    Lexer (string_view source) : s {source.data()}, pre_state {s} {}


    bool has_more ()    { return s.peek() != '\0'; }


    Token next_token ()
    {
        skip_whitespace();

        pre_state = s;

        switch (s.peek())
        {
            case '*'  :    return simply(TokenName::OP_MULTIPLY);
            case '%'  :    return simply(TokenName::OP_MOD);
            case '+'  :    return simply(TokenName::OP_ADD);
            case '-'  :    return simply(TokenName::OP_SUBTRACT);
            case '{'  :    return simply(TokenName::LEFTBRACE);
            case '}'  :    return simply(TokenName::RIGHTBRACE);
            case '('  :    return simply(TokenName::LEFTPAREN);
            case ')'  :    return simply(TokenName::RIGHTPAREN);
            case ';'  :    return simply(TokenName::SEMICOLON);
            case ','  :    return simply(TokenName::COMMA);
            case '&'  :    return expect('&', TokenName::OP_AND);
            case '|'  :    return expect('|', TokenName::OP_OR);
            case '<'  :    return follow('=', TokenName::OP_LESSEQUAL,    TokenName::OP_LESS);
            case '>'  :    return follow('=', TokenName::OP_GREATEREQUAL, TokenName::OP_GREATER);
            case '='  :    return follow('=', TokenName::OP_EQUAL,        TokenName::OP_ASSIGN);
            case '!'  :    return follow('=', TokenName::OP_NOTEQUAL,     TokenName::OP_NOT);
            case '/'  :    return divide_or_comment();
            case '\'' :    return char_lit();
            case '"'  :    return string_lit();

            default   :    if (is_id_start(s.peek()))    return identifier();
                           if (is_digit(s.peek()))       return integer_lit();
                           return error("Unrecognized character '", s.peek(), "'");

            case '\0' :    return simple_token(TokenName::END_OF_INPUT);
        }
    }


private:
    Scanner s;
    Scanner pre_state;
    static const map<string, TokenName> keywords;


    template <class... Args>
    Token error (Args&&... ostream_args)
    {
        string code {pre_state.pos, (string::size_type) s.col - pre_state.col};

        ostringstream msg;
        (msg << ... << ostream_args) << '\n'
            << string(28, ' ') << "(" << s.line << ", " << s.col << "): " << code;

        if (s.peek() != '\0')    s.advance();

        return {TokenName::ERROR, msg.str(), pre_state.line, pre_state.col};
    }


    void skip_whitespace ()
    {
        while (isspace(static_cast<unsigned char>(s.peek())))
            s.advance();
    }


    Token simple_token (TokenName name)
    {
        return {name, 0, pre_state.line, pre_state.col};
    }


    Token simply (TokenName name)
    {
        s.advance();
        return simple_token(name);
    }


    Token expect (char expected, TokenName name)
    {
        if (s.next() == expected)    return simply(name);
        else                         return error("Unrecognized character '", s.peek(), "'");
    }


    Token follow (char expected, TokenName ifyes, TokenName ifno)
    {
        if (s.next() == expected)    return simply(ifyes);

        return simple_token(ifno);
    }


    Token divide_or_comment ()
    {
        if (s.next() != '*')    return simple_token(TokenName::OP_DIVIDE);

        while (s.next() != '\0')
        {
            if (s.peek() == '*' && s.next() == '/')
            {
                s.advance();
                return next_token();
            }
        }

        return error("End-of-file in comment. Closing comment characters not found.");
    }


    Token char_lit ()
    {
        int n = s.next();

        if (n == '\'')    return error("Empty character constant");

        if (n == '\\')    switch (s.next())
                          {
                              case 'n'  :    n = '\n'; break;
                              case '\\' :    n = '\\'; break;
                              default   :    return error("Unknown escape sequence \\", s.peek());
                          }

        if (s.next() != '\'')    return error("Multi-character constant");

        s.advance();
        return {TokenName::INTEGER, n, pre_state.line, pre_state.col};
    }


    Token string_lit ()
    {
        string text = "";

        while (s.next() != '"')
            switch (s.peek())
            {
                case '\\' :    switch (s.next())
                               {
                                   case 'n'  :    text += '\n'; continue;
                                   case '\\' :    text += '\\'; continue;
                                   default   :    return error("Unknown escape sequence \\", s.peek());
                               }

                case '\n' :    return error("End-of-line while scanning string literal."
                                            " Closing string character not found before end-of-line.");

                case '\0' :    return error("End-of-file while scanning string literal."
                                            " Closing string character not found.");

                default   :    text += s.peek();
            }

        s.advance();
        return {TokenName::STRING, text, pre_state.line, pre_state.col};
    }


    inline bool is_id_start (char c)    { return isalpha(static_cast<unsigned char>(c)) || c == '_'; }
    inline bool is_id_end   (char c)    { return isalnum(static_cast<unsigned char>(c)) || c == '_'; }
    inline bool is_digit    (char c)    { return isdigit(static_cast<unsigned char>(c));             }


    Token identifier ()
    {
        string text (1, s.peek());

        while (is_id_end(s.next()))    text += s.peek();

        const auto i = keywords.find(text);
        if (i != keywords.end())    return {i->second, 0, pre_state.line, pre_state.col};

        return {TokenName::IDENTIFIER, text, pre_state.line, pre_state.col};
    }


    Token integer_lit ()
    {
        string text (1, s.peek());

        while (is_digit(s.next()))    text += s.peek();

        if (is_id_start(s.peek()))
            return error("Invalid number. Starts like a number, but ends in non-numeric characters.");

        int n;

        try                     { n = stol(text);                               }
        catch (out_of_range)    { return error("Number exceeds maximum value"); }
        catch (invalid_argument)    { return error(text); }

        return {TokenName::INTEGER, n, pre_state.line, pre_state.col};
    }
}; // class Lexer


const map<string, TokenName> Lexer::keywords =
{
    {"else",  TokenName::KEYWORD_ELSE},
    {"if",    TokenName::KEYWORD_IF},
    {"print", TokenName::KEYWORD_PRINT},
    {"putc",  TokenName::KEYWORD_PUTC},
    {"while", TokenName::KEYWORD_WHILE}
};


int main (int argc, char* argv[])
{
    string in  = (argc > 1) ? argv[1] : "stdin";
    string out = (argc > 2) ? argv[2] : "stdout";

    with_IO(in, out, [] (string input)
    {
        Lexer lexer {input};
        vector<Token> tokens;

        while (lexer.has_more())    tokens.push_back(lexer.next_token());

        return list_tokens(tokens);
    });
}
