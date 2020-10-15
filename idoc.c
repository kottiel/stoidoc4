/**
 *  idoc.c contains the main and supporting functions to read a text-delimited
 *  file that contains label column headers and row label data and generate an
 *  idoc file.
 */
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "label.h"
#include "strl.h"
#include "lookup.h"

/* end of line new line character                                        */
#define LF '\n'

/* length of '_idoc (stoidoc 2.0)->txt' extension                        */
#define FILE_EXT_LEN   36

/* length of GTIN-13                                                     */
#define GTIN_13        13

/* divide a 14-digit GTIN by this value to isolate its first digit       */
#define GTIN_14_DIGIT 10000000000000

/* divide a 13-digit GTIN by this value to isolate its first digit       */
#define GTIN_13_DIGIT 1000000000000

/* divide a GTIN by these values to isolate company prefix               */
#define GTIN_14_CPNY_DIVISOR 1000000
#define GTIN_13_CPNY_DIVISOR 100000

/* the number of spaces to indent the TDline lines                       */
#define TDLINE_INDENT  61

/* normal graphics folder path                                           */
#define GRAPHICS_PATH  "T:\\MEDICAL\\NA\\RTP\\TEAM CENTER\\TEMPLATES\\GRAPHICS\\"

/* alternate graphics folder path                                        */
#define ALT_GRAPHICS_PATH  "C:\\Users\\jkottiel\\OneDrive - Teleflex Incorporated\\1 - Teleflex\\Labeling Resources\\Personal Graphics\\"

/* determine the graphics path at run time                               */
bool alt_path = false;

/* whether or not to include non-SAP fields in IDoc                      */
bool non_SAP_fields = false;

/* global variable that holds the spreadsheets specific column headings  */
char **spreadsheet;

/* tracks the spreadsheet column headings capacity                       */
int spreadsheet_cap = 0;

/* tracks the actual number of label rows in the spreadsheet             */
int spreadsheet_row_number = 0;

/* global variable to track the idoc sequence number                     */
int sequence_number = 1;

char prev_material[MED] = {0};

/** a global struct variable of IDoc sequence numbers                    */
struct control_numbers {
    char ctrl_num[8];
    int matl_seq_number;
    int labl_seq_number;
    int tdline_seq_number;
    int char_seq_number;
};

/** defining the struct variable as a new type for convenience           */
typedef struct control_numbers Ctrl;

/**
    Returns true (non-zero) if character-string parameter represents
    a signed or unsigned floating-point number. Otherwise returns
    false (zero).
    @param str is the numeric string value to evaluate
    @return true if str is a number, false otherwise
 */
int isNumeric(const char *str) {

    if (str == NULL || str[0] == '\0')
        return 0;

    int i = 0;
    while (str[i] != '\0')
        if (isdigit(str[i]) == 0)
            return 0;
        else
            i++;
    return 1;
}

/**
    Returns true (non-zero) if character-string parameter contains any
    spaces. Otherwise returns false (zero).
    @param str is the string to evaluate
    @return true if str contains spaces, false otherwise
 */
int containsSpaces(const char *str) {

    if (str == NULL || str[0] == '\0')
        return 0;

    int i = 0;
    while (str[i] != '\0')
        if (str[i] == ' ')
            return 1;
        else
            i++;
    return 0;
}

/**
    determine the check digit of a GTIN-13 format value
    @param lp is the GTIN-13 value to calculate a check digit for
    @return a check digit
 */
int checkDigit(const long long *llp) {

    long long gtin = *llp;
    gtin = gtin / 10;
    short digit;
    int sum = 0;

    while (gtin > 0) {
        digit = (short) (gtin % 10);
        sum += 3 * digit;
        gtin /= 10;
        digit = (short) (gtin % 10);
        sum += 1 * digit;
        gtin /= 10;
    }

    return (sum % 10 == 0 ? 0 : ((((sum / 10) * 10) + 10) - sum));
}

/**
    check to ensure SAP Characteristic Value Lookup table is alphabetized
    and that all entries are unique
    @return true if all entries are alphabetized and there are no duplicates.
*/
bool check_lookup_array() {
    for (int i = 0; i < lookupsize - 1; i++) {
        if (strcasecmp(lookup[i][0], lookup[i + 1][0]) >= 0) {
            printf("Correct values in SAP Characteristics array: %d) %s, %d) %s\n", i, lookup[i][0], i + 1,
                   lookup[i + 1][0]);
            return 0;
        }
    }
    return 1;
}

/**
    perform a binary search in the lookup array to find the SAP
    characteristic definition given the characteristic value
    @param needle is the search term
    @return the corresponding SAP lookup value, or null if not found
*/
char *sap_lookup(char *needle) {

    int start = 0;
    int end = lookupsize - 1;
    int middle = 0;
    int result = 0;

    bool exit = false;
    char *haystack;

    while (!exit) {
        if (middle != (end - start) / 2 + start)
            middle = (end - start) / 2 + start;
        else
            exit = true;
        haystack = lookup[middle][0];

        result = strcasecmp(needle, haystack);
        if (result == 0) {
            return lookup[middle][1];
        } else if (result < 0) {
            end = middle - 1;
        } else {
            start = middle + 1;
        }
    }
    return NULL;
}

/**
    reads a tab-delimited Excel spreadsheet into memory, dynamically
    allocating memory to hold the rows as needed. All CRLF and LF are
    replaced with null characters to delimit the end of the spreadsheet
    row / string. Rows containing just tab characters are ignored.
    @param fp points to the input file
*/
void read_spreadsheet(FILE *fp) {

    char c;
    char buffer[MAX_COLUMNS];
    bool line_not_empty = false;
    int i = 0;

    while ((c = (char) fgetc(fp)) != EOF) {
        if (c == LF) {
            //check if preceded by "##" - in that case do nothing
            if ((i > 1) && buffer[i - 1] != '#' || buffer[i - 2] != '#') {
                buffer[i] = '\0';
                if (line_not_empty) {
                    if (spreadsheet_row_number >= spreadsheet_cap) {
                        spreadsheet_expand();
                    }
                    spreadsheet[spreadsheet_row_number] =
                            (char *) malloc(i * sizeof(char) + 2);
                    strlcpy(spreadsheet[spreadsheet_row_number], buffer, MAX_COLUMNS);
                    spreadsheet_row_number++;
                }
                i = 0;
                line_not_empty = false;
            }
        } else {
            buffer[i++] = c;
            if (c != '\t')
                if (c != '\r')
                    line_not_empty = true;
        }
    }
}

/**
    prints a specified number of spaces to a file stream
    @param fpout points to the output file
    @param n is the number of spaces to print
*/
void print_spaces(FILE *fpout, int n) {
    for (int i = 0; i < n; i++)
        fprintf(fpout, " ");
}

/**
    prints a portion of an idoc field record based on the passed parameter
    @param fpout points to the output file
    @param graphic is the name of the graphic to append to the path and to print
*/
void print_graphic_path(FILE *fpout, char *graphic) {
    int n = 0;
    if (alt_path) {
        fprintf(fpout, "%s", ALT_GRAPHICS_PATH);
        n = 255 - ((int) strlen(ALT_GRAPHICS_PATH) + (int) strnlen(graphic, MED + 1));
    } else {
        fprintf(fpout, "%s", GRAPHICS_PATH);
        n = 255 - ((int) strlen(GRAPHICS_PATH) + (int) strnlen(graphic, MED + 1));
    }
    fprintf(fpout, "%s", graphic);

    for (int i = 0; i < n; i++)
        fprintf(fpout, " ");
}

/**
    prints a specified number of spaces to a file stream
    @param fpout points to the output file
    @param ctrl_num
    @param n is the number of spaces to print
*/
void print_Z2BTLC01000(FILE *fpout, char *ctrl_num, int char_seq_number) {
    fprintf(fpout, "Z2BTLC01000");
    print_spaces(fpout, 19);
    fprintf(fpout, "500000000000");
    // cols 22-29 - 7 digit control number?
    fprintf(fpout, "%s", ctrl_num);
    fprintf(fpout, "%06d", sequence_number++);
    fprintf(fpout, "%06d", char_seq_number);
    fprintf(fpout, CHAR_REC);
}

void print_info_column_header(FILE *fpout, char *col_name, char *col_value, Ctrl *idoc) {

    if (strlen(col_value) > 0) {
        if (equals_no(col_value) > 0) // it is blank, but should be treated as "NO"
            strcpy(col_value, "NO");

        print_Z2BTLC01000(fpout, idoc->ctrl_num, idoc->char_seq_number);
        fprintf(fpout, "%-30s", col_name);
        fprintf(fpout, "%-30s", col_value);
        fprintf(fpout, "%-255s", col_value);
        fprintf(fpout, "\n");
    }
}

/**
    print a passed column-field that contains a "Y" / "Yes", "N" or "NO," (case insensitive),
    or a value requiring SAP lookup and substitution, or a value that translates
    into a graphic name with a .tif suffix.
    @param fpout points to the output file
    @param col_name is the column name from the spreadsheet
    @param col_value is the contents of the labels cell beneath the column name
    @param default_yes is the graphic item to print if col_value is a Y / Yes
    @param idoc contains the sequence and control numbers struct
 */
void print_graphic_column_header(FILE *fpout, char *col_name, char *col_value, char *default_yes, Ctrl *idoc) {

    char cell_contents[MED];
    strncpy(cell_contents, col_value, MED - 1);

    // only print a record if the cell_contents contains a value
    if (strlen(cell_contents) > 0) {

        print_Z2BTLC01000(fpout, idoc->ctrl_num, idoc->char_seq_number);
        fprintf(fpout, "%-30s", col_name);
        fprintf(fpout, "%-30s", col_value);

        if (equals_yes(col_value)) {
            strncpy(cell_contents, default_yes, MED - 1);
            print_graphic_path(fpout, cell_contents);
        } else if (equals_no(col_value)) {
            print_graphic_path(fpout, "blank-01.tif");
        } else {

            // graphic_name will be converted to its SAP lookup value from the static lookup array
            // or, if there is no lookup value, graphic_name itself will be used
            char *gnp = sap_lookup(col_value);

            if (gnp) {
                char graphic_name[LRG];
                strncpy(graphic_name, gnp, LRG - 1);
                print_graphic_path(fpout, strcat(graphic_name, ".tif"));
            } else {
                print_graphic_path(fpout, strcat(cell_contents, ".tif"));
            }
        }
        fprintf(fpout, "\n");
    }
}

/**
    print a passed column-field that contains a "Y" / "Yes", "N" or "NO," (case insensitive),
    or a value requiring SAP lookup and substitution, or a value that translates
    into a graphic name with a .tif suffix.
    @param fpout points to the output file
    @param col_name is the column name from the spreadsheet
    @param col_value is the contents of the labels cell beneath the column name
    @param default_yes is the graphic item to print if col_value is a Y / Yes
    @param idoc contains the sequence and control numbers struct
 */
void print_blank_graphic_column_header(FILE *fpout, char *col_name, char *col_value, Ctrl *idoc) {

    char cell_contents[MED];
    strncpy(cell_contents, col_value, MED - 1);

    print_Z2BTLC01000(fpout, idoc->ctrl_num, idoc->char_seq_number);
    fprintf(fpout, "%-30s", col_name);
    fprintf(fpout, "%-30s", col_value);

    print_graphic_path(fpout, "");
    fprintf(fpout, "\n");
}

void print_info_lookup_column_header(FILE *fpout, char *col_name, char *col_value, char *lookup, Ctrl *idoc) {

    char cell_contents[MED];
    strncpy(cell_contents, col_value, MED - 1);

    print_Z2BTLC01000(fpout, idoc->ctrl_num, idoc->char_seq_number);
    fprintf(fpout, "%-30s", col_name);
    fprintf(fpout, "%-30s", col_value);
    fprintf(fpout, "%-255s", lookup);
    fprintf(fpout, "\n");
}

/**
    print a passed column-field that is in the special GRAPHICS01 - GRAPHICS14 category and is defined as
    boolean in the Label_record. It contains a "Y" or a "N." If "Y," print the hard-coded value associated with
    the graphic and a .tif suffix. Otherwise, print a "blank-01.tif" record.
    @param fpout points to the output file
    @param col_name is the column header
    @param value is the boolean value of the column-field
    @param graphic_name is the graphic to print if the boolean is true
    @param idoc is the struct that tracks the control numbers
 */
void print_graphic0x_record(FILE *fpout, int *g_cnt, char *graphic_name, unsigned int value, Ctrl *idoc) {

    if (value == 2) {
        char g_cnt_str[03];
        char graphic[10] = "GRAPHIC0";
        print_Z2BTLC01000(fpout, idoc->ctrl_num, idoc->char_seq_number);
        sprintf(g_cnt_str, "%d", (*g_cnt)++);
        fprintf(fpout, "%-30s", strcat(graphic, g_cnt_str));
        fprintf(fpout, "%-30s", "Y");
        print_graphic_path(fpout, graphic_name);
        fprintf(fpout, "\n");
    } else if (value == 3) {
        char g_cnt_str[03];
        char graphic[10] = "GRAPHIC0";
        char F_graphic_name[LRG] = "F_";
        print_Z2BTLC01000(fpout, idoc->ctrl_num, idoc->char_seq_number);
        sprintf(g_cnt_str, "%d", (*g_cnt)++);
        fprintf(fpout, "%-30s", strcat(graphic, g_cnt_str));
        fprintf(fpout, "%-30s", "F_Y");
        strcat(F_graphic_name, graphic_name);
        print_graphic_path(fpout, F_graphic_name);
        fprintf(fpout, "\n");
    }  else if (value == 4) {
        char g_cnt_str[03];
        char graphic[10] = "GRAPHIC0";
        char ISO_graphic_name[LRG] = "ISO_";
        print_Z2BTLC01000(fpout, idoc->ctrl_num, idoc->char_seq_number);
        sprintf(g_cnt_str, "%d", (*g_cnt)++);
        fprintf(fpout, "%-30s", strcat(graphic, g_cnt_str));
        fprintf(fpout, "%-30s", "ISO_Y");
        strcat(ISO_graphic_name, graphic_name);
        print_graphic_path(fpout, ISO_graphic_name);
        fprintf(fpout, "\n");
    }
}

/**
    print a passed column-field that is defined as boolean in the Label_record. It contains a "Y" or a "N."
    If "Y," print the hard-coded value associated with the graphic and a .tif suffix. Otherwise, print a
    "blank-01.tif" record.
    @param fpout points to the output file
    @param col_name is the column header
    @param value is the boolean value of the column-field
    @param graphic_name is the graphic to print if the boolean is true
    @param idoc is the struct that tracks the control numbers
 */
void print_boolean_record(FILE *fpout, char *col_name, int value, char *graphic_name, Ctrl *idoc) {
    if (value) {
        print_Z2BTLC01000(fpout, idoc->ctrl_num, idoc->char_seq_number);
        fprintf(fpout, "%-30s", col_name);

        if (value == 2) {
            fprintf(fpout, "%-30s", "Y");
            print_graphic_path(fpout, graphic_name);
        }
        else if (value == 3) {
            char F_graphic_name[LRG] = "F_";
            strcat(F_graphic_name, graphic_name);
            fprintf(fpout, "%-30s", "F_Y");
            print_graphic_path(fpout, F_graphic_name);
        } else if (value == 4) {
            char ISO_graphic_name[LRG] = "ISO_";
            strcat(ISO_graphic_name, graphic_name);
            fprintf(fpout, "%-30s", "ISO_Y");
            print_graphic_path(fpout, ISO_graphic_name);
        } else {
            fprintf(fpout, "%-30s", "N");
            print_graphic_path(fpout, "blank-01.tif");
        }
        fprintf(fpout, "\n");
    }
}

/**
    print a passed column-field that is defined as boolean in the Label_record. It contains a "Y" or a "N."
    If "Y," print just a "Yes." Otherwise, print just a "No."
    @param fpout points to the output file
    @param col_name is the column header
    @param value is the boolean value of the column-field
    @param graphic_name is the graphic to print if the boolean is true
    @param idoc is the struct that tracks the control numbers
 */
void print_boolean_column_header(FILE *fpout, char *col_name, bool value, Ctrl *idoc) {

    print_Z2BTLC01000(fpout, idoc->ctrl_num, idoc->char_seq_number);
    fprintf(fpout, "%-30s", col_name);

    if (value) {
        fprintf(fpout, "%-30s", "Y");
        print_graphic_path(fpout, "Yes");
    } else {
        fprintf(fpout, "%-30s", "N");
        print_graphic_path(fpout, "No");
    }
    fprintf(fpout, "\n");
}

/**
    prints the IDoc control record
    @param fpout points to the output file
*/
int print_control_record(FILE *fpout, Ctrl *idoc) {

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    // line 1
    fprintf(fpout, "EDI_DC40  500000000000");
    // cols 22-29 - 7 digit control number?
    fprintf(fpout, "%s", idoc->ctrl_num);
    // BarTender ibtdoc release
    fprintf(fpout, "740");
    fprintf(fpout, " 3012  Z1BTDOC");
    print_spaces(fpout, 53);
    fprintf(fpout, "ZSC_BTEND");
    print_spaces(fpout, 40);
    fprintf(fpout, "SAPMEP    LS  MEPCLNT500");
    print_spaces(fpout, 91);
    fprintf(fpout, "I041      US  BARTENDER");
    print_spaces(fpout, 92);
    fprintf(fpout, "%d%02d%02d%02d%02d%02d", tm.tm_year + 1900, tm.tm_mon + 1,
            tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    print_spaces(fpout, 112);
    fprintf(fpout, "Material_EN");
    print_spaces(fpout, 9);
    fprintf(fpout, "\n");

    return 0;
}

/**
    prints the remaining IDoc records based on the number
    of label records.
    @param fpout points to the output file
    @param labels is the array of label records
    @param cols is the column header struct that contains all of the column names
    @param record is the record number being processed
    @param idoc is a Ctrl structure containing sequence numbers
    @return true if a label_idoc_record was printed successfully
*/
int print_label_idoc_records(FILE *fpout, Label_record *labels, int record, Ctrl *idoc) {

    // Print the records for a given IDOC (labels[record])

    // temporary variables to examine field contents
    char graphic_val[MED];
    //char prev_material[MED] = {0};

    // MATERIAL record (optional)
    // (this is skipped if the previous material record is the same)
    if ((strlen(labels[record].material) > 0)) {

        // check whether it's a new material
        if (strcmp(prev_material, labels[record].material) != 0) {

            // new material record
            fprintf(fpout, "Z2BTMH01000");
            print_spaces(fpout, 19);
            fprintf(fpout, "500000000000");
            // cols 22-29 - 7 digit control number?
            fprintf(fpout, "%s", idoc->ctrl_num);
            fprintf(fpout, "%06d", sequence_number);

            // every NEW material number carries over the sequence_number
            idoc->matl_seq_number = sequence_number - 1;
            idoc->labl_seq_number = sequence_number;
            fprintf(fpout, "%06d", idoc->matl_seq_number);
            sequence_number++;

            fprintf(fpout, MATERIAL_REC);
            fprintf(fpout, "%-18s", labels[record].material);
            fprintf(fpout, "\n");
            strlcpy(prev_material, labels[record].material, LRG);
        }
    }
    // LABEL record (required). If the contents of .label are not "LBL", program aborts.
    char graphic_val_shrt[4] = {0};
    strncpy(graphic_val_shrt, labels[record].label, 3);
    if (strcmp(graphic_val_shrt, "LBL") != 0) {
        printf("The first 3 characters of the record are not \"LBL\", record %d.\n", record);
        return 0;
    } else {
        fprintf(fpout, "Z2BTLH01000");
        print_spaces(fpout, 19);
        fprintf(fpout, "500000000000");

        // cols 22-29 - 7 digit control number?
        fprintf(fpout, "%s", idoc->ctrl_num);
        fprintf(fpout, "%06d", sequence_number);
        fprintf(fpout, "%06d", idoc->labl_seq_number);
        idoc->tdline_seq_number = sequence_number;
        idoc->char_seq_number = sequence_number;
        sequence_number++;
        fprintf(fpout, LABEL_REC);
        fprintf(fpout, "%-18s", labels[record].label);
        fprintf(fpout, "\n");
    }

    // TDLINE record(s) (optional) - repeat as many times as there are "##"

    memcpy(graphic_val, labels[record].tdline, 4);

    if ((strlen(graphic_val) > 0) &&
        (strcasecmp(graphic_val, "n/a") != 0) &&
        (equals_no(graphic_val) != 1)) {

        //* get the first token *//*
        int tdline_count = 0;

        char *token = labels[record].tdline;

        //check for and remove any leading...
        if (token[0] == '\"')
            memmove(token, token + 1, (int) strlen(token));

        // ...and/or trailing quotes
        if (token[(int) strlen(token) - 1] == '\"')
            token[(int) strlen(token) - 1] = '\0';

        // and convert all instances of double quotes to single quotes
        char *a;
        while (a = strstr(token, "\"\""))
            memmove(a, a + 1, (int) strlen(a));

        while (strlen(token) > 0) {
            fprintf(fpout, "Z2BTTX01000");
            print_spaces(fpout, 19);
            fprintf(fpout, "500000000000");
            // cols 22-29 - 7 digit control number?
            fprintf(fpout, "%s", idoc->ctrl_num);
            fprintf(fpout, "%06d", sequence_number++);
            fprintf(fpout, "%06d", idoc->tdline_seq_number);
            fprintf(fpout, TDLINE_REC);
            fprintf(fpout, "GRUNE  ENMATERIAL  ");
            fprintf(fpout, "%s", labels[record].label);
            print_spaces(fpout, TDLINE_INDENT);

            char *dpos = strstr(token, "##");

            if (dpos != NULL) {
                *dpos = '\0';
                fprintf(fpout, "%s", token);
                fprintf(fpout, "##");
                print_spaces(fpout, 70 - (int) (strlen(token) - 2));

                // get the next segment of label record, after the "##"
                token = dpos + (int) strlen("##");
            } else {
                fprintf(fpout, "%-74s", token);
                token[0] = '\0';
            }
            if (tdline_count == 0)
                fprintf(fpout, "*");
            else
                fprintf(fpout, "/");
            tdline_count++;
            fprintf(fpout, "\n");
        }
    }

    // TEMPLATENUMBER record (required)
    if (labels[record].template) {
        print_info_column_header(fpout, "TEMPLATENUMBER", labels[record].template, idoc);
    } else {
        printf("Missing template number in record %d. Aborting.\n", record);
        return 0;
    }

    // REVISION record (optional)
    if (labels[record].revision) {
        int rev = 0;
        if ((sscanf(labels[record].revision, "R%d", &rev) == 1) && rev >= 0 && rev <= 99) {
            print_info_column_header(fpout, "REVISION", labels[record].revision, idoc);
        } else
            printf("Invalid revision value \"%s\" in record %d. REVISION record skipped.\n",
                   labels[record].revision, record);
    }

    // LABEL_RELEASE_DATE record
    if (strlen(labels[record].release) > 0) {
        int year = 0;
        int month = 0;
        if ((sscanf(labels[record].release, "%d-%d", &year, &month) == 2) && year > 2019  && month > 0 && month < 13) {
            print_info_column_header(fpout, "LABEL_RELEASE_DATE", labels[record].release, idoc);
        } else
            printf("Invalid release date value \"%s\" in record %d. LABEL_RELEASE_DATE record skipped.\n",
                   labels[record].release, record);
    }

    // SIZE record (optional)
    memcpy(graphic_val, labels[record].size, MED);

    if ((strlen(labels[record].size) > 0) && (!equals_no(graphic_val))) {
        char *token = labels[record].size;

        //check for and remove any leading...
        if (token[0] == '\"')
            memmove(token, token + 1, strlen(token));

        // ...and/or trailing quotes
        if (token[strlen(token) - 1] == '\"')
            token[strlen(token) - 1] = '\0';

        // and convert all instances of double quotes to single quotes
        char *a;
        int diff;
        while ((a = strstr(token, "\"\"")) != NULL) {
            diff = (int) (a - token);
            memmove(token + diff, token + diff + 1, strlen(token) - 1);
        }

        // size name will be checked against its SAP lookup value.
        // just in case there's a matching entry...
        char *gnp = sap_lookup(labels[record].size);
        if (gnp != NULL)
            print_info_lookup_column_header(fpout, "SIZE", labels[record].size, gnp, idoc);
        else
            print_info_column_header(fpout, "SIZE", labels[record].size, idoc);
    }

    /** LEVEL record (optional) */

    if ((strlen(labels[record].level) > 0) && (!equals_no(labels[record].level))) {

        // level name will be checked against its SAP lookup value.
        // if it's not in there, it'll be reported as such. Otherwise, the  (but will not be changed).
        char *gnp = sap_lookup(labels[record].level);
        if (gnp == NULL)
            printf("Level value \"%s\" in record %d is not a standard LEVEL value. Please check it.\n",
                   labels[record].level, record);

        print_info_lookup_column_header(fpout, "LEVEL", labels[record].level, gnp, idoc);

    }

    /** QUANTITY record (optional) */
    if ((labels[record].quantity) && (!equals_no(labels[record].quantity))) {
        print_info_column_header(fpout, "QUANTITY", labels[record].quantity, idoc);
    }

    /** BARCODETEXT record (optional) */
    char *endptr;
    if ((strlen(labels[record].barcodetext) > 0) && (!equals_no(labels[record].barcodetext))) {

        if (isNumeric(labels[record].barcodetext)) {

            // convert string to long long integer to verify GTIN length and check digit
            long long gtin = strtoll(labels[record].barcodetext, &endptr, 10);
            int gtin_ctry_prefix;
            int gtin_cpny_prefix;

            // 14-digit GTIN - verify the checkDigit
            if ((strlen(labels[record].barcodetext) == GTIN_13 + 1)) {
                if (gtin % 10 != checkDigit(&gtin)) {
                    printf("Invalid GTIN check digit \"%s\" in record %d.\n", labels[record].barcodetext, record);
                }
                gtin_ctry_prefix = (int) (gtin / GTIN_14_DIGIT);
                gtin_cpny_prefix = (int) ((gtin - (gtin_ctry_prefix * GTIN_14_DIGIT)) / GTIN_14_CPNY_DIVISOR);

            } else if (strlen(labels[record].barcodetext) == GTIN_13) {
                gtin_ctry_prefix = (int) (gtin / GTIN_13_DIGIT);
                gtin_cpny_prefix = (int) ((gtin - (gtin_ctry_prefix * GTIN_13_DIGIT)) / GTIN_13_CPNY_DIVISOR);
            } else {
                printf("Invalid GTIN check digit or length \"%s\" in record %d.\n", labels[record].barcodetext, record);
            }

            // verify GTIN prefixes if it's nonzero (otherwise it's just a placeholder)
            // verify the GTIN prefixes (country: 0, 1, 2, 3, company: 4026704 or 5060112)
            if ((gtin_ctry_prefix > 4) || (gtin != 0) && (gtin_cpny_prefix != 4026704 && gtin_cpny_prefix != 5060112))
                printf("Invalid GTIN prefix \"%d\" in record %d.\n", gtin_cpny_prefix, record);

            // GTIN non-numeric so we'll report that it's non-numeric before printing.
        } else {
            printf("Nonnumeric GTIN \"%s\" in record %d. \n", labels[record].barcodetext, record);
        }
        print_info_column_header(fpout, "BARCODETEXT", labels[record].barcodetext, idoc);
    }

    /** GTIN record (optional) - this is a non-SAP field that prints only if [-n] flag is present at runtime */
    if (labels[record].gtin) {
        if (non_SAP_fields) {
            char gtin_digit1[2] = {0};
            strncpy(gtin_digit1, labels[record].gtin, 1);

            if ((strlen(labels[record].gtin) > 0) && (!equals_no(labels[record].gtin))) {

                if (isNumeric(labels[record].gtin)) {

                    // convert string to long long integer to verify GTIN length and check digit
                    long long gtin = strtoll(labels[record].gtin, &endptr, 10);
                    int gtin_ctry_prefix;
                    int gtin_cpny_prefix;

                    // 14-digit GTIN - verify the checkDigit
                    if ((strlen(labels[record].gtin) == GTIN_13 + 1)) {
                        if (gtin % 10 != checkDigit(&gtin)) {
                            printf("Invalid GTIN check digit \"%s\" in record %d.\n", labels[record].gtin, record);
                        }
                        gtin_ctry_prefix = (int) (gtin / GTIN_14_DIGIT);
                        gtin_cpny_prefix = (int) ((gtin - (gtin_ctry_prefix * GTIN_14_DIGIT)) / GTIN_14_CPNY_DIVISOR);

                    } else if (strlen(labels[record].gtin) == GTIN_13) {
                        gtin_ctry_prefix = (int) (gtin / GTIN_13_DIGIT);
                        gtin_cpny_prefix = (int) ((gtin - (gtin_ctry_prefix * GTIN_13_DIGIT)) / GTIN_13_CPNY_DIVISOR);
                    } else {
                        printf("Invalid GTIN check digit or length \"%s\" in record %d.\n", labels[record].gtin,
                               record);
                    }

                    // verify GTIN prefixes if it's nonzero (otherwise it's just a placeholder)
                    // verify the GTIN prefixes (country: 0, 1, 2, 3, company: 4026704 or 5060112)
                    if ((gtin_ctry_prefix > 4) ||
                        (gtin != 0) && (gtin_cpny_prefix != 4026704 && gtin_cpny_prefix != 5060112))
                        printf("Invalid GTIN prefix \"%d\" in record %d.\n", gtin_cpny_prefix, record);

                    // GTIN non-numeric so we'll report that it's non-numeric before printing.
                } else {
                    printf("Nonnumeric GTIN \"%s\" in record %d. \n", labels[record].gtin, record);
                }
                print_info_column_header(fpout, "GTIN", labels[record].gtin, idoc);
            }
        }
    }
    // LTNUMBER record (optional)
    if (labels[record].ltnumber) {
        print_info_column_header(fpout, "LTNUMBER", labels[record].ltnumber, idoc);
    }

    // IPN record (optional) - - this is a non-SAP field that prints only if [-n] flag is present at runtime */
    if (non_SAP_fields)
        if (labels[record].ipn) {
            print_info_column_header(fpout, "IPN", labels[record].ipn, idoc);
        }

    //
    // GRAPHIC01 - GRAPHIC14 Fields (optional)
    // If the cell value is "Y" or "YES', a corresponding record is printed.
    //
    int g_cnt = 1;
    char temp_graphic[MED];

    print_graphic0x_record(fpout, &g_cnt, "Caution.tif", labels[record].caution, idoc);
    print_graphic0x_record(fpout, &g_cnt, "ConsultIFU.tif", labels[record].consultifu, idoc);
    print_graphic0x_record(fpout, &g_cnt, "Latex.tif", labels[record].latex, idoc);
    print_graphic0x_record(fpout, &g_cnt, "DoNotUsePakDam.tif", labels[record].donotusedamaged, idoc);
    print_graphic0x_record(fpout, &g_cnt, "Latex Free.tif", labels[record].latexfree, idoc);
    print_graphic0x_record(fpout, &g_cnt, "ManInBox.tif", labels[record].maninbox, idoc);
    print_graphic0x_record(fpout, &g_cnt, "DoNotRe-sterilize.tif", labels[record].noresterilize, idoc);
    print_graphic0x_record(fpout, &g_cnt, "Non-sterile.tif", labels[record].nonsterile, idoc);
    print_graphic0x_record(fpout, &g_cnt, "PVC_Free.tif", labels[record].pvcfree, idoc);
    print_graphic0x_record(fpout, &g_cnt, "Reusable.tif", labels[record].reusable, idoc);
    print_graphic0x_record(fpout, &g_cnt, "SINGLEUSE.tif", labels[record].singleuseonly, idoc);
    print_graphic0x_record(fpout, &g_cnt, "SinglePatienUse.tif", labels[record].singlepatientuse, idoc);
    print_graphic0x_record(fpout, &g_cnt, "ElectroSurIFU.tif", labels[record].electroifu, idoc);
    print_graphic0x_record(fpout, &g_cnt, "KeepDry.tif", labels[record].keepdry, idoc);

    /*print_graphic0x_record(fpout, &g_cnt, "Caution.tif", labels[record].caution, idoc);
    print_graphic0x_record(fpout, &g_cnt, "ConsultIFU.tif", labels[record].consultifu, idoc);
    print_graphic0x_record(fpout, &g_cnt, "Latex.tif", labels[record].latex, idoc);
    print_graphic0x_record(fpout, &g_cnt, "DoNotUsePakDam.tif", labels[record].donotusedamaged, idoc);
    print_graphic0x_record(fpout, &g_cnt, "F_LatexFree3.tif", labels[record].latexfree, idoc);
    print_graphic0x_record(fpout, &g_cnt, "ManInBox.tif", labels[record].maninbox, idoc);
    print_graphic0x_record(fpout, &g_cnt, "DoNotRe-sterilize.tif", labels[record].noresterilize, idoc);
    print_graphic0x_record(fpout, &g_cnt, "Non-sterile.tif", labels[record].nonsterile, idoc);
    print_graphic0x_record(fpout, &g_cnt, "PVC_Free.tif", labels[record].pvcfree, idoc);
    print_graphic0x_record(fpout, &g_cnt, "REUSABLE.tif", labels[record].reusable, idoc);
    print_graphic0x_record(fpout, &g_cnt, "SINGLEUSE.tif", labels[record].singleuseonly, idoc);
    print_graphic0x_record(fpout, &g_cnt, "SINGLEPATIENUSE.tif", labels[record].singlepatientuse, idoc);
    print_graphic0x_record(fpout, &g_cnt, "ElectroSurIFU.tif", labels[record].electroifu, idoc);
    print_graphic0x_record(fpout, &g_cnt, "KeepDry.tif", labels[record].keepdry, idoc);*/
    //
    // END of GRAPHIC01 - GRAPHIC14 Fields (optional)
    //

    /** BARCODE1 record (optional) */
    if ((labels[record].barcode1) && (!equals_no(labels[record].barcode1))) {

        if (isNumeric(labels[record].barcode1)) {

            // convert string to long long integer to verify GTIN length and check digit
            long long gtin = strtoll(labels[record].barcode1, &endptr, 10);
            int gtin_ctry_prefix;
            int gtin_cpny_prefix;

            // 14-digit GTIN - verify the checkDigit
            if ((strlen(labels[record].barcode1) == GTIN_13 + 1)) {
                if (gtin % 10 != checkDigit(&gtin)) {
                    printf("Invalid GTIN check digit \"%s\" in record %d.\n", labels[record].barcode1, record);
                }
                gtin_ctry_prefix = (int) (gtin / GTIN_14_DIGIT);
                gtin_cpny_prefix = (int) ((gtin - (gtin_ctry_prefix * GTIN_14_DIGIT)) / GTIN_14_CPNY_DIVISOR);

            } else if (strlen(labels[record].barcode1) == GTIN_13) {
                gtin_ctry_prefix = (int) (gtin / GTIN_13_DIGIT);
                gtin_cpny_prefix = (int) ((gtin - (gtin_ctry_prefix * GTIN_13_DIGIT)) / GTIN_13_CPNY_DIVISOR);
            } else {
                printf("Invalid GTIN check digit or length \"%s\" in record %d.\n", labels[record].barcode1, record);
            }

            // verify GTIN prefixes if it's nonzero (otherwise it's just a placeholder)
            // verify the GTIN prefixes (country: 0, 1, 2, 3, company: 4026704 or 5060112)
            if ((gtin_ctry_prefix > 4) || (gtin != 0) && (gtin_cpny_prefix != 4026704 && gtin_cpny_prefix != 5060112))
                printf("Invalid GTIN prefix \"%d\" in record %d.\n", gtin_cpny_prefix, record);
        }
        print_graphic_column_header(fpout, "BARCODE1", labels[record].barcode1, "Nothing", idoc);
    }

    /** GS1 record (optional) */

    if ((labels[record].gs1) && (!equals_no(labels[record].gs1))) {

        char *endptr;
        if (isNumeric(labels[record].gs1)) {

            // convert string to long long integer to verify GTIN length and check digit
            long long gtin = strtoll(labels[record].gs1, &endptr, 10);
            int gtin_ctry_prefix;
            int gtin_cpny_prefix;

            // 14-digit GTIN - verify the checkDigit
            if ((strlen(labels[record].gs1) == GTIN_13 + 1)) {
                if (gtin % 10 != checkDigit(&gtin)) {
                    printf("Invalid GTIN check digit \"%s\" in record %d.\n", labels[record].gs1, record);
                }
                gtin_ctry_prefix = (int) (gtin / GTIN_14_DIGIT);
                gtin_cpny_prefix = (int) ((gtin - (gtin_ctry_prefix * GTIN_14_DIGIT)) / GTIN_14_CPNY_DIVISOR);

            } else if (strlen(labels[record].gs1) == GTIN_13) {
                gtin_ctry_prefix = (int) (gtin / GTIN_13_DIGIT);
                gtin_cpny_prefix = (int) ((gtin - (gtin_ctry_prefix * GTIN_13_DIGIT)) / GTIN_13_CPNY_DIVISOR);
            } else {
                printf("Invalid GTIN check digit or length \"%s\" in record %d.\n", labels[record].gs1, record);
            }

            // verify GTIN prefixes if it's nonzero (otherwise it's just a placeholder)
            // verify the GTIN prefixes (country: 0, 1, 2, 3, company: 4026704 or 5060112)
            if ((gtin_ctry_prefix > 4) || (gtin != 0) && (gtin_cpny_prefix != 4026704 && gtin_cpny_prefix != 5060112))
                printf("Invalid GTIN prefix \"%d\" in record %d.\n", gtin_cpny_prefix, record);
        }

        // if the GS1 field contains any spaces, just print the column heading, but no value
        if (containsSpaces(labels[record].gs1))
            print_blank_graphic_column_header(fpout, "GS1", labels[record].gs1, idoc);
        else
            print_graphic_column_header(fpout, "GS1", labels[record].gs1, "GS1", idoc);
    }

    print_boolean_record(fpout, "ECREP", labels[record].ecrep, "EC Rep.tif", idoc);
    print_boolean_record(fpout, "EXPDATE", labels[record].expdate, "Expiration Date.tif", idoc);
    print_boolean_record(fpout, "KEEPAWAYHEAT", labels[record].keepawayheat, "KeepAwayHeat.tif", idoc);
    print_boolean_record(fpout, "LOTGRAPHIC", labels[record].lotgraphic, "Lot.tif", idoc);
    print_boolean_record(fpout, "MANUFACTURER", labels[record].manufacturer, "Manufacturer.tif", idoc);
    print_boolean_record(fpout, "MFGDATE", labels[record].mfgdate, "DateofManufacture.tif", idoc);
    print_boolean_record(fpout, "PHTDEHP", labels[record].phtdehp, "PHT-DEHP.tif", idoc);
    print_boolean_record(fpout, "PHTBBP", labels[record].phtbbp, "PHT-BBP.tif", idoc);
    print_boolean_record(fpout, "PHTDINP", labels[record].phtdinp, "PHT-DINP.tif", idoc);
    print_boolean_record(fpout, "REFNUMBER", labels[record].refnumber, "REF.tif", idoc);
    print_boolean_record(fpout, "REF", labels[record].ref, "REF.tif", idoc);
    print_boolean_record(fpout, "RXONLY", labels[record].rxonly, "RX Only.tif", idoc);
    print_boolean_record(fpout, "SERIAL", labels[record].serial, "Serial Number.tif", idoc);
    print_boolean_record(fpout, "TFXLOGO", labels[record].tfxlogo, "TeleflexMedical.tif", idoc);

    print_boolean_column_header(fpout, "SIZELOGO", labels[record].sizelogo, idoc);

    print_graphic_column_header(fpout, "ADDRESS", labels[record].address, "Nothing", idoc);
    print_graphic_column_header(fpout, "CAUTIONSTATE", labels[record].cautionstatement, "Nothing", idoc);
    print_graphic_column_header(fpout, "CE0120", labels[record].cemark, "Nothing", idoc);
    print_graphic_column_header(fpout, "COOSTATE", labels[record].coostate, "Nothing", idoc);
    print_graphic_column_header(fpout, "DISTRIBUTEDBY", labels[record].distby, "Nothing", idoc);
    print_graphic_column_header(fpout, "ECREPADDRESS", labels[record].ecrepaddress, "Nothing", idoc);
    print_graphic_column_header(fpout, "FLGRAPHIC", labels[record].flgraphic, "Nothing", idoc);
    print_graphic_column_header(fpout, "LABELGRAPH1", labels[record].labelgraph1, "Nothing", idoc);
    print_graphic_column_header(fpout, "LABELGRAPH2", labels[record].labelgraph2, "Nothing", idoc);
    print_graphic_column_header(fpout, "LATEXSTATEMENT", labels[record].latexstatement, "Nothing", idoc);
    print_graphic_column_header(fpout, "LOGO1", labels[record].logo1, "Nothing", idoc);
    print_graphic_column_header(fpout, "LOGO2", labels[record].logo2, "Nothing", idoc);
    print_graphic_column_header(fpout, "LOGO3", labels[record].logo3, "Nothing", idoc);
    print_graphic_column_header(fpout, "LOGO4", labels[record].logo4, "Nothing", idoc);
    print_graphic_column_header(fpout, "LOGO5", labels[record].logo5, "Nothing", idoc);
    print_graphic_column_header(fpout, "MDR1", labels[record].mdr1, "Nothing", idoc);
    print_graphic_column_header(fpout, "MDR2", labels[record].mdr2, "Nothing", idoc);
    print_graphic_column_header(fpout, "MDR3", labels[record].mdr3, "Nothing", idoc);
    print_graphic_column_header(fpout, "MDR4", labels[record].mdr4, "Nothing", idoc);
    print_graphic_column_header(fpout, "MDR5", labels[record].mdr5, "Nothing", idoc);
    print_graphic_column_header(fpout, "MANUFACTUREDBY", labels[record].manufacturedby, "Nothing", idoc);
    print_graphic_column_header(fpout, "PATENTSTA", labels[record].patentstatement, "Nothing", idoc);
    print_graphic_column_header(fpout, "STERILESTA", labels[record].sterilitystatement, "Nothing", idoc);
    print_graphic_column_header(fpout, "STERILITYTYPE", labels[record].sterilitytype, "blank-01.txt", idoc);
    print_graphic_column_header(fpout, "TEMPRANGE", labels[record].temprange, "Nothing", idoc);
    print_graphic_column_header(fpout, "VERSION", labels[record].version, "Nothing", idoc);
    print_graphic_column_header(fpout, "INSERTGRAPHIC", labels[record].insertgraphic, "yes", idoc);





    if (non_SAP_fields) {
        print_info_column_header(fpout, "OLDLABEL", labels[record].oldlabel, idoc);
        print_info_column_header(fpout, "OLDTEMPLATE", labels[record].oldtemplate, idoc);
        print_info_column_header(fpout, "PREVLABEL", labels[record].prevlabel, idoc);
        print_info_column_header(fpout, "PREVTEMPLATE", labels[record].prevtemplate, idoc);
        print_info_column_header(fpout, "BOMLEVEL", labels[record].bomlevel, idoc);

        // DESCRIPTION record (optional)

        //check for and remove any leading...
        if (labels[record].description[0] == '\"')
            memmove(labels[record].description, labels[record].description + 1,
                    (int) strlen(labels[record].description));

        // ...and/or trailing quotes
        if (labels[record].description[(int) strlen(labels[record].description) - 1] == '\"')
            labels[record].description[(int) strlen(labels[record].description) - 1] = '\0';

        print_info_column_header(fpout, "DESCRIPTION", labels[record].description, idoc);
    }
    return 1;
}

int main(int argc, char *argv[]) {

    // elapsed time
    clock_t start = clock();

    // the Column_header struct that contains all spreadsheet col labels
    // Column_header columns = {0};

    // the Label_record array
    Label_record *labels;

    typedef struct control_numbers Ctrl;

    // boolean to track whether "Label Data" -L flag is set
    bool label_data = false;

    Ctrl idoc = {"2541435", 0, 1, 0, 0};

    if (!check_lookup_array())
        return EXIT_FAILURE;

    if (spreadsheet_init() != 0) {
        printf("Could not initialize spreadsheet array. Exiting\n");
        return EXIT_FAILURE;
    }

    FILE *fp, *fpout_idoc, *fpout_data;

    if ((argc != 2) && (argc != 3) && (argc != 4)) {
        printf("usage: %s filename.txt [-J] [-I] [-L] [-n]\n", argv[0]);
        return EXIT_FAILURE;
    }

    // check for optional command line parameters '-J', 'L' and 'n':
    // -J turns the alt_path to true which substitutes ALT_GRAPHICS_PATH for GRAPHICS_PATH
    // -L creates a second "Label Data" output file
    // -n prints "non-standard" column names in the IDoc: GTIN, IPN, OLDLABEL, OLDTEMPLATE, DESCRIPTION, PREVLABEL and PREVTEMPLATE
    if (argc > 4)
        if ((argv[4] != NULL) && (strncmpci(argv[4], "-J", 2) == 0)) {
            alt_path = true;
            printf("Alternate graphics path selected. Run program without '-J' flag to remove.\n");
        }
        else if ((argv[4] != NULL) && (strncmpci(argv[4], "-L", 2) == 0)) {
            label_data = true;
            printf("\"Label Data\" output file option selected. Run program without '-L' flag to remove.\n");
        }
        else if ((argv[4] != NULL) && (strncmpci(argv[4], "-n", 2) == 0)) {
            non_SAP_fields = true;
            printf("Including non-SAP column headings in IDoc. Run program without '-n' flag to remove.\n");
        }
    if (argc > 3)
        if ((argv[3] != NULL) && (strncmpci(argv[3], "-J", 2) == 0)){
            alt_path = true;
            printf("Alternate graphics path selected. Run program without '-J' flag to remove.\n");
        }
        else if ((argv[3] != NULL) && (strncmpci(argv[3], "-L", 2) == 0)){
            label_data = true;
            printf("\"Label Data\" output file option selected. Run program without '-L' flag to remove.\n");
        }
        else if ((argv[3] != NULL) && (strncmpci(argv[3], "-n", 2) == 0)) {
            non_SAP_fields = true;
            printf("Including non-SAP column headings in IDoc. Run program without '-n' flag to remove.\n");
        }
    if (argc > 2) {
        if ((argv[2] != NULL) && (strncmpci(argv[2], "-J", 2) == 0)){
            alt_path = true;
            printf("Alternate graphics path selected. Run program without '-J' flag to remove.\n");
        }
        else if ((argv[2] != NULL) && (strncmpci(argv[2], "-L", 2) == 0)){
            label_data = true;
            printf("\"Label Data\" output file option selected. Run program without '-L' flag to remove.\n");
        }
        else if ((argv[2] != NULL) && (strncmpci(argv[2], "-n", 2) == 0)) {
            non_SAP_fields = true;
            printf("Including non-SAP column headings in IDoc. Run program without '-n' flag to remove.\n");
        }
    }

    if ((fp = fopen(argv[1], "r")) == NULL) {
        printf("File not found.\n");
        return EXIT_FAILURE;
    } else {
        read_spreadsheet(fp);
    }
    fclose(fp);

    labels = (Label_record *) calloc(spreadsheet_row_number, spreadsheet_row_number * sizeof(Label_record));

    // check spreadsheet columns for duplicates
    if (duplicate_column_names(spreadsheet[0])) {
        printf("Duplicate column names in spreadsheet. Aborting.\n");
        return EXIT_FAILURE;
    }

    // move data into label_record fields by column header
    if (parse_spreadsheet(spreadsheet[0], labels) == -1) {
        printf("Aborting.\n");
        return EXIT_FAILURE;
    }

    // the labels array must be sorted by label number
    sort_labels(labels);

    // output files (the idoc file and the label_data file)
    char *output_idocfile = (char *) malloc(strlen(argv[1]) + FILE_EXT_LEN);
    sscanf(argv[1], "%[^.]%*[txt]", output_idocfile);

    strcat(output_idocfile, "_IDoc (stoidoc).txt");

    printf("Creating IDoc file \"%s\"\n", output_idocfile);

    if ((fpout_idoc = fopen(output_idocfile, "w")) == NULL) {
        printf("Could not open output file %s", output_idocfile);
    }

    char *output_datafile = NULL;
    if (label_data) {
        output_datafile = (char *) malloc(strlen(argv[1]) + FILE_EXT_LEN);
        sscanf(argv[1], "%[^.]%*[txt]", output_datafile);
        strcat(output_datafile, "_labeldata.txt");
        if ((fpout_data = fopen(output_datafile, "w")) == NULL) {
            printf("Could not open output file %s", output_datafile);
        }
    }

    if (print_control_record(fpout_idoc, &idoc) != 0)
        return EXIT_FAILURE;

    {
        int i = 1;
        while (i < spreadsheet_row_number) {
            // if label_data, include fpout_data
            if ((print_label_idoc_records(fpout_idoc, labels, i, &idoc)))
                i++;
            else {
                printf("Content error in text-delimited spreadsheet, line %d. Aborting.\n", i);
                return EXIT_FAILURE;
            }

        }
    }

    fclose(fpout_idoc);

    if (output_datafile) {
        fclose(fpout_data);
        free(output_datafile);
    }
    free(output_idocfile);


    for (int i = 0; i < spreadsheet_row_number; i++)
        free(spreadsheet[i]);
    free(spreadsheet);

    for (int i = 1; i < spreadsheet_row_number; i++)
        free(labels[i].tdline);

    free(labels);

    clock_t stop = clock();
    double elapsed = (double) (stop - start) / CLOCKS_PER_SEC;
    printf("\nTime elapsed in stoidoc: %.5f\n", elapsed);

    return EXIT_SUCCESS;
}