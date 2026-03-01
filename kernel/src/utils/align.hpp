#define ALIGNUP(VALUE, c) ((VALUE + c - 1) & ~(c - 1))
#define ALIGNDOWN(VALUE, c) ((VALUE / c) * c)