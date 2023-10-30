#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <string_view>
#include <algorithm>

#include <clang-c/Index.h>

#define STB_C_LEXER_IMPLEMENTATION
#include "stb_c_lexer.h"

struct Arg {
    std::string arg_name;
    std::string arg_type;
};

struct SourceLoc {
    std::string filename;
    unsigned int line;
    unsigned int col;

    SourceLoc() = default;

    SourceLoc(const char* filename_,
              unsigned int line_,
              unsigned int col_)
    : filename(filename_), line(line_), col(col_) {}

    std::string repr() const {
        return filename + ":" + std::to_string(line) + ":" + std::to_string(col) + ":";
    }
};

struct Function {
    SourceLoc source;
    std::string return_type;
    std::string function_name;
    std::vector<Arg> args;

    Function() = default;

    Function(const char* filename_,
             unsigned int line_,
             unsigned int col_,
             const char* return_type_,
             const char* function_name_)
    :   source(filename_, line_, col_),
        return_type(return_type_),
        function_name(function_name_) {}

    void add_arg(const char* arg_name, const char* arg_type) {
        args.push_back(Arg{arg_name, arg_type});
    }

    std::string repr() const {
        return function_name + " :: " + normal();
    }

    std::string full_repr() const {
        return source.repr() + " " + repr();
    }

    std::string normal() const {
        std::string representation = return_type + " ( ";
        for(int i = 0; i < args.size(); ++i) {
            if (i > 0) representation += " , ";
            representation += args[i].arg_type;
        }
        representation += " ) ";
        return representation;
    }
};

struct Typedef {
    SourceLoc source;
    std::string alias;
    std::string aliased;

    Typedef() = default;

    Typedef(const char* filename_,
            unsigned int line_,
            unsigned int col_,
            const char* alias_,
            const char* aliased_)
    :   source(filename_, line_, col_),
        alias(alias_), aliased(aliased_) {}

    std::string repr() const {
        return alias + " :: " + aliased;
    }

    std::string normal() const {
        return alias;
    }
};

struct Attribute {
    std::string attr_name;
    std::string attr_type;
};

struct Struct {
    SourceLoc source;
    std::string struct_name;
    std::vector<Attribute> attributes;

    Struct() = default;

    Struct(const char* filename_,
           unsigned int line_,
           unsigned int col_,
           const char* struct_name_)
    :   source(filename_, line_, col_),
        struct_name(struct_name_) {}

    void add_attr(const char* attr_name, const char* attr_type) {
        attributes.push_back(Attribute{attr_name, attr_type});
    }

    std::string repr() const {
        std::string representation = struct_name + " { ";
        for(int i = 0; i < attributes.size(); ++i) {
            if (i > 0) representation += ", ";
            representation += attributes[i].attr_name + " :: " + attributes[i].attr_type;
        }
        representation += " }";
        return representation;
    }

    std::string normal() const {
        return struct_name;
    }
};

struct Class {
    SourceLoc source;
    std::string class_name;
    std::vector<Attribute> attributes;
    std::vector<Function> methods;

    Class() = default;

    Class(const char* filename_,
           unsigned int line_,
           unsigned int col_,
           const char* class_name_)
    :   source(filename_, line_, col_),
        class_name(class_name_) {}

    void add_attr(const char* attr_name, const char* attr_type) {
        attributes.push_back(Attribute{attr_name, attr_type});
    }

    void add_method(
        const char* filename_, unsigned int line_, unsigned int col_,
        std::string return_type_, std::string method_name_)
    {
        methods.push_back(Function{filename_, line_, col_, return_type_.c_str(), method_name_.c_str()});
    }

    std::string repr() const {
        std::string representation = class_name + " { ";
        for(int i = 0; i < attributes.size(); ++i) {
            if (i > 0) representation += ", ";
            representation += attributes[i].attr_name + " :: " + attributes[i].attr_type;
        }
        for(int i = 0; i < methods.size(); ++i) {
            if (i > 0) representation += ", ";
            representation += methods[i].repr();
        }
        representation += " }";
        return representation;
    }

    std::string normal() const {
        return class_name;
    }
};

struct Score {
    std::string id;
    int score;
};

typedef std::vector<Function> FunctionVec;
typedef std::vector<Typedef> TypedefVec;
typedef std::vector<Struct> StructVec;
typedef std::vector<Class> ClassVec;
typedef std::vector<Score> ScoreVec;
typedef std::vector<std::string> TokenVec;

struct EntityAggregate {
    FunctionVec functions;
    TypedefVec typedefs;
    StructVec structs;
    ClassVec classes;
};

template<typename T>
typename T::value_type* last(T* container) {
    return &*std::prev((*container).end());
}

CXChildVisitResult cursorVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data);
CXChildVisitResult functionDeclVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data);
CXChildVisitResult attributeDeclVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data);

void usage(char** argv) {
    printf("USAGE: %s <srcfile> [-f|-t|-s|-c|-p] [query]\n", argv[0]);
    printf("            srcfile : source or header file to search in\n");
    printf("            -f      : search for functions\n");
    printf("            -t      : search for typedefs\n");
    printf("            -s      : search for structs\n");
    printf("            -c      : search for classes\n");
    printf("            -p      : don't query, just print everything\n");
    printf("            query   : the query to search for\n");
    printf("If no query is provided, just print\n");
}

template<typename T>
void printCX(const T& ts, const char* header="") {
    printf("==================================\n");
    printf("              %s           \n", header);
    printf("==================================\n");
    for (auto& t : ts) {
        printf("%s %s\n", t.source.repr().c_str(), t.repr().c_str());
    }
    printf("\n");
}

CXChildVisitResult cursorVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    // skip included headers
    if (clang_Location_isFromMainFile(clang_getCursorLocation(cursor)) == 0) {
        return CXChildVisit_Continue;
    }

    CXCursorKind cursor_kind = clang_getCursorKind(cursor);
    CXString cursor_spelling = clang_getCursorSpelling(cursor);

    CXSourceLocation location = clang_getCursorLocation(cursor);
    CXString filename;
    unsigned int line, col;
    clang_getPresumedLocation(location, &filename, &line, &col);

    if (cursor_kind == CXCursor_FunctionDecl) {
        CXType return_type = clang_getCursorResultType(cursor);
        CXString return_spelling = clang_getTypeSpelling(return_type);

        ((EntityAggregate*)client_data)->functions.push_back(Function{
            clang_getCString(filename), line, col,
            clang_getCString(return_spelling),
            clang_getCString(cursor_spelling)
        });

        clang_visitChildren(cursor, *functionDeclVisitor, client_data);

        return CXChildVisit_Continue;
    }
    else if (cursor_kind == CXCursor_TypedefDecl) {
        CXString typedef_name = clang_getTypedefName(clang_getCursorType(cursor));
        CXString typedef_type = clang_getTypeSpelling(clang_getTypedefDeclUnderlyingType(cursor));

        ((EntityAggregate*)client_data)->typedefs.push_back(Typedef{
            clang_getCString(filename), line, col,
            clang_getCString(typedef_name),
            clang_getCString(typedef_type)
        });

        return CXChildVisit_Continue;
    }
    else if (cursor_kind == CXCursor_StructDecl) {
        CXString struct_name = clang_getCursorSpelling(cursor);

        ((EntityAggregate*)client_data)->structs.push_back(Struct {
            clang_getCString(filename), line, col,
            clang_getCString(struct_name)
        });

        clang_visitChildren(cursor, *attributeDeclVisitor, client_data);
    }
    else if (cursor_kind == CXCursor_ClassDecl) {
        CXString class_name = clang_getCursorSpelling(cursor);

        ((EntityAggregate*)client_data)->classes.push_back(Class {
            clang_getCString(filename), line, col,
            clang_getCString(class_name)
        });

        clang_visitChildren(cursor, *attributeDeclVisitor, client_data);
        clang_visitChildren(cursor, *functionDeclVisitor, client_data);
    }

    return CXChildVisit_Recurse;
}

CXChildVisitResult functionDeclVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    CXCursorKind kind = clang_getCursorKind(cursor);
    CXType type = clang_getCursorType(cursor);

    if (kind == CXCursor_ParmDecl) {
        CXString param_name = clang_getCursorSpelling(cursor);
        CXString param_type = clang_getTypeSpelling(type);

        Function* fn = last(&((EntityAggregate*)client_data)->functions);
        fn->add_arg(clang_getCString(param_name), clang_getCString(param_type));
    }

    return CXChildVisit_Continue;
}

CXChildVisitResult attributeDeclVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    CXCursorKind kind = clang_getCursorKind(cursor);
    CXType type = clang_getCursorType(cursor);

    if (kind == CXCursor_FieldDecl) {
        CXString attr_name = clang_getCursorSpelling(cursor);
        CXString attr_type = clang_getTypeSpelling(type);

        Struct* fn = last(&((EntityAggregate*)client_data)->structs);
        fn->add_attr(clang_getCString(attr_name), clang_getCString(attr_type));
    }

    return CXChildVisit_Continue;
}

template<typename... Ts>
auto min(Ts... ts) {
    return std::min({std::forward<Ts>(ts)...});
}

/** Calculate the Levenshtein distance */
int lev(std::string_view a, std::string_view b) {
    int n = a.size();
    int m = b.size();
    int distance[n+1][m+1];
    std::memset(distance, 0, sizeof(int)*(n+1)*(m+1));

    // initialize first row and first column
    for (int i = 0; i < n+1; ++i) { distance[i][0] = i; }
    for (int j = 0; j < m+1; ++j) { distance[0][j] = j; }

    // calculate the rest of the matrix
    for (int i = 1; i < n+1; ++i) {
        for (int j = 1; j < m+1; ++j) {
            if (a[i-1] == b[j-1]) {
                distance[i][j] = distance[i-1][j-1];
            } else {
                distance[i][j] = min(distance[i  ][j-1],
                                     distance[i-1][j  ],
                                     distance[i-1][j-1]) + 1;
            }
        }
    }

    // answer is the last element
    return distance[n][m];
}

ScoreVec getScores(const FunctionVec& functions, const std::string& query) {
    ScoreVec scores;
    scores.reserve(functions.size());
    for(auto& fn : functions) {
        scores.push_back({ fn.full_repr(), lev(fn.normal(), query) });
    }
    return scores;
}

template<typename T>
ScoreVec getScores(const T& ts, const std::string& query) {
    ScoreVec scores;
    scores.reserve(ts.size());
    for(auto& t : ts) {
        scores.push_back({ t.repr(), lev(t.normal(), query) });
    }
    return scores;
}

std::string bestMatch(const ScoreVec& scores) {
    return (*std::min_element(scores.begin(), scores.end(),
                [](auto& a, auto& b){ return a.score < b.score; })
            ).id;
}

void sortScores(ScoreVec& scores) {
    std::sort(scores.begin(), scores.end(),
        [](auto& a, auto& b){ return a.score < b.score; });
}

TokenVec tokenizeQuery(std::string& query) {
    TokenVec tokens;

    stb_lexer lexer;
    char string_storage[1024];
    // void stb_c_lexer_init(stb_lexer *lexer,
    //                       const char *input_stream,
    //                       const char *input_stream_end,
    //                       char *string_store,
    //                       int store_length);
    stb_c_lexer_init(&lexer, query.c_str(), query.c_str() + query.size(), string_storage, 1024);

    while (stb_c_lexer_get_token(&lexer)) {
        if (lexer.token < CLEX_eof) {
            tokens.push_back(std::string(1, (char)lexer.token));
        }
        else if (lexer.token == CLEX_id) {
            tokens.push_back(std::string(lexer.string));
        }
    }

    return tokens;
}

std::string normalizeQuery(TokenVec&& tokens) {
    std::string normalized_query;
    for (auto& tok : tokens) {
        normalized_query += tok + " ";
    }
    return normalized_query;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        usage(argv);
        return 0;
    }

    std::string filename(argv[1]);
    std::string mode(argv[2]);
    std::string query(argv[3]);

    CXIndex index = clang_createIndex(0, 0);
    if (index == 0) {
        fprintf(stderr, "ERROR: clang_createIndex() failed\n");
        return 1;
    }

    // clang_parseTranslationUnit(CXIndex CIdx,
    //                        const char *source_filename,
    //                        const char *const *command_line_args,
    //                        int num_command_line_args,
    //                        struct CXUnsavedFile *unsaved_files,
    //                        unsigned num_unsaved_files,
    //                        unsigned options);
    CXTranslationUnit translation_unit = clang_parseTranslationUnit(
        index, filename.c_str(), NULL, 0, NULL, 0, CXTranslationUnit_None);
    if (translation_unit == 0) {
        fprintf(stderr, "ERROR: clang_parseTranslationUnit() failed\n");
    }

    CXCursor root_cursor = clang_getTranslationUnitCursor(translation_unit);

    EntityAggregate entities;
    unsigned int res = clang_visitChildren(root_cursor, *cursorVisitor, (CXClientData*)&entities);

    if (mode == "-p") {
        printCX(entities.functions, "FUNCTIONS");
        printCX(entities.typedefs, "TYPEDEFS");
        printCX(entities.structs, "STRUCTS");
        printCX(entities.classes, "CLASSES");
    }
    else {
        std::string normalized_query = normalizeQuery(tokenizeQuery(query));

        ScoreVec scores;
        if (mode == "-f")      { scores = getScores(entities.functions, normalized_query); }
        else if (mode == "-t") { scores = getScores(entities.typedefs, normalized_query); }
        else if (mode == "-s") { scores = getScores(entities.structs, normalized_query); }
        else if (mode == "-c") { scores = getScores(entities.classes, normalized_query); }

        sortScores(scores);
        printf("======== Best matches ========\n");
        for (size_t i = 0; i < std::min((size_t)10, scores.size()); ++i) {
            printf("%s\n", scores[i].id.c_str());
        }
        // printf("%s\n", bestMatch(scores).c_str());
    }

    clang_disposeTranslationUnit(translation_unit);
    clang_disposeIndex(index);

    return 0;
}