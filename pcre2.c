/* gcc -o pcre2 pcre2.c -lpcre2-8 */
#include <stdio.h>
#include <string.h>

//muste be set before including "pcre2.h"
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

// re_ismatch() checks if subject string matches pattern.
// Returns:
//  -1 if the pattern is invalid
//  0 if there is no match
//  1 if there is a match
int re_ismatch(char *pattern, const char *subject, int ignore_case) {
  int result = -1;
  int rc;
  pcre2_code *re = NULL;
  pcre2_match_data *match_data = NULL;

  int error;
  PCRE2_SIZE error_offset = 0;

  re = pcre2_compile(
      (PCRE2_SPTR)pattern,
      PCRE2_ZERO_TERMINATED,
      PCRE2_NO_UTF_CHECK | (ignore_case ? PCRE2_CASELESS : 0),
      &error,
      &error_offset,
      NULL);

  // Check for errors
  if (!re) return -1;

  // Check if we have a match.
  match_data = pcre2_match_data_create_from_pattern(re, NULL);
  rc = pcre2_match(re, (PCRE2_SPTR)subject, strlen(subject), 0, 0, match_data, NULL);
  if (rc >= 0) {
    result = 1;
  } else if (rc == PCRE2_ERROR_NOMATCH) {
    result = 0;
  }

  // Make sure we clean up.
  pcre2_match_data_free(match_data);
  pcre2_code_free(re);

  return result;
}

//re_match() checks if subject string matches pattern.
int re_match(char *pattern, char *subject, char **matches[]) {
  pcre2_code *re = NULL;
  int rc;
  PCRE2_SIZE *ovector;
  pcre2_match_data *match_data;

  int error;
  PCRE2_SIZE error_offset = 0;

  re = pcre2_compile(
      (PCRE2_SPTR)pattern,
      PCRE2_ZERO_TERMINATED,
      PCRE2_NO_UTF_CHECK,
      &error,
      &error_offset,
      NULL);

  // Check for an error compiling the regexp.
  if (!re) return -1;

  PCRE2_SPTR pcre2_subject = (PCRE2_SPTR)subject;
  match_data = pcre2_match_data_create_from_pattern(re, NULL);
  rc = pcre2_match(
              re,
              pcre2_subject,
              strlen(subject),
              0,
              0,
              match_data,
              NULL);

  if (rc > 1) {
    *matches = malloc(rc * sizeof(char*));
    ovector = pcre2_get_ovector_pointer(match_data);

    for (int i = 0; i < rc; i++) {
      PCRE2_SPTR substring_start = pcre2_subject + ovector[2*i];
      size_t substring_length = ovector[2*i+1] - ovector[2*i];
      char *substring_copy = malloc(substring_length + 1);
      strncpy(substring_copy, (char *)substring_start, substring_length);
      substring_copy[substring_length] = 0;
      (*matches)[i] = substring_copy;
    }
  }

  pcre2_match_data_free(match_data);
  pcre2_code_free(re);
  return rc;
}

void free_matches(int matches_count, char **matches[])
{
  for (int i = 0; i < matches_count; i++)
  {
    free((*matches)[i]);
  }
  free(*matches);
}

/**
 * re_replace() replace matches of regular expression with replacement in the subject string.
 *   re: regular expression
 *   replacement: replacement string
 *   subject: source string
 *   result: buffer to write the result string to
 *   result_len length of the buffer
 * return 0 on success, -1 on error, upon error result will the contain the error message
 */
int re_replace(const char *re, char *replacement, char *subject, char *result, size_t result_len, int all) {
  int rc, errorcode, res = 0;
  PCRE2_SIZE erroroffset;
  PCRE2_UCHAR reg_error[4096] = {0};

  PCRE2_SPTR pcre2_re = (PCRE2_SPTR) re;
  pcre2_code *compiled_re = pcre2_compile(pcre2_re, PCRE2_ZERO_TERMINATED, PCRE2_NO_UTF_CHECK, &errorcode, &erroroffset, NULL);

  if (compiled_re) {
    rc = pcre2_substitute(
        compiled_re,
        (PCRE2_SPTR) subject,
        PCRE2_ZERO_TERMINATED,
        0,
        PCRE2_SUBSTITUTE_EXTENDED | (all ? PCRE2_SUBSTITUTE_GLOBAL : 0),
        NULL,
        NULL,
        (PCRE2_SPTR) replacement,
        PCRE2_ZERO_TERMINATED,
        (PCRE2_UCHAR *) result,
        &result_len
    );

    if (rc < 0) {
      // Syntax error in the replacement string
      pcre2_get_error_message(rc, reg_error, sizeof(reg_error));
      sprintf(result, "Error during replace: '%s'.", reg_error);
      res = -1;
    }
    pcre2_code_free(compiled_re);
  } else {
    // Syntax error in the regular expression at erroroffset
    pcre2_get_error_message(errorcode, reg_error, sizeof(reg_error));
    sprintf(result, "Error compiling regexp at offset #%ld: '%s'.", erroroffset, reg_error);
    res = -1;
  }

  return res;
}

int re_group_bynumber(const char *pattern, const char *subject) {
  int rc, errorcode, res = 0;
  PCRE2_SIZE erroroffset;
  PCRE2_UCHAR reg_error[4096] = {0};

  pcre2_code *regex = pcre2_compile((PCRE2_SPTR)pattern,
                                     PCRE2_ZERO_TERMINATED,
                                     PCRE2_NO_UTF_CHECK,
                                     &errorcode,
                                     &erroroffset,
                                     NULL);
  /* Compilation failed: print the error message and return failure. */
  if (regex == NULL) {
      PCRE2_UCHAR buffer[256];
      pcre2_get_error_message(errorcode, buffer, sizeof(buffer));
      printf("PCRE2 compilation failed at offset %d: %s\n", (int)erroroffset, buffer);
      return -1;
  }

  pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(regex, NULL);

  rc = pcre2_match(
        regex,                /* the compiled pattern */
        (PCRE2_SPTR8)subject,  /* the subject string */
        strlen(subject),       /* the length of the subject */
        0,                    /* start at offset 0 in the subject */
        0,                    /* default options */
        match_data,           /* block for storing the result */
        NULL);                /* use default match context */

  if (rc < 0) {
      pcre2_code_free(regex);
      return 1;
  }

  PCRE2_UCHAR *substr_buf = NULL;
  PCRE2_SIZE substr_buf_len = 0;

  int rc1 = pcre2_substring_get_bynumber(match_data, 1, &substr_buf, &substr_buf_len);
  if (rc1 == 0) {
    printf("group1=[%.*s]\n", (int)substr_buf_len, substr_buf);
  }

  int rc2 = pcre2_substring_get_bynumber(match_data, 2, &substr_buf, &substr_buf_len);
  if (rc2 == 0) {
    printf("group2=[%.*s]\n", (int)substr_buf_len, substr_buf);
  }

  pcre2_match_data_free(match_data);   /* Release memory used for the match */
  pcre2_substring_free(substr_buf);
  pcre2_code_free(regex);
  return 0;
}

int re_group_byname(const char *pattern, const char *subject) {
  int rc, errorcode, res = 0;
  PCRE2_SIZE erroroffset;
  PCRE2_UCHAR reg_error[4096] = {0};

  pcre2_code *regex = pcre2_compile((PCRE2_SPTR)pattern,
                                          PCRE2_ZERO_TERMINATED,
                                          PCRE2_NO_UTF_CHECK,
                                          &errorcode,
                                          &erroroffset,
                                          NULL);
  /* Compilation failed: print the error message and return failure. */
  if (regex == NULL) {
      PCRE2_UCHAR buffer[256];
      pcre2_get_error_message(errorcode, buffer, sizeof(buffer));
      printf("PCRE2 compilation failed at offset %d: %s\n", (int)erroroffset, buffer);
      return -1;
  }

  pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(regex, NULL);

  rc = pcre2_match(
        regex,                /* the compiled pattern */
        (PCRE2_SPTR8)subject,  /* the subject string */
        strlen(subject),       /* the length of the subject */
        0,                    /* start at offset 0 in the subject */
        0,                    /* default options */
        match_data,           /* block for storing the result */
        NULL);                /* use default match context */

  if (rc < 0) {
      pcre2_code_free(regex);
      return 1;
  }

  PCRE2_UCHAR *substr_buf = NULL;
  PCRE2_SIZE substr_buf_len = 0;

  int rc1 = pcre2_substring_get_byname(match_data, (PCRE2_SPTR)"name", &substr_buf, &substr_buf_len);
  if (rc1 == 0) {
    printf("group1=[%.*s]\n", (int)substr_buf_len, substr_buf);
  }

  int rc2 = pcre2_substring_get_byname(match_data, (PCRE2_SPTR)"help", &substr_buf, &substr_buf_len);
  if (rc2 == 0) {
    printf("group2=[%.*s]\n", (int)substr_buf_len, substr_buf);
  }

  pcre2_match_data_free(match_data);   /* Release memory used for the match */
  pcre2_code_free(regex);

  return 0;
}

int main(int argc, char **argv) {
  //is match?
  printf("===========test is match===========\n");
  //if (re_ismatch("\\d+", "Hello number 10", 1)) {
  //if (re_ismatch("NUMBER", "Hello number 10", 0)) {
  if (re_ismatch("NUMBER", "Hello number 10", 1)) {
    printf("Matched\n\n");
  }

  //match
  printf("===========test match===========\n");
  char **matches = NULL;
  int match_count = re_match("^(di|ke|se)(\\w+)$", "disable", &matches);

  if(match_count == 3) {
    printf("matches[1]=%s\n", matches[1]);
    printf("matches[2]=%s\n", matches[2]);
  }

  free_matches(match_count, &matches);
  printf("\n");

  //replace
  printf("===========test replace===========\n");
  char result[1024] = { 0 };
  //re_replace("\\d+", "AA", "Hello world, 12, 34, 56", result, sizeof(result), 0);
  re_replace("\\d+", "AA", "Hello world, 12, 34, 56", result, sizeof(result), 1);
  printf("result=[%s]\n\n", result);


  printf("=====test group by number=====\n");
  re_group_bynumber("^([\\da-zA-Z_/-]+):.*?## (.*)$", "build:  ## compile this software");
  printf("\n=====test group by name=====\n");
  re_group_bynumber("^(?P<name>[\\da-zA-Z_/-]+):.*?## (?P<help>.*)$", "build:  ## compile this software");

  return 0;
}
