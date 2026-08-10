	.section script_data, "aw"

	.macro mystery_script_cmd value:req
	.if LINUX64
	.quad \value
	.else
	.int \value
	.endif
	.endm

	.align 2
gMysteryEventScriptCmdTable::
	mystery_script_cmd MEScrCmd_nop                 /* 0x00*/
	mystery_script_cmd MEScrCmd_checkcompat         /* 0x01*/
	mystery_script_cmd MEScrCmd_end                 /* 0x02*/
	mystery_script_cmd MEScrCmd_setmsg              /* 0x03*/
	mystery_script_cmd MEScrCmd_setstatus           /* 0x04*/
	mystery_script_cmd MEScrCmd_runscript            /* 0x05*/
	mystery_script_cmd MEScrCmd_initramscript        /* 0x06*/
	mystery_script_cmd MEScrCmd_setenigmaberry       /* 0x07*/
	mystery_script_cmd MEScrCmd_giveribbon           /* 0x08*/
	mystery_script_cmd MEScrCmd_givenationaldex      /* 0x09*/
	mystery_script_cmd MEScrCmd_addrareword          /* 0x0a*/
	mystery_script_cmd MEScrCmd_setrecordmixinggift  /* 0x0b*/
	mystery_script_cmd MEScrCmd_givepokemon           /* 0x0c*/
	mystery_script_cmd MEScrCmd_addtrainer            /* 0x0d*/
	mystery_script_cmd MEScrCmd_enableresetrtc        /* 0x0e*/
	mystery_script_cmd MEScrCmd_checksum              /* 0x0f*/
	mystery_script_cmd MEScrCmd_crc                   /* 0x10*/
gMysteryEventScriptCmdTableEnd::
