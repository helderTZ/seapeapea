// build me with: `clang main.cpp -I/usr/lib/llvm-10/include/ -lclang`
// Adapted from: https://gist.github.com/raphaelmor/3150866

#include <cstdio>
#include <string>
#include <vector>

#include <clang-c/Index.h>
#include <clang-c/Platform.h>

struct Arg {
    std::string arg_name;
    std::string arg_type;
};

struct SourceLoc {
    std::string filename;
    unsigned int line;
    unsigned int col;
};

struct Function {
    SourceLoc source;
    std::string return_type;
    std::string function_name;
    std::vector<Arg> args;

    Function();

    Function(const char* filename_,
             unsigned int line_,
             unsigned int col_,
             const char* return_type_,
             const char* function_name_)
    {
        source = SourceLoc{filename_, line_, col_};
        return_type = return_type_;
        function_name = function_name_;
        args = std::vector<Arg>();
    }

    void add_arg(const char* arg_name, const char* arg_type) {
        args.push_back(Arg{arg_name, arg_type});
    }
};

typedef std::vector<Function> FunctionVec;

template<typename T>
typename T::value_type* last(T* container) {
    return &*std::prev((*container).end());
}

void printDiagnostics(CXTranslationUnit translationUnit);
CXChildVisitResult cursorVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data);
CXChildVisitResult functionDeclVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data);
void printFunctions(FunctionVec& functions);

void usage(char** argv) {
    printf("USAGE: %s [srcfiles] [compiler flags]\n", argv[0]);
}

void printDiagnostics(CXTranslationUnit translation_unit) {
    int num_diag = clang_getNumDiagnostics(translation_unit);
    printf("There are %d diagnostics\n", num_diag);

    for (unsigned int diag_idx = 0; diag_idx < num_diag; ++diag_idx) {
        CXDiagnostic diagnostic = clang_getDiagnostic(translation_unit, diag_idx);
        CXString err_str = clang_formatDiagnostic(diagnostic, clang_defaultDiagnosticDisplayOptions());
        printf("%s\n", clang_getCString(err_str));
        clang_disposeString(err_str);
    }
}

void printFunctions(FunctionVec& functions) {
    for (auto& fn : functions) {
        printf("%s:%d:%d: %s :: %s(",
            fn.source.filename.c_str(),
            fn.source.line,
            fn.source.col,
            fn.function_name.c_str(),
            fn.return_type.c_str());
        if (fn.args.size() > 0) {
            printf("%s",
                fn.args[0].arg_type.c_str());
                // fn.args[0].arg_name.c_str());
            for (int i = 1; i < fn.args.size(); ++i) {
                printf(", %s",
                    fn.args[i].arg_type.c_str());
                    // fn.args[i].arg_name.c_str());
            }
        }
        printf(")\n");
    }
}

CXChildVisitResult cursorVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    CXCursorKind cursor_kind = clang_getCursorKind(cursor);
    CXString cursor_spelling = clang_getCursorSpelling(cursor);

    if (cursor_kind == CXCursor_FunctionDecl)
    {
        CXSourceLocation location = clang_getCursorLocation(cursor);
        CXString filename;
        unsigned int line, col;
        clang_getPresumedLocation(location, &filename, &line, &col);

        // skip included headers
        if (clang_Location_isFromMainFile(clang_getCursorLocation(cursor)) == 0) {
            return CXChildVisit_Continue;
        }

        CXType return_type = clang_getCursorResultType(cursor);
        CXString return_spelling = clang_getTypeKindSpelling(return_type.kind);

        ((FunctionVec*)client_data)->push_back(Function{
            clang_getCString(filename), line, col,
            clang_getCString(return_spelling),
            clang_getCString(cursor_spelling)
        });

        clang_visitChildren(cursor, *functionDeclVisitor, client_data);

        return CXChildVisit_Continue;
    }

    return CXChildVisit_Recurse;
}

CXChildVisitResult functionDeclVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    CXCursorKind kind = clang_getCursorKind(cursor);
    CXType type = clang_getCursorType(cursor);

    if (kind == CXCursor_ParmDecl) {
        CXString param_name = clang_getCursorSpelling(cursor);
        CXString param_type = clang_getTypeSpelling(type);

        Function* fn = last((FunctionVec*)client_data);
        fn->add_arg(clang_getCString(param_name), clang_getCString(param_type));
    }

    return CXChildVisit_Continue;
}

int main(int argc, char** argv) {

    if (argc < 2) {
        usage(argv);
        return 0;
    }

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
        index,
        argv[1],
        &(argv[2]),
        argc-2,
        NULL,
        0,
        CXTranslationUnit_None);
    if (translation_unit == 0) {
        fprintf(stderr, "ERROR: clang_parseTranslationUnit() failed\n");
    }

    printDiagnostics(translation_unit);

    CXCursor root_cursor = clang_getTranslationUnitCursor(translation_unit);

    FunctionVec functions;
    unsigned int res = clang_visitChildren(root_cursor, *cursorVisitor, (CXClientData*)&functions);
    printFunctions(functions);

    clang_disposeTranslationUnit(translation_unit);
    clang_disposeIndex(index);

    return 0;
}