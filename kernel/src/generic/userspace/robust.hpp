#pragma once

struct robust_list {
        struct robust_list *next;
};

struct robust_list_head {
        struct robust_list list;
        long futex_offset;
        struct robust_list *list_op_pending;
};
