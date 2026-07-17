/*
 * calc.c
 *
 * Purpose : int32_t overflow-safe chained-arithmetic core, operand digit
 *           accumulator, and decimal formatter for ASCENT.
 * Hardware: none — pure arithmetic, no register access.
 * Datasheet: n/a.
 *
 * Two's-complement asymmetry: int32_t's range is [-2147483648, 2147483647]
 * — there is no positive int32_t equal to +2147483648, so negating
 * INT32_MIN (as in a naive `abs()`/`-n`) is undefined behaviour. Every
 * place in this file that needs a magnitude goes through abs_u32() below,
 * which converts to uint32_t (well-defined for every int32_t value,
 * including INT32_MIN, per C11 6.3.1.3p2) before ever negating — never
 * calls a signed abs() or writes `-n` directly. See docs/Theory.md for a
 * longer explanation of why this matters on an 8-bit target with no
 * hardware trap on signed overflow.
 */

#include "calc.h"

#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

/* Converts any int32_t (including INT32_MIN) to its unsigned magnitude
 * with zero signed-overflow UB anywhere in the computation. */
static uint32_t abs_u32(int32_t v)
{
    return (v < 0) ? ((uint32_t)0u - (uint32_t)v) : (uint32_t)v;
}

/* True if a*b would overflow int32_t. Handles every INT32_MIN combination
 * safely via abs_u32() — not just the classic INT32_MIN/-1 case. */
static bool mul_overflows(int32_t a, int32_t b)
{
    if (a == 0 || b == 0) {
        return false;
    }
    if ((a == INT32_MIN && b == -1) || (b == INT32_MIN && a == -1)) {
        return true;   /* result would be +2^31, one past INT32_MAX */
    }
    uint32_t ua = abs_u32(a);
    uint32_t ub = abs_u32(b);
    return ua > ((uint32_t)INT32_MAX / ub);
}

calc_status_t calc_apply(calc_op_t op, int32_t a, int32_t b, int32_t *out)
{
    switch (op) {
    case OP_ADD:
        if ((b > 0 && a > INT32_MAX - b) || (b < 0 && a < INT32_MIN - b)) {
            return CALC_ERR_OVERFLOW;
        }
        *out = a + b;
        return CALC_OK;

    case OP_SUB:
        if ((b < 0 && a > INT32_MAX + b) || (b > 0 && a < INT32_MIN + b)) {
            return CALC_ERR_OVERFLOW;
        }
        *out = a - b;
        return CALC_OK;

    case OP_MUL:
        if (a == 0 || b == 0) {
            *out = 0;
            return CALC_OK;
        }
        if (mul_overflows(a, b)) {
            return CALC_ERR_OVERFLOW;
        }
        *out = a * b;
        return CALC_OK;

    case OP_DIV:
        if (b == 0) {
            return CALC_ERR_DIV_ZERO;   /* checked before any division executes */
        }
        if (a == INT32_MIN && b == -1) {
            return CALC_ERR_OVERFLOW;   /* quotient would be +2^31; guard before '/' */
        }
        *out = a / b;   /* C99+: signed division truncates toward zero */
        return CALC_OK;

    case OP_NONE:
    default:
        *out = b;   /* unreachable via ascent.c's commit rule, but defined */
        return CALC_OK;
    }
}

uint8_t calc_format(int32_t n, char buf[12])
{
    uint32_t mag  = abs_u32(n);
    char     tmp[10];               /* max 10 digits: 4294967295 */
    uint8_t  tlen = 0;

    do {
        tmp[tlen++] = (char)('0' + (mag % 10u));
        mag /= 10u;
    } while (mag != 0u);

    uint8_t blen = 0;
    if (n < 0) {
        buf[blen++] = '-';
    }
    while (tlen > 0u) {
        buf[blen++] = tmp[--tlen];
    }
    buf[blen] = '\0';
    return blen;
}

void operand_clear(operand_buf_t *o)
{
    o->len = 0u;   /* digit contents don't need zeroing — len gates all reads */
}

bool operand_append_digit(operand_buf_t *o, char d)
{
    if (o->len >= 10u) {
        return false;   /* digit cap reached (spec: max 10 operand digits) */
    }
    o->digits[o->len++] = d;
    return true;
}

int32_t operand_to_int(const operand_buf_t *o)
{
    uint64_t acc = 0;
    for (uint8_t i = 0; i < o->len; i++) {
        acc = (acc * 10u) + (uint64_t)(o->digits[i] - '0');
    }
    if (acc > (uint64_t)INT32_MAX) {
        acc = (uint64_t)INT32_MAX;   /* saturate — see calc.h doc comment */
    }
    return (int32_t)acc;
}
