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
        string code = R"(#include<stdio.h>
#include<string.h>
#include<time.h>
#include<stdlib.h>
#define next(l)\
    seed ^= seed <<13;\
    seed ^= seed >>17;\
    seed ^= seed <<5;\
    branch = seed%l


#define BUFFER_SIZE 512*1024*1024

#define extend(c)\
    (buffer).data[(buffer).top++] = c;

#define clean()\
    buffer.top = 0
     
#define printbuff()\
    for (int i = 0; i < (buffer).top; i++) { \
        printf("%c", (buffer).data[i]); \
    } \
    printf("\n")
    
typedef struct {
    char data[BUFFER_SIZE];
    unsigned top;
} Buffer;
Buffer buffer;

unsigned branch;
unsigned cursor;
unsigned seed;
)";
        code += "#define MAXDEPTH " + to_string(this->maxdepth) + "\n";
        // creat signature
        for (int i = 0; i < this->nodes.size(); i++)
        {
            code += "void func_" + to_string(reinterpret_cast<uintptr_t>(mp[nodes[i]->name])) + "(unsigned depth);\n";
        }

        string functions = "";
        for (int i = 0; i < this->nodes.size(); i++)
        {
            string function = "";
            string functionname = "void func_" + to_string(reinterpret_cast<uintptr_t>(mp[nodes[i]->name])) + "(unsigned depth){\n";
            string body = "";
            if (nodes[i]->tp == Type::terminal)
            {
                for (int j = 0; j < nodes[i]->name.size(); j++)
                {
                    body += "    extend(" + to_string((unsigned)nodes[i]->name[j]) + ");\n";
                }
            }
            else if (nodes[i]->tp == Type::non_terminal)
            {
                body += "    if(depth>MAXDEPTH){\n";
                for (int j = 0; j < shortcut[nodes[i]].size(); j++)
                {
                    body += "        extend(" + to_string((unsigned)shortcut[nodes[i]][j]) + ");\n";
                }
                body += "        return;\n";
                body += "    }\n";
                body += "    next(" + to_string(nodes[i]->subnode.size()) + ");\n";
                body += "    switch(branch){\n";
                for (int j = 0; j < nodes[i]->subnode.size(); j++)
                {
                    body += "       case " + to_string(j) + ":\n";
                    body += "           func_" + to_string(reinterpret_cast<uintptr_t>(nodes[i]->subnode[j])) + "(depth+1);\n           break;\n";
                }
                body += "    }\n";
            }
            else if (nodes[i]->tp == Type::expression)
            {
                // body += "    if(depth>MAXDEPTH){\n";
                // for (int j = 0; j < shortcut[nodes[i]].size(); j++)
                // {
                //     body += "        extend(" + to_string((unsigned)shortcut[nodes[i]][j]) + ");\n";
                // }
                // body += "        return;\n";
                // body += "    }\n";
                for (int j = 0; j < nodes[i]->subnode.size(); j++)
                {
                    body += "    func_" + to_string(reinterpret_cast<uintptr_t>(nodes[i]->subnode[j])) + "(depth+1);\n";
                }
            }
            string end = "}\n";
            function = functionname + body + end;
            functions += function;
        }
        string main_func = "int main(){\n";
        main_func += "    srand((unsigned int)time(NULL));\n";
        main_func += "    seed = rand();\n";
        main_func += "    unsigned sum = 0;\n";
        main_func += "    clock_t start = clock();\n";
        main_func += "    for(unsigned i=0;;i++){\n";
        main_func += "        func_" + to_string(reinterpret_cast<uintptr_t>(this->start)) + "(0);\n";
        main_func += "        sum+=buffer.top;\n";
        main_func += "        if ((i&0xFFFF) == 0){\n";
        main_func += "            clock_t end = clock();\n";
        main_func += "            double duration = (double)(end-start)/CLOCKS_PER_SEC;\n";
        main_func += "            printf(\"Throughput Rate: \%.4f MB/s\\n\",(double)sum/duration/1024/1024);\n";
        if (show)
        {
            main_func += "            printbuff();\n";
        }
        main_func += "            start = end;\n";
        main_func += "            sum = 0;\n";
        main_func += "        }\n";
        main_func += "        clean();\n";
        main_func += "    }\n";
        main_func += "    return 0;\n";
        main_func += "}";
        code += functions;
        code += main_func;
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
    string compile = "clang " + outputFile + " -O2 " + "-o io.out";
    system(compile.c_str());
    string runcode = "./io.out";
    system(runcode.c_str());
    return 0;
}