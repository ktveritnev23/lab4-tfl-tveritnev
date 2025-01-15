#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <stack>
#include <algorithm>
#include <sstream>


using namespace std;

struct regex_element
{
    int begin_position;
    string substr;
    vector<string> alternatives;
    vector<bool> kleenes;
    void output_alts() const
    {
        for (const string &st : alternatives)
            cout << st << '|';
        cout << endl;
    }

    void output_unlexed() const
    {
        cout << substr << endl;
    }
};

struct cgroup
{
    string group_regex;
    regex_element value;
    bool kleene_full;
};

struct group_reference
{
    int pos;
    int w;
};

class regex_lexer
{
public:
    string full_regex;

    const char alter = '|';
    const char kleene = '*';
    const string non_capture = "?:";
    const string lookahead = "?=";
    const char question_mark = '?';
    const char open = '(', close = ')';

    vector<cgroup> captured_groups;
    vector<regex_element> independent_elements;
    vector<group_reference> refs;
    vector<regex_element> lookaheads;

    int dig(char x) { return x - '0'; }

    regex_lexer(string raw_input)
    {
        full_regex = raw_input;
    }

    void split_groups_and_independent()
    {
        stack<int> openIndices;
        vector<pair<int, int>> matchingPairs;

        for (int i = 0; i < full_regex.length(); ++i)
        {
            char ch = full_regex[i];
            if (ch == open)
            {
                // Push the index of '(' onto the stack
                openIndices.push(i);
            }
            else if (ch == close)
            {
                if (!openIndices.empty())
                {
                    // Pop the top of the stack to get the matching '(' index
                    int openingIndex = openIndices.top();
                    openIndices.pop();
                    // Store the pair of indices
                    matchingPairs.emplace_back(openingIndex, i);
                }
                else
                {
                    std::cerr << "Error: Unmatched closing parenthesis at position " << i << std::endl;
                }
            }
        }
        sort(matchingPairs.begin(), matchingPairs.end(), [](auto &left, auto &right)
             { return left.first < right.first; });
        for (const auto &r : matchingPairs)
        {
            cout << full_regex.substr(r.first, r.second-r.first+1) << endl;
            // this should replace th
            if (r.second - r.first + 1 > 3 && full_regex.substr(r.first + 1, 2) == non_capture)
                independent_elements.push_back(
                    regex_element{r.first,
                                  full_regex.substr(r.first + 3, r.second - r.first - 3)});
            else if (r.second - r.first + 1 > 3 && full_regex.substr(r.first + 1, 2) == lookahead)
            {
                lookaheads.push_back(regex_element{r.first,
                                                   full_regex.substr(r.first + 3, r.second - r.first - 3)});
            }
            else if (r.second - r.first + 1 > 2 && full_regex[r.first + 1] == question_mark && isdigit(full_regex[r.first + 2])){
                refs.push_back(
                    group_reference{r.first,
                                    full_regex[r.first + 2] - '0'});
            }
            else
                captured_groups.push_back(cgroup{full_regex.substr(r.first, r.second - r.first + 1), regex_element{r.first}});
        }

        // finding_loose, refactored
        char cc;
        int cntr = 0;
        int beg = -1;
        int ends = -1;
        for (int i = 0; i < full_regex.size(); i++)
        {
            cc = full_regex[i];

            if (cc != open && cc != close)
            {
                if (cntr == 0 && beg == -1 && ends == -1)
                {
                    beg = i;
                    ends = i;
                }
                else if (cntr == 0 && beg != -1 && ends != -1)
                {
                    ends = i;
                }
                if (cntr != 0 && beg != -1 && ends != -1 && ends - beg > 0 || i == full_regex.size() - 1 && ends - beg > 0)
                {
                    // don't steal others' Kleene stars
                    if (full_regex[beg] == kleene)
                        independent_elements.push_back(regex_element{beg + 1, full_regex.substr(beg + 1, ends - beg)});
                    else
                        independent_elements.push_back(regex_element{beg, full_regex.substr(beg, ends - beg + 1)});
                    beg = -1;
                    ends = -1;
                }
            }
            cntr += (cc == open) - (cc == close);
        }
    }

    void lex_groups()
    {
        for (cgroup &cg : captured_groups)
        {
            if (cg.value.begin_position + cg.group_regex.size() < full_regex.size() && full_regex[cg.value.begin_position + cg.group_regex.size()] == kleene)
            {
                // cout << "Kleene captured group found!";
                cg.kleene_full = true;
            }
            else
                cg.kleene_full = false;
        }
    }

    bool check_terminals()
    {
        // alllowed characters:
        // a-z; 1-9; ?; =; :
        for (int i = 0; i < full_regex.size(); i++)
        {
            cout << full_regex[i];
            if ((!isalnum(full_regex[i]) || isupper(full_regex[i])) &&
                full_regex[i] != question_mark &&
                full_regex[i] != alter &&
                full_regex[i] != '=' &&
                full_regex[i] != ':' &&
                full_regex[i] != kleene &&
                full_regex[i] != open &&
                full_regex[i] != close)
            {
                return false;
            }
        }
        return true;
    }

    bool check_groups()
    {
        // basic sanity check
        for (group_reference &gr : refs)
        {
            //cout << gr.w;
            if (gr.w > captured_groups.size())
            {
                cout << "The captured group #" << gr.w << " has not been initialized yet!";
                return false;
            }
        }
        for (int i = 0; i < captured_groups.size(); i++)
        {
            int curr_pos = captured_groups[i].value.begin_position;
            // cout << curr_pos << " ";
            for (group_reference &gr : refs)
            {
                if (curr_pos < gr.pos && gr.pos < curr_pos + captured_groups[i].group_regex.size() - 1)
                {
                    /*if (gr.w == i + 1)
                        //cout << "Recursive reference defined right";
                    else*/
                    if (gr.w > i + 1)
                        return false;
                }
                if (gr.pos < curr_pos && gr.w >= i + 1)
                {
                    cout << "Wrong reference placement";
                    return false;
                }
            }
        }
        return true;
    }



    void lex_independent()
    {
        for (regex_element &re : independent_elements)
        {
            if (re.substr[re.substr.size() - 1] == kleene)
            {
                re.kleenes.push_back(true);
            }
            else
            {
                re.kleenes.push_back(false);
            }
        }
    }

    void lex_alternator()
    {
        // all the couts in there are debug ones
        for (regex_element &re : independent_elements)
        {
            if (re.substr.size() > 2)
            {
                stringstream splitter(re.substr);
                string segment;
                while (getline(splitter, segment, alter))
                {
                    re.alternatives.push_back(segment);
                }

                for (const string &alt : re.alternatives)
                {
                    if (alt[alt.length() - 1] == kleene)
                        re.kleenes.push_back(true);
                    else
                        re.kleenes.push_back(false);
                }
            }
            else
            {
                re.alternatives.push_back(re.substr);
            }
        }

        for (cgroup &cg : captured_groups)
        {
            if (cg.group_regex.size() > 2)
            {
                stringstream splitter(cg.group_regex.substr(1, cg.group_regex.size() - 2));
                string segment;
                while (getline(splitter, segment, alter))
                {
                    cg.value.alternatives.push_back(segment);
                }
                for (const string &alt : cg.value.alternatives)
                {
                    if (alt[alt.length() - 1] == kleene)
                        cg.value.kleenes.push_back(true);
                    else
                        cg.value.kleenes.push_back(false);
                }
            }
            else
            {
                cg.value.alternatives.push_back(cg.group_regex.substr(1, cg.group_regex.size() - 2));
            }
        }
    }

    bool check_lookaheads()
    {
        for (const regex_element &re : lookaheads)
        {
            vector<int> opening_positions;
            int pos = re.substr.find(open, 0);
            while (pos != string::npos)
            {
                opening_positions.push_back(pos);
                pos = re.substr.find(open, pos);
            }
            for (const int &p : opening_positions)
            {
                if (p < re.substr.size() - 1 && re.substr[p + 1] != '?')
                {
                    cout << "Only non-capturing groups are allowed!";
                    return false;
                }
                if (p < re.substr.size() - 2 && re.substr.substr(p + 1, 2) == lookahead)
                {
                    cout << "Lookaheads inside of lookaheads are not allowed!";
                    return false;
                }
            }
        }
        return true;
    }

    void print_unbloated()
    {
        cout << "Printing captured groups: " << endl;
        for (const cgroup &cg : captured_groups)
        {
            if (cg.group_regex != "")
            {
                cout << cg.group_regex;
                cg.value.output_unlexed();
            }
            // cg.value.output_alts();
        }
        cout << "Printing independent subregexes: " << endl;
    }

    void print_lexed_regex()
    {
        cout << "Printing lexed regular expression: " << endl;
        cout << "Printing captured groups: " << endl;
        for (const cgroup &cg : captured_groups)
        {
            if (cg.group_regex != "")
            {
                cout << cg.group_regex;
                cg.value.output_unlexed();
            }
            // cg.value.output_alts();
        }
        cout << "Printing independent subregexes: " << endl;
        for (const regex_element &re : independent_elements)
        {
            re.output_unlexed();
            // re.output_alts();
        }
        cout << "Printing references: " << endl;
        for (const group_reference &g : refs)
        {
            cout << g.pos << " " << g.w;
        }
        cout << endl
             << "Printing alternations: " << endl;
        // non-kleened
        for (const regex_element &re : independent_elements)
        {
            for (const string &alt : re.alternatives)
            {
                cout << alt << " ";
            }
            cout << endl;
        }
        cout << endl
             << "Printing lookaheads: " << endl;
        for (const regex_element &re : lookaheads)
        {
            cout << re.substr << endl;
        }
        // kleened
        cout << "With Kleene closures: ";
        for (const regex_element &re : independent_elements)
        {
            int i = 0;
            for (i = 0; i < re.alternatives.size(); i++)
            {
                if (!re.kleenes.empty())
                    cout << re.alternatives[i] << " ";
                else
                    cout << re.alternatives[i] << (re.kleenes[i] == true ? "* " : " ");
            }
            cout << endl;
        }
    }
};

struct production_rule
{
    string nonterm;
    string terms;
    production_rule(string n, string t) : nonterm(n), terms(t) {}
};

struct token_order
{
    int pos;
    int coord;
    int type;
    token_order(int p, int c, int t) : pos(p), coord(c), type(t) {}
    bool operator<(const token_order &r) const
    {
        return pos < r.pos;
    }
};

struct token
{
    string content;
    int type;
    int pos;
    token(string c, int t, int p) : content(c), type(t), pos(p) {}
};

class lab4_parser
{
private:
    regex_lexer *lex;
    vector<token> tokens;
    vector<production_rule> rules;
    // cfg grammar;

public:
    lab4_parser(string input)
    {
        lex = new regex_lexer(input);
    }

    void lex_regex()
    {
        lex->split_groups_and_independent();
        lex->lex_groups();
        lex->check_groups();
        lex->lex_alternator();
        //lex->print_lexed_regex();
        lex->print_unbloated();
        //cout << endl
        //     << "_________________________" << endl; 
    }

    void construct_parser()
    {
        // total number of nonterms
        int total_expressions = lex->captured_groups.size() + lex->independent_elements.size() + lex->refs.size();

        vector<token_order> tos;
        int i = 0;
        for (i = 0; i < lex->captured_groups.size(); i++)
        {
            tos.emplace_back(lex->captured_groups[i].value.begin_position, i, 0);
            // cout << lex->captured_groups[i].group_regex << " ";
        }
        for (i = 0; i < lex->independent_elements.size(); i++)
        {
            tos.emplace_back(lex->independent_elements[i].begin_position, i, 1);
            // cout << lex->independent_elements[i].substr << " ";
        }
        for (i = 0; i < lex->lookaheads.size(); i++)
        {
            tos.emplace_back(lex->lookaheads[i].begin_position, i, 2);
        }
        for (i = 0; i < lex->refs.size(); i++)
        {
            tos.emplace_back(lex->refs[i].pos, i, 3);
        }
        sort(tos.begin(), tos.end());

        for (const token_order &to : tos)
        {
            // VERY boilerplate type of code, but I can't do anything about it (at least for now)
            switch (to.type)
            {
            case 0:
                tokens.push_back(token{
                    lex->captured_groups[to.coord].group_regex, to.type, to.coord});
                break;
            case 1:
                tokens.push_back(token{
                    lex->independent_elements[to.coord].substr, to.type, to.coord});
                break;
            case 2:
                tokens.push_back(token{
                    lex->lookaheads[i].substr, to.type, to.coord});
                break;
            case 3:
                //cout << to.coord << endl;
                tokens.push_back(token{
                    to_string(/*lex->refs[i].w*/lex->refs[to.coord].w-1), to.type, to.coord});
                break;
            default:
                cout << "Что-то пошло не так..." << endl;
            }

 
        }
    }

    void tokens_to_production_rules()
    {
        
        string very_big_rule;
        int enumerators[4] = {0, 0, 0, 0};
        string nonterm_name;
        vector<string> cg_nonterms;
        for (const token &tok : tokens)
        {
            int temp_pos;
            int group_to_be_referenced;
            int ref_pos;
            switch (tok.type)
            {
            case 0:
                nonterm_name = "A" + to_string(enumerators[0]++);
                very_big_rule += nonterm_name;
     
                for (int i = 0; i < lex->captured_groups[tok.pos].value.alternatives.size(); i++)
                {
                    const string &alt = lex->captured_groups[tok.pos].value.alternatives[i];
                    bool const_kleenes = false;
                    if (i < lex->captured_groups[tok.pos].value.kleenes.size())
                        const_kleenes = lex->captured_groups[tok.pos].value.kleenes[i];
                    if (alt.size() >= 4 && alt[1] == lex->question_mark)
                    {
                        group_to_be_referenced = alt[2] - '0' - 1;
                       //cout << cg_nonterms.size() << "||" << group_to_be_referenced << endl;
                        if (cg_nonterms.size() <= group_to_be_referenced){
                            //cout << "The capturing group A has not been initialized yet!";
                            rules.emplace_back(nonterm_name, alt.substr(0,4));
                        }
                        else
                            rules.emplace_back(nonterm_name, cg_nonterms[group_to_be_referenced]);
                    }
                    else if (const_kleenes)
                    {
                        string kleene_nonterm = "A" + to_string(enumerators[0]++);
                        string latest_bfk = string{alt[alt.size() - 2]};
                        rules.emplace_back(nonterm_name, alt.substr(0, alt.size() - 2) + kleene_nonterm);
                        rules.emplace_back(kleene_nonterm, latest_bfk + kleene_nonterm);
                        rules.emplace_back(kleene_nonterm, "ε");
                    }
                    else
                        rules.emplace_back(nonterm_name, alt);

                    // rules.emplace_back(nonterm_name, alt);
                    //  rules.emplace_back(lex->captured_groups[tok.pos].group_regex, alt);
                }
                if (lex->captured_groups[tok.pos].kleene_full == true)
                    rules.emplace_back(nonterm_name, "ε");
                // rules.emplace_back(lex->captured_groups[tok.pos].group_regex, "ε");
                cg_nonterms.push_back(nonterm_name);
                break;
            case 1:

                nonterm_name = "B" + to_string(enumerators[1]++);
                very_big_rule += nonterm_name;

                for (int i = 0; i < lex->independent_elements[tok.pos].alternatives.size(); i++)
                {
                    bool const_kleenes = false;
                    const string& alt = lex->independent_elements[tok.pos].alternatives[i];
                    if (i < lex->independent_elements[tok.pos].kleenes.size())
                        const_kleenes = lex->independent_elements[tok.pos].kleenes[i];

                    if (alt.size() >= 4 &&
                        alt[1] == lex->question_mark)
                    {
                        group_to_be_referenced = alt[2] - '0' - 1;
                        if (cg_nonterms.size() <= group_to_be_referenced)
                            cout << "The capturing group has not been initialized yet!";
                        else
                            rules.emplace_back(nonterm_name, cg_nonterms[group_to_be_referenced]);
                    }
                    else if (const_kleenes)
                    {
                        string kleene_nonterm = "B" + to_string(enumerators[1]++);
                        string latest_bfk = string{alt[alt.size() - 2]};
                        rules.emplace_back(nonterm_name, alt.substr(0, alt.size() - 2) + kleene_nonterm);
                        rules.emplace_back(kleene_nonterm, latest_bfk + kleene_nonterm);
                        rules.emplace_back(kleene_nonterm, "ε");
                    }
                    else
                        rules.emplace_back(nonterm_name, alt);

                    // rules.emplace_back(nonterm_name, lex->independent_elements[tok.pos].alternatives[i]);
                    //  if (lex->independent_elements.size() > i && lex->independent_elements[tok.pos].kleenes[i] ==true)
                    //      rules.emplace_back(lex->independent_elements[tok.pos].substr, "ε");
                }
                break;
            case 2:
                nonterm_name = "C" + to_string(enumerators[2]++);
                very_big_rule += nonterm_name;
                for (const string &alt : lex->lookaheads[tok.pos].alternatives)
                {
                    if (alt.size() >= 4 && alt[1] == lex->question_mark)
                    {
                        group_to_be_referenced = alt[2] - '0' - 1;
                        if (cg_nonterms.size() <= group_to_be_referenced)
                            cout << "The capturing group has not been initialized yet!";
                        else
                            rules.emplace_back(nonterm_name, cg_nonterms[group_to_be_referenced]);
                    }
                    else
                        rules.emplace_back(nonterm_name, alt);
                }
                // rules.emplace_back(lex->lookaheads[tok.pos].substr, alt);
                break;
            case 3:

                nonterm_name = "D" + to_string(enumerators[3]++);
                // references, the 「easiest」
                temp_pos = stoi(tok.content);
                
                ref_pos = distance(lex->refs.begin(),find_if(lex->refs.begin(),lex->refs.end(),
                [&cp = temp_pos](const group_reference& r) -> bool {return cp == r.w;}));

                //cout << endl << temp_pos+1 << " "<< lex->refs[ref_pos-1].pos<<":{"<<lex->captured_groups[temp_pos].value.begin_position << "," << lex->captured_groups[temp_pos].value.begin_position +
                //   lex->captured_groups[temp_pos].group_regex.size() <<"}" << endl;
                if(ref_pos-1 < lex->captured_groups.size() && 
                   lex->refs[ref_pos-1].pos >= lex->captured_groups[temp_pos].value.begin_position &&
                   lex->refs[ref_pos-1].pos < lex->captured_groups[temp_pos].value.begin_position +
                   lex->captured_groups[temp_pos].group_regex.size()){
                        string to_find = "(?"+to_string(temp_pos+1)+")";
                        //cout << to_find;
                        for(production_rule& p : rules){
                            //cout << "ZZZ" << p.nonterm << "||" << cg_nonterms[temp_pos] << endl;
                            if(p.nonterm == cg_nonterms[temp_pos]){
                                //cout << p.terms;
                                int pos = p.terms.find(to_find,0);
                                while(pos != string::npos){
                                    p.terms.replace(pos, to_find.size(), p.nonterm);
                                    pos += p.terms.size();
                                    pos = p.terms.find(to_find,pos);
                                }  

                            }
                        } 
                   }
                else {
                    for (const string &alt : lex->captured_groups[temp_pos].value.alternatives)
                        rules.emplace_back(nonterm_name, alt);
                    very_big_rule += nonterm_name;
                }

                break;
            default:
                cout << "Что-то пошло не так..." << endl;
            }
        }
        rules.insert(rules.begin(), production_rule{"S", very_big_rule});
    }


    void print_tokens()
    {
        for (const token &str : tokens)
        {
            cout << str.content << endl;
        }
    }

    void print_production_rules()
    {
        for (const production_rule &pr : rules)
        {
            cout << pr.nonterm << " --> " << pr.terms << endl;
        }
    }

    ~lab4_parser()
    {
        delete lex;
    }
};


// Вариант 8
// 8 баллов: Парсер + контекстно-свободная грамматика
int main()
{
    vector<string> potential_patterns = {"(aa|bb)(?1)",
                                         "(a|(bb))(a|(?3))",
                                         "(a(?1)b|c)",
                                         "(aa|bb)cc(?:dd|ee)",
                                         "(aa|bb*)*cc|dd(?:ee*|ff*)"};
    string regex_string;

    for (int i = 0; i < potential_patterns.size(); i++)
    {
        cout << potential_patterns[i];
        cout << endl
             << "________________________________" << endl;
        lab4_parser ps(potential_patterns[i]);
        ps.lex_regex();
        ps.construct_parser();
        ps.tokens_to_production_rules();
        // ps.print_tokens();
        ps.print_production_rules();
    }

    cout << "Input regex: ";
    cin >> regex_string;
    lab4_parser regex_parser(regex_string);
    regex_parser.lex_regex();
    regex_parser.construct_parser();
    regex_parser.tokens_to_production_rules();
    // regex_parser.print_tokens();
    regex_parser.print_production_rules();

    return 0;
}
