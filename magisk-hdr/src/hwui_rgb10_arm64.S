.text
.global no_displays
no_displays:
 adrp x2, no_displays_message
 add x2,x2,:lo12:no_displays_message
 b fatal
stack_failure:
 bl stack_fail
 brk #0
// Preserve the original display enumeration loop at 0x47b888.
 add x21,x21,#8
 cmp x22,x21
 b.ne display_loop
no_internal:
 adrp x2,no_internal_message
 add x2,x2,:lo12:no_internal_message
 b fatal
non_native:
 cmp w3,#22
 b.eq fp16
 cmp w3,#43
 b.eq rgb10
 adrp x2,bad_format_message
 add x2,x2,:lo12:bad_format_message
fatal:
 adrp x1,tag
 add x1,x1,:lo12:tag
 mov x0,xzr
 bl log_assert
fp16:
 mov w8,#13
 b store_type
rgb10:
 mov w8,#7
 b store_type
