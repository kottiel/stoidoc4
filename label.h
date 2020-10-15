/**
    @file command.h
    @author Jeff Kottiel (jhkottie)
    Together with .c, this component is responsible for functionality
    pertaining to building the label records.
*/

#ifndef LABEL_H
#define LABEL_H

#include <stdbool.h>

/* the spreadsheet's initial capacity */
#define INITIAL_CAP             3

#define MAX_COLUMNS          1000

/* Field lengths                     */
#define LRG                    41
#define MED                    30
#define MAX_TEMPLATE_LEN       18
#define MAX_LEVEL              18
#define MAX_LABEL_LEN          10
#define MAX_IPN_LEN            10
#define MAX_GTIN_LEN           15
#define SML                     5
#define MED2                    8
#define MAX_REV_LEN             4

/* IDoc Field types                  */
#define MATERIAL_REC          "02"
#define LABEL_REC             "03"
#define TDLINE_REC            "04"
#define CHAR_REC              "04"

#define TAB                  '\t'

/** global variable spreadsheet that holds the label records  */
extern char **spreadsheet;
extern int spreadsheet_cap;
extern int spreadsheet_row_number;

/* whether or not to include non-SAP fields in IDoc                      */
extern bool non_SAP_fields;

/**

*/
typedef struct {
    char material[LRG];
    char coostate[LRG];
    char address[MED];
    char barcode1[MED];
    char cautionstatement[MED];
    char cemark[MED];
    char distby[MED];
    char ecrepaddress[MED];
    char flgraphic[MED];
    char gs1[MED];
    char insertgraphic[MED];
    char labelgraph1[MED];
    char labelgraph2[MED];
    char latexstatement[MED];
    char logo1[MED];
    char logo2[MED];
    char logo3[MED];
    char logo4[MED];
    char logo5[MED];
    char mdr1[MED];
    char mdr2[MED];
    char mdr3[MED];
    char mdr4[MED];
    char mdr5[MED];
    char manufacturedby[MED];
    char patentstatement[MED];
    char size[MED];
    char sterilitystatement[MED];
    char sterilitytype[MED];
    char temprange[MED];
    char version[MED];
    char oldlabel[MED];
    char oldtemplate[MED];
    char prevlabel[MED];
    char prevtemplate[MED];
    char description[MED];
    char pcode[MED];
    char ltnumber[MED];
    char ipn[MED];
    char barcodetext[MAX_GTIN_LEN];
    char gtin[MAX_GTIN_LEN];
    char level[MAX_LEVEL];
    char label[MAX_LABEL_LEN];

    char quantity[LRG];
    char template[MAX_TEMPLATE_LEN];
    char bomlevel[SML];
    char revision[MAX_REV_LEN];
    char release[MED2];
    char *tdline;

    unsigned int caution : 4;
    unsigned int consultifu : 4;
    unsigned int donotusedamaged : 4;
    unsigned int electroifu : 4;
    unsigned int keepdry : 4;
    unsigned int latex : 4;
    unsigned int latexfree : 4;
    unsigned int maninbox : 4;
    unsigned int nonsterile : 4;
    unsigned int noresterilize : 4;
    unsigned int pvcfree : 4;
    unsigned int reusable : 4;
    unsigned int singlepatientuse : 4;
    unsigned int singleuseonly : 4;
    unsigned int ecrep : 4;
    unsigned int expdate : 4;
    unsigned int keepawayheat : 4;
    unsigned int lotgraphic : 4;
    unsigned int manufacturer : 4;
    unsigned int mfgdate : 4;
    unsigned int phtbbp : 4;
    unsigned int phtdehp : 4;
    unsigned int phtdinp : 4;
    unsigned int ref : 4;
    unsigned int refnumber : 4;
    unsigned int rxonly : 4;
    unsigned int serial : 4;
    unsigned int sizelogo : 4;
    unsigned int tfxlogo : 4;

} Label_record;

int duplicate_column_names(const char *column_names);

/**
    identifies the column headings in a line and assigns true values to the
    elements in the Column_header struct that correspond to those elements.
    @param buffer is a pointer to the column headings line
    @param cols is a pointer to a Column_header struct
    @return the number of column headings identified
*/
int parse_spreadsheet(char *buffer, Label_record *labels);

/**
    get_token dynamically allocates a text substring and copies the substring
    of buffer that occurs before the next occurrence of the delimiter,
    tab_str. It then removes that substring and delimiter from buffer. If there
    is no substr to capture between delimiters, the returned substring is
    empty.

    Calling code is responsible for freeing the returned char pointer.

    @param buffer contains the string being divided into tokens
    @param tab_str is the one character delimiter
    @return a pointer to a dynamically allocated char array
*/
char *get_token(char *buffer, char tab_str);

/**
 * Given a specific delimiter, extracts the value of that field with a delimited char string.
 * Returns the length of that value if it exists and isn't "N" or "NO"
 * @param contents
 * @param i
 * @param count
 * @param tab_str
 * @return
 */

int get_field_contents_from_row(char *contents, int i, int count, char tab_str);

int peek_nth_token(int n, const char *buffer, char delimiter);

int strncmpci(const char *str1, const char *str2, int num);

int equals_yes(char *field);

int equals_no(char *field);

int spreadsheet_init();

int spreadsheet_expand();

int sort_labels(Label_record *labels);

void swap_label_records(Label_record *labels, int i, int min_index);

#endif