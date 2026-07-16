
#include <avr/io.h>
#include <avr/eeprom.h>
#include "USART.h"

#define MAX_TEXT_LENGTH 256
#define CODE_LEN 64

// Global EEPROM variables. These are the code phrases
char EEMEM code0[CODE_LEN] = "ettubrute";
char EEMEM code1[CODE_LEN] = "attackatdawn";
char EEMEM code2[CODE_LEN] = "theraininspainfallsmainlyontheplain";
char EEMEM code3[CODE_LEN] = "ablewasiereisawelba";
// We initialize the string with a length that's LESS THAN the string we assign it. This wastes memory

// Store the pointers to the first element of each array in another array
// To find the address of a code phrase, we do it via this array
// We index it then pass it into print_from_EEPROM(address)
char *code_pointers[] = {code0, code1, code2, code3};

// Store Menu UI in EEPROM because we can
char EEMEM welcome_string[] = "\r\n--== Vigenere Cipher ==--\r\n";

char EEMEM menu_encode[] = " [e] to encode text\r\n";
char EEMEM menu_decode[] = " [d] to decode text\r\n\r\n";

char EEMEM menu_enter_text[] = " [n] to enter new text\r\n";
char EEMEM menu_code_text[] = " [c] to select your code phrase\r\n";
char EEMEM menu_change_code[] = " [x] to modify code phrases\r\n";

char EEMEM prompt_code[] = "code: ";
char EEMEM prompt_text[] = "\r\ntext: ";
char EEMEM prompt_select_code[] = "Select code phrase:\r\n\r\n";
char EEMEM prompt_type_text[] = "Type your text: ";

// Given the address, print the string at that address. Used for Menu elements
void print_from_eeprom(char *eeprom_string);

void enter_text(char text[]);

// Read code phases from EEPROM and print them.
// Uses code_buffer as temp storage ??
void display_codes(void);

// Alter a code phrase
void change_code(char code_buffer[]);

// Select a code phrase
void select_code(char code[]);

// Pass in string and coding scheme to perform an encryption
void encode_vigenere(char text[], char code[]);

// Same thing but decryption
void decode_vigenere(char text[], char code[]);
