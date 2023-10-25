// build me with: `clang main.cpp -I/usr/lib/llvm-10/include/ -lclang`
// Adapted from: https://gist.github.com/raphaelmor/3150866

#include <cstdio>

#include <clang-c/Index.h>
#include <clang-c/Platform.h>

void printDiagnostics(CXTranslationUnit translationUnit);

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

        printf("%s:%d:%d ", clang_getCString(filename), line, col);

        CXType return_type = clang_getCursorResultType(cursor);
        CXString return_spelling = clang_getTypeKindSpelling(return_type.kind);
        printf("%s %s(", clang_getCString(return_spelling), clang_getCString(cursor_spelling));

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

    unsigned int res = clang_visitChildren(root_cursor, *cursorVisitor, 0);

    clang_disposeTranslationUnit(translation_unit);
    clang_disposeIndex(index);

    return 0;
}