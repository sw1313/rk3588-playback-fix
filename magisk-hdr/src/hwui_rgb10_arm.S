.syntax unified
.thumb
.text
.hword 0
.global no_displays
.thumb_func
no_displays:
.set store_type,no_displays-0x18
.set display_loop,no_displays-0x40
 ldr r2,msg1_relative
msg1_add: add r2,pc
 b fatal
fp16:
 movs r1,#13
 b.n store_type
rgb10:
 movs r1,#7
 b.n store_type
// Original display enumeration loop, same three instructions.
 adds r7,#1
 cmp r5,r7
 bne.n display_loop
no_internal:
 ldr r2,msg2_relative
msg2_add: add r2,pc
 b fatal
 .space 8
non_native:
 cmp r3,#22
 beq fp16
 cmp r3,#43
 beq rgb10
 ldr r2,msg3_relative
msg3_add: add r2,pc
fatal:
 ldr r1,tag_relative
tag_add: add r1,pc
 movs r0,#0
 blx log_assert
 nop
 .org 0x3c
 .word stack_guard_relative
 .word isolated_flag_relative
msg1_relative: .word no_displays_message-(msg1_add+4)
msg2_relative: .word no_internal_message-(msg2_add+4)
msg3_relative: .word bad_format_message-(msg3_add+4)
tag_relative: .word tag-(tag_add+4)
 .space 8
