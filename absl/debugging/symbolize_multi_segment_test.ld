/*
 * Linker script fragment for symbolize_multi_segment_test.
 *
 * Places the `.text_far_segment` section in its own executable PT_LOAD segment, after all other
 * loaded sections and at a virtual address well past them. The file offset does not jump with the
 * address, so `p_vaddr - p_offset` of this segment differs from that of the main text segment.
 * This is the layout that llvm-bolt produces when it appends its extra text segment.
 */
SECTIONS
{
  .text_far_segment ALIGN(0x1000000) + 0x1000000 : { *(.text_far_segment) }
}
INSERT AFTER .bss;
