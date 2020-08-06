/**
 *  label.c
 */
#include "label.h"
#include "strl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <limits.h>

/**
    This function initializes the dynamically allocated spreadsheet array.
    @return 0 if successful, -1 if unsuccessful.
*/
int spreadsheet_init() {

    spreadsheet_cap = INITIAL_CAP;

    if ((spreadsheet = (char **) malloc(INITIAL_CAP * sizeof(char *))) == NULL)
        return -1;
    else
        return 0;
}

int spreadsheet_expand() {

    spreadsheet_cap *= 2;
    if ((spreadsheet = (char **) realloc(spreadsheet, spreadsheet_cap * sizeof(char *))) == NULL)
        return -1;
    else
        return 0;
}

char *get_token(char *buffer, char tab_str) {
    char *delimiter;
    int buffer_len = (int) strlen(buffer);
    char *token = (char *) malloc(MAX_COLUMNS * sizeof(char));

    if ((delimiter = strchr(buffer, tab_str)) != NULL) {
        delimiter++;
        int c = 0;
        for (char *i = buffer; i < delimiter - 1; i++) {
            token[c++] = *i;
        }
        token[c] = '\0';
        int delimiter_len = (int) (delimiter - buffer);
        memmove(buffer, delimiter, (size_t) (buffer_len - delimiter_len + 1));
        // buffer_len = strlen(buffer);

    } else if (buffer_len > 0) { // get last token
        int c = 0;
        char *cp = buffer;
        while (*cp) {
            token[c++] = *cp;
            cp++;
        }
        token[c] = '\0';
        buffer[0] = '\0';
        // buffer_len = 0;
    }

    return token;
}

/**
    returns the position of the nth occurrence of a delimiter in a char array
*/
int peek_nth_token(int n, const char *buffer, char delimiter) {

    if (n == 0)
        return 0;

    int i = 0;
    int hit_count = 0;
    int length = (int) strlen(buffer);

    while ((i < length) && (hit_count < n)) {
        if (buffer[i] == delimiter)
            hit_count++;
        i++;
    }
    if (hit_count == n)
        return i - 1;

    return -1;
}

/*

Case-insensitive string compare (strncmp case-insensitive)
- Identical to strncmp except case-insensitive. See: http://www.cplusplus.com/reference/cstring/strncmp/
- Aided/inspired, in part, by: https://stackoverflow.com/a/5820991/4561887

str1    C string 1 to be compared
str2    C string 2 to be compared
num     max number of chars to compare

return:
(essentially identical to strncmp)
INT_MIN  invalid arguments (one or both of the input strings is a NULL pointer)
<0       the first character that does not match has a lower value in str1 than in str2
 0       the contents of both strings are equal
>0       the first character that does not match has a greater value in str1 than in str2

*/
int strncmpci(const char *str1, const char *str2, int num) {
    int ret_code = INT_MIN;

    size_t chars_compared = 0;

    // Check for NULL pointers
    if (!str1 || !str2) {
        goto done;
    }

    // Continue doing case-insensitive comparisons, one-character-at-a-time, of str1 to str2,
    // as long as at least one of the strings still has more characters in it, and we have
    // not yet compared num chars.
    while ((*str1 || *str2) && (chars_compared < num)) {
        ret_code = tolower((int) (*str1)) - tolower((int) (*str2));
        if (ret_code != 0) {
            // The 2 chars just compared don't match
            break;
        }
        chars_compared++;
        str1++;
        str2++;
    }

    done:
    return ret_code;
}

int equals_yes(char *field) {
    return ((strcasecmp(field, "Y") == 0) || (strcasecmp(field, "Yes") == 0));
}

int equals_no(char *field) {
    return ((strcasecmp(field, "N") == 0) || (strcasecmp(field, "NO") == 0));
}

int get_field_contents_from_row(char *contents, int i, int count, char tab_str) {

    int start = 0;
    int stop = 0;

    if (count == 0)
        start = 0;
    else if ((start = peek_nth_token(count, spreadsheet[i], tab_str)) == -1)
        start = (int) strlen(spreadsheet[i]);
    else
        start++;

    if ((stop = peek_nth_token(count + 1, spreadsheet[i], tab_str)) == -1)
        stop = (int) strlen(spreadsheet[i]);

    int length = stop - start;
    strncpy(contents, spreadsheet[i] + start, (size_t) length);
    contents[length] = '\0';

    // check if there's an .tif extension and remove it if so
    if ((length > 4) && ((contents[length - 4]) == '.')) {
        char *suffix = contents + length - 3;
        if (strcmp(suffix, "tif") == 0)
            contents[length - 4] = '\0';
    }

    if (equals_no(contents))
        return 0;
    else
        return length;
}

int duplicate_column_names(const char *cols) {

    char buffer[MAX_COLUMNS];
    unsigned short count = 0;
    char tab_str = TAB;
    char **column_names;

    char min_column[MED], temp[MED];
    bool sorted = true;
    int return_code = 0;

    strcpy(buffer, cols);

    // create an array of column names
    column_names = (char **) malloc(sizeof(char *));
    while (strlen(buffer) > 0) {

        // Keep extracting tokens while the delimiter is present in buffer
        char *token = get_token(buffer, tab_str);
        if (strlen(token)) {
            column_names = (char **) realloc(column_names, sizeof(char *) + count * sizeof(char *));
            //column_names[count] = (char *) malloc(sizeof(char *) + sizeof(token) + 1);
            column_names[count] = (char *) malloc(sizeof(char *) + sizeof(token) + 3);
            strcpy(column_names[count], token);
            count++;
            free(token);
        }
    }

    // sort the list
    for (int i = 0; i < count; i++) {
        int min_index = i;
        strcpy(min_column, column_names[i]);

        for (int j = i + 1; j < count; j++) {
            if (strcmp(column_names[j], min_column) < 0) {
                strcpy(min_column, column_names[j]);
                min_index = j;
            }
        }

        if (i != min_index) {
            strcpy(temp, column_names[i]);
            strcpy(column_names[i], column_names[min_index]);
            strcpy(column_names[min_index], temp);
            sorted = false;
        }
    }

    for (int i = 0; i < count - 1; i++)
        if (strcmp(column_names[i], column_names[i + 1]) == 0)
            return_code = 1;

    for (int i = 0; i < count; i++)
        free(column_names[i]);

    free(column_names);
    return return_code;
}


int parse_spreadsheet(char *buffer, Label_record *labels) {
    unsigned short count = 0;
    bool material = 0;
    bool pcode = 0;

    char tab_str = TAB;
    char contents[MAX_COLUMNS];

    while (strlen(buffer) > 0) {

        // Keep extracting tokens while the delimiter is present in buffer
        char *token = get_token(buffer, tab_str);

        if (strcmp(token, "LABEL") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].label, contents, sizeof(labels[i].label));
            }
        } else if ((strcmp(token, "MATERIAL") == 0) ||
                   (strcmp(token, "PCODE") == 0)) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].material, contents, sizeof(labels[i].material));
            }
            if (strcmp(token, "MATERIAL") == 0)
                material = true;
            else {
                pcode = true;
                printf("Column \"PCODE\" subsituted for \"MATERIAL\"\n");
            }
            if (pcode && material) {
                printf("Found both \"MATERIAL\" and \"PCODE\" column headings. Eliminate one of these.\n");
                return -1;
            }

        } else if (strcmp(token, "TDLINE") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                labels[i].tdline = (char *) malloc(strlen(contents) + 1);
                strlcpy(labels[i].tdline, contents, sizeof(contents));
            }
        } else if (strcmp(token, "ADDRESS") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].address, contents, sizeof(labels[i].address));
            }

        } else if (strcmp(token, "BARCODETEXT") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].barcodetext, contents, sizeof(labels[i].barcodetext));
            }

        } else if (strcmp(token, "BARCODE1") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].barcode1, contents, sizeof(labels[i].barcode1));
            }

        } else if (strcmp(token, "GS1") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].gs1, contents, sizeof(labels[i].gs1));
            }

        } else if (strcmp(token, "GTIN") == 0) {
            if (non_SAP_fields) {
                for (int i = 1; i < spreadsheet_row_number; i++) {
                    get_field_contents_from_row(contents, i, count, tab_str);
                    strlcpy(labels[i].gtin, contents, sizeof(labels[i].gtin));
                }
            } else
                printf("Ignoring column \"%s\"\n", token);


        } else if (strcmp(token, "BOMLEVEL") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].bomlevel, contents, sizeof(labels[i].bomlevel));
            }

        } else if (strcmp(token, "CAUTION") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                if (equals_yes(contents))
                    labels[i].caution = 2;
                else if (equals_no(contents))
                    labels[i].caution = 1;
            }

        } else if (strcmp(token, "CAUTIONSTATE") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].cautionstatement, contents, sizeof(labels[i].cautionstatement));
            }

        } else if ((strcmp(token, "CE0120") == 0)  ||
                  (strcmp(token, "CEMARK") == 0)  ||
                  (strcmp(token, "CE") == 0)) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].cemark, contents, sizeof(labels[i].cemark));
            }

        } else if (strcmp(token, "CONSULTIFU") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                if (equals_yes(contents))
                    labels[i].consultifu = 2;
                else if (equals_no(contents))
                    labels[i].consultifu = 1;
            }

        } else if (strcmp(token, "CONTAINSLATEX") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                if (equals_yes(contents))
                    labels[i].latex = 2;
                else if (equals_no(contents))
                    labels[i].latex = 1;
            }

        } else if (strcmp(token, "COOSTATE") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].coostate, contents, sizeof(labels[i].coostate));
            }

        } else if (strcmp(token, "DESCRIPTION") == 0) {
            if (non_SAP_fields)
                for (int i = 1; i < spreadsheet_row_number; i++) {
                    get_field_contents_from_row(contents, i, count, tab_str);
                    strlcpy(labels[i].description, contents, sizeof(labels[i].description));
                }
            else
                printf("Ignoring column \"%s\"\n", token);

        } else if (strcmp(token, "DISTRIBUTEDBY") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].distby, contents, sizeof(labels[i].distby));
            }

        } else if ((strcmp(token, "DONOTUSEDAM") == 0) ||
                (strcmp(token, "DONOTPAKDAM") == 0)) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                if (equals_yes(contents))
                    labels[i].donotusedamaged = 2;
                else if (equals_no(contents))
                    labels[i].donotusedamaged = 1;
            }

        } else if (strcmp(token, "ECREP") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                if (equals_yes(contents))
                    labels[i].ecrep = 2;
                else if (equals_no(contents))
                    labels[i].ecrep = 1;
            }

        } else if (strcmp(token, "ECREPADDRESS") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].ecrepaddress, contents, sizeof(labels[i].ecrepaddress));
            }

        } else if (strcmp(token, "ELECTROSURIFU") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                if (equals_yes(contents))
                    labels[i].electroifu = 2;
                else if (equals_no(contents))
                    labels[i].electroifu = 1;
            }

        } else if (strcmp(token, "EXPDATE") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                if (equals_yes(contents))
                    labels[i].expdate = 2;
                else if (equals_no(contents))
                    labels[i].expdate = 1;
            }

        } else if (strcmp(token, "FLGRAPHIC") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].flgraphic, contents, sizeof(labels[i].flgraphic));
            }

        } else if (strcmp(token, "KEEPAWAYHEAT") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                if (equals_yes(contents))
                    labels[i].keepawayheat = 2;
                else if (equals_no(contents))
                    labels[i].keepawayheat = 1;
            }

        } else if (strcmp(token, "INSERTGRAPHIC") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].insertgraphic, contents, sizeof(labels[i].insertgraphic));
            }

        } else if (strcmp(token, "KEEPDRY") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                if (equals_yes(contents))
                    labels[i].keepdry = 2;
            }

        } else if (strcmp(token, "LABELGRAPH1") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].labelgraph1, contents, sizeof(labels[i].labelgraph1));
            }

        } else if (strcmp(token, "LABELGRAPH2") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].labelgraph2, contents, sizeof(labels[i].labelgraph2));
            }

        } else if (strcmp(token, "LATEXFREE") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                if (equals_yes(contents))
                    labels[i].latexfree = 2;
                else if (equals_no(contents))
                    labels[i].latexfree = 1;
            }

        } else if (strcmp(token, "LATEXSTATEMENT") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].latexstatement, contents, sizeof(labels[i].latexstatement));
            }

        } else if (strcmp(token, "LEVEL") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].level, contents, sizeof(labels[i].level));
            }

        } else if (strcmp(token, "LOGO1") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].logo1, contents, sizeof(labels[i].logo1));
            }

        } else if (strcmp(token, "LOGO2") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].logo2, contents, sizeof(labels[i].logo2));
            }

        } else if (strcmp(token, "LOGO3") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].logo3, contents, sizeof(labels[i].logo3));
            }

        } else if (strcmp(token, "LOGO4") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].logo4, contents, sizeof(labels[i].logo4));
            }

        } else if (strcmp(token, "LOGO5") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].logo5, contents, sizeof(labels[i].logo5));
            }

        } else if (strcmp(token, "MDR1") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].mdr1, contents, sizeof(labels[i].mdr1));
            }

        } else if (strcmp(token, "MDR2") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].mdr2, contents, sizeof(labels[i].mdr2));
            }

        } else if (strcmp(token, "MDR3") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].mdr3, contents, sizeof(labels[i].mdr3));
            }

        } else if (strcmp(token, "MDR4") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].mdr4, contents, sizeof(labels[i].mdr4));
            }

        } else if (strcmp(token, "MDR5") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                if (get_field_contents_from_row(contents, i, count, tab_str)) {
                    strlcpy(labels[i].mdr5, contents, sizeof(labels[i].mdr5));
                }
            }

        } else if (strcmp(token, "LOTGRAPHIC") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                if (equals_yes(contents)) {
                    labels[i].lotgraphic = 2;
                } else if (equals_no(contents))
                    labels[i].lotgraphic = 1;
            }
        } else if (strcmp(token, "LTNUMBER") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strncpy(labels[i].ltnumber, contents, sizeof(labels[0].ltnumber));
            }

        } else if (strcmp(token, "IPN") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strncpy(labels[i].ipn, contents, sizeof(labels[0].ipn));
            }

        } else if (strcmp(token, "MANINBOX") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                if (equals_yes(contents))
                    labels[i].maninbox = 2;
                else if (equals_no(contents))
                    labels[i].maninbox = 1;
            }
        } else if (strcmp(token, "MANUFACTUREDBY") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].manufacturedby, contents, sizeof(labels[i].manufacturedby));
            }

        } else if (strcmp(token, "MANUFACTURER") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                if (equals_yes(contents)) {
                    labels[i].manufacturer = 2;
                } else if (equals_no(contents))
                    labels[i].manufacturer = 1;
            }
        } else if (strcmp(token, "MFGDATE") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                if (equals_yes(contents)) {
                    labels[i].mfgdate = 2;
                } else if (equals_no(contents))
                    labels[i].mfgdate = 1;
            }
        } else if (strcmp(token, "NORESTERILE") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                if (equals_yes(contents)) {
                    labels[i].noresterilize = 2;
                } else if (equals_no(contents))
                    labels[i].noresterilize = 1;
            }
        } else if (strcmp(token, "NONSTERILE") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                if (equals_yes(contents)) {
                    labels[i].nonsterile = 2;
                } else if (equals_no(contents))
                    labels[i].nonsterile = 1;
            }
        } else if (strcmp(token, "OLDLABEL") == 0) {
            if (non_SAP_fields)
                for (int i = 1; i < spreadsheet_row_number; i++) {
                    get_field_contents_from_row(contents, i, count, tab_str);
                    strlcpy(labels[i].oldlabel, contents, sizeof(labels[i].oldlabel));
                }
            else
                printf("Ignoring column \"%s\"\n", token);

        } else if (strcmp(token, "OLDTEMPLATE") == 0) {
            if (non_SAP_fields)
                for (int i = 1; i < spreadsheet_row_number; i++) {
                    get_field_contents_from_row(contents, i, count, tab_str);
                    strlcpy(labels[i].oldtemplate, contents, sizeof(labels[i].oldtemplate));
                }
            else
                printf("Ignoring column \"%s\"\n", token);

        } else if (strcmp(token, "PREVLABEL") == 0) {
            if (non_SAP_fields)
                for (int i = 1; i < spreadsheet_row_number; i++) {
                    get_field_contents_from_row(contents, i, count, tab_str);
                    strlcpy(labels[i].prevlabel, contents, sizeof(labels[i].prevlabel));
                }
            else
                printf("Ignoring column \"%s\"\n", token);

        } else if (strcmp(token, "PREVTEMPLATE") == 0) {
            if (non_SAP_fields)
                for (int i = 1; i < spreadsheet_row_number; i++) {
                    get_field_contents_from_row(contents, i, count, tab_str);
                    strlcpy(labels[i].prevtemplate, contents, sizeof(labels[i].prevtemplate));
                }
            else
                printf("Ignoring column \"%s\"\n", token);

        } else if (strcmp(token, "PATENTSTA") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].patentstatement, contents, sizeof(labels[i].patentstatement));
            }

        } else if (strcmp(token, "PHTDEHP") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                if (equals_yes(contents)) {
                    labels[i].phtdehp = 2;
                } else if (equals_no(contents))
                    labels[i].phtdehp = 1;
            }

        } else if (strcmp(token, "PHTBBP") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                if (equals_yes(contents)) {
                    labels[i].phtbbp = 2;
                } else if (equals_no(contents))
                    labels[i].phtbbp = 1;
            }

        } else if (strcmp(token, "PHTDINP") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                if (equals_yes(contents))
                    labels[i].phtdinp = 2;
                else if (equals_no(contents))
                    labels[i].phtdinp = 1;
            }

        } else if (strcmp(token, "PVCFREE") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                if (equals_yes(contents))
                    labels[i].pvcfree = 2;
                else if (equals_no(contents))
                    labels[i].pvcfree = 1;
            }
        } else if (strcmp(token, "QUANTITY") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].quantity, contents, sizeof(labels[i].quantity));
            }

        } else if (strcmp(token, "REF") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                if (equals_yes(contents)) {
                    labels[i].ref = 2;
                } else if (equals_no(contents))
                    labels[i].ref = 1;
            }

        } else if (strcmp(token, "REFNUMBER") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                if (equals_yes(contents)) {
                    labels[i].refnumber = 2;
                } else if (equals_no(contents))
                    labels[i].refnumber = 1;
            }

        } else if (strcmp(token, "REUSABLE") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                if (equals_yes(contents)) {
                    labels[i].reusable = 2;
                } else if (equals_no(contents))
                    labels[i].reusable = 1;
            }
        } else if (strcmp(token, "REVISION") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].revision, contents, sizeof(labels[i].revision));
            }

        } else if (strcmp(token, "LABEL_RELEASE_DATE") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].release, contents, sizeof(labels[i].release));
            }

        } else if (strcmp(token, "RXONLY") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                if (equals_yes(contents)) {
                    labels[i].rxonly = 2;
                } else if (equals_no(contents))
                    labels[i].rxonly = 1;
            }

        } else if (strcmp(token, "SINGLEUSE") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                if (equals_yes(contents)) {
                    labels[i].singleuseonly = 2;
                } else if (equals_no(contents))
                    labels[i].singleuseonly = 1;
            }

        } else if (strcmp(token, "SERIAL") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                if (equals_yes(contents)) {
                    labels[i].serial = 2;
                } else if (equals_no(contents))
                    labels[i].serial = 1;
            }

        } else if (strcmp(token, "SINGLEPATIENTUSE") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                if (equals_yes(contents)) {
                    labels[i].singlepatientuse = 2;
                } else if (equals_no(contents))
                    labels[i].singlepatientuse = 1;
            }

        } else if (strcmp(token, "SIZE") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].size, contents, sizeof(labels[i].size));
            }

        } else if (strcmp(token, "SIZELOGO") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                if (equals_yes(contents)) {
                    labels[i].sizelogo = 2;
                } else if (equals_no(contents))
                    labels[i].sizelogo = 1;
            }

        } else if (strcmp(token, "STERILITYTYPE") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].sterilitytype, contents, sizeof(labels[i].sterilitytype));
            }

        } else if (strcmp(token, "STERILESTA") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].sterilitystatement, contents, sizeof(labels[i].sterilitystatement));
            }

        } else if (strcmp(token, "TEMPRANGE") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].temprange, contents, sizeof(labels[i].temprange));
            }

        } else if ((strcmp(token, "TEMPLATENUMBER") == 0) ||
                (strcmp(token, "TEMPLATE") == 0)) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].template, contents, sizeof(labels[i].template));
            }

        } else if (strcmp(token, "TFXLOGO") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                if (equals_yes(contents)) {
                    labels[i].tfxlogo = 2;
                } else if (equals_no(contents))
                    labels[i].tfxlogo = 1;
            }

        } else if (strcmp(token, "VERSION") == 0) {
            for (int i = 1; i < spreadsheet_row_number; i++) {
                get_field_contents_from_row(contents, i, count, tab_str);
                strlcpy(labels[i].version, contents, sizeof(labels[i].version));
            }
        } else {
            if (strlen(token) > 0) {
                if (strcmp(token, "CAUTIONSTATEMENT") == 0)
                    printf("Change \"%s\" to \"CAUTIONSTATE.\" ", token);
                printf("Ignoring column \"%s\"\n", token);
            }
        }
        count++;
        free(token);
    }
    return count;
}

int sort_labels(Label_record *labels) {

    Label_record min_label;
    bool sorted = true;

    for (int i = 1; i < spreadsheet_row_number; i++) {
        int min_index = i;
        min_label = labels[i];

        for (int j = i + 1; j < spreadsheet_row_number; j++) {
            if (strcmp(labels[j].label, min_label.label) < 0) {
                min_label = labels[j];
                min_index = j;
            }
        }

        if (i != min_index) {
            swap_label_records(labels, i, min_index);
            sorted = false;
        }
    }
    return sorted;
}

void swap_label_records(Label_record *labels, int i, int min_index) {
    Label_record temp = labels[i];
    labels[i] = labels[min_index];
    labels[min_index] = temp;
}
