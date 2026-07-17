/*
 * calc.h
 *
 * int32_t chained-arithmetic core: one overflow-safe binary op at a time
 * (the top-level state machine in ascent.c owns left-to-right ordering —
 * no operator precedence lives here), plus the digit-entry operand buffer
 * and a signed-to-decimal-string formatter.
 */

#ifndef ASCENT_CALC_H_
#define ASCENT_CALC_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum { OP_NONE, OP_ADD, OP_SUB, OP_MUL, OP_DIV } calc_op_t;

typedef enum {
    CALC_OK,
    CALC_ERR_OVERFLOW,
    CALC_ERR_DIV_ZERO
} calc_status_t;

/*
 * Computes `a op b` into *out, using overflow-safe precondition checks
 * (no int64_t intermediates). On any non-CALC_OK return, *out is left
 * untouched — the caller's running result is never corrupted by a failed
 * operation.
 */
calc_status_t calc_apply(calc_op_t op, int32_t a, int32_t b, int32_t *out);

/*
 * Writes the decimal representation of n (with a leading '-' if negative)
 * into buf, NUL-terminated. buf must be at least 12 bytes
 * ("-2147483648\0"). Returns the string length, excluding the NUL.
 */
uint8_t calc_format(int32_t n, char buf[12]);

/* Digit-entry accumulator for the operand currently being typed. Capped
 * at 10 digits per the functional spec (Max operand digits: 10). */
typedef struct {
    char    digits[11];
    uint8_t len;
} operand_buf_t;

void    operand_clear(operand_buf_t *o);

/* Appends one ASCII digit ('0'-'9'). Returns false (and leaves o
 * unchanged) if already at the 10-digit cap. */
bool    operand_append_digit(operand_buf_t *o, char d);

/*
 * Parses the accumulated digits as a non-negative decimal value. Values
 * above INT32_MAX (possible with a full 10-digit entry, e.g. "9999999999")
 * saturate to INT32_MAX rather than wrapping — the caller's next
 * operator-commit or '=' will then trip the ordinary CALC_ERR_OVERFLOW
 * path, since a saturated value sits exactly on the overflow boundary.
 */
int32_t operand_to_int(const operand_buf_t *o);

#endif /* ASCENT_CALC_H_ */
