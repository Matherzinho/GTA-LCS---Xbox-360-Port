#pragma once
#include <stddef.h>
// Lexical Xbox path normalization; does not require the destination to exist.
int lcs_xsdk_normalize_path(const char *path,char *out,size_t capacity);
const char *_psGetUserFilesFolder();
