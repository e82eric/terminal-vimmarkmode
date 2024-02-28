#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable:4100)
#include "fzf.h"

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <icu.h>
// TODO(conni2461): UNICODE HEADER
#define UNICODE_MAXASCII 0x7f

#define SFREE(x)                                                               \
  if (x) {                                                                     \
    free(x);                                                                   \
  }

/* Helpers */
#define free_alloc(obj)                                                        \
  if ((obj).allocated) {                                                       \
    free((obj).data);                                                          \
  }

#define gen_simple_slice(name, type)                                           \
  typedef struct {                                                             \
    type *data;                                                                \
    size_t size;                                                               \
  } name##_slice_t;                                                            \
  static name##_slice_t slice_##name(type *input, size_t from, size_t to) {    \
    return (name##_slice_t){.data = input + from, .size = to - from};          \
  }

#define gen_slice(name, type)                                                  \
  gen_simple_slice(name, type);                                                \
  static name##_slice_t slice_##name##_right(type *input, size_t to) {         \
    return slice_##name(input, 0, to);                                         \
  }

gen_slice(i16, int16_t);
gen_simple_slice(i32, int32_t);
gen_slice(str, const char);
gen_slice(ustr, const UChar);
#undef gen_slice
#undef gen_simple_slice

/* TODO(conni2461): additional types (utf8) */
typedef int32_t char_class;
typedef char byte;

typedef enum {
  ScoreMatch = 16,
  ScoreGapStart = -3,
  ScoreGapExtention = -1,
  BonusBoundary = ScoreMatch / 2,
  BonusNonWord = ScoreMatch / 2,
  BonusCamel123 = BonusBoundary + ScoreGapExtention,
  BonusConsecutive = -(ScoreGapStart + ScoreGapExtention),
  BonusFirstCharMultiplier = 2,
} score_t;

typedef enum {
  CharNonWord = 0,
  CharLower,
  CharUpper,
  CharLetter,
  CharNumber
} char_types;

static int32_t uindex_byte(ufzf_string_t *string, UChar b) {
    for (size_t i = 0; i < string->size; i++) {
        if (string->data[i] == b) {
            return (int32_t)i;
        }
    }
    return -1;
}

static int32_t index_byte(fzf_string_t *string, char b) {
  for (size_t i = 0; i < string->size; i++) {
    if (string->data[i] == b) {
      return (int32_t)i;
    }
  }
  return -1;
}

static size_t uleading_whitespaces(ufzf_string_t *str) {
    size_t whitespaces = 0;
    for (size_t i = 0; i < str->size;) {
        UChar32 ch;
        U16_NEXT(str->data, i, str->size, ch);
        if (!u_isUWhiteSpace(ch)) {
            break;
        }
        whitespaces++;
    }
    return whitespaces;
}

static size_t leading_whitespaces(fzf_string_t *str) {
  size_t whitespaces = 0;
  for (size_t i = 0; i < str->size; i++) {
    if (!isspace((uint8_t)str->data[i])) {
      break;
    }
    whitespaces++;
  }
  return whitespaces;
}

static size_t utrailing_whitespaces(ufzf_string_t *str) {
    size_t whitespaces = 0;
    for (size_t i = str->size - 1; i >= 0; i--) {
        UChar32 ch;
        U16_GET(str->data, 0, i, str->size, ch);
        if (!u_isUWhiteSpace(ch)) {
            break;
        }
        whitespaces++;
        if (U16_IS_LEAD(ch) && i > 0 && U16_IS_TRAIL(str->data[i - 1])) {
            i--;
        }
    }
    return whitespaces;
}

static size_t trailing_whitespaces(fzf_string_t *str) {
  size_t whitespaces = 0;
  for (size_t i = str->size - 1; i >= 0; i--) {
    if (!isspace((uint8_t)str->data[i])) {
      break;
    }
    whitespaces++;
  }
  return whitespaces;
}

static void ucopy_runes(ufzf_string_t *src, fzf_i32_t *destination) {
    for (size_t i = 0; i < src->size; i++) {
        destination->data[i] = (int32_t)src->data[i];
    }
}

static void copy_runes(fzf_string_t *src, fzf_i32_t *destination) {
  for (size_t i = 0; i < src->size; i++) {
    destination->data[i] = (int32_t)src->data[i];
  }
}

static void copy_into_i16(i16_slice_t *src, fzf_i16_t *dest) {
  for (size_t i = 0; i < src->size; i++) {
    dest->data[i] = src->data[i];
  }
}

static UChar *utrim_whitespace_left(UChar *str, size_t *len) {
    for (size_t i = 0; i < *len; i++) {
        if (str[0] == 0x0020) { // Check for space character in UTF-16
            (*len)--;
            str++;
        } else {
            break;
        }
    }
    return str;
}

// char* helpers
static char *trim_whitespace_left(char *str, size_t *len) {
  for (size_t i = 0; i < *len; i++) {
    if (str[0] == ' ') {
      (*len)--;
      str++;
    } else {
      break;
    }
  }
  return str;
}

static bool has_prefix(const char *str, const char *prefix, size_t prefix_len) {
  return strncmp(prefix, str, prefix_len) == 0;
}

static bool uhas_prefix(const UChar *str, const UChar *prefix, int32_t prefix_len) {
  // Use u_strncmp for comparison of UChar strings
  return u_strncmp(prefix, str, prefix_len) == 0;
}

static bool uhas_suffix(const UChar *str, size_t len, const UChar *suffix, size_t suffix_len) {
    if (len < suffix_len) {
        return false;
    }

    // Calculate the start of the suffix in the original string
    const UChar *str_suffix_start = str + (len - suffix_len);

    // Use u_memcmp to compare the suffix with the end of the string
    return u_memcmp(str_suffix_start, suffix, (int32_t)suffix_len) == 0;
}

static bool has_suffix(const char *str, size_t len, const char *suffix,
                       size_t suffix_len) {
  return len >= suffix_len &&
         strncmp(slice_str(str, len - suffix_len, len).data, suffix,
                 suffix_len) == 0;
}

static char *str_replace_char(char *str, char find, char replace) {
  char *current_pos = strchr(str, find);
  while (current_pos) {
    *current_pos = replace;
    current_pos = strchr(current_pos, find);
  }
  return str;
}

static char *str_replace(char *orig, char *rep, char *with) {
  if (!orig || !rep || !with) {
    return NULL;
  }

  char *result;
  char *ins;
  char *tmp;

  size_t len_rep = strlen(rep);
  size_t len_front = 0;
  size_t len_orig = strlen(orig);
  size_t len_with = strlen(with);
  size_t count = 0;

  if (len_rep == 0) {
    return NULL;
  }

  ins = orig;
  for (; (tmp = strstr(ins, rep)); ++count) {
    ins = tmp + len_rep;
  }

  tmp = result = (char *)malloc(len_orig + (len_with - len_rep) * count + 1);
  if (!result) {
    return NULL;
  }

  while (count--) {
    ins = strstr(orig, rep);
    len_front = (size_t)(ins - orig);
    tmp = strncpy(tmp, orig, len_front) + len_front;
    tmp = strcpy(tmp, with) + len_with;
    orig += len_front + len_rep;
    len_orig -= len_front + len_rep;
  }
  strncpy(tmp, orig, len_orig);
  tmp[len_orig] = 0;
  return result;
}

static UChar *ustr_replace(const UChar *orig, const UChar *rep, const UChar *with) {
    if (!orig || !rep || !with) {
        return NULL;
    }

    int32_t len_rep = u_strlen(rep);
    int32_t len_with = u_strlen(with);
    int32_t len_orig = u_strlen(orig);

    // Temporary variables for the replacement process
    const UChar *ins = orig; // Current position in the original string
    const UChar *tmp;
    int32_t count = 0; // Number of replacements

    // First pass: count the number of replacements needed
    while ((tmp = u_strstr(ins, rep)) != NULL) {
        count++;
        ins = tmp + len_rep;
    }

    // Allocate memory for the new string
    int32_t new_len = len_orig + (len_with - len_rep) * count;
    UChar *result = (UChar*)malloc((new_len + 1) * sizeof(UChar));
    if (!result) {
        return NULL;
    }

    UChar *current_pos = result;
    while (count--) {
        tmp = u_strstr(orig, rep); // Find next occurrence of rep
        int32_t len_front = (int32_t)(tmp - orig); // Length before the rep
        u_strncpy(current_pos, orig, len_front); // Copy the part before rep
        current_pos += len_front;
        u_strcpy(current_pos, with); // Replace rep with with
        current_pos += len_with;
        orig += len_front + len_rep; // Move past the replaced part
    }

    // Copy the remaining part of the original string
    u_strcpy(current_pos, orig);

    return result;
}

// TODO(conni2461): REFACTOR
static char *str_tolower(char *str, size_t size) {
  char *lower_str = (char *)malloc((size + 1) * sizeof(char));
  for (size_t i = 0; i < size; i++) {
    lower_str[i] = (char)tolower((uint8_t)str[i]);
  }
  lower_str[size] = '\0';
  return lower_str;
}

UChar* u_str_tolower(const UChar* src, int32_t srcLength) {
    if (src == NULL) {
        return NULL;
    }

    // Allocate memory for the lowercase string. The resulting string
    // might have a different length in some cases due to Unicode rules,
    // but for simplicity, we assume it does not.
    UChar* lowerStr = (UChar*)malloc((srcLength + 1) * sizeof(UChar));
    if (lowerStr == NULL) {
        // Memory allocation failed
        return NULL;
    }

    // Locale-specific lowercase conversion. Use the root locale ("") for
    // language-agnostic behavior. Adjust if you target a specific language.
    UErrorCode status = U_ZERO_ERROR;
    int32_t lowerLen = u_strToLower(lowerStr, srcLength + 1, src, srcLength, "", &status);

    if (U_FAILURE(status)) {
        // Handle the error, e.g., by freeing allocated memory and returning NULL
        free(lowerStr);
        return NULL;
    }

    // Ensure the string is null-terminated, even though u_strToLower should
    // take care of this given correct length parameters.
    lowerStr[lowerLen] = 0;

    return lowerStr;
}

static int16_t max16(int16_t a, int16_t b) {
  return (a > b) ? a : b;
}

static size_t min64u(size_t a, size_t b) {
  return (a < b) ? a : b;
}

fzf_position_t *fzf_pos_array(size_t len) {
  fzf_position_t *pos = (fzf_position_t *)malloc(sizeof(fzf_position_t));
  pos->size = 0;
  pos->cap = len;
  if (len > 0) {
    pos->data = (uint32_t *)malloc(len * sizeof(uint32_t));
  } else {
    pos->data = NULL;
  }
  return pos;
}

static void resize_pos(fzf_position_t *pos, size_t add_len, size_t comp) {
  if (!pos) {
    return;
  }
  if (pos->size + comp > pos->cap) {
    pos->cap += add_len > 0 ? add_len : 1;
    pos->data = (uint32_t *)realloc(pos->data, sizeof(uint32_t) * pos->cap);
  }
}

static void unsafe_append_pos(fzf_position_t *pos, size_t value) {
  resize_pos(pos, pos->cap, 1);
  pos->data[pos->size] = (int32_t)value;
  pos->size++;
}

static void append_pos(fzf_position_t *pos, size_t value) {
  if (pos) {
    unsafe_append_pos(pos, value);
  }
}

static void insert_range(fzf_position_t *pos, size_t start, size_t end) {
  if (!pos) {
    return;
  }

  int32_t diff = ((int32_t)end - (int32_t)start);
  if (diff <= 0) {
    return;
  }

  resize_pos(pos, end - start, end - start);
  for (size_t k = start; k < end; k++) {
    pos->data[pos->size] = (uint32_t)k;
    pos->size++;
  }
}

static fzf_i16_t alloc16(size_t *offset, fzf_slab_t *slab, size_t size) {
  if (slab != NULL && slab->I16.cap > *offset + size) {
    i16_slice_t slice = slice_i16(slab->I16.data, *offset, (*offset) + size);
    *offset = *offset + size;
    return (fzf_i16_t){.data = slice.data,
                       .size = slice.size,
                       .cap = slice.size,
                       .allocated = false};
  }
  int16_t *data = (int16_t *)malloc(size * sizeof(int16_t));
  memset(data, 0, size * sizeof(int16_t));
  return (fzf_i16_t){
      .data = data, .size = size, .cap = size, .allocated = true};
}

static fzf_i32_t alloc32(size_t *offset, fzf_slab_t *slab, size_t size) {
  if (slab != NULL && slab->I32.cap > *offset + size) {
    i32_slice_t slice = slice_i32(slab->I32.data, *offset, (*offset) + size);
    *offset = *offset + size;
    return (fzf_i32_t){.data = slice.data,
                       .size = slice.size,
                       .cap = slice.size,
                       .allocated = false};
  }
  int32_t *data = (int32_t *)malloc(size * sizeof(int32_t));
  memset(data, 0, size * sizeof(int32_t));
  return (fzf_i32_t){
      .data = data, .size = size, .cap = size, .allocated = true};
}

static char_class uchar_class_of_ascii(UChar ch) {
    if (u_islower(ch)) {
        return CharLower;
    }
    if (u_isupper(ch)) {
        return CharUpper;
    }
    if (u_isdigit(ch)) {
        return CharNumber;
    }
    return CharNonWord;
}

static char_class char_class_of_ascii(char ch) {
  if (ch >= 'a' && ch <= 'z') {
    return CharLower;
  }
  if (ch >= 'A' && ch <= 'Z') {
    return CharUpper;
  }
  if (ch >= '0' && ch <= '9') {
    return CharNumber;
  }
  return CharNonWord;
}

// static char_class char_class_of_non_ascii(char ch) {
//   return 0;
// }

static char_class uchar_class_of(UChar ch) {
    return uchar_class_of_ascii(ch);
    // if (ch <= 0x7f) {
    //   return char_class_of_ascii(ch);
    // }
    // return char_class_of_non_ascii(ch);
}

static char_class char_class_of(char ch) {
  return char_class_of_ascii(ch);
  // if (ch <= 0x7f) {
  //   return char_class_of_ascii(ch);
  // }
  // return char_class_of_non_ascii(ch);
}

static int16_t bonus_for(char_class prev_class, char_class class) {
  if (prev_class == CharNonWord && class != CharNonWord) {
    return BonusBoundary;
  }
  if ((prev_class == CharLower && class == CharUpper) ||
      (prev_class != CharNumber && class == CharNumber)) {
    return BonusCamel123;
  }
  if (class == CharNonWord) {
    return BonusNonWord;
  }
  return 0;
}

static int16_t ubonus_at(ufzf_string_t *input, size_t idx) {
    if (idx == 0) {
        return BonusBoundary;
    }
    return bonus_for(uchar_class_of(input->data[idx - 1]),
                     uchar_class_of(input->data[idx]));
}

static int16_t bonus_at(fzf_string_t *input, size_t idx) {
  if (idx == 0) {
    return BonusBoundary;
  }
  return bonus_for(char_class_of(input->data[idx - 1]),
                   char_class_of(input->data[idx]));
}

/* TODO(conni2461): maybe just not do this */
static UChar unormalize_rune(UChar r) {
    // TODO(conni2461)
    /* if (r < 0x00C0 || r > 0x2184) { */
    /*   return r; */
    /* } */
    /* rune n = normalized[r]; */
    /* if n > 0 { */
    /*   return n; */
    /* } */
    return r;
}

/* TODO(conni2461): maybe just not do this */
static char normalize_rune(char r) {
  // TODO(conni2461)
  /* if (r < 0x00C0 || r > 0x2184) { */
  /*   return r; */
  /* } */
  /* rune n = normalized[r]; */
  /* if n > 0 { */
  /*   return n; */
  /* } */
  return r;
}

static int32_t utry_skip(ufzf_string_t *input, bool case_sensitive, UChar b,
                        int32_t from) {
    ustr_slice_t slice = slice_ustr(input->data, (size_t)from, input->size);
    ufzf_string_t byte_array = {.data = slice.data, .size = slice.size};
    int32_t idx = uindex_byte(&byte_array, b);
    if (idx == 0) {
        return from;
    }

    if (!case_sensitive && b >= 'a' && b <= 'z') {
        UChar upperCaseB = (UChar)u_toupper(b);
        if (idx > 0) {
            ustr_slice_t tmp = slice_ustr_right(byte_array.data, (size_t)idx);
            byte_array.data = tmp.data;
            byte_array.size = tmp.size;
        }
        int32_t uidx = uindex_byte(&byte_array, upperCaseB);
//        int32_t uidx = index_byte(&byte_array, b - (byte)32);
        if (uidx >= 0) {
            idx = uidx;
        }
    }
    if (idx < 0) {
        return -1;
    }

    return from + idx;
}

static int32_t try_skip(fzf_string_t *input, bool case_sensitive, byte b,
                        int32_t from) {
  str_slice_t slice = slice_str(input->data, (size_t)from, input->size);
  fzf_string_t byte_array = {.data = slice.data, .size = slice.size};
  int32_t idx = index_byte(&byte_array, b);
  if (idx == 0) {
    return from;
  }

  if (!case_sensitive && b >= 'a' && b <= 'z') {
    if (idx > 0) {
      str_slice_t tmp = slice_str_right(byte_array.data, (size_t)idx);
      byte_array.data = tmp.data;
      byte_array.size = tmp.size;
    }
    int32_t uidx = index_byte(&byte_array, b - (byte)32);
    if (uidx >= 0) {
      idx = uidx;
    }
  }
  if (idx < 0) {
    return -1;
  }

  return from + idx;
}

static bool uis_ascii(const UChar *runes, size_t size) {
    // TODO(conni2461): future use
    /* for (size_t i = 0; i < size; i++) { */
    /*   if (runes[i] >= 256) { */
    /*     return false; */
    /*   } */
    /* } */
    return true;
}

static bool is_ascii(const char *runes, size_t size) {
  // TODO(conni2461): future use
  /* for (size_t i = 0; i < size; i++) { */
  /*   if (runes[i] >= 256) { */
  /*     return false; */
  /*   } */
  /* } */
  return true;
}

static int32_t uascii_fuzzy_index(ufzf_string_t *input, const UChar *pattern,
                                 size_t size, bool case_sensitive) {
    if (!uis_ascii(pattern, size)) {
        return -1;
    }

    int32_t first_idx = 0;
    int32_t idx = 0;
    for (size_t pidx = 0; pidx < size; pidx++) {
        idx = utry_skip(input, case_sensitive, pattern[pidx], idx);
        if (idx < 0) {
            return -1;
        }
        if (pidx == 0 && idx > 0) {
            first_idx = idx - 1;
        }
        idx++;
    }

    return first_idx;
}

static int32_t ascii_fuzzy_index(fzf_string_t *input, const char *pattern,
                                 size_t size, bool case_sensitive) {
  if (!is_ascii(pattern, size)) {
    return -1;
  }

  int32_t first_idx = 0;
  int32_t idx = 0;
  for (size_t pidx = 0; pidx < size; pidx++) {
    idx = try_skip(input, case_sensitive, pattern[pidx], idx);
    if (idx < 0) {
      return -1;
    }
    if (pidx == 0 && idx > 0) {
      first_idx = idx - 1;
    }
    idx++;
  }

  return first_idx;
}

static int32_t calculate_score(bool case_sensitive, bool normalize,
                               fzf_string_t *text, fzf_string_t *pattern,
                               size_t sidx, size_t eidx, fzf_position_t *pos) {
  const size_t M = pattern->size;

  size_t pidx = 0;
  int32_t score = 0;
  int32_t consecutive = 0;
  bool in_gap = false;
  int16_t first_bonus = 0;

  resize_pos(pos, M, M);
  int32_t prev_class = CharNonWord;
  if (sidx > 0) {
    prev_class = char_class_of(text->data[sidx - 1]);
  }
  for (size_t idx = sidx; idx < eidx; idx++) {
    char c = text->data[idx];
    int32_t class = char_class_of(c);
    if (!case_sensitive) {
      /* TODO(conni2461): He does some unicode stuff here, investigate */
      c = (char)tolower((uint8_t)c);
    }
    if (normalize) {
      c = normalize_rune(c);
    }
    if (c == pattern->data[pidx]) {
      append_pos(pos, idx);
      score += ScoreMatch;
      int16_t bonus = bonus_for(prev_class, class);
      if (consecutive == 0) {
        first_bonus = bonus;
      } else {
        if (bonus == BonusBoundary) {
          first_bonus = bonus;
        }
        bonus = max16(max16(bonus, first_bonus), BonusConsecutive);
      }
      if (pidx == 0) {
        score += (int32_t)(bonus * BonusFirstCharMultiplier);
      } else {
        score += (int32_t)bonus;
      }
      in_gap = false;
      consecutive++;
      pidx++;
    } else {
      if (in_gap) {
        score += ScoreGapExtention;
      } else {
        score += ScoreGapStart;
      }
      in_gap = true;
      consecutive = 0;
      first_bonus = 0;
    }
    prev_class = class;
  }
  return score;
}

static int32_t ucalculate_score(bool case_sensitive, bool normalize,
                               ufzf_string_t *text, ufzf_string_t *pattern,
                               size_t sidx, size_t eidx, fzf_position_t *pos) {
    const size_t M = pattern->size;

    size_t pidx = 0;
    int32_t score = 0;
    int32_t consecutive = 0;
    bool in_gap = false;
    int16_t first_bonus = 0;

    resize_pos(pos, M, M);
    int32_t prev_class = CharNonWord;
    if (sidx > 0) {
        prev_class = uchar_class_of(text->data[sidx - 1]);
    }
    for (size_t idx = sidx; idx < eidx; idx++) {
        UChar c = text->data[idx];
        int32_t class = uchar_class_of(c);
        if (!case_sensitive) {
            /* TODO(conni2461): He does some unicode stuff here, investigate */
//            c = (char)tolower((uint8_t)c);
            c = (UChar)u_tolower(c);
        }
        if (normalize) {
            c = unormalize_rune(c);
        }
        if (c == pattern->data[pidx]) {
            append_pos(pos, idx);
            score += ScoreMatch;
            int16_t bonus = bonus_for(prev_class, class);
            if (consecutive == 0) {
                first_bonus = bonus;
            } else {
                if (bonus == BonusBoundary) {
                    first_bonus = bonus;
                }
                bonus = max16(max16(bonus, first_bonus), BonusConsecutive);
            }
            if (pidx == 0) {
                score += (int32_t)(bonus * BonusFirstCharMultiplier);
            } else {
                score += (int32_t)bonus;
            }
            in_gap = false;
            consecutive++;
            pidx++;
        } else {
            if (in_gap) {
                score += ScoreGapExtention;
            } else {
                score += ScoreGapStart;
            }
            in_gap = true;
            consecutive = 0;
            first_bonus = 0;
        }
        prev_class = class;
    }
    return score;
}

fzf_result_t fzf_fuzzy_match_v1(bool case_sensitive, bool normalize,
                                fzf_string_t *text, fzf_string_t *pattern,
                                fzf_position_t *pos, fzf_slab_t *slab) {
  const size_t M = pattern->size;
  const size_t N = text->size;
  if (M == 0) {
    return (fzf_result_t){0, 0, 0};
  }
  if (ascii_fuzzy_index(text, pattern->data, M, case_sensitive) < 0) {
    return (fzf_result_t){-1, -1, 0};
  }

  int32_t pidx = 0;
  int32_t sidx = -1;
  int32_t eidx = -1;
  for (size_t idx = 0; idx < N; idx++) {
    char c = text->data[idx];
    /* TODO(conni2461): Common pattern maybe a macro would be good here */
    if (!case_sensitive) {
      /* TODO(conni2461): He does some unicode stuff here, investigate */
      c = (char)tolower((uint8_t)c);
    }
    if (normalize) {
      c = normalize_rune(c);
    }
    if (c == pattern->data[pidx]) {
      if (sidx < 0) {
        sidx = (int32_t)idx;
      }
      pidx++;
      if (pidx == M) {
        eidx = (int32_t)idx + 1;
        break;
      }
    }
  }
  if (sidx >= 0 && eidx >= 0) {
    size_t start = (size_t)sidx;
    size_t end = (size_t)eidx;
    pidx--;
    for (size_t idx = end - 1; idx >= start; idx--) {
      char c = text->data[idx];
      if (!case_sensitive) {
        /* TODO(conni2461): He does some unicode stuff here, investigate */
        c = (char)tolower((uint8_t)c);
      }
      if (c == pattern->data[pidx]) {
        pidx--;
        if (pidx < 0) {
          start = idx;
          break;
        }
      }
    }

    int32_t score = calculate_score(case_sensitive, normalize, text, pattern,
                                    start, end, pos);
    return (fzf_result_t){(int32_t)start, (int32_t)end, score};
  }
  return (fzf_result_t){-1, -1, 0};
}

fzf_result_t fzf_fuzzy_match_v2(bool case_sensitive, bool normalize,
                                fzf_string_t *text, fzf_string_t *pattern,
                                fzf_position_t *pos, fzf_slab_t *slab) {
  const size_t M = pattern->size;
  const size_t N = text->size;
  if (M == 0) {
    return (fzf_result_t){0, 0, 0};
  }
  if (slab != NULL && N * M > slab->I16.cap) {
    return fzf_fuzzy_match_v1(case_sensitive, normalize, text, pattern, pos,
                              slab);
  }

  size_t idx;
  {
    int32_t tmp_idx = ascii_fuzzy_index(text, pattern->data, M, case_sensitive);
    if (tmp_idx < 0) {
      return (fzf_result_t){-1, -1, 0};
    }
    idx = (size_t)tmp_idx;
  }

  size_t offset16 = 0;
  size_t offset32 = 0;

  fzf_i16_t h0 = alloc16(&offset16, slab, N);
  fzf_i16_t c0 = alloc16(&offset16, slab, N);
  // Bonus point for each positions
  fzf_i16_t bo = alloc16(&offset16, slab, N);
  // The first occurrence of each character in the pattern
  fzf_i32_t f = alloc32(&offset32, slab, M);
  // Rune array
  fzf_i32_t t = alloc32(&offset32, slab, N);
  copy_runes(text, &t); // input.CopyRunes(T)

  // Phase 2. Calculate bonus for each point
  int16_t max_score = 0;
  size_t max_score_pos = 0;

  size_t pidx = 0;
  size_t last_idx = 0;

  char pchar0 = pattern->data[0];
  char pchar = pattern->data[0];
  int16_t prev_h0 = 0;
  int32_t prev_class = CharNonWord;
  bool in_gap = false;

  i32_slice_t t_sub = slice_i32(t.data, idx, t.size); // T[idx:];
  i16_slice_t h0_sub =
      slice_i16_right(slice_i16(h0.data, idx, h0.size).data, t_sub.size);
  i16_slice_t c0_sub =
      slice_i16_right(slice_i16(c0.data, idx, c0.size).data, t_sub.size);
  i16_slice_t b_sub =
      slice_i16_right(slice_i16(bo.data, idx, bo.size).data, t_sub.size);

  for (size_t off = 0; off < t_sub.size; off++) {
    char_class class;
    char c = (char)t_sub.data[off];
    class = char_class_of_ascii(c);
    if (!case_sensitive && class == CharUpper) {
      /* TODO(conni2461): unicode support */
      c = (char)tolower((uint8_t)c);
    }
    if (normalize) {
      c = normalize_rune(c);
    }

    t_sub.data[off] = (uint8_t)c;
    int16_t bonus = bonus_for(prev_class, class);
    b_sub.data[off] = bonus;
    prev_class = class;
    if (c == pchar) {
      if (pidx < M) {
        f.data[pidx] = (int32_t)(idx + off);
        pidx++;
        pchar = pattern->data[min64u(pidx, M - 1)];
      }
      last_idx = idx + off;
    }

    if (c == pchar0) {
      int16_t score = ScoreMatch + bonus * BonusFirstCharMultiplier;
      h0_sub.data[off] = score;
      c0_sub.data[off] = 1;
      if (M == 1 && (score > max_score)) {
        max_score = score;
        max_score_pos = idx + off;
        if (bonus == BonusBoundary) {
          break;
        }
      }
      in_gap = false;
    } else {
      if (in_gap) {
        h0_sub.data[off] = max16(prev_h0 + ScoreGapExtention, 0);
      } else {
        h0_sub.data[off] = max16(prev_h0 + ScoreGapStart, 0);
      }
      c0_sub.data[off] = 0;
      in_gap = true;
    }
    prev_h0 = h0_sub.data[off];
  }
  if (pidx != M) {
    free_alloc(t);
    free_alloc(f);
    free_alloc(bo);
    free_alloc(c0);
    free_alloc(h0);
    return (fzf_result_t){-1, -1, 0};
  }
  if (M == 1) {
    free_alloc(t);
    free_alloc(f);
    free_alloc(bo);
    free_alloc(c0);
    free_alloc(h0);
    fzf_result_t res = {(int32_t)max_score_pos, (int32_t)max_score_pos + 1,
                        max_score};
    append_pos(pos, max_score_pos);
    return res;
  }

  size_t f0 = (size_t)f.data[0];
  size_t width = last_idx - f0 + 1;
  fzf_i16_t h = alloc16(&offset16, slab, width * M);
  {
    i16_slice_t h0_tmp_slice = slice_i16(h0.data, f0, last_idx + 1);
    copy_into_i16(&h0_tmp_slice, &h);
  }

  fzf_i16_t c = alloc16(&offset16, slab, width * M);
  {
    i16_slice_t c0_tmp_slice = slice_i16(c0.data, f0, last_idx + 1);
    copy_into_i16(&c0_tmp_slice, &c);
  }

  i32_slice_t f_sub = slice_i32(f.data, 1, f.size);
  str_slice_t p_sub =
      slice_str_right(slice_str(pattern->data, 1, M).data, f_sub.size);
  for (size_t off = 0; off < f_sub.size; off++) {
    size_t f = (size_t)f_sub.data[off];
    pchar = p_sub.data[off];
    pidx = off + 1;
    size_t row = pidx * width;
    in_gap = false;
    t_sub = slice_i32(t.data, f, last_idx + 1);
    b_sub = slice_i16_right(slice_i16(bo.data, f, bo.size).data, t_sub.size);
    i16_slice_t c_sub = slice_i16_right(
        slice_i16(c.data, row + f - f0, c.size).data, t_sub.size);
    i16_slice_t c_diag = slice_i16_right(
        slice_i16(c.data, row + f - f0 - 1 - width, c.size).data, t_sub.size);
    i16_slice_t h_sub = slice_i16_right(
        slice_i16(h.data, row + f - f0, h.size).data, t_sub.size);
    i16_slice_t h_diag = slice_i16_right(
        slice_i16(h.data, row + f - f0 - 1 - width, h.size).data, t_sub.size);
    i16_slice_t h_left = slice_i16_right(
        slice_i16(h.data, row + f - f0 - 1, h.size).data, t_sub.size);
    h_left.data[0] = 0;
    for (size_t j = 0; j < t_sub.size; j++) {
      char ch = (char)t_sub.data[j];
      size_t col = j + f;
      int16_t s1 = 0;
      int16_t s2 = 0;
      int16_t consecutive = 0;

      if (in_gap) {
        s2 = h_left.data[j] + ScoreGapExtention;
      } else {
        s2 = h_left.data[j] + ScoreGapStart;
      }

      if (pchar == ch) {
        s1 = h_diag.data[j] + ScoreMatch;
        int16_t b = b_sub.data[j];
        consecutive = c_diag.data[j] + 1;
        if (b == BonusBoundary) {
          consecutive = 1;
        } else if (consecutive > 1) {
          b = max16(b, max16(BonusConsecutive,
                             bo.data[col - ((size_t)consecutive) + 1]));
        }
        if (s1 + b < s2) {
          s1 += b_sub.data[j];
          consecutive = 0;
        } else {
          s1 += b;
        }
      }
      c_sub.data[j] = consecutive;
      in_gap = s1 < s2;
      int16_t score = max16(max16(s1, s2), 0);
      if (pidx == M - 1 && (score > max_score)) {
        max_score = score;
        max_score_pos = col;
      }
      h_sub.data[j] = score;
    }
  }

  resize_pos(pos, M, M);
  size_t j = max_score_pos;
  if (pos) {
    size_t i = M - 1;
    bool prefer_match = true;
    for (;;) {
      size_t ii = i * width;
      size_t j0 = j - f0;
      int16_t s = h.data[ii + j0];

      int16_t s1 = 0;
      int16_t s2 = 0;
      if (i > 0 && j >= f.data[i]) {
        s1 = h.data[ii - width + j0 - 1];
      }
      if (j > f.data[i]) {
        s2 = h.data[ii + j0 - 1];
      }

      if (s > s1 && (s > s2 || (s == s2 && prefer_match))) {
        unsafe_append_pos(pos, j);
        if (i == 0) {
          break;
        }
        i--;
      }
      prefer_match = c.data[ii + j0] > 1 || (ii + width + j0 + 1 < c.size &&
                                             c.data[ii + width + j0 + 1] > 0);
      j--;
    }
  }

  free_alloc(h);
  free_alloc(c);
  free_alloc(t);
  free_alloc(f);
  free_alloc(bo);
  free_alloc(c0);
  free_alloc(h0);
  return (fzf_result_t){(int32_t)j, (int32_t)max_score_pos + 1,
                        (int32_t)max_score};
}

fzf_result_t ufzf_fuzzy_match_v2(bool case_sensitive, bool normalize,
                                ufzf_string_t *text, ufzf_string_t *pattern,
                                fzf_position_t *pos, fzf_slab_t *slab) {
    const size_t M = pattern->size;
    const size_t N = text->size;
    if (M == 0) {
        return (fzf_result_t){0, 0, 0};
    }
    if (slab != NULL && N * M > slab->I16.cap) {
//        return fzf_fuzzy_match_v1(case_sensitive, normalize, text, pattern, pos,
//                                  slab);
    }

    size_t idx;
    {
        int32_t tmp_idx = uascii_fuzzy_index(text, pattern->data, M, case_sensitive);
        if (tmp_idx < 0) {
            return (fzf_result_t){-1, -1, 0};
        }
        idx = (size_t)tmp_idx;
    }

    size_t offset16 = 0;
    size_t offset32 = 0;

    fzf_i16_t h0 = alloc16(&offset16, slab, N);
    fzf_i16_t c0 = alloc16(&offset16, slab, N);
    // Bonus point for each positions
    fzf_i16_t bo = alloc16(&offset16, slab, N);
    // The first occurrence of each character in the pattern
    fzf_i32_t f = alloc32(&offset32, slab, M);
    // Rune array
    fzf_i32_t t = alloc32(&offset32, slab, N);
    ucopy_runes(text, &t); // input.CopyRunes(T)

    // Phase 2. Calculate bonus for each point
    int16_t max_score = 0;
    size_t max_score_pos = 0;

    size_t pidx = 0;
    size_t last_idx = 0;

    UChar pchar0 = pattern->data[0];
    UChar pchar = pattern->data[0];
    int16_t prev_h0 = 0;
    int32_t prev_class = CharNonWord;
    bool in_gap = false;

    i32_slice_t t_sub = slice_i32(t.data, idx, t.size); // T[idx:];
    i16_slice_t h0_sub =
            slice_i16_right(slice_i16(h0.data, idx, h0.size).data, t_sub.size);
    i16_slice_t c0_sub =
            slice_i16_right(slice_i16(c0.data, idx, c0.size).data, t_sub.size);
    i16_slice_t b_sub =
            slice_i16_right(slice_i16(bo.data, idx, bo.size).data, t_sub.size);

    for (size_t off = 0; off < t_sub.size; off++) {
        char_class class;
        UChar c = (UChar)t_sub.data[off];
        class = uchar_class_of_ascii(c);
        if (!case_sensitive && class == CharUpper) {
            /* TODO(conni2461): unicode support */
            c = (UChar)u_tolower(c);
        }
        if (normalize) {
            c = unormalize_rune(c);
        }

        t_sub.data[off] = c;
        int16_t bonus = bonus_for(prev_class, class);
        b_sub.data[off] = bonus;
        prev_class = class;
        if (c == pchar) {
            if (pidx < M) {
                f.data[pidx] = (int32_t)(idx + off);
                pidx++;
                pchar = pattern->data[min64u(pidx, M - 1)];
            }
            last_idx = idx + off;
        }

        if (c == pchar0) {
            int16_t score = ScoreMatch + bonus * BonusFirstCharMultiplier;
            h0_sub.data[off] = score;
            c0_sub.data[off] = 1;
            if (M == 1 && (score > max_score)) {
                max_score = score;
                max_score_pos = idx + off;
                if (bonus == BonusBoundary) {
                    break;
                }
            }
            in_gap = false;
        } else {
            if (in_gap) {
                h0_sub.data[off] = max16(prev_h0 + ScoreGapExtention, 0);
            } else {
                h0_sub.data[off] = max16(prev_h0 + ScoreGapStart, 0);
            }
            c0_sub.data[off] = 0;
            in_gap = true;
        }
        prev_h0 = h0_sub.data[off];
    }
    if (pidx != M) {
        free_alloc(t);
        free_alloc(f);
        free_alloc(bo);
        free_alloc(c0);
        free_alloc(h0);
        return (fzf_result_t){-1, -1, 0};
    }
    if (M == 1) {
        free_alloc(t);
        free_alloc(f);
        free_alloc(bo);
        free_alloc(c0);
        free_alloc(h0);
        fzf_result_t res = {(int32_t)max_score_pos, (int32_t)max_score_pos + 1,
                            max_score};
        append_pos(pos, max_score_pos);
        return res;
    }

    size_t f0 = (size_t)f.data[0];
    size_t width = last_idx - f0 + 1;
    fzf_i16_t h = alloc16(&offset16, slab, width * M);
    {
        i16_slice_t h0_tmp_slice = slice_i16(h0.data, f0, last_idx + 1);
        copy_into_i16(&h0_tmp_slice, &h);
    }

    fzf_i16_t c = alloc16(&offset16, slab, width * M);
    {
        i16_slice_t c0_tmp_slice = slice_i16(c0.data, f0, last_idx + 1);
        copy_into_i16(&c0_tmp_slice, &c);
    }

    i32_slice_t f_sub = slice_i32(f.data, 1, f.size);
    ustr_slice_t p_sub =
            slice_ustr_right(slice_ustr(pattern->data, 1, M).data, f_sub.size);
    for (size_t off = 0; off < f_sub.size; off++) {
        size_t f = (size_t)f_sub.data[off];
        pchar = p_sub.data[off];
        pidx = off + 1;
        size_t row = pidx * width;
        in_gap = false;
        t_sub = slice_i32(t.data, f, last_idx + 1);
        b_sub = slice_i16_right(slice_i16(bo.data, f, bo.size).data, t_sub.size);
        i16_slice_t c_sub = slice_i16_right(
                slice_i16(c.data, row + f - f0, c.size).data, t_sub.size);
        i16_slice_t c_diag = slice_i16_right(
                slice_i16(c.data, row + f - f0 - 1 - width, c.size).data, t_sub.size);
        i16_slice_t h_sub = slice_i16_right(
                slice_i16(h.data, row + f - f0, h.size).data, t_sub.size);
        i16_slice_t h_diag = slice_i16_right(
                slice_i16(h.data, row + f - f0 - 1 - width, h.size).data, t_sub.size);
        i16_slice_t h_left = slice_i16_right(
                slice_i16(h.data, row + f - f0 - 1, h.size).data, t_sub.size);
        h_left.data[0] = 0;
        for (size_t j = 0; j < t_sub.size; j++) {
            UChar ch = (UChar)t_sub.data[j];
            size_t col = j + f;
            int16_t s1 = 0;
            int16_t s2 = 0;
            int16_t consecutive = 0;

            if (in_gap) {
                s2 = h_left.data[j] + ScoreGapExtention;
            } else {
                s2 = h_left.data[j] + ScoreGapStart;
            }

            if (pchar == ch) {
                s1 = h_diag.data[j] + ScoreMatch;
                int16_t b = b_sub.data[j];
                consecutive = c_diag.data[j] + 1;
                if (b == BonusBoundary) {
                    consecutive = 1;
                } else if (consecutive > 1) {
                    b = max16(b, max16(BonusConsecutive,
                                       bo.data[col - ((size_t)consecutive) + 1]));
                }
                if (s1 + b < s2) {
                    s1 += b_sub.data[j];
                    consecutive = 0;
                } else {
                    s1 += b;
                }
            }
            c_sub.data[j] = consecutive;
            in_gap = s1 < s2;
            int16_t score = max16(max16(s1, s2), 0);
            if (pidx == M - 1 && (score > max_score)) {
                max_score = score;
                max_score_pos = col;
            }
            h_sub.data[j] = score;
        }
    }

    resize_pos(pos, M, M);
    size_t j = max_score_pos;
    if (pos) {
        size_t i = M - 1;
        bool prefer_match = true;
        for (;;) {
            size_t ii = i * width;
            size_t j0 = j - f0;
            int16_t s = h.data[ii + j0];

            int16_t s1 = 0;
            int16_t s2 = 0;
            if (i > 0 && j >= f.data[i]) {
                s1 = h.data[ii - width + j0 - 1];
            }
            if (j > f.data[i]) {
                s2 = h.data[ii + j0 - 1];
            }

            if (s > s1 && (s > s2 || (s == s2 && prefer_match))) {
                unsafe_append_pos(pos, j);
                if (i == 0) {
                    break;
                }
                i--;
            }
            prefer_match = c.data[ii + j0] > 1 || (ii + width + j0 + 1 < c.size &&
                                                   c.data[ii + width + j0 + 1] > 0);
            j--;
        }
    }

    free_alloc(h);
    free_alloc(c);
    free_alloc(t);
    free_alloc(f);
    free_alloc(bo);
    free_alloc(c0);
    free_alloc(h0);
    return (fzf_result_t){(int32_t)j, (int32_t)max_score_pos + 1,
                          (int32_t)max_score};
}

fzf_result_t ufzf_exact_match_naive(bool case_sensitive, bool normalize,
                                   ufzf_string_t *text, ufzf_string_t *pattern,
                                   fzf_position_t *pos, fzf_slab_t *slab) {
    const size_t M = pattern->size;
    const size_t N = text->size;

    if (M == 0) {
        return (fzf_result_t){0, 0, 0};
    }
    if (N < M) {
        return (fzf_result_t){-1, -1, 0};
    }
    if (uascii_fuzzy_index(text, pattern->data, M, case_sensitive) < 0) {
        return (fzf_result_t){-1, -1, 0};
    }

    size_t pidx = 0;
    int32_t best_pos = -1;
    int16_t bonus = 0;
    int16_t best_bonus = -1;
    for (size_t idx = 0; idx < N; idx++) {
        UChar c = text->data[idx];
        if (!case_sensitive) {
            /* TODO(conni2461): He does some unicode stuff here, investigate */
//            c = (char)tolower((uint8_t)c);
            c = (UChar)u_tolower(c);
        }
        if (normalize) {
            c = unormalize_rune(c);
        }
        if (c == pattern->data[pidx]) {
            if (pidx == 0) {
                bonus = ubonus_at(text, idx);
            }
            pidx++;
            if (pidx == M) {
                if (bonus > best_bonus) {
                    best_pos = (int32_t)idx;
                    best_bonus = bonus;
                }
                if (bonus == BonusBoundary) {
                    break;
                }
                idx -= pidx - 1;
                pidx = 0;
                bonus = 0;
            }
        } else {
            idx -= pidx;
            pidx = 0;
            bonus = 0;
        }
    }
    if (best_pos >= 0) {
        size_t bp = (size_t)best_pos;
        size_t sidx = bp - M + 1;
        size_t eidx = bp + 1;
        int32_t score = ucalculate_score(case_sensitive, normalize, text, pattern,
                                        sidx, eidx, NULL);
        insert_range(pos, sidx, eidx);
        return (fzf_result_t){(int32_t)sidx, (int32_t)eidx, score};
    }
    return (fzf_result_t){-1, -1, 0};
}

fzf_result_t fzf_exact_match_naive(bool case_sensitive, bool normalize,
                                   fzf_string_t *text, fzf_string_t *pattern,
                                   fzf_position_t *pos, fzf_slab_t *slab) {
  const size_t M = pattern->size;
  const size_t N = text->size;

  if (M == 0) {
    return (fzf_result_t){0, 0, 0};
  }
  if (N < M) {
    return (fzf_result_t){-1, -1, 0};
  }
  if (ascii_fuzzy_index(text, pattern->data, M, case_sensitive) < 0) {
    return (fzf_result_t){-1, -1, 0};
  }

  size_t pidx = 0;
  int32_t best_pos = -1;
  int16_t bonus = 0;
  int16_t best_bonus = -1;
  for (size_t idx = 0; idx < N; idx++) {
    char c = text->data[idx];
    if (!case_sensitive) {
      /* TODO(conni2461): He does some unicode stuff here, investigate */
      c = (char)tolower((uint8_t)c);
    }
    if (normalize) {
      c = normalize_rune(c);
    }
    if (c == pattern->data[pidx]) {
      if (pidx == 0) {
        bonus = bonus_at(text, idx);
      }
      pidx++;
      if (pidx == M) {
        if (bonus > best_bonus) {
          best_pos = (int32_t)idx;
          best_bonus = bonus;
        }
        if (bonus == BonusBoundary) {
          break;
        }
        idx -= pidx - 1;
        pidx = 0;
        bonus = 0;
      }
    } else {
      idx -= pidx;
      pidx = 0;
      bonus = 0;
    }
  }
  if (best_pos >= 0) {
    size_t bp = (size_t)best_pos;
    size_t sidx = bp - M + 1;
    size_t eidx = bp + 1;
    int32_t score = calculate_score(case_sensitive, normalize, text, pattern,
                                    sidx, eidx, NULL);
    insert_range(pos, sidx, eidx);
    return (fzf_result_t){(int32_t)sidx, (int32_t)eidx, score};
  }
  return (fzf_result_t){-1, -1, 0};
}

fzf_result_t ufzf_prefix_match(bool case_sensitive, bool normalize,
                              ufzf_string_t *text, ufzf_string_t *pattern,
                              fzf_position_t *pos, fzf_slab_t *slab) {
    const size_t M = pattern->size;
    if (M == 0) {
        return (fzf_result_t){0, 0, 0};
    }
    size_t trimmed_len = 0;
    /* TODO(conni2461): i feel this is wrong */
    if (!u_isWhitespace((uint8_t)pattern->data[0])) {
        trimmed_len = uleading_whitespaces(text);
    }
    if (text->size - trimmed_len < M) {
        return (fzf_result_t){-1, -1, 0};
    }
    for (size_t i = 0; i < M; i++) {
        UChar c = text->data[trimmed_len + i];
        if (!case_sensitive) {
            c = (UChar)u_tolower(c);
        }
        if (normalize) {
            c = unormalize_rune(c);
        }
        if (c != pattern->data[i]) {
            return (fzf_result_t){-1, -1, 0};
        }
    }
    size_t start = trimmed_len;
    size_t end = trimmed_len + M;
    int32_t score = ucalculate_score(case_sensitive, normalize, text, pattern,
                                    start, end, NULL);
    insert_range(pos, start, end);
    return (fzf_result_t){(int32_t)start, (int32_t)end, score};
}

fzf_result_t fzf_prefix_match(bool case_sensitive, bool normalize,
                              fzf_string_t *text, fzf_string_t *pattern,
                              fzf_position_t *pos, fzf_slab_t *slab) {
  const size_t M = pattern->size;
  if (M == 0) {
    return (fzf_result_t){0, 0, 0};
  }
  size_t trimmed_len = 0;
  /* TODO(conni2461): i feel this is wrong */
  if (!isspace((uint8_t)pattern->data[0])) {
    trimmed_len = leading_whitespaces(text);
  }
  if (text->size - trimmed_len < M) {
    return (fzf_result_t){-1, -1, 0};
  }
  for (size_t i = 0; i < M; i++) {
    char c = text->data[trimmed_len + i];
    if (!case_sensitive) {
      c = (char)tolower((uint8_t)c);
    }
    if (normalize) {
      c = normalize_rune(c);
    }
    if (c != pattern->data[i]) {
      return (fzf_result_t){-1, -1, 0};
    }
  }
  size_t start = trimmed_len;
  size_t end = trimmed_len + M;
  int32_t score = calculate_score(case_sensitive, normalize, text, pattern,
                                  start, end, NULL);
  insert_range(pos, start, end);
  return (fzf_result_t){(int32_t)start, (int32_t)end, score};
}

fzf_result_t ufzf_suffix_match(bool case_sensitive, bool normalize,
                              ufzf_string_t *text, ufzf_string_t *pattern,
                              fzf_position_t *pos, fzf_slab_t *slab) {
    size_t trimmed_len = text->size;
    const size_t M = pattern->size;
    /* TODO(conni2461): i think this is wrong */
    if (M == 0 || !u_isWhitespace(pattern->data[M - 1])) {
        trimmed_len -= utrailing_whitespaces(text);
    }
    if (M == 0) {
        return (fzf_result_t){(int32_t)trimmed_len, (int32_t)trimmed_len, 0};
    }
    size_t diff = trimmed_len - M;
    if (diff < 0) {
        return (fzf_result_t){-1, -1, 0};
    }

    for (size_t idx = 0; idx < M; idx++) {
        UChar c = text->data[idx + diff];
        if (!case_sensitive) {
            c = (UChar)u_tolower(c);
        }
        if (normalize) {
            c = unormalize_rune(c);
        }
        if (c != pattern->data[idx]) {
            return (fzf_result_t){-1, -1, 0};
        }
    }
    size_t start = trimmed_len - M;
    size_t end = trimmed_len;
    int32_t score = ucalculate_score(case_sensitive, normalize, text, pattern,
                                    start, end, NULL);
    insert_range(pos, start, end);
    return (fzf_result_t){(int32_t)start, (int32_t)end, score};
}

fzf_result_t fzf_suffix_match(bool case_sensitive, bool normalize,
                              fzf_string_t *text, fzf_string_t *pattern,
                              fzf_position_t *pos, fzf_slab_t *slab) {
  size_t trimmed_len = text->size;
  const size_t M = pattern->size;
  /* TODO(conni2461): i think this is wrong */
  if (M == 0 || !isspace((uint8_t)pattern->data[M - 1])) {
    trimmed_len -= trailing_whitespaces(text);
  }
  if (M == 0) {
    return (fzf_result_t){(int32_t)trimmed_len, (int32_t)trimmed_len, 0};
  }
  size_t diff = trimmed_len - M;
  if (diff < 0) {
    return (fzf_result_t){-1, -1, 0};
  }

  for (size_t idx = 0; idx < M; idx++) {
    char c = text->data[idx + diff];
    if (!case_sensitive) {
      c = (char)tolower((uint8_t)c);
    }
    if (normalize) {
      c = normalize_rune(c);
    }
    if (c != pattern->data[idx]) {
      return (fzf_result_t){-1, -1, 0};
    }
  }
  size_t start = trimmed_len - M;
  size_t end = trimmed_len;
  int32_t score = calculate_score(case_sensitive, normalize, text, pattern,
                                  start, end, NULL);
  insert_range(pos, start, end);
  return (fzf_result_t){(int32_t)start, (int32_t)end, score};
}

fzf_result_t ufzf_equal_match(bool case_sensitive, bool normalize,
                             ufzf_string_t *text, ufzf_string_t *pattern,
                             fzf_position_t *pos, fzf_slab_t *slab) {
    const size_t M = pattern->size;
    if (M == 0) {
        return (fzf_result_t){-1, -1, 0};
    }

    size_t trimmed_len = uleading_whitespaces(text);
    size_t trimmed_end_len = utrailing_whitespaces(text);

    if ((text->size - trimmed_len - trimmed_end_len) != M) {
        return (fzf_result_t){-1, -1, 0};
    }

    bool match = true;
    if (normalize) {
        // TODO(conni2461): to rune
        for (size_t idx = 0; idx < M; idx++) {
            UChar pchar = pattern->data[idx];
            UChar c = text->data[trimmed_len + idx];
            if (!case_sensitive) {
                c = (UChar)u_tolower(c);
            }
            if (unormalize_rune(c) != unormalize_rune(pchar)) {
                match = false;
                break;
            }
        }
    } else {
        // TODO(conni2461): to rune
        for (size_t idx = 0; idx < M; idx++) {
            UChar pchar = pattern->data[idx];
            UChar c = text->data[trimmed_len + idx];
            if (!case_sensitive) {
                c = (UChar)u_tolower(c);
            }
            if (c != pchar) {
                match = false;
                break;
            }
        }
    }
    if (match) {
        insert_range(pos, trimmed_len, trimmed_len + M);
        return (fzf_result_t){(int32_t)trimmed_len,
                              ((int32_t)trimmed_len + (int32_t)M),
                              (ScoreMatch + BonusBoundary) * (int32_t)M +
                              (BonusFirstCharMultiplier - 1) * BonusBoundary};
    }
    return (fzf_result_t){-1, -1, 0};
}

fzf_result_t fzf_equal_match(bool case_sensitive, bool normalize,
                             fzf_string_t *text, fzf_string_t *pattern,
                             fzf_position_t *pos, fzf_slab_t *slab) {
  const size_t M = pattern->size;
  if (M == 0) {
    return (fzf_result_t){-1, -1, 0};
  }

  size_t trimmed_len = leading_whitespaces(text);
  size_t trimmed_end_len = trailing_whitespaces(text);

  if ((text->size - trimmed_len - trimmed_end_len) != M) {
    return (fzf_result_t){-1, -1, 0};
  }

  bool match = true;
  if (normalize) {
    // TODO(conni2461): to rune
    for (size_t idx = 0; idx < M; idx++) {
      char pchar = pattern->data[idx];
      char c = text->data[trimmed_len + idx];
      if (!case_sensitive) {
        c = (char)tolower((uint8_t)c);
      }
      if (normalize_rune(c) != normalize_rune(pchar)) {
        match = false;
        break;
      }
    }
  } else {
    // TODO(conni2461): to rune
    for (size_t idx = 0; idx < M; idx++) {
      char pchar = pattern->data[idx];
      char c = text->data[trimmed_len + idx];
      if (!case_sensitive) {
        c = (char)tolower((uint8_t)c);
      }
      if (c != pchar) {
        match = false;
        break;
      }
    }
  }
  if (match) {
    insert_range(pos, trimmed_len, trimmed_len + M);
    return (fzf_result_t){(int32_t)trimmed_len,
                          ((int32_t)trimmed_len + (int32_t)M),
                          (ScoreMatch + BonusBoundary) * (int32_t)M +
                              (BonusFirstCharMultiplier - 1) * BonusBoundary};
  }
  return (fzf_result_t){-1, -1, 0};
}

static void append_set(fzf_term_set_t *set, fzf_term_t value) {
  if (set->cap == 0) {
    set->cap = 1;
    set->ptr = (fzf_term_t *)malloc(sizeof(fzf_term_t));
  } else if (set->size + 1 > set->cap) {
    set->cap *= 2;
    set->ptr = realloc(set->ptr, sizeof(fzf_term_t) * set->cap);
  }
  set->ptr[set->size] = value;
  set->size++;
}

static void uappend_set(ufzf_term_set_t *set, ufzf_term_t value) {
    if (set->cap == 0) {
        set->cap = 1;
        set->ptr = (ufzf_term_t *)malloc(sizeof(ufzf_term_t));
    } else if (set->size + 1 > set->cap) {
        set->cap *= 2;
        set->ptr = realloc(set->ptr, sizeof(ufzf_term_t) * set->cap);
    }
    set->ptr[set->size] = value;
    set->size++;
}

static void append_pattern(fzf_pattern_t *pattern, fzf_term_set_t *value) {
  if (pattern->cap == 0) {
    pattern->cap = 1;
    pattern->ptr = (fzf_term_set_t **)malloc(sizeof(fzf_term_set_t *));
  } else if (pattern->size + 1 > pattern->cap) {
    pattern->cap *= 2;
    pattern->ptr =
        realloc(pattern->ptr, sizeof(fzf_term_set_t *) * pattern->cap);
  }
  pattern->ptr[pattern->size] = value;
  pattern->size++;
}

static void uappend_pattern(ufzf_pattern_t *pattern, ufzf_term_set_t *value) {
    if (pattern->cap == 0) {
        pattern->cap = 1;
        pattern->ptr = (ufzf_term_set_t **)malloc(sizeof(ufzf_term_set_t *));
    } else if (pattern->size + 1 > pattern->cap) {
        pattern->cap *= 2;
        pattern->ptr =
                realloc(pattern->ptr, sizeof(ufzf_term_set_t *) * pattern->cap);
    }
    pattern->ptr[pattern->size] = value;
    pattern->size++;
}

#define CALL_ALG(term, normalize, input, pos, slab)                            \
  term->fn((term)->case_sensitive, normalize, &(input),                        \
           (fzf_string_t *)(term)->text, pos, slab)

#define UCALL_ALG(term, normalize, input, pos, slab)                           \
  term->fn((term)->case_sensitive, normalize, &(input),                        \
           (ufzf_string_t *)(term)->text, pos, slab)

// TODO(conni2461): REFACTOR
/* assumption (maybe i change that later)
 * - always v2 alg
 * - bool extended always true (thats the whole point of this isn't it)
 */
fzf_pattern_t *fzf_parse_pattern(fzf_case_types case_mode, bool normalize,
                                 char *pattern, bool fuzzy) {
  fzf_pattern_t *pat_obj = (fzf_pattern_t *)malloc(sizeof(fzf_pattern_t));
  memset(pat_obj, 0, sizeof(*pat_obj));

  size_t pat_len = strlen(pattern);
  if (pat_len == 0) {
    return pat_obj;
  }
  pattern = trim_whitespace_left(pattern, &pat_len);
  while (has_suffix(pattern, pat_len, " ", 1) &&
         !has_suffix(pattern, pat_len, "\\ ", 2)) {
    pattern[pat_len - 1] = 0;
    pat_len--;
  }

  char *pattern_copy = str_replace(pattern, "\\ ", "\t");
  const char *delim = " ";
  char *ptr = strtok(pattern_copy, delim);

  fzf_term_set_t *set = (fzf_term_set_t *)malloc(sizeof(fzf_term_set_t));
  memset(set, 0, sizeof(*set));

  bool switch_set = false;
  bool after_bar = false;
  while (ptr != NULL) {
    fzf_algo_t fn = fzf_fuzzy_match_v2;
    bool inv = false;

    size_t len = strlen(ptr);
    str_replace_char(ptr, '\t', ' ');
    char *text = (char*)_strdup(ptr);

    char *og_str = text;
    char *lower_text = str_tolower(text, len);
    bool case_sensitive =
        case_mode == CaseRespect ||
        (case_mode == CaseSmart && strcmp(text, lower_text) != 0);
    if (!case_sensitive) {
      SFREE(text);
      text = lower_text;
      og_str = lower_text;
    } else {
      SFREE(lower_text);
    }
    if (!fuzzy) {
      fn = fzf_exact_match_naive;
    }
    if (set->size > 0 && !after_bar && strcmp(text, "|") == 0) {
      switch_set = false;
      after_bar = true;
      ptr = strtok(NULL, delim);
      SFREE(og_str);
      continue;
    }
    after_bar = false;
    if (has_prefix(text, "!", 1)) {
      inv = true;
      fn = fzf_exact_match_naive;
      text++;
      len--;
    }

    if (strcmp(text, "$") != 0 && has_suffix(text, len, "$", 1)) {
      fn = fzf_suffix_match;
      text[len - 1] = 0;
      len--;
    }

    if (has_prefix(text, "'", 1)) {
      if (fuzzy && !inv) {
        fn = fzf_exact_match_naive;
        text++;
        len--;
      } else {
        fn = fzf_fuzzy_match_v2;
        text++;
        len--;
      }
    } else if (has_prefix(text, "^", 1)) {
      if (fn == fzf_suffix_match) {
        fn = fzf_equal_match;
      } else {
        fn = fzf_prefix_match;
      }
      text++;
      len--;
    }

    if (len > 0) {
      if (switch_set) {
        append_pattern(pat_obj, set);
        set = (fzf_term_set_t *)malloc(sizeof(fzf_term_set_t));
        set->cap = 0;
        set->size = 0;
      }
      fzf_string_t *text_ptr = (fzf_string_t *)malloc(sizeof(fzf_string_t));
      text_ptr->data = text;
      text_ptr->size = len;
      append_set(set, (fzf_term_t){.fn = fn,
                                   .inv = inv,
                                   .ptr = og_str,
                                   .text = text_ptr,
                                   .case_sensitive = case_sensitive});
      switch_set = true;
    } else {
      SFREE(og_str);
    }

    ptr = strtok(NULL, delim);
  }
  if (set->size > 0) {
    append_pattern(pat_obj, set);
  } else {
    SFREE(set->ptr);
    SFREE(set);
  }
  bool only = true;
  for (size_t i = 0; i < pat_obj->size; i++) {
    fzf_term_set_t *term_set = pat_obj->ptr[i];
    if (term_set->size > 1) {
      only = false;
      break;
    }
    if (term_set->ptr[0].inv == false) {
      only = false;
      break;
    }
  }
  pat_obj->only_inv = only;
  SFREE(pattern_copy);
  return pat_obj;
}

UChar* ustrtok_r(UChar* src, const UChar delim, UChar** context) {
    if (src == NULL && (src = *context) == NULL) {
        return NULL; // No more tokens
    }

    UChar *end = src;
    while (*end && *end != delim) {
        ++end;
    }

    if (*end == 0) { // End of string
        *context = NULL;
    } else {
        *end = 0; // Replace delim with null terminator to end token
        *context = end + 1;
    }

    return src;
}

UChar* u_strdup(const UChar* src) {
    if (src == NULL) {
        return NULL;
    }

    int32_t len = u_strlen(src); // Get the length of the source string
    // Allocate memory for the duplicate string, including space for the null terminator
    UChar* dup = (UChar*)malloc((len + 1) * sizeof(UChar));
    if (dup == NULL) {
        return NULL; // Memory allocation failed
    }

    // Copy the string into the newly allocated memory
    u_memcpy(dup, src, len + 1); // +1 to include the null terminator

    return dup;
}

ufzf_pattern_t *ufzf_parse_pattern(fzf_case_types case_mode, bool normalize,
                                 UChar *pattern, bool fuzzy) {
    ufzf_pattern_t *pat_obj = (ufzf_pattern_t *)malloc(sizeof(ufzf_pattern_t));
    memset(pat_obj, 0, sizeof(*pat_obj));

    size_t pat_len = u_strlen(pattern);
    if (pat_len == 0) {
        return pat_obj;
    }
    pattern = utrim_whitespace_left(pattern, &pat_len);

    UChar spaceChar = 0x0020;
    UChar space_suffix[] = {0x0020, 0x0000};
    UChar escaped_space_suffix[] = {0x005C, 0x0020, 0};
    UChar tabChar = 0x0009;
    UChar tab[] = {0x0009, 0x0000};
    while (uhas_suffix(pattern, pat_len, space_suffix, 1) &&
           !uhas_suffix(pattern, pat_len, escaped_space_suffix, 2)) {
        pattern[pat_len - 1] = 0;
        pat_len--;
    }

    UChar *pattern_copy = ustr_replace(pattern, escaped_space_suffix, tab);
    //const char *delim = " ";
    UChar* context = NULL;
    UChar udelim = 0x0020;
    UChar *ptr = ustrtok_r(pattern_copy, udelim, &context);

    ufzf_term_set_t *set = (ufzf_term_set_t *)malloc(sizeof(ufzf_term_set_t));
    memset(set, 0, sizeof(*set));

    bool switch_set = false;
    bool after_bar = false;
    while (ptr != NULL) {
        ufzf_algo_t fn = ufzf_fuzzy_match_v2;
        bool inv = false;

        size_t len = u_strlen(ptr);
        ustr_replace(ptr, &tabChar, &spaceChar);
        UChar *text = u_strdup(ptr);

        UChar *og_str = text;
        UChar *lower_text = u_str_tolower(text, (int32_t)len);
        bool case_sensitive =
                case_mode == CaseRespect ||
                (case_mode == CaseSmart && u_strcmp(text, lower_text) != 0);
        if (!case_sensitive) {
            SFREE(text);
            text = lower_text;
            og_str = lower_text;
        } else {
            SFREE(lower_text);
        }
        if (!fuzzy) {
            fn = ufzf_exact_match_naive;
        }
        UChar pipeChar[] = {0x007C, 0x0000};
        if (set->size > 0 && !after_bar && u_strcmp(text, pipeChar) == 0) {
            switch_set = false;
            after_bar = true;
            ptr = ustrtok_r(NULL, udelim, &context);
            SFREE(og_str);
            continue;
        }
        after_bar = false;
        UChar exclamationPrefix[] = {0x0021, 0};
        if (uhas_prefix(text, exclamationPrefix, 1)) {
            inv = true;
            fn = ufzf_exact_match_naive;
            text++;
            len--;
        }

        UChar dollarSignChar[] = {0x0024, 0};
        if (u_strcmp(text, dollarSignChar) != 0 && uhas_suffix(text, len, dollarSignChar, 1)) {
            fn = ufzf_suffix_match;
            text[len - 1] = 0;
            len--;
        }

        UChar singleQuoteChar[] = {0x0027, 0};
        UChar caretChar[] = {0x005E, 0};
        if (uhas_prefix(text, singleQuoteChar, 1)) {
            if (fuzzy && !inv) {
                fn = ufzf_exact_match_naive;
                text++;
                len--;
            } else {
                fn = ufzf_fuzzy_match_v2;
                text++;
                len--;
            }
        } else if (uhas_prefix(text, caretChar, 1)) {
            if (fn == ufzf_suffix_match) {
                fn = ufzf_equal_match;
            } else {
                fn = ufzf_prefix_match;
            }
            text++;
            len--;
        }

        if (len > 0) {
            if (switch_set) {
                uappend_pattern(pat_obj, set);
                set = (ufzf_term_set_t *)malloc(sizeof(ufzf_term_set_t));
                set->cap = 0;
                set->size = 0;
            }
            ufzf_string_t *text_ptr = (ufzf_string_t *)malloc(sizeof(ufzf_string_t));
            text_ptr->data = text;
            text_ptr->size = len;
            uappend_set(set, (ufzf_term_t){.fn = fn,
                    .inv = inv,
                    .ptr = og_str,
                    .text = text_ptr,
                    .case_sensitive = case_sensitive});
            switch_set = true;
        } else {
            SFREE(og_str);
        }

        ptr = ustrtok_r(NULL, udelim, &context);
    }
    if (set->size > 0) {
        uappend_pattern(pat_obj, set);
    } else {
        SFREE(set->ptr);
        SFREE(set);
    }
    bool only = true;
    for (size_t i = 0; i < pat_obj->size; i++) {
        ufzf_term_set_t *term_set = pat_obj->ptr[i];
        if (term_set->size > 1) {
            only = false;
            break;
        }
        if (term_set->ptr[0].inv == false) {
            only = false;
            break;
        }
    }
    pat_obj->only_inv = only;
    SFREE(pattern_copy);
    return pat_obj;
}

void fzf_free_pattern(fzf_pattern_t *pattern) {
  if (pattern->ptr) {
    for (size_t i = 0; i < pattern->size; i++) {
      fzf_term_set_t *term_set = pattern->ptr[i];
      for (size_t j = 0; j < term_set->size; j++) {
        fzf_term_t *term = &term_set->ptr[j];
        free(term->ptr);
        free(term->text);
      }
      free(term_set->ptr);
      free(term_set);
    }
    free(pattern->ptr);
  }
  SFREE(pattern);
}

int32_t fzf_get_score(const char *text, fzf_pattern_t *pattern,
                      fzf_slab_t *slab) {
  // If the pattern is an empty string then pattern->ptr will be NULL and we
  // basically don't want to filter. Return 1 for telescope
  if (pattern->ptr == NULL) {
    return 1;
  }

  fzf_string_t input = {.data = text, .size = strlen(text)};
  if (pattern->only_inv) {
    int final = 0;
    for (size_t i = 0; i < pattern->size; i++) {
      fzf_term_set_t *term_set = pattern->ptr[i];
      fzf_term_t *term = &term_set->ptr[0];

      final += CALL_ALG(term, false, input, NULL, slab).score;
    }
    return (final > 0) ? 0 : 1;
  }

  int32_t total_score = 0;
  for (size_t i = 0; i < pattern->size; i++) {
    fzf_term_set_t *term_set = pattern->ptr[i];
    int32_t current_score = 0;
    bool matched = false;
    for (size_t j = 0; j < term_set->size; j++) {
      fzf_term_t *term = &term_set->ptr[j];
      fzf_result_t res = CALL_ALG(term, false, input, NULL, slab);
      if (res.start >= 0) {
        if (term->inv) {
          continue;
        }
        current_score = res.score;
        matched = true;
        break;
      }

      if (term->inv) {
        current_score = 0;
        matched = true;
      }
    }
    if (matched) {
      total_score += current_score;
    } else {
      total_score = 0;
      break;
    }
  }

  return total_score;
}

int32_t ufzf_get_score(const UChar *text, ufzf_pattern_t *pattern,
                      fzf_slab_t *slab) {
    // If the pattern is an empty string then pattern->ptr will be NULL and we
    // basically don't want to filter. Return 1 for telescope
    if (pattern->ptr == NULL) {
        return 1;
    }

    ufzf_string_t input = {.data = text, .size = u_strlen(text)};
    if (pattern->only_inv) {
        int final = 0;
        for (size_t i = 0; i < pattern->size; i++) {
            ufzf_term_set_t *term_set = pattern->ptr[i];
            ufzf_term_t *term = &term_set->ptr[0];

            final += UCALL_ALG(term, false, input, NULL, slab).score;
        }
        return (final > 0) ? 0 : 1;
    }

    int32_t total_score = 0;
    for (size_t i = 0; i < pattern->size; i++) {
        ufzf_term_set_t *term_set = pattern->ptr[i];
        int32_t current_score = 0;
        bool matched = false;
        for (size_t j = 0; j < term_set->size; j++) {
            ufzf_term_t *term = &term_set->ptr[j];
            fzf_result_t res = UCALL_ALG(term, false, input, NULL, slab);
            if (res.start >= 0) {
                if (term->inv) {
                    continue;
                }
                current_score = res.score;
                matched = true;
                break;
            }

            if (term->inv) {
                current_score = 0;
                matched = true;
            }
        }
        if (matched) {
            total_score += current_score;
        } else {
            total_score = 0;
            break;
        }
    }

    return total_score;
}

fzf_position_t *fzf_get_positions(const char *text, fzf_pattern_t *pattern,
                                  fzf_slab_t *slab) {
  // If the pattern is an empty string then pattern->ptr will be NULL and we
  // basically don't want to filter. Return 1 for telescope
  if (pattern->ptr == NULL) {
    return NULL;
  }

  fzf_string_t input = {.data = text, .size = strlen(text)};
  fzf_position_t *all_pos = fzf_pos_array(0);
  for (size_t i = 0; i < pattern->size; i++) {
    fzf_term_set_t *term_set = pattern->ptr[i];
    bool matched = false;
    for (size_t j = 0; j < term_set->size; j++) {
      fzf_term_t *term = &term_set->ptr[j];
      if (term->inv) {
        // If we have an inverse term we need to check if we have a match, but
        // we are not interested in the positions (for highlights) so to speed
        // this up we can pass in NULL here and don't calculate the positions
        fzf_result_t res = CALL_ALG(term, false, input, NULL, slab);
        if (res.start < 0) {
          matched = true;
        }
        continue;
      }
      fzf_result_t res = CALL_ALG(term, false, input, all_pos, slab);
      if (res.start >= 0) {
        matched = true;
        break;
      }
    }
    if (!matched) {
      fzf_free_positions(all_pos);
      return NULL;
    }
  }
  return all_pos;
}

fzf_position_t *ufzf_get_positions(const UChar *text, ufzf_pattern_t *pattern,
                                  fzf_slab_t *slab) {
    // If the pattern is an empty string then pattern->ptr will be NULL and we
    // basically don't want to filter. Return 1 for telescope
    if (pattern->ptr == NULL) {
        return NULL;
    }

    ufzf_string_t input = {.data = text, .size = u_strlen(text)};
    fzf_position_t *all_pos = fzf_pos_array(0);
    for (size_t i = 0; i < pattern->size; i++) {
        ufzf_term_set_t *term_set = pattern->ptr[i];
        bool matched = false;
        for (size_t j = 0; j < term_set->size; j++) {
            ufzf_term_t *term = &term_set->ptr[j];
            if (term->inv) {
                // If we have an inverse term we need to check if we have a match, but
                // we are not interested in the positions (for highlights) so to speed
                // this up we can pass in NULL here and don't calculate the positions
                fzf_result_t res = UCALL_ALG(term, false, input, NULL, slab);
                if (res.start < 0) {
                    matched = true;
                }
                continue;
            }
            fzf_result_t res = UCALL_ALG(term, false, input, all_pos, slab);
            if (res.start >= 0) {
                matched = true;
                break;
            }
        }
        if (!matched) {
            fzf_free_positions(all_pos);
            return NULL;
        }
    }
    return all_pos;
}

void fzf_free_positions(fzf_position_t *pos) {
  if (pos) {
    SFREE(pos->data);
    free(pos);
  }
}

fzf_slab_t *fzf_make_slab(fzf_slab_config_t config) {
  fzf_slab_t *slab = (fzf_slab_t *)malloc(sizeof(fzf_slab_t));
  memset(slab, 0, sizeof(*slab));

  slab->I16.data = (int16_t *)malloc(config.size_16 * sizeof(int16_t));
  memset(slab->I16.data, 0, config.size_16 * sizeof(*slab->I16.data));
  slab->I16.cap = config.size_16;
  slab->I16.size = 0;
  slab->I16.allocated = true;

  slab->I32.data = (int32_t *)malloc(config.size_32 * sizeof(int32_t));
  memset(slab->I32.data, 0, config.size_32 * sizeof(*slab->I32.data));
  slab->I32.cap = config.size_32;
  slab->I32.size = 0;
  slab->I32.allocated = true;

  return slab;
}

fzf_slab_t *fzf_make_default_slab(void) {
  return fzf_make_slab((fzf_slab_config_t){(size_t)100 * 1024, 2048});
}

void fzf_free_slab(fzf_slab_t *slab) {
  if (slab) {
    free(slab->I16.data);
    free(slab->I32.data);
    free(slab);
  }
}
#undef _CRT_SECURE_NO_WARNINGS
