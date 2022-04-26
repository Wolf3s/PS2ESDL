################## SYSCALL HOOKING FUNCTIONS FILE ###############################
#	Last updated: 2012/02/22						#
#	Version 1.54								#
#################################################################################
#include "main.h"

.set push
.set noreorder

.data

.globl SifSetReg_lock;
.globl SifSetDma_lock;

.globl ori_SifSetReg;
.globl ori_SifSetDma;

ori_SifSetDma:		.word 0	# Pointer to the SCE SifSetDma() syscall handler
ori_SifSetReg:		.word 0	# Pointer to the SCE SifSetReg() syscall handler

SifSetReg_lock: .byte 0
SifSetDma_lock: .byte 1

.p2align 6 #64-byte alignment
EE_core_stack: .space 0x2000
.p2align 2 #4-byte alignment
EE_core_stack_sz: .word 0x2000
################################################################
.text

.extern New_SifSetDma
.extern GetSyscallHandler
.extern SetSyscall

.extern IOP_resets;
.extern mode3_enabled;
########################

################################################################
#SIF DMA transfer header:
#=========================
#typedef struct t_SifDmaTransfer
#{
#   void			*src,
#      				*dest;
#   int				size;
#   int				attr;
#} SifDmaTransfer_t;

#IOP reset packet payload layout:
#================================
#typedef struct t_SifCmdHeader
#{
#   u32				size;
#   void			*dest;
#   int				cid;
#   u32				unknown;
#} SifCmdHeader_t;

#struct _iop_reset_pkt {
#	struct t_SifCmdHeader header;
#	int	arglen;
#	int	mode;
#	char	arg[80];
#} __attribute__((aligned(16)));
################################################################

.globl	Hook_SifSetDma
.ent	Hook_SifSetDma # SifDmaTransfer_t *sdd -> $a0, s32 len -> $a1
Hook_SifSetDma:
	############ PHRASE 0 ############ ; Check if this hook is disabled (Pass through all DMA requests)
	lb $t0, SifSetDma_lock	# if((SifSetDma_lock==1)
	beq $t0, $zero, SifSetDma_normal
	nop

	############ PHRASE 1 ############ ; Check attributes and size stated in the DMA header
	lw $t3, 8($a0) # sdd->size
	addiu $t2, $zero, 0x68
	bne $t3, $t2, SifSetDma_normal # if(sdd->size!=0x68)
	nop

	lw $t1, 12($a0) # sdd->attr
	addiu $t2, $zero, 0x44
	bne $t1, $t2, SifSetDma_normal # if sdd->attr!=0x44
	nop

	############ PHRASE 2 ############ ; Check the SIF command header, and compare IDs + attributes
	lw $t0, 0($a0) #reset_pkt=sdd->src

	lw $t1, 0($t0) #reset_pkt->header.psize
	bne $t1, $t3, SifSetDma_normal #if(reset_pkt->header.size!=sdd->size); NOTE!! $t3 contains sdd->size!!
	nop

	lw $t1,8($t0) #reset_pkt->header.cid
	li $t2,0x80000003
	bne $t1,$t2,SifSetDma_normal #if(reset_pkt->header.cid!=0x80000003)
	nop

	############ PHRASE 3 ############ ; Intercept the IOP reset packet
	la $v1, New_SifSetDma
	jr $ra
	sw $v1, 8($sp)		# Overwrite the EPC with the address of our own function.

SifSetDma_normal: # It's a regular DMA packet(With some other function). Pass it through.
	lw $t0, ori_SifSetDma
	jr $t0 #Use $a0 and $a1 provided to this function
	nop
.end	Hook_SifSetDma

########################
.ent	New_SifSetDma
New_SifSetDma:
	#Change the stack pointer to point to the stack that's inside this EE core.
	move $v1, $sp
	lw $t2, EE_core_stack_sz
	la $sp, EE_core_stack
	addu $sp, $sp, $t2	# Move the stack pointer to the top of the stack.
	addiu $sp, $sp, -16	# Allocate 16 bytes to store the original stack pointer and the return vector (The contents of $ra).
	sd $v1, ($sp)		# Save the original stack pointer
	sd $ra, 8($sp)		# Save the return vector (The contents of $ra).

	lw $t0, 0($a0)		#reset_pkt=sdd->src

	lw $a1, 16($t0)		#reset_pkt.arglen
	jal New_SifIopReset
	addiu $a0, $t0, 24	#reset_pkt.arg

	#Restore the original return vector (The contents of $ra) and the original stack pointer.
	ld $ra, 8($sp)		# Load the return vector (The contents of $ra).
	ld $v1, ($sp)		# Load the original stack pointer
	move $sp, $v1

	li $t0, 4
	sb $t0, SifSetReg_lock	#SifSetReg_lock=4;

	lb $t0, IOP_resets	#IOP_resets++;
	addiu $t0, $t0, 1
	sb $t0, IOP_resets

	jr $ra
	addiu $v0, $zero, 1	# return 1;

.end 	New_SifSetDma

########################
.globl	Hook_SifSetReg
.ent	Hook_SifSetReg
Hook_SifSetReg:
	lb $t0, SifSetReg_lock
	bgtz $t0, SifSetReg_skip #Block SifSetReg call if this hook is enabled
	addiu $t0, $t0, -1 #Reduce SifSetReg_lock by 1; The result will only be stored into "SifSetReg_lock" if the processor makes this jump.

	lw $t0, ori_SifSetReg
	jr $t0 # Use values currently in $a0 and $a1
	nop

SifSetReg_skip:
	sb $t0, SifSetReg_lock

	addiu $t1, $zero, 1
	bge $t0, $t1, SifSetReg_exit
	nop

	lbu $t0, mode3_enabled
	bgtz $t0, SifSetReg_Mode3
	nop

	j SifSetReg_exit
	nop

SifSetReg_Mode3: #Prepare to reset syscalls
	la $v1, reset_Syscalls
	jr $ra
	sw $v1, 8($sp)

SifSetReg_exit:
	jr $ra
	addiu $v0, $zero, 1 #Return 1

.end	Hook_SifSetReg

########################

.ent	reset_Syscalls
reset_Syscalls:
	addiu $sp, $sp, -8
	sd $ra, ($sp) #Save the caller's address onto the stack

	##########
	lw $a1, ori_SifSetDma
	jal SetSyscall
	addiu $a0, $zero, 119
	###########
	lw $a1, ori_SifSetReg
	jal SetSyscall
	addiu $a0, $zero, 121
	##########

	addiu $v0, $zero, 1 #Return 1
	ld $ra, ($sp) #Load the caller's address from the stack
	jr $ra
	addiu $sp, $sp, 8
.end	reset_Syscalls

.set pop
