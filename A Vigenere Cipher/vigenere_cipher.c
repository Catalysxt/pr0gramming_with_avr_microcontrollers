
#include "vigenere_cipher.h"

void print_from_eeprom(char *eeprom_string)
{
    uint8_t letter;
    do 
    {
        letter = eeprom_read_byte( (uint8_t *) eeprom_string);
        transmitByte(letter);
        eeprom_string++; // Increment pointer thereby pointing to the next char in the string
    } while (letter); // Keep printing until we reach \0.
}

// This is print_from_eeprom but in reverse
void enter_text(char text[])
{
    uint8_t i = 0;
    char letter;
    do
    {
        letter = receiveByte();
        transmitByte(letter);   // Echo
        text[i] = letter;   // We're updating the string that the argument points to
        i++;    
    } while (!(letter == '\r') && (i < MAX_TEXT_LENGTH - 1) );
    text[i-1] = 0;  // Add zero to indicate end of string
}

void display_codes(void)
{
    for(int i = 0; i < 4; i++) 
    {
        transmitByte(' ');
        transmitByte('0' + i);
        printString(": ");
        print_from_eeprom(code_pointers[i]);    // Access the code phrases via the indexing array. 
        printString("\r\n");
    }
}

// Prompt user input then use that to index the array and change the code phrase
void change_code(char code_buffer[])
{
    char input;
    char* code_address;
    printString(" -- Choose code phrase to replace:\r\n");
    do 
    {
        display_codes();
        input = receiveByte();
    } while ( (input < '0') || (input > '3') ); // Please enter a valid number

    code_address = code_pointers[input - '0'];
    printString(" -- Enter new code: ");
    enter_text(code_buffer);
    eeprom_update_block(code_buffer, code_address, CODE_LEN);   // Modify the code in place
}

// Print all the codes, capture user input
void select_code(char code[])
{
    print_from_eeprom(prompt_select_code);
	char input;
    do
    {
        display_codes();
        input = receiveByte();
    } while( (input < '0') || (input > '3') );


    char* code_address = code_pointers[input - '0'];
    eeprom_read_block(code, code_address, CODE_LEN); // Fetch the string at the index the user selected then write it to code
    //, the first argument
}

void encode_vigenere(char text[], char code[])
{
    uint8_t code_position = 0;
    uint8_t text_position = 0;

    do {
        if (code[code_position] == 0) { 
            code_position = 0;
        }
        // Apply the encryption
        text[text_position] += code[code_position] - 32;
        if (text[text_position] > 126) {
            text[text_position] -= 95; // Confine to printing characters
        }
        code_position++;
        text_position++;
    } while (text[text_position]);
}

// This is the opposite of encoding
void decode_vigenere(char text[], char code[])
{
    uint8_t code_position = 0;
    uint8_t text_position = 0;

	do {
	if (code[code_position] == 0) { 
		code_position = 0;
	}
	
	// Apply the encryption
	if (code[code_position] > text[text_position]) {
		text[text_position] += 95; // Confine to printing characters
	}
	text[text_position] -= code[code_position] - 32;
	
	code_position++;
	text_position++;
	} while (text[text_position]);
}


int main(void)
{
    char text_buffer[MAX_TEXT_LENGTH] = {0}; // Store encrypted/decrypted text
    char code_string[CODE_LEN] = {0}; // Store code phrase
    initUSART();

    while (1)
    {
        print_from_eeprom(welcome_string);
        print_from_eeprom(prompt_text);
        printString(text_buffer);
        printString("\r\n");
        print_from_eeprom(prompt_code);
        printString(code_string);
        printString("\r\n\r\n ---------------------\r\n");
        print_from_eeprom(menu_enter_text);
        print_from_eeprom(menu_code_text);
        print_from_eeprom(menu_change_code);
        print_from_eeprom(menu_encode);
        print_from_eeprom(menu_decode);
		
        char input = receiveByte();
		

        // Apply switch/case to user input - i.e. event dispatcher 
        switch (input)
        {
            case 'e':
                encode_vigenere(text_buffer, code_string); 
                break;

            case 'd':
                decode_vigenere(text_buffer, code_string); 
                break;

            case 'n':
                print_from_eeprom(prompt_type_text);
                enter_text(text_buffer);
                break;
            
            case 'c':
                select_code(code_string); 
                break;
            
            case 'x':
                change_code(code_string);
                break;
        }
    }
}
