#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <format>
#include <queue>
#include <cstdlib>
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

enum Type
{
    non_terminal,
    terminal,
    expression
};

struct Node
{
    string name;
    Type tp;
    vector<Node *> subnode;
};

class Grammar
{
public:
    Grammar(json &content, unsigned maxdepth)
    {
        map<string, vector<vector<string>>> contentInstd = content.template get<map<string, vector<vector<string>>>>();
        for (auto key : contentInstd)
        {
            allocate_node(key.first, Type::non_terminal);
        }
        for (auto rule : contentInstd)
        {
            for (auto expression : rule.second)
            {
                // check subnode is an expression or sigel node to compress the grammar;
                if (expression.size() == 1)
                {
                    Node *newnode;
                    if (this->mp.count(expression[0]) == 0)
                    {
                        newnode = this->allocate_node(expression[0], Type::terminal);
                    }
                    else
                    {
                        newnode = mp[expression[0]];
                    }
                    mp[rule.first]->subnode.push_back(newnode);
                    continue;
                }
                Node *optnodes = allocate_node("", Type::expression);
                for (auto option : expression)
                {
                    Node *newnode;
                    if (this->mp.count(option) == 0)
                    {
                        newnode = this->allocate_node(option, Type::terminal);
                    }
                    else
                    {
                        newnode = mp[option];
                    }
                    optnodes->subnode.push_back(newnode);
                }
                mp[rule.first]->subnode.push_back(optnodes);
            }
        }
        this->start = mp["<start>"];
        this->maxdepth = maxdepth;
        this->getshortcut();
    }

    void JIT(string file, bool show)
    {
        string code = R"(#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define BUFFER_SIZE 512*1024*1024   // buffer for storing text
)";
        code += "#define MAX_DEPTH " + to_string(this->maxdepth) + "\n";
        code += R"(
//xor to get random number
#define xor(l) \
    seed ^= seed << 13; \
    seed ^= seed >> 17; \
    seed ^= seed << 5; \
    branch = seed % l

#define NEXT() goto *program[pc++] // fetch next instruction

#define initStack() do { \
    stack.top = -1; \
    stack.capacity = MAX_DEPTH; \
} while (0)

#define pop() (stack.top--) 

typedef struct {
    char data[BUFFER_SIZE];
    unsigned top;
} Buffer;

Buffer buffer;  // Declare a global buffer

#define extend(c) { \
    if (buffer.top < BUFFER_SIZE) { \
        buffer.data[buffer.top++] = c; \
    } \
}

#define printbuff() { \
    for (unsigned i = 0; i < buffer.top; i++) { \
        putchar(buffer.data[i]); \
    } \
    putchar('\n'); \
}

#define clean() { \
    buffer.top = 0; \
}

//frame used to record current context
typedef struct {
    void* Program[64]; //allow 64 options for each rule, adding hitting rate in chache.  
    unsigned int PC;    // program counter 
} frame;

// the stack from frame
typedef struct {
    frame frames[MAX_DEPTH];     //the first pointer of frame
    int top;              // the top of stack
    int capacity;         // the capacity of stack
} Stack;

unsigned seed;  // Random seed
unsigned branch;  // To hold branch value
Stack stack;
unsigned pc; //program counter;
void* program[1000];


int main() {
    srand(time(NULL));
    seed = rand();  // Initialize the seed
    initStack();
)";
        code += "    unsigned sum = 0;\n";
        code += "    clock_t start = clock();\n";
        code += "    unsigned int iterator = 0;\n";
        code += "    memcpy(program, (void*[]){";
        code += "&&func_" + to_string(reinterpret_cast<uintptr_t>(this->start)) + ",&&HALT}, sizeof(void*[2]));\n";
        code += "START:\n";
        code += R"(    NEXT();
    RET:
        pc = stack.frames[stack.top].PC;
        memcpy(program, stack.frames[stack.top].Program, sizeof(void*[64]));
        pop();
        NEXT();
    HALT:
        stack.top = -1;
        pc = 0;
        goto end;
)";
        string blocks = "";
        for(auto &x:this->nodes){
            string block = "";
            block += "    func_"+to_string(reinterpret_cast<uintptr_t>(x))+":\n";
            if(x -> tp == Type::terminal){
                for (int j = 0; j < x->name.size(); j++)
                {
                    block += "        extend(" + to_string((unsigned)x->name[j]) + ");\n";
                }
                block += "        NEXT();\n";
            } else if(x -> tp == Type::non_terminal){
                block += "        if(stack.top==MAX_DEPTH-1){\n";
                for (int j = 0; j < this->shortcut[x].size(); j++)
                {
                    block += "            extend(" + to_string((unsigned)shortcut[x][j]) + ");\n";
                }
                block += "            NEXT();\n";
                block += "        }\n";
                block += "        xor(" + to_string(x->subnode.size()) + ");\n";
                block += "        stack.top++;\n";
                block += "        memcpy(stack.frames[stack.top].Program, program, sizeof(void*[64]));\n";
                block += "        stack.frames[stack.top].PC = pc;\n";
                block += "        pc = 0;\n";
                block += "        program[0] = &&RET;\n";
                block += "        switch (branch)\n";
                block += "        {\n";
                for(int j=0;j<x->subnode.size();j++){
                    block += "        case "+to_string(j)+":\n";
                    block += "            goto func_"+to_string(reinterpret_cast<uintptr_t>(x->subnode[j]))+";\n";
                    block += "            break;\n";
                }
                block += "        }\n";
            } else if(x->tp == Type::expression){
                block += "        pc = 0;\n";
                block += "        memcpy(program, (void*[]){";
                for(auto &j:x->subnode){
                    block+="&&func_" + to_string(reinterpret_cast<uintptr_t>(j)) + ",";
                }
                block += "&&RET}, sizeof(void*["+to_string(x->subnode.size()+1)+"]));\n";
                block += "        NEXT();\n";
            }
            blocks += block;
        }
        code += blocks;
        code += R"(    end:
    iterator++;
    sum += buffer.top;
    if ((iterator&0xFFFF) == 0){
        clock_t end = clock();
        double duration = (double)(end-start)/CLOCKS_PER_SEC;
        printf("Throughput Rate: %.4f MB/s\n",(double)sum/duration/1024/1024);
    )";
        if(show){
            code+="        printbuff();\n";
        }
        code += "        start = end;\n";
        code += "        sum = 0;\n";
        code += "    }\n";
        code += "    clean();\n";
        code += "    goto START;\n";
        code += "}";
        std::ofstream ofs(file, std::ofstream::out | std::ofstream::trunc);
        ofs << code;
        ofs.close();
        std::cout << "Code written to file successfully." << std::endl;
    }

private:
    vector<Node *> nodes;
    map<string, Node *> mp;
    int count = 0;
    Node *start;
    unsigned maxdepth;
    map<Node *, string> shortcut;
    Node *allocate_node(string name, Type tp)
    {
        Node *newnode = new Node();
        newnode->tp = tp;
        if (tp == Type::expression)
        {
            newnode->name = "expression" + to_string(count++);
        }
        else
        {
            newnode->name = name;
        }
        nodes.push_back(newnode);
        mp[newnode->name] = newnode;
        return newnode;
    }

    void getshortcut()
    {
        for (auto &i : nodes)
        {
            if (i->tp == Type::terminal)
            {
                shortcut[i] = i->name;
            }
        }
        while (1)
        {
            // indicate at least find a shortcut or not
            bool flag = false;
            for (auto &i : nodes)
            {
                if (i->tp == Type::terminal || shortcut.count(i))
                {
                    continue;
                }
                if (i->tp == Type::non_terminal)
                {
                    for (auto &j : i->subnode)
                    {
                        if (shortcut.count(j))
                        {
                            shortcut[i] = shortcut[j];
                            flag = true;
                        }
                    }
                }
                else if (i->tp == Type::expression)
                {
                    string combination = "";
                    bool allNode = true;
                    for (auto &j : i->subnode)
                    {
                        if (shortcut.count(j))
                        {
                            combination += shortcut[j];
                        }
                        else
                        {
                            allNode = false;
                            break;
                        }
                    }
                    if (allNode)
                    {
                        shortcut[i] = combination;
                        flag = true;
                    }
                }
            }
            if (!flag)
            {
                break;
            }
        }
    }
};

int main(int argc, char *argv[])
{
    unsigned depth = 0;
    std::string path;
    std::string outputFile;
    bool show = false;
    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "-depth" && i + 1 < argc)
        {
            depth = static_cast<unsigned int>(std::atoi(argv[++i]));
        }
        else if (arg == "-path" && i + 1 < argc)
        {
            path = argv[++i];
        }
        else if (arg == "-o" && i + 1 < argc)
        {
            outputFile = argv[++i];
        }
        else if (arg == "--show")
        {
            show = true;
        }
    }

    if (depth == 0 || path.empty() || outputFile.empty())
    {
        std::cerr << "Usage: " << argv[0] << " -depth <number> -path <path> -o <output file> [--show]" << std::endl;
        return 1;
    }

    std::ifstream f(path);
    json content = json::parse(f);
    Grammar gram = Grammar(content, depth);
    gram.JIT(outputFile, show);
    string compile = "clang " + outputFile + " -O2 " + "-o " + outputFile + ".out";
    system(compile.c_str());
    // string runcode = "./vmio.out";
    // system(runcode.c_str());
    return 0;
}