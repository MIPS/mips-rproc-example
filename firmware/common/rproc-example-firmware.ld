OUTPUT_ARCH(mips)
ENTRY(__start)

SECTIONS
{
	. = 0x10000000;
	_start = .;
	
	.exception_vector : {
		_exception_vector = .;
		*(.text.exception_vector.xtlb_refill);
		. = _exception_vector + 0x100;
		*(.text.exception_vector.cache_error);
		. = _exception_vector + 0x180;
		*(.text.exception_vector.exception);

		. = _exception_vector + 0x400;
		*(.text.exception_vector.reset);
	}

	. = ALIGN(8);

	_text = .;
	.text : {
		*(.text.*);
	}
	
	/* Static data */
	_rodata = .;
	.rodata : {
		*(.rodata);
		*(.rodata.*);
	}
	/* non-static data */
	_data = .;
	.data : {
		*(.data*);
	}

	. = ALIGN(8);
	_bss_start = .;
	.bss : {
		*(.bss*);
	}
	. = ALIGN(8);
	_bss_end = .;

	. = ALIGN(0x1000);
	_stack_bottom = .;
	. += 0x1000; /* 4k stack */
	_stack_top = .;

	_end = .;
}