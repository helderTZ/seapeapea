// build me with: `clang main.cpp -I/usr/lib/llvm-10/include/ -lclang`
// Adapted from: https://gist.github.com/raphaelmor/3150866

#include <cstdio>

#include <clang-c/Index.h>
#include <clang-c/Platform.h>

void printDiagnostics(CXTranslationUnit translationUnit);
void printTokenInfo(CXTranslationUnit translationUnit,CXToken currentToken);
void printCursorTokens(CXTranslationUnit translationUnit,CXCursor currentCursor);

CXChildVisitResult cursorVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data);
CXChildVisitResult functionDeclVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data);

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

void printTokenInfo(CXTranslationUnit translation_unit, CXToken token) {
    CXString token_str = clang_getTokenSpelling(translation_unit, token);
    CXTokenKind token_kind = clang_getTokenKind(token);

    switch(token_kind) {
        case CXToken_Comment:
            printf("Token COMMENT     : %s\n", clang_getCString(token_str));
            break;
        case CXToken_Identifier:
            printf("Token IDENTIFIER  : %s\n", clang_getCString(token_str));
            break;
        case CXToken_Keyword:
            printf("Token KEYWORD     : %s\n", clang_getCString(token_str));
            break;
        case CXToken_Literal:
            printf("Token LITERAL     : %s\n", clang_getCString(token_str));
            break;
        case CXToken_Punctuation:
            printf("Token PUNCTUATION : %s\n", clang_getCString(token_str));
            break;
        default:
            fprintf(stderr, "ERROR: Could not parse token: %s\n", clang_getCString(token_str));
            break;
    }
}

void printCursorTokens(CXTranslationUnit translation_unit, CXCursor cursor) {
    unsigned int num_tokens;
    CXToken* tokens;

    CXSourceRange source_range = clang_getCursorExtent(cursor);
    clang_tokenize(translation_unit, source_range, &tokens, &num_tokens);

    for (int i = 0; i < num_tokens; ++i) {
        printTokenInfo(translation_unit, tokens[i]);
    }

    clang_disposeTokens(translation_unit, tokens, num_tokens);
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
        printf("%s:%d:%d ", clang_getCString(filename), line, col);

        printf("%s(", clang_getCString(cursor_spelling));

        int num_params = 0;
        clang_visitChildren(cursor, *functionDeclVisitor, &num_params);

        printf(")\n");

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
        printf("\%s: %s, ", clang_getCString(param_name), clang_getCString(param_type));
        int *num_params = (int*)client_data;
        (*num_params)++;
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
    // printCursorTokens(translation_unit, root_cursor);

    unsigned int res = clang_visitChildren(root_cursor, *cursorVisitor, 0);

    clang_disposeTranslationUnit(translation_unit);
    clang_disposeIndex(index);

    return 0;
}