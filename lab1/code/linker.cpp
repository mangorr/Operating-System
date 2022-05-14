#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <string.h>
#include <cmath>
#include <iomanip>

using namespace std;

string infile;
char delim[] = " \t\n";

//  Normally variables should be defined as private members.
//  However it's complex to write lots of get() and set() methods.
//  So the following variables are all declared as public ones.

class Token {
public:
    Token (int _line, int _offset, string _tok) : line(_line), offset(_offset), tok(_tok) {}
    string tok;
    int line;
    int offset;
};

class Symbol {
public:
    string symbol;
    int baseAddress = 0;
    int relativeAddress = 0;
    int absoluteAddress = 0;
    int moduleNo = 0;  // The module the symbol belongs to 
    bool used = false; // For checking whether the symbol has been used or not
    bool multiDefined = false;  // For checking multi defined

    Symbol () {}
    Symbol (string _symbol) : symbol(_symbol) {}

    void operator = (const Symbol& _sym) {
        this->symbol = _sym.symbol;
        this->baseAddress = _sym.baseAddress;
        this->relativeAddress = _sym.relativeAddress;
        this->absoluteAddress = _sym.absoluteAddress;
        this->moduleNo = _sym.moduleNo;
        this->used = _sym.used;
        this->multiDefined = _sym.multiDefined;
    }

    void setAddress (int _baseAddress, int _relativeAddress) {
        baseAddress = _baseAddress;
        relativeAddress = _relativeAddress;
        absoluteAddress = baseAddress + relativeAddress;
    }
};

class Tokenizer {
public:
    ifstream &source;
    string text;
    int offset = 1, line;
    bool end = false;

    Tokenizer (ifstream& _source, int _line) : source(_source), line(_line) {
        if (getline(source, text)) {
            end = false;
        } else end = true;
    }

    Token getToken ();

    void reset() {
        if(getline(source, text)){
            end = false;
            line++;
            offset=1;
        }
        else{
            end = true;
        }
    }
};

Token Tokenizer::getToken () {
    while (text.length() == 0) { // In case of input-11
        if (getline(source, text)) {  // New Line
            //  Reset offset and line
            offset = 1;
            ++line;
        } else {
            return Token(line, offset, "");
        }
    }

    // Reference : https://www.cplusplus.com/reference/cstring/strtok/
    char* rText = new char[text.length() + 1];
    strcpy(rText, text.c_str());
    char* tok = strtok(rText, delim);
    Token token(line, offset, string(tok));
    // cout<<"line offset tok: "<<line<<" "<<offset<<" "<<token.tok<<endl;
    offset += (strlen(tok) + 1);
    char* remain_text = strtok(NULL, "");

    if(remain_text == NULL) {
            --offset;
            text = "";
    } else {
        // Delimeters may be left
        text = string(remain_text);
        if(strtok(remain_text, delim) == NULL){
            // Don't parse delimeters
            text = "";
        }
    }

    delete [] rText;
    return token;
}

// Tokenizer createModule(ifstream &_source, int _line){
//     Tokenizer tokenizer(_source, _line);
//     return tokenizer;
// }

void __parseerror(int errcode, Token token) {
    static char const *errstr[] = {
        "NUM_EXPECTED", // Number expect, anything >= 2^30 is not a number either
        "SYM_EXPECTED", // Symbol Expected
        "ADDR_EXPECTED", // Addressing Expected which is A/E/I/R
        "SYM_TOO_LONG", // Symbol Name is too long
        "TOO_MANY_DEF_IN_MODULE", // > 16
        "TOO_MANY_USE_IN_MODULE", // > 16
        "TOO_MANY_INSTR", // total num_instr exceeds memory size (512)
    };
    printf("Parse Error line %d offset %d: %s\n", token.line, token.offset, errstr[errcode]);
}

bool isNumber (string tok) {
    for (auto tt : tok) {
        if (!isdigit(tt)) return false;
    }
    return true;
}

int readInt (Token token) {
    // 1.Must be a number
    // 2.Bounds check
    // 3.Return the number
    // Other check should be done in pass1 or pass2, since different count represent for different meanings

    if (token.tok.empty() || !isNumber(token.tok)) {
        __parseerror(0, token);
        exit(-1);
    }

    int tmp = stoi(token.tok);
    if (tmp > pow(2, 30)) {
        __parseerror(0, token);
        exit(-1);
    }

    return tmp;
}

bool checkSymbolName (Token token) {
    for (int i = 0; i < token.tok.length(); i++) {
        if (i == 0) { // First character cannot be a num
            if (!isalpha(token.tok[i]) && token.tok[i] != '_') {
                __parseerror(1, token);
                return false;
            }
        } else { // The following character can be nums, digits and '_'
            if (!isalnum(token.tok[i]) && token.tok[i] != '_') {
                __parseerror(1, token);
            return false;
            }
        }
    }

    return true;
}

Symbol readSymbol (Token token) {
    // 1.Must be valid variable name
    // 2.Must be less then 16 characters
    // Other check should be done in pass1 or pass2

    if (token.tok.empty()) {
        __parseerror(1, token);
        exit(-1);
    }

    if (!checkSymbolName(token)) exit(-1);

    if (token.tok.length() > 16) {
        __parseerror(3, token);
        exit(-1);
    }

    return Symbol(token.tok);
}

string readIAER (Token token) {
    // 1.Check whether the tok is "I" or "A" or "E" or "R"
    // Other check should be done in pass1 or pass2

    if (token.tok != "I" && token.tok != "A" && token.tok != "E" && token.tok != "R") {
        __parseerror(2, token);
        exit(-1);
    }

    return token.tok;
}

// In order to ensure order of symbols, use vector instead of map or unordered_map
vector<pair<string, Symbol>> SymTable;

void Pass1 () {
    ifstream in;
    in.open(infile.c_str());
    if (!in.is_open()) {
        std::cerr << "cant open " << infile << std::endl;
        exit(-1);
    }

    int line, moduleNo, baseAddress, codeTotalNum;
    string text;
    line = 1;
    moduleNo = baseAddress = codeTotalNum = 0;
    Tokenizer tokenizer(in, line);

    while (!in.eof()) {
        ++moduleNo;
        vector<Symbol> moduleSym;

        if (tokenizer.text.empty()) {
            tokenizer.reset();
        }

        if (tokenizer.end) break;

        Token tok = tokenizer.getToken();

        // defcount and deflist
        int defCount = readInt(tok);
        if (defCount > 16) {
            __parseerror(4, tok);
            exit(-1);
        }

        for (int i = 0; i < defCount; i++) {
            tok = tokenizer.getToken();
            Symbol sym = readSymbol(tok);
            // Since warning information needs addresses of symbols, store addresses
            tok = tokenizer.getToken();
            int sym_relativeAddress = readInt(tok);
            sym.setAddress(baseAddress, sym_relativeAddress);
            sym.moduleNo = moduleNo;
            moduleSym.push_back(sym);
        }

        // usecount and uselist
        tok = tokenizer.getToken();
        int useCount = readInt(tok);
        if (useCount > 16) {
            __parseerror(5, tok);
            exit(-1);
        }

        for (int i = 0; i < useCount; i++) {
            // In pass1, no need to get exact symbol in the uselist
            // But pre-checking is necessary. So using readSymbol to check but don't store it
            tok = tokenizer.getToken();
            readSymbol(tok);
        }

        // codecount and paragram text
        tok = tokenizer.getToken();
        int codeCount = readInt(tok);
        codeTotalNum += codeCount; // Check number of code (programe text)
        if (codeTotalNum > 512) {
            __parseerror(6, tok); 
            exit(-1);
        }
        
        for (int i = 0; i < codeCount; i++) {
            tok = tokenizer.getToken();
            readIAER(tok);
            tok = tokenizer.getToken();
            readInt(tok);
        }

        // Check whether multi defined and insert into SymTable
        for (auto mm : moduleSym) {
            bool find = false;
            int index = 0;
            for (auto ss : SymTable) {
                if (ss.first == mm.symbol) {
                    find = true;
                    break;
                }
                ++index;
            }
            if (!find) { // Has not been defined before
                if (mm.relativeAddress >= codeCount) {
                    std::cout << "Warning: Module " << moduleNo << ": " <<mm.symbol << " too big " << mm.relativeAddress << " (max=" << codeCount - 1 << ") assume zero relative" << std::endl;
                    mm.setAddress(baseAddress, 0);
                }
                SymTable.push_back({mm.symbol, mm});
            } else { // Has been defined before
                SymTable[index].second.multiDefined = true; // Mark as multiDefined, for the following multi defined cheking
                if (SymTable[index].second.relativeAddress >= codeCount) {
                    std::cout << "Warning: Module " << moduleNo << ": " <<mm.symbol << " too big " << mm.relativeAddress << " (max=" << codeCount - 1 << ") assume zero relative" << std::endl;
                    SymTable[index].second.setAddress(baseAddress, 0);
                }
            }
        }

        baseAddress += codeCount;
    }

    // cout<<"-------------------check Symbol Table-------------------------"<<endl;
    // cout<<"SymTable.size():"<<SymTable.size()<<endl;
    // for (auto ss : SymTable) {
    //     cout<<"symbol: "<<ss.second.symbol<<endl;
    //     cout<<"absoluteAddress: "<<ss.second.absoluteAddress<<endl;
    //     cout<<"baseAddress: "<<ss.second.baseAddress<<endl;
    //     cout<<"relativeAddress: "<<ss.second.relativeAddress<<endl;
    //     cout<<"multiDefined: "<<ss.second.multiDefined<<endl;
    //     cout<<"belongedModule: "<<ss.second.moduleNo<<endl;
    //     cout<<"used: "<<ss.second.used<<endl;
    // }
    // cout<<endl;

    // Print Symbol Table first due to map<string, Symbol> SymTable
    std::cout << "Symbol Table" << std::endl;
    for (auto ss : SymTable) {
        // Check whether multi defined -> error message
        std::cout << ss.first << "=" << ss.second.absoluteAddress;
        if (ss.second.multiDefined) {
            std::cout << " Error: This variable is multiple times defined; first value used" << std::endl;
        } else std::cout << std::endl;
    }
    std::cout << std::endl;
}

void Pass2 () {
    ifstream in;
    in.open(infile.c_str());
    if (!in.is_open()) {
        std::cerr << "cant open " << infile << std::endl;
        exit(-1);
    }

    int line, moduleNo, baseAddress, codeTotalNum;
    int index = 0;
    string text;
    line = 1;
    moduleNo = baseAddress = codeTotalNum = 0;
    Tokenizer tokenizer(in, line);

    // Deal with Memory Map
    std::cout << "Memory Map" << std::endl;
    while (!in.eof()) {
        ++moduleNo;
        vector<Symbol> moduleSym;

        if (tokenizer.text.empty()) {
            tokenizer.reset();
        }

        if (tokenizer.end) {--moduleNo; break;}
        // cout<<"moduleNo:"<<moduleNo<<endl;

        Token tok = tokenizer.getToken();

        // defcount and deflist
        int defCount = readInt(tok);

        for (int i = 0; i < defCount; i++) {
            tok = tokenizer.getToken();
            Symbol sym = readSymbol(tok);
            tok = tokenizer.getToken();
            int sym_relativeAddress = readInt(tok);
            sym.setAddress(baseAddress, sym_relativeAddress);
            sym.moduleNo = moduleNo;
            moduleSym.push_back(sym);
        }

        // usecount and uselist
        tok = tokenizer.getToken();
        int useCount = readInt(tok);
        vector<Symbol> useSymbolList;

        for (int i = 0; i < useCount; i++) {
            // In pass2, we need to get the exact symbol in the uselist and do the following check
            tok = tokenizer.getToken();
            Symbol useSym = readSymbol(tok);
            // Copy Symbol Object
            for (auto ss : SymTable) {
                if (ss.first == useSym.symbol) {
                    useSym = ss.second;
                }
            }
            useSymbolList.push_back(useSym);
        }

        // codecount and paragram text
        tok = tokenizer.getToken();
        int codeCount = readInt(tok);
        codeTotalNum += codeCount;
        
        for (int i = 0; i < codeCount; i++) {
            tok = tokenizer.getToken();
            string type = readIAER(tok);
            tok = tokenizer.getToken();
            int instruction = readInt(tok);
            int opcode = instruction / 1000;
            int operand = instruction % 1000;
            bool find = false;

            std::cout << setfill('0') << setw(3) << index << ": ";

            int indexInner = 0;
            bool opcodeError;
            if (opcode > 9) opcodeError = true; // Error
            else opcodeError = false;

            if (type == "I") {
                if (instruction >= 10000) {
                    std::cout << 9999 << " Error: Illegal immediate value; treated as 9999" <<std::endl;
                } else {
                    std::cout << setfill('0') << setw(4) << instruction << std::endl;
                }
            } else if (opcodeError) {
                std::cout << 9999 << " Error: Illegal opcode; treated as 9999" <<std::endl;
            } else if (type == "A") {
                // Rule 8 : absolute address should not exceed the size of the machine
                if (operand > 512) {
                    std::cout << setfill('0') << setw(4) << opcode * 1000 + 0;
                    std::cout << " Error: Absolute address exceeds machine size; zero used" <<std::endl;
                } else {
                    std::cout << setfill('0') << instruction << std::endl;
                }
            } else if (type == "E") {
                // Rule 6 : address appearing in the definition should not exceed the size of the module
                if (operand >= useCount) {
                    std::cout << setfill('0') << setw(4) << instruction;
                    std::cout << " Error: External address exceeds length of uselist; treated as immediate" << std::endl;
                } else {
                    // Rule 3 : a symbol is used but not defined before
                    Symbol usedSymbol = useSymbolList[operand];
                    find = false;
                    for (auto ss : SymTable) {
                        if (ss.first == usedSymbol.symbol) {find = true; break;}
                        ++indexInner; // Mark indexInner in the SymTable
                    }

                    if (!find) {
                        std::cout << setfill('0') << setw(4) << opcode * 1000 + 0;
                        std::cout << " Error: " << usedSymbol.symbol << " is not defined; zero used" << std::endl;
                    } else {
                        std::cout << setfill('0') << setw(4) << opcode * 1000 + useSymbolList[operand].absoluteAddress << std::endl;
                    }
                    useSymbolList[operand].used = true; // Marked as used
                    SymTable[indexInner].second.used = true;
                }
            } else if (type == "R") {
                // Rule 8 : absolute address should not exceed the size of the machine
                int RabsAddress = baseAddress + operand;
                if (RabsAddress >= 512) {
                    std::cout << setfill('0') << setw(4) << opcode * 1000 + 0;
                    std::cout <<" Error: Absolute address exceeds machine size; zero used" << std::endl;
                } else if (operand >= codeCount) { // Rule 9
                    std::cout << setfill('0') << setw(4) << opcode * 1000 + baseAddress + 0;
                    std::cout << " Error: Relative address exceeds module size; zero used" << std::endl;
                } else {
                    std::cout << setfill('0') << setw(4) << opcode * 1000 + RabsAddress << std::endl;
                }
            } else {
                // This would never happen
                std::cout << "This would never happen" << std::endl;
            }
            ++index;
        }

        // Rule 7 : a symbol appears in the useList but never been used
        // This message should be printed after the print of mudule information
        for (auto uu : useSymbolList) {
            if (!uu.used) {
                std::cout << "Warning: Module " << moduleNo << ": " << uu.symbol << " appeared in the uselist but was not actually used" << std::endl;
            }
        }

        baseAddress += codeCount;
    }

    std::cout << std::endl;
    // Rule 4 : a symbol is defined but never used
    // This could be done only after all modules have been analysed
    for (auto ss : SymTable) {
        if (!ss.second.used) {
            std::cout << "Warning: Module " << ss.second.moduleNo << ": " << ss.second.symbol << " was defined but never used" << std::endl;
        }
    }
    std::cout << std::endl;
}

int main(int argc, char *argv[]){
    // infile = "/Users/yachiru/Documents/nyu/class/OS/lab1/lab1_assign/input-1";
    infile = argv[1];
    ifstream in;
    in.open(infile.c_str());
    if (!in.is_open()) {
        std::cerr << "cant open " << infile << std::endl;
        exit(-1);
    }

    Pass1();
    Pass2();
 
    return 0;
}