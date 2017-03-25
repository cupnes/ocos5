= カーネル
ブートローダーからジャンプしてくると、カーネルの初期化処理が実行されます。カーネルの各機能の初期化を行った後は、カーネルは割り込み駆動で動作し、何もしない時はhlt命令でCPUを寝させるようにしています。

カーネルの各機能の実装については、2016年4月に発表させていただいた勉強会のスライドにまとめています。各機能の実装について、図を使って説明していますので、興味があれば見てみてください。

 * http://www.slideshare.net/yarakawa/2000x86
== 初期化
=== kernel/Makefile
//listnum[kernel_Makefile][kernel/Makefile][make]{
CFLAGS	=	-Wall -Wextra
CFLAGS	+=	-nostdinc -nostdlib -fno-builtin -c
CFLAGS	+=	-Iinclude
CFLAGS	+=	-m32

.S.o:
	gcc $(CFLAGS) -o $@ $<
.c.o:
	gcc $(CFLAGS) -o $@ $<

kernel.bin: sys.o cpu.o intr.o excp.o memory.o sched.o fs.o task.o	\
	syscall.o lock.o timer.o console_io.o queue.o common.o		\
	debug.o init.o kern_task_init.o
	ld -m elf_i386 -o $@ $+ -Map System.map -s -T sys.ld -x

sys.o: sys.S
cpu.o: cpu.c
intr.o: intr.c
excp.o: excp.c
memory.o: memory.c
sched.o: sched.c
fs.o: fs.c
task.o: task.c
syscall.o: syscall.c
lock.o: lock.c
timer.o: timer.c
console_io.o: console_io.c
queue.o: queue.c
common.o: common.c
debug.o: debug.c
init.o: init.c
kern_task_init.o: kern_task_init.c

clean:
	rm -f *~ *.o *.bin *.dat *.img *.map

.PHONY: clean
//}

カーネルをコンパイルするルールを記載したMakefileです。OS5のカーネルはLinux等と同じくモノリシックカーネルです。そのため、コンパイルが完了すると単一のバイナリができあがります。(このバイナリを上位のMakefileでブートローダーやユーザーランドと結合します。)

#@# 書くほどではない
#@# 3行目の `CFLAGS	+=	-Iinclude` では、 `#include <>` で参照するヘッダーファイルのパスを指定しています。ここでは、kernel/include を指定しています。

アセンブラのソースコードファイルの拡張子が、ブートローダーでは boot/boot.s と小文字の's'でしたが、@<code>{sys.o: sys.S}のターゲット指定の通り、大文字の'S'になっています。また、アセンブラのビルド時に使用するコマンドも、ブートローダーではasコマンドでしたが、ここではgccを使用しています。これは、プリプロセスで展開されるマクロを記述するためです(kernel/sys.S で@<code>{#include}マクロを使っています)。

ブートローダーのMakefileでも書きましたが、@<code>{.S.o}や@<code>{.c.o}といった書き方をしている(6～9行目)ため、各ターゲットを@<code>{sys.o: sys.S}のように指定せねばならず、冗長ですね。

#@# 12行目のldコマンドのオプションで、"System.map"というファイル名でmapファイルを出力しています。ブートローダーやユーザーランドでは"boot.map"や"0shell.map"(シェルアプリケーション)というファイル名なのですが、カーネルだけ"System.map"なのは、カーネルとユーザーランドが分離できていなかった頃の名残です(「名残」と言うのか、「単に統一できていないだけ」と言うのか。。。)。当初はLinuxカーネルを思い出し、「mapファイルとしてば、"System.map"というファイル名だろう」と思い、名付けていました。

=== kernel/sys.ld
//listnum[kernel_sys_ld][kernel/sys.ld][]{
OUTPUT_FORMAT("binary");

SECTIONS
{
	. = 0x7e00;
	.text	: {*(.text)}
	.rodata	: {
		*(.strings)
		*(.rodata)
		*(.rodata.*)
	}
	.data	: {*(.data)}
	.bss	: {*(.bss)}

	. = 0x7e00 + 0x91fe;
	.sign	: {SHORT(0xbeef)}
}
//}

boot/boot.s の説明で、「OS5では、カーネルは0x0000 7e00から配置するように決めています。」と書いていましたが、「決めている」のがこのファイルです。5行目の@<code>{. = 0x7e00;}で、カーネルのバイナリは0x0000 7e00から配置されることを指定しています。そして、5行目に続いて@<code>{.text	: {*(.text)}}と記載しており、カーネルのバイナリの先頭にテキスト領域を配置することを指定しています。

また、15行目の@<code>{. = 0x7e00 + 0x91fe;}で、カーネル領域のサイズを決めています。上位のMakefileでは「ブートローダー・カーネル・ユーザーランドのバイナリを単にcatで連結している」旨を説明しました。カーネルサイズの変化によってユーザーランドの開始アドレスが変化することが無いよう、カーネルサイズを固定しています。0x91feは10進数で37374です。2バイトのマジックナンバー(0xbeef)を含め、37376バイト(36.5KB)がカーネルで使用できる最大サイズです。また、0x7e00 + 0x91fe + 2(マジックナンバー) = 0x11000なので、ユーザーランドの開始アドレスは0x0001 1000です。

#@# なお、(TODO: リリースバージョン追記)の現在、カーネルサイズは(TODO: 確認)バイトなので、あと(TODO: 確認)バイトの余地があります。

====[column] マジックナンバーとHexspeak
マジックナンバーに0xbeef(肉)等、16進数で表せる英単語を使用すると、16進数でメモリダンプしたときに確認できて便利です。このような表記法は"Hexspeak"と呼ばれ、0xBAADF00D("bad food")が「Microsoft WindowsのLocalAlloc関数の第一引数にLMEM_FIXEDを渡して呼び出してメモリを確保した場合に、ヒープに確保されたメモリが初期化されていないことを表す値として使用されている。」@<fn>{hexspeak}等、他にも様々なものがあります。詳しくは、Wikipedia等を参照してみると面白いです。
//footnote[hexspeak][Hexspeak - Wikipedia: https://ja.wikipedia.org/wiki/Hexspeak]

=== kernel/include/asm/cpu.h
//listnum[kernel_include_asm_cpu_h][kernel/include/asm/cpu.h][c]{
#ifndef _ASM_CPU_H_
#define _ASM_CPU_H_

#define GDT_SIZE	16

#endif /* _ASM_CPU_H_ */
//}

後述する kernel/sys.S で@<code>{#include}されるヘッダーファイルです。@<code>{#define}1つだけなので先に紹介してしまいます。

ここでは、GDTのサイズを定義しています。GDTはC言語からも参照することがあるので、ヘッダーファイルに分離しています。

=== kernel/sys.S
//listnum[kernel_sys_S][kernel/sys.S][s]{
#include <asm/cpu.h>

	.code32

	.text

	.global	kern_init, idt, gdt, keyboard_handler, timer_handler
	.global	exception_handler, divide_error_handler, debug_handler
	.global nmi_handler, breakpoint_handler, overflow_handler
	.global bound_range_exceeded_handler, invalid_opcode_handler
	.global device_not_available_handler, double_fault_handler
	.global coprocessor_segment_overrun_handler, invalid_tss_handler
	.global segment_not_present_handler, stack_fault_handler
	.global general_protection_handler, page_fault_handler
	.global x87_fpu_floating_point_error_handler, alignment_check_handler
	.global machine_check_handler, simd_floating_point_handler
	.global virtualization_handler, syscall_handler

	movl	$0x00080000, %esp

	lgdt	gdt_descr

	lea	ignore_int, %edx
	movl	$0x00080000, %eax
	movw	%dx, %ax
	movw	$0x8E00, %dx
	lea	idt, %edi
	mov	$256, %ecx
rp_sidt:
	movl	%eax, (%edi)
	movl	%edx, 4(%edi)
	addl	$8, %edi
	dec	%ecx
	jne	rp_sidt
	lidt	idt_descr

	pushl	$0
	pushl	$0
	pushl	$0
	pushl	$end
	pushl	$kern_init
	ret

end:
	jmp	end

keyboard_handler:
	pushal
	call	do_ir_keyboard
	popal
	iret

timer_handler:
	pushal
	call	do_ir_timer
	popal
	iret

exception_handler:
	popl	excp_error_code
	pushal
	call	do_exception
	popal
	iret

/* interrupt 0 (#DE) */
divide_error_handler:
	jmp	divide_error_handler
	iret

/* interrupt 1 (#DB) */
debug_handler:
	jmp	debug_handler
	iret

/* interrupt 2 */
nmi_handler:
	jmp	nmi_handler
	iret

/* interrupt 3 (#BP) */
breakpoint_handler:
	jmp	breakpoint_handler
	iret

/* interrupt 4 (#OF) */
overflow_handler:
	jmp	overflow_handler
	iret

/* interrupt 5 (#BR) */
bound_range_exceeded_handler:
	jmp	bound_range_exceeded_handler
	iret

/* interrupt 6 (#UD) */
invalid_opcode_handler:
	jmp	invalid_opcode_handler
	iret

/* interrupt 7 (#NM) */
device_not_available_handler:
	jmp	device_not_available_handler
	iret

/* interrupt 8 (#DF) */
double_fault_handler:
	jmp	double_fault_handler
	iret

/* interrupt 9 */
coprocessor_segment_overrun_handler:
	jmp	coprocessor_segment_overrun_handler
	iret

/* interrupt 10 (#TS) */
invalid_tss_handler:
	jmp	invalid_tss_handler
	iret

/* interrupt 11 (#NP) */
segment_not_present_handler:
	jmp	segment_not_present_handler
	iret

/* interrupt 12 (#SS) */
stack_fault_handler:
	jmp	stack_fault_handler
	iret

/* interrupt 13 (#GP) */
general_protection_handler:
	jmp	general_protection_handler
	iret

/* interrupt 14 (#PF) */
page_fault_handler:
	popl	excp_error_code
	pushal
	movl	%cr2, %eax
	pushl	%eax
	pushl	excp_error_code
	call	do_page_fault
	popl	%eax
	popl	%eax
	popal
	iret

/* interrupt 16 (#MF) */
x87_fpu_floating_point_error_handler:
	jmp	x87_fpu_floating_point_error_handler
	iret

/* interrupt 17 (#AC) */
alignment_check_handler:
	jmp	alignment_check_handler
	iret

/* interrupt 18 (#MC) */
machine_check_handler:
	jmp	machine_check_handler
	iret

/* interrupt 19 (#XM) */
simd_floating_point_handler:
	jmp	simd_floating_point_handler
	iret

/* interrupt 20 (#VE) */
virtualization_handler:
	jmp	virtualization_handler
	iret

/* interrupt 128 */
syscall_handler:
	pushl	%esp
	pushl	%ebp
	pushl	%esi
	pushl	%edi
	pushl	%edx
	pushl	%ecx
	pushl	%ebx
	pushl	%eax
	call	do_syscall
	popl	%ebx
	popl	%ebx
	popl	%ecx
	popl	%edx
	popl	%edi
	popl	%esi
	popl	%ebp
	popl	%esp
	iret

ignore_int:
	iret

	.data
idt_descr:
	.word	256*8-1		/* idt contains 256 entries */
	.long	idt

gdt_descr:
	.word	GDT_SIZE*8-1
	.long	gdt

	.balign	8
idt:
	.fill	256, 8, 0	/* idt is uninitialized */

gdt:
	.quad	0x0000000000000000	/* NULL descriptor */
	.quad	0x00cf9a000000ffff	/* 4GB(r-x:Code, DPL=0) */
	.quad	0x00cf92000000ffff	/* 4GB(rw-:Data, DPL=0) */
	.quad	0x00cffa000000ffff	/* 4GB(r-x:Code, DPL=3) */
	.quad	0x00cff2000000ffff	/* 4GB(rw-:Data, DPL=3) */
	.fill	GDT_SIZE-5, 8, 0

excp_error_code:
	.long	0x00000000
//}

カーネルのエントリ部分のソースコードです。 boot/boot.s 286行目の@<code>{ljmp	$8, $0x7e00}でジャンプする先はこのファイルの先頭です。3行目に@<code>{.code32}と書かれている通り、ここからは32ビットの命令です。

まず、19行目でスタックポインタを0x00080000に設定しています。そして、21行目でカーネルとユーザーランド動作中に使用するGDT(グローバルディスクリプタテーブル)を設定しています。@<code>{gdt_descr}ラベルの内容は203～205行目にあります。ここで定数@<code>{GDT_SIZE}を使うために、asm/cpu.hをincludeしています。

23～35行目で割り込みハンドラの設定をしています。IDTの全256エントリを@<code>{ignore_int}ハンドラで初期化しています。@<code>{ignore_int}は195～196行目にあり、内容は@<code>{iret}でreturnするだけです。なお、スタックポインタを表す0x00080000は定数化すべきですね。同じ値を2度も書いているし、@<code>{KERN_STACK_BASE}とかの定数名にしておけば、この値がカーネルのスタックのベースアドレスであると一目でわかります。

その後、37～42行目で、関数からreturnするときと同じようにスタックに値を積み、ret命令でkern_init関数(kernel/init.c)へジャンプしています。スタックポインタの整合性さえ取れていれば、jmp命令でもよさそうですが、C言語の世界の関数呼び出しは、ret命令でcall命令でジャンプしret命令で戻る、という流れなので、一部のアセンブラコードで違ったことをするより、C言語の関数呼び出しの形式に統一しています。

以降は、個別の割り込みハンドラです。kernel/sys.Sでは割り込みハンドラの出入口部分のみで、メインの処理はC言語で記述された関数を呼び出しています。システムコールのソフトウェア割り込み(128番の割り込み)のみ、pushal/popalを使わず、汎用レジスタを一つずつpush/popしているのは、EAXをシステムコールの戻り値に使っているからです。

=== kernel/init.c
//listnum[kernel_init_c][kernel/init.c][c]{
#include <stddef.h>
#include <cpu.h>
#include <intr.h>
#include <excp.h>
#include <memory.h>
#include <console_io.h>
#include <timer.h>
#include <kernel.h>
#include <syscall.h>
#include <task.h>
#include <fs.h>
#include <sched.h>
#include <kern_task.h>
#include <list.h>
#include <queue.h>
#include <common.h>

int kern_init(void)
{
	extern unsigned char syscall_handler;

	unsigned char mask;
	unsigned char i;

	/* Setup console */
	cursor_pos.y += 2;
	update_cursor();

	/* Setup exception handler */
	for (i = 0; i < EXCEPTION_MAX; i++)
		intr_set_handler(i, (unsigned int)&exception_handler);
	intr_set_handler(EXCP_NUM_DE, (unsigned int)&divide_error_handler);
	intr_set_handler(EXCP_NUM_DB, (unsigned int)&debug_handler);
	intr_set_handler(EXCP_NUM_NMI, (unsigned int)&nmi_handler);
	intr_set_handler(EXCP_NUM_BP, (unsigned int)&breakpoint_handler);
	intr_set_handler(EXCP_NUM_OF, (unsigned int)&overflow_handler);
	intr_set_handler(EXCP_NUM_BR, (unsigned int)&bound_range_exceeded_handler);
	intr_set_handler(EXCP_NUM_UD, (unsigned int)&invalid_opcode_handler);
	intr_set_handler(EXCP_NUM_NM, (unsigned int)&device_not_available_handler);
	intr_set_handler(EXCP_NUM_DF, (unsigned int)&double_fault_handler);
	intr_set_handler(EXCP_NUM_CSO,
			 (unsigned int)&coprocessor_segment_overrun_handler);
	intr_set_handler(EXCP_NUM_TS, (unsigned int)&invalid_tss_handler);
	intr_set_handler(EXCP_NUM_NP, (unsigned int)&segment_not_present_handler);
	intr_set_handler(EXCP_NUM_SS, (unsigned int)&stack_fault_handler);
	intr_set_handler(EXCP_NUM_GP, (unsigned int)&general_protection_handler);
	intr_set_handler(EXCP_NUM_PF, (unsigned int)&page_fault_handler);
	intr_set_handler(EXCP_NUM_MF,
			 (unsigned int)&x87_fpu_floating_point_error_handler);
	intr_set_handler(EXCP_NUM_AC, (unsigned int)&alignment_check_handler);
	intr_set_handler(EXCP_NUM_MC, (unsigned int)&machine_check_handler);
	intr_set_handler(EXCP_NUM_XM, (unsigned int)&simd_floating_point_handler);
	intr_set_handler(EXCP_NUM_VE, (unsigned int)&virtualization_handler);

	/* Setup devices */
	con_init();
	timer_init();
	mem_init();

	/* Setup File System */
	fs_init((void *)0x00011000);

	/* Setup tasks */
	kern_task_init();
	task_init(fshell, 0, NULL);

	/* Start paging */
	mem_page_start();

	/* Setup interrupt handler and mask register */
	intr_set_handler(INTR_NUM_TIMER, (unsigned int)&timer_handler);
	intr_set_handler(INTR_NUM_KB, (unsigned int)&keyboard_handler);
	intr_set_handler(INTR_NUM_USER128, (unsigned int)&syscall_handler);
	intr_init();
	mask = intr_get_mask_master();
	mask &= ~(INTR_MASK_BIT_TIMER | INTR_MASK_BIT_KB);
	intr_set_mask_master(mask);
	sti();

	/* End of kernel initialization process */
	while (1) {
		x86_halt();
	}

	return 0;
}
//}

ブートローダー・カーネルと来て、ここがC言語のスタート地点です。ここではカーネルの初期化を行う@<code>{kern_init}関数を定義しています。なお、@<code>{kern_init}関数は69行もあり、OS5の中で3番目に長い関数です。

初期化の流れは以下の通りです。

 1. コンソール設定(25〜27行目)
 2. 例外ハンドラ設定(29〜53行目)
 3. デバイスドライバ設定(55〜58行目)
 4. ファイルシステム設定(60〜61行目)
 5. タスク設定(スケジューラ設定)(63〜65行目)
 6. ページング設定(MMU設定)(67〜68行目)
 7. 割り込みハンドラ設定(70〜78行目)
 8. カーネルタスクとして動作(80〜83行目)

2.と7.を除き、各処理は関数化されており、初期化処理の本体は各関数内で行っています。初期化処理の内容については各ソースコードで説明します。なお、2.と7.の処理について、例外・割り込み共に@<code>{intr_set_handler}関数でハンドラを設定しています。第1引数が割り込み/例外のベクタ番号で、第2引数がハンドラの先頭アドレスです。kernel/sys.S で定義していたハンドラはここで設定されます。そして、7.の@<code>{intr_init}関数で割り込み初期化後、割り込みマスクの設定でタイマーとキーボードのみ割り込みを有効化しています。@<code>{sti}関数でアセンブラの@<code>{sti}命令を呼び出すことにより、CPUの割り込み機能が有効化されます。

カーネル自体はイベントドリブンで動作するように作っています。そのため、カーネルの各機能の設定を終えると8.でx86_halt関数(割り込み等が発生するまで命令実行を停止させるx86のhlt命令を呼び出す関数)を呼び出し、でCPUを寝かせます。

カーネル起動時に最初に起動されるタスクであるシェルはどのように起動されるのかというと、4.でシェルの実行バイナリを見つけ、5.でカーネルのタスクの枠組みへシェルを設定し、6.でメモリ周りの設定を行い、7.の割り込み有効化でタイマー割り込みが開始することでスケジューラが動作を開始する、という流れです。スケジューラが動作を開始すると、10ms周期のタイマー割り込み契機でランキューのタスクを切り替えてタスクを実行します。

定数化できていないマジックナンバーについて、@<code>{fs_init}関数に渡している"0x00011000"は、ユーザーランドの先頭アドレスです。

#@# また、アセンブラ命令を直接呼び出すマクロの名前が、 `sti` 命令を呼び出す `sti` マクロもあれば、 `hlt` 命令を呼び出す `x86_halt` マクロもあったりと、統一できていないです。

=== kernel/include/kernel.h
//listnum[kernel_include_kernel_h][kernel/include/kernel.h][c]{
#ifndef _KERNEL_H_
#define _KERNEL_H_

enum {
	SYSCALL_TIMER_GET_GLOBAL_COUNTER = 1,
	SYSCALL_SCHED_WAKEUP_MSEC,
	SYSCALL_SCHED_WAKEUP_EVENT,
	SYSCALL_CON_GET_CURSOR_POS_Y,
	SYSCALL_CON_PUT_STR,
	SYSCALL_CON_PUT_STR_POS,
	SYSCALL_CON_DUMP_HEX,
	SYSCALL_CON_DUMP_HEX_POS,
	SYSCALL_CON_GET_LINE,
	SYSCALL_OPEN,
	SYSCALL_EXEC,
	SYSCALL_EXIT
};

enum {
	EVENT_TYPE_KBD = 1,
	EVENT_TYPE_EXIT
};

#endif /* _KERNEL_H_ */
//}

カーネルがアプリケーションへ公開しているシステムコールについて定義しています。

現状、カーネルとアプリケーションの間のAPIはシステムコールのみです。

また、UNIXのようにデバイスなどをファイルにしていない事もあり、ひたすらシステムコールが増えていく枠組みです。ただし、システムコールのインタフェースは単純なので、これはこれでシンプルで良いかなとも思います。

== 割り込み/例外
=== kernel/include/intr.h
//listnum[kernel_include_intr_h][kernel/include/intr.h][c]{
#ifndef _INTR_H_
#define _INTR_H_

#define IOADR_MPIC_OCW2	0x0020
#define IOADR_MPIC_OCW2_BIT_MANUAL_EOI	0x60
#define IOADR_MPIC_ICW1	0x0020
#define IOADR_MPIC_ICW2	0x0021
#define IOADR_MPIC_ICW3	0x0021
#define IOADR_MPIC_ICW4	0x0021
#define IOADR_MPIC_OCW1	0x0021
#define IOADR_SPIC_ICW1	0x00a0
#define IOADR_SPIC_ICW2	0x00a1
#define IOADR_SPIC_ICW3	0x00a1
#define IOADR_SPIC_ICW4	0x00a1
#define IOADR_SPIC_OCW1	0x00a1

#define INTR_NUM_USER128	0x80

void intr_init(void);
void intr_set_mask_master(unsigned char mask);
unsigned char intr_get_mask_master(void);
void intr_set_mask_slave(unsigned char mask);
unsigned char intr_get_mask_slave(void);
void intr_set_handler(unsigned char intr_num, unsigned int handler_addr);

#endif /* _INTR_H_ */
//}

割り込みコントローラ(PIC:Programmable Interrupt Controller)のレジスタのIOアドレスのdefineと、割り込み設定関数のプロトタイプ宣言です。

=== kernel/intr.c
//listnum[kernel_intr_c][kernel/intr.c][c]{
#include <intr.h>
#include <io_port.h>

void intr_init(void)
{
	/* マスタPICの初期化 */
	outb_p(0x11, IOADR_MPIC_ICW1);
	outb_p(0x20, IOADR_MPIC_ICW2);
	outb_p(0x04, IOADR_MPIC_ICW3);
	outb_p(0x01, IOADR_MPIC_ICW4);
	outb_p(0xff, IOADR_MPIC_OCW1);

	/* スレーブPICの初期化 */
	outb_p(0x11, IOADR_SPIC_ICW1);
	outb_p(0x28, IOADR_SPIC_ICW2);
	outb_p(0x02, IOADR_SPIC_ICW3);
	outb_p(0x01, IOADR_SPIC_ICW4);
	outb_p(0xff, IOADR_SPIC_OCW1);
}

void intr_set_mask_master(unsigned char mask)
{
	outb_p(mask, IOADR_MPIC_OCW1);
}

unsigned char intr_get_mask_master(void)
{
	return inb_p(IOADR_MPIC_OCW1);
}

void intr_set_mask_slave(unsigned char mask)
{
	outb_p(mask, IOADR_SPIC_OCW1);
}

unsigned char intr_get_mask_slave(void)
{
	return inb_p(IOADR_SPIC_OCW1);
}

void intr_set_handler(unsigned char intr_num, unsigned int handler_addr)
{
	extern unsigned char idt;
	unsigned int intr_dscr_top_half, intr_dscr_bottom_half;
	unsigned int *idt_ptr;

	idt_ptr = (unsigned int *)&idt;
	intr_dscr_bottom_half = handler_addr;
	intr_dscr_top_half = 0x00080000;
	intr_dscr_top_half = (intr_dscr_top_half & 0xffff0000)
		| (intr_dscr_bottom_half & 0x0000ffff);
	intr_dscr_bottom_half = (intr_dscr_bottom_half & 0xffff0000) | 0x00008e00;
	if (intr_num == INTR_NUM_USER128)
		intr_dscr_bottom_half |= 3 << 13;
	idt_ptr += intr_num * 2;
	*idt_ptr = intr_dscr_top_half;
	*(idt_ptr + 1) = intr_dscr_bottom_half;
}
//}

割り込み/例外の初期化や設定を行う関数群を定義しています。ハードウェアとしてはプログラマブルインタラプトコントローラ(PIC)を扱う関数群です。

@<code>{intr_init}関数では割り込み番号の開始を「0x20(32)番以降」に設定しています(@<code>{outb_p(0x20, IOADR_MPIC_ICW2)}でマスタPICを0x20〜に設定し、@<code>{outb_p(0x28, IOADR_SPIC_ICW2)}でスレーブPICを0x28〜に設定)。割り込み番号も例外番号も同じ番号の空間なので、割り込み番号の開始を「0番以降」としてしまうと、割り込みの0番と例外の0番でバッティングします。例外は0〜20(0x14)番まであって、こちらは変更できないので、割り込み番号を0x20〜にしています。

割り込み番号の開始番号(0x20)や初期化の値は書籍「パソコンのレガシィI/O 活用大全」を参考にしています。なお、割り込み番号の開始が0x20なのはLinuxカーネルでもそうだったと思います(違っていたらゴメンナサイ)。

=== kernel/include/excp.h
//listnum[kernel_include_excp_h][kernel/include/excp.h][c]{
#ifndef _EXCP_H_
#define _EXCP_H_

#define EXCEPTION_MAX	21
#define EXCP_NUM_DE	0
#define EXCP_NUM_DB	1
#define EXCP_NUM_NMI	2
#define EXCP_NUM_BP	3
#define EXCP_NUM_OF	4
#define EXCP_NUM_BR	5
#define EXCP_NUM_UD	6
#define EXCP_NUM_NM	7
#define EXCP_NUM_DF	8
#define EXCP_NUM_CSO	9
#define EXCP_NUM_TS	10
#define EXCP_NUM_NP	11
#define EXCP_NUM_SS	12
#define EXCP_NUM_GP	13
#define EXCP_NUM_PF	14
#define EXCP_NUM_MF	16
#define EXCP_NUM_AC	17
#define EXCP_NUM_MC	18
#define EXCP_NUM_XM	19
#define EXCP_NUM_VE	20

extern unsigned char exception_handler;
extern unsigned char divide_error_handler;
extern unsigned char debug_handler;
extern unsigned char nmi_handler;
extern unsigned char breakpoint_handler;
extern unsigned char overflow_handler;
extern unsigned char bound_range_exceeded_handler;
extern unsigned char invalid_opcode_handler;
extern unsigned char device_not_available_handler;
extern unsigned char double_fault_handler;
extern unsigned char coprocessor_segment_overrun_handler;
extern unsigned char invalid_tss_handler;
extern unsigned char segment_not_present_handler;
extern unsigned char stack_fault_handler;
extern unsigned char general_protection_handler;
extern unsigned char page_fault_handler;
extern unsigned char x87_fpu_floating_point_error_handler;
extern unsigned char alignment_check_handler;
extern unsigned char machine_check_handler;
extern unsigned char simd_floating_point_handler;
extern unsigned char virtualization_handler;

void do_exception(void);
void do_page_fault(unsigned int error_code, unsigned int address);

#endif /* _EXCP_H_ */
//}

例外の番号とアセンブラ・C言語側のハンドラの定義です。

# なぜenumを使わないのか。。。

=== kernel/excp.c
//listnum[kernel_excp_c][kernel/excp.c][c]{
#include <excp.h>
#include <console_io.h>

void do_exception(void)
{
	put_str("exception\r\n");
	while (1);
}

void do_page_fault(unsigned int error_code, unsigned int address)
{
	put_str("page fault\r\n");
	put_str("error code: 0x");
	dump_hex(error_code, 8);
	put_str("\r\n");
	put_str("address   : 0x");
	dump_hex(address, 8);
	put_str("\r\n");
	while (1);
}
//}

kernel/sys.Sの例外のハンドラから呼び出される本体の処理を記述しています。

例外は起きてしまった場合、デバッグしなければならないので、例外に陥った時点でハンドラ内で@<code>{while (1)}でブロックするようにしています。

== メモリ管理
=== kernel/include/memory.h
//listnum[kernel_include_memory_h][kernel/include/memory.h][c]{
#ifndef __MEMORY_H__
#define __MEMORY_H__

#define PAGE_SIZE	0x1000
#define PAGE_ADDR_MASK	0xfffff000

struct page_directory_entry {
	union {
		struct {
			unsigned int all;
		};
		struct {
			unsigned int p: 1, r_w: 1, u_s: 1, pwt: 1, pcd: 1, a: 1,
				reserved: 1, ps: 1, g: 1, usable: 3,
				pt_base: 20;
		};
	};
};
struct page_table_entry {
	union {
		struct {
			unsigned int all;
		};
		struct {
			unsigned int p: 1, r_w: 1, u_s: 1, pwt: 1, pcd: 1, a: 1,
				d: 1, pat: 1, g: 1, usable: 3, page_base: 20;
		};
	};
};

void mem_init(void);
void mem_page_start(void);
void *mem_alloc(void);
void mem_free(void *page);

#endif /* __MEMORY_H__ */
//}

MMU(メモリ管理ユニット)周りの定数、構造体、関数の定義です。

=== kernel/memory.c
//listnum[kernel_memory_c][kernel/memory.c][c]{
#include <stddef.h>
#include <memory.h>

#define CR4_BIT_PGE	(1U << 7)
#define MAX_HEAP_PAGES	64
#define HEAP_START_ADDR	0x00040000

static char heap_alloc_table[MAX_HEAP_PAGES] = {0};

void mem_init(void)
{
	struct page_directory_entry *pde;
	struct page_table_entry *pte;
	unsigned int paging_base_addr;
	unsigned int i;
	unsigned int cr4;

	/* Enable PGE(Page Global Enable) flag of CR4*/
	__asm__("movl	%%cr4, %0":"=r"(cr4):);
	cr4 |= CR4_BIT_PGE;
	__asm__("movl	%0, %%cr4"::"r"(cr4));

	/* Initialize kernel page directory */
	pde = (struct page_directory_entry *)0x0008f000;
	pde->all = 0;
	pde->p = 1;
	pde->r_w = 1;
	pde->pt_base = 0x00090;
	pde++;
	for (i = 1; i < 0x400; i++) {
		pde->all = 0;
		pde++;
	}

	/* Initialize kernel page table */
	pte = (struct page_table_entry *)0x00090000;
	for (i = 0x000; i < 0x007; i++) {
		pte->all = 0;
		pte++;
	}
	paging_base_addr = 0x00007;
	for (; i <= 0x085; i++) {
		pte->all = 0;
		pte->p = 1;
		pte->r_w = 1;
		pte->g = 1;
		pte->page_base = paging_base_addr;
		paging_base_addr += 0x00001;
		pte++;
	}
	for (; i < 0x095; i++) {
		pte->all = 0;
		pte++;
	}
	paging_base_addr = 0x00095;
	for (; i <= 0x09f; i++) {
		pte->all = 0;
		pte->p = 1;
		pte->r_w = 1;
		pte->g = 1;
		pte->page_base = paging_base_addr;
		paging_base_addr += 0x00001;
		pte++;
	}
	for (; i < 0x0b8; i++) {
		pte->all = 0;
		pte++;
	}
	paging_base_addr = 0x000b8;
	for (; i <= 0x0bf; i++) {
		pte->all = 0;
		pte->p = 1;
		pte->r_w = 1;
		pte->pwt = 1;
		pte->pcd = 1;
		pte->g = 1;
		pte->page_base = paging_base_addr;
		paging_base_addr += 0x00001;
		pte++;
	}
	for (; i < 0x400; i++) {
		pte->all = 0;
		pte++;
	}
}

void mem_page_start(void)
{
	unsigned int cr0;

	__asm__("movl	%%cr0, %0":"=r"(cr0):);
	cr0 |= 0x80000000;
	__asm__("movl	%0, %%cr0"::"r"(cr0));
}

void *mem_alloc(void)
{
	unsigned int i;

	for (i = 0; heap_alloc_table[i] && (i < MAX_HEAP_PAGES); i++);

	if (i >= MAX_HEAP_PAGES)
		return (void *)NULL;

	heap_alloc_table[i] = 1;
	return (void *)(HEAP_START_ADDR + i * PAGE_SIZE);
}

void mem_free(void *page)
{
	unsigned int i = ((unsigned int)page - HEAP_START_ADDR) / PAGE_SIZE;
	heap_alloc_table[i] = 0;
}
//}

メモリ関係の関数群で、主にCPUのメモリ管理ユニット(MMU)の設定を行います。MMUはページングという機能を提供するものです。ページングは、メモリを「ページ」という単位で分割し、「仮想アドレス」という実際のアドレス(物理アドレス)とは別のアドレスを割り当てて管理します。そして、仮想アドレスから物理アドレスへの変換表を「ページテーブル」と呼びます。変換の流れを@<img>{mmu}に示します。なお、複数のページテーブルをまとめたものを「ページディレクトリ」と呼びます。

//image[mmu][ページテーブルとMMU][scale=0.8]{
CPUがVAでMMUへアドレス変換を依頼し、
MMUがPAを返すまでの流れ
//}

CPUの設定でページングを有効化すると、カーネルやアプリケーションは仮想アドレスで動作するようになります。これにより、「アプリケーションはカーネルの領域へアクセスさせない」、「アプリケーションはすべて同じアドレスから実行を開始する」といったことを実現しています。なお、ページサイズは4KBです@<fn>{page_size}。また、仮想アドレスは"Virtual Address"で"VA"、物理アドレスは"Physical Address"で"PA"などと記載されていたりもします。
//footnote[page_size][CPUの設定で変更できます。]

ページディレクトリとページテーブルへ設定を追加し、あるVAをPAに対応付けることを「マッピング」、「マップする」の様に呼びます。OS5でのマッピングに関して、まず、OS5ではVAの0x0000 0000～0x1FFF FFFFをカーネル空間、0x2000 0000～0xFFFF FFFFをユーザ空間としています(@<img>{pa_va_1})。カーネル空間はVA=PAとなるようマップしています(@<img>{pa_va_2})。コンベンショナルメモリのアドレスは0x0000 0000～0x1FFF FFFFに含まれますので、カーネルはコンベンショナルメモリのすべてにアクセスできることになります。ユーザ空間は実行するタスク@<fn>{task}ごとにマップを変えます。例えば、shellの実行時はユーザ空間をshellが配置されているPAへマップします(@<img>{pa_va_3})。
//footnote[task][タスクとアプリケーションは同じものを指します。カーネルではCPUのデータシートの表現に合わせて「タスク」と呼び、ユーザーランドでは直観的な分かりやすさから「アプリケーション」と呼んでいます。]

//image[pa_va_1][VAのマッピングについて(1)][scale=0.7]{
VAとPAが並んだ図
コンベンショナルメモリ領域は4GBメモリ空間で見ると、
線になってしまうくらい狭い
//}

//image[pa_va_2][VAのマッピングについて(2)][scale=0.7]{
カーネル空間はPAをストレートマップ
//}

//image[pa_va_3][VAのマッピングについて(3)][scale=0.7]{
ユーザ空間はアプリケーションの実行バイナリが配置されている領域をマップする
//}

#@# なお、VAからVAへの変換はルールが決まっています(@<img>{va2pa})。
#@# //image[va2pa][VAからPAへの変換方法]{
#@# x86データシートのVAからPAへの変換方法の抜粋
#@# リニアアドレス(VA)のどのビットがPD・PT・PageOffsetに対応しているかを説明
#@# //}

kernel/init.cの@<code>{kern_init}関数からは、@<code>{mem_init}関数と@<code>{mem_page_start}関数を呼び出しています。共にMMUのページング機能の関数で、@<code>{mem_init}で設定し、@<code>{mem_page_start}で有効化します。@<code>{mem_init}ではグローバルページ機能を有効化した後、カーネルのページディレクトリ/テーブルを設定しています。なお、@<code>{mem_init}関数はOS5で2番目に長い関数です(76行)。

残る2つの関数はメモリの動的確保(@<code>{mem_alloc}関数)と解放(@<code>{mem_free}関数)です。動的確保/解放はページサイズに合わせて4KB単位で、@<code>{heap_alloc_table}という配列で管理しています。

仮想アドレス空間は0x0000 0000〜0x1fff ffffがカーネル空間で、0x2000 0000〜0xffff ffffがユーザ空間です。カーネル空間へは物理アドレスの同じアドレス(0x0000 0000〜0x1fff ffff)が対応付けられています(マップされています)。

====[column] 「仮想アドレス」はARM CPUの言い回しです
「仮想アドレス」という言い回しは、実はIntel CPUの言い回しではないです。ページングの仕組みをARM CPUで先に勉強していて、Intel CPUの「リニアアドレス」よりわかりやすい気がして、ページングに関しては「仮想アドレス」という言い回しを使っています。ちなみに、x86はページングの他にセグメンテーションという仕組みもあるため、物理アドレスへ至る流れは「論理アドレス(セグメンテーション)」→「リニアアドレス(ページング)」→「物理アドレス」となります。

====[column] OS5ではセグメンテーションを使用していません
セグメンテーションもページングと同じく物理アドレスを分割し、「論理アドレス」というアドレスを割り当てて管理する機能です。「論理アドレス」という形でページングと同様に「仮想アドレス」を提供できます。(ちなみに、セグメンテーションにもページフォルト例外同様に「セグメント不在例外」があります。)

ハードウェアが持つ機能は積極的に使うようにしていきたいのですが、ページングで事足りているため、セグメンテーションは使用していません。ただし、ページングと違いセグメンテーションは機能として無効化することができないので、1つのセグメントがメモリ空間全て(0x0000 0000〜0xffff ffff)を指すように設定しています(kernel/sys.Sの211〜217行目)。

== タスク管理
=== kernel/include/sched.h
//listnum[kernel_include_sched_h][kernel/include/sched.h][c]{
#ifndef _SCHED_H_
#define _SCHED_H_

#include <cpu.h>
#include <task.h>

#define TASK_NUM	3

extern struct task task_instance_table[TASK_NUM];
extern struct task *current_task;

unsigned short sched_get_current(void);
int sched_runq_enq(struct task *t);
int sched_runq_del(struct task *t);
void schedule(void);
int sched_update_wakeupq(void);
void wakeup_after_msec(unsigned int msec);
int sched_update_wakeupevq(unsigned char event_type);
void wakeup_after_event(unsigned char event_type);

#endif /* _SCHED_H_ */
//}

スケジューラに関するヘッダファイルです。

=== kernel/sched.c
//listnum[kernel_sched_c][kernel/sched.c][c]{
#include <stddef.h>
#include <sched.h>
#include <cpu.h>
#include <io_port.h>
#include <intr.h>
#include <timer.h>
#include <lock.h>
#include <kern_task.h>

static struct {
	struct task *head;
	unsigned int len;
} run_queue = {NULL, 0};
static struct {
	struct task *head;
	unsigned int len;
} wakeup_queue = {NULL, 0};
static struct {
	struct task *head;
	unsigned int len;
} wakeup_event_queue = {NULL, 0};
static struct task dummy_task;
static unsigned char is_task_switched_in_time_slice = 0;

struct task task_instance_table[TASK_NUM];
struct task *current_task = NULL;

unsigned short sched_get_current(void)
{
	return x86_get_tr() / 8;
}

int sched_runq_enq(struct task *t)
{
	unsigned char if_bit;

	kern_lock(&if_bit);

	if (run_queue.head) {
		t->prev = run_queue.head->prev;
		t->next = run_queue.head;
		run_queue.head->prev->next = t;
		run_queue.head->prev = t;
	} else {
		t->prev = t;
		t->next = t;
		run_queue.head = t;
	}
	run_queue.len++;

	kern_unlock(&if_bit);

	return 0;
}

int sched_runq_del(struct task *t)
{
	unsigned char if_bit;

	if (!run_queue.head)
		return -1;

	kern_lock(&if_bit);

	if (run_queue.head->next != run_queue.head) {
		if (run_queue.head == t)
			run_queue.head = run_queue.head->next;
		t->prev->next = t->next;
		t->next->prev = t->prev;
	} else
		run_queue.head = NULL;
	run_queue.len--;

	kern_unlock(&if_bit);

	return 0;
}

void schedule(void)
{
	if (!run_queue.head) {
		if (current_task) {
			current_task = NULL;
			outb_p(IOADR_MPIC_OCW2_BIT_MANUAL_EOI | INTR_IR_TIMER,
			       IOADR_MPIC_OCW2);
			task_instance_table[KERN_TASK_ID].context_switch();
		}
	} else if (current_task) {
		if (current_task != current_task->next) {
			current_task = current_task->next;
			if (is_task_switched_in_time_slice) {
				current_task->task_switched_in_time_slice = 1;
				is_task_switched_in_time_slice = 0;
			}
			outb_p(IOADR_MPIC_OCW2_BIT_MANUAL_EOI | INTR_IR_TIMER,
			       IOADR_MPIC_OCW2);
			current_task->context_switch();
		}
	} else {
		current_task = run_queue.head;
		if (is_task_switched_in_time_slice) {
			current_task->task_switched_in_time_slice = 1;
			is_task_switched_in_time_slice = 0;
		}
		outb_p(IOADR_MPIC_OCW2_BIT_MANUAL_EOI | INTR_IR_TIMER,
		       IOADR_MPIC_OCW2);
		current_task->context_switch();
	}
}

int sched_wakeupq_enq(struct task *t)
{
	unsigned char if_bit;

	kern_lock(&if_bit);

	if (wakeup_queue.head) {
		t->prev = wakeup_queue.head->prev;
		t->next = wakeup_queue.head;
		wakeup_queue.head->prev->next = t;
		wakeup_queue.head->prev = t;
	} else {
		t->prev = t;
		t->next = t;
		wakeup_queue.head = t;
	}
	wakeup_queue.len++;

	kern_unlock(&if_bit);

	return 0;
}

int sched_wakeupq_del(struct task *t)
{
	unsigned char if_bit;

	if (!wakeup_queue.head)
		return -1;

	kern_lock(&if_bit);

	if (wakeup_queue.head->next != wakeup_queue.head) {
		if (wakeup_queue.head == t)
			wakeup_queue.head = wakeup_queue.head->next;
		t->prev->next = t->next;
		t->next->prev = t->prev;
	} else
		wakeup_queue.head = NULL;
	wakeup_queue.len--;

	kern_unlock(&if_bit);

	return 0;
}

int sched_update_wakeupq(void)
{
	struct task *t, *next;
	unsigned char if_bit;

	if (!wakeup_queue.head)
		return -1;

	kern_lock(&if_bit);

	t = wakeup_queue.head;
	do {
		next = t->next;
		if (t->wakeup_after_msec > TIMER_TICK_MS) {
			t->wakeup_after_msec -= TIMER_TICK_MS;
		} else {
			t->wakeup_after_msec = 0;
			sched_wakeupq_del(t);
			sched_runq_enq(t);
		}
		t = next;
	} while (wakeup_queue.head && t != wakeup_queue.head);

	kern_unlock(&if_bit);

	return 0;
}

void wakeup_after_msec(unsigned int msec)
{
	unsigned char if_bit;

	kern_lock(&if_bit);

	if (current_task->next != current_task)
		dummy_task.next = current_task->next;
	current_task->wakeup_after_msec = msec;
	sched_runq_del(current_task);
	sched_wakeupq_enq(current_task);
	current_task = &dummy_task;
	is_task_switched_in_time_slice = 1;
	schedule();

	kern_unlock(&if_bit);
}

int sched_wakeupevq_enq(struct task *t)
{
	unsigned char if_bit;

	kern_lock(&if_bit);

	if (wakeup_event_queue.head) {
		t->prev = wakeup_event_queue.head->prev;
		t->next = wakeup_event_queue.head;
		wakeup_event_queue.head->prev->next = t;
		wakeup_event_queue.head->prev = t;
	} else {
		t->prev = t;
		t->next = t;
		wakeup_event_queue.head = t;
	}
	wakeup_event_queue.len++;

	kern_unlock(&if_bit);

	return 0;
}

int sched_wakeupevq_del(struct task *t)
{
	unsigned char if_bit;

	if (!wakeup_event_queue.head)
		return -1;

	kern_lock(&if_bit);

	if (wakeup_event_queue.head->next != wakeup_event_queue.head) {
		if (wakeup_event_queue.head == t)
			wakeup_event_queue.head = wakeup_event_queue.head->next;
		t->prev->next = t->next;
		t->next->prev = t->prev;
	} else
		wakeup_event_queue.head = NULL;
	wakeup_event_queue.len--;

	kern_unlock(&if_bit);

	return 0;
}

int sched_update_wakeupevq(unsigned char event_type)
{
	struct task *t, *next;
	unsigned char if_bit;

	if (!wakeup_event_queue.head)
		return -1;

	kern_lock(&if_bit);

	t = wakeup_event_queue.head;
	do {
		next = t->next;
		if (t->wakeup_after_event == event_type) {
			t->wakeup_after_event = 0;
			sched_wakeupevq_del(t);
			sched_runq_enq(t);
		}
		t = next;
	} while (wakeup_event_queue.head && t != wakeup_event_queue.head);

	kern_unlock(&if_bit);

	return 0;
}

void wakeup_after_event(unsigned char event_type)
{
	unsigned char if_bit;

	kern_lock(&if_bit);

	if (current_task->next != current_task)
		dummy_task.next = current_task->next;
	current_task->wakeup_after_event = event_type;
	sched_runq_del(current_task);
	sched_wakeupevq_enq(current_task);
	current_task = &dummy_task;
	is_task_switched_in_time_slice = 1;
	schedule();

	kern_unlock(&if_bit);
}
//}

スケジューラの関数を定義しています。カーネルの中ではkernel/console_io.cに次いで長いソースファイルです(291行)。

一番重要な関数は@<code>{schedule}関数です。主にタイマー割り込み(10ms)で呼び出され、実行するタスクを切り替える(コンテキストスイッチ)役割を担います(@<img>{timeslice})。実行可能なタスクは「ランキュー」というキューへ設定します。そのため、コンテキストスイッチの際は、ランキューの中から次のタスクを選択します(@<img>{context_switch})。

//image[timeslice][タイムスライスについて][scale=0.7]{
10ms周期のタイマー割り込みでタスクを切り替える図
タイムスライスの説明も付記している
//}

//image[context_switch][コンテキストスイッチまでの流れ][scale=0.7]{
1. 現在実行中のタスク
2. コンテキストスイッチイベント発生
3. 次に実行するタスクを選択
4. コンテキストスイッチ
//}

#@# タイマー割り込みで呼び出され、タスクスケジュールを行う@<code>{schedule}関数と、ランキュー、ウェイクアップキュー、ウェイクアップイベントキューの操作が主な関数です。定義している関数と簡単な説明は以下の通りです。

#@#  * unsigned short sched_get_current(void)
#@#  ** TRレジスタからタスクIDを取り出す
#@#  ** 誰も使っていない
#@#  * int sched_runq_enq(struct task *t)
#@#  ** ランキューへタスク追加
#@#  ** kernel/timer.c: do_ir_timer() から呼ばれる
#@#  * int sched_runq_del(struct task *t)
#@#  ** ランキューからタスク削除
#@#  * void schedule(void)
#@#  ** スケジューリング
#@#  ** kernel/timer.c: do_ir_timer()から呼ばれる
#@#  * int sched_wakeupq_enq(struct task *t)
#@#  ** ウェイクアップキューへタスク追加
#@#  * int sched_wakeupq_del(struct task *t)
#@#  ** ウェイクアップキューからタスク追加
#@#  * int sched_update_wakeupq(void)
#@#  ** ウェイクアップキュー更新
#@#  ** タイマーハンドラから呼び出される(kernel/timer.c: do_ir_timer())
#@#  * void wakeup_after_msec(unsigned int msec)
#@#  ** タスクをXms後に起床させる(それまで寝る)
#@#  ** SYSCALL_SCHED_WAKEUP_MSEC システムコールから呼ばれる
#@#  * int sched_wakeupevq_enq(struct task *t)
#@#  ** ウェイクアップイベントキューへタスク追加
#@#  * int sched_wakeupevq_del(struct task *t)
#@#  ** ウェイクアップイベントキューからタスク削除
#@#  * int sched_update_wakeupevq(unsigned char event_type)
#@#  ** ウェイクアップイベントキュー更新
#@#  ** 以下から呼ばれる
#@#  *** kernel/console_io.c: do_ir_keyboard()
#@#  *** kernel/task.c: task_exit()
#@#  * void wakeup_after_event(unsigned char event_type)
#@#  ** イベント後にタスクを起床させる(それまで寝る)
#@#  ** 以下から呼ばれる
#@#  *** kernel/console_io.c: get_keydata()

その他には、ランキューとウェイクアップキューの操作の関数を定義しています。OS5のスケジューラでは、タスクは時間経過やイベント(キーボード入力、タスク終了)を待つことができます。待っている間はランキューから外し、ウェイクアップキュー、あるいはウェイクアップイベントキューへ追加します。ウェイクアップキューが時間経過待ちのキューで、ウェイクアップイベントキューがイベント発生待ちのキューです。

これらのキューの使い方の例としてuptimeというアプリケーションがウェイクアップキューを使用してスリープする流れを説明します。まず、uptimeが「33ms後に起こしてほしい」とカーネルへ通知します(@<img>{sleep_1})。アプリケーションとカーネルのインタフェースはシステムコールで、この場合、"SYSCALL_SCHED_WAKEUP_MSEC"というシステムコールを引数に「33ms」を設定して発行します。ソースコードの対応する箇所はapps/uptime/uptime.cの@<code>{syscall(SYSCALL_SCHED_WAKEUP_MSEC, 33, 0, 0);}です(23行目)。

//image[sleep_1][スリープの流れ(1)][scale=0.5]{
uptimeが33ms後のウェイクアップを設定
(「33ms後に起こしてー」)
//}

すると、カーネルはuptimeをランキューから外し、ウェイクアップキューへ移します(@<img>{sleep_2})。ランキューにはshellのみになるので、以降はshellのみ実行されます。ソースコードとしては、システムコールの入り口処理を実装しているkernel/syscall.cから、"SYSCALL_SCHED_WAKEUP_MSEC"の場合、kernel/sched.cの@<code>{wakeup_after_msec()}(185～201行目)を呼び出しています。

//image[sleep_2][スリープの流れ(2)][scale=0.5]{
uptimeをウェイクアップキューへ移す
ランキューにはshellのみになる
//}

そして、タイマー割り込み発生時に所定の時間(今回の場合「33ms」)経過していたことを確認すると、uptimeをランキューへ戻します(@<img>{sleep_3})。ソースコードとしては、kernel/timer.cで@<code>{sched_update_wakeupq()}を呼び出しています。@<code>{sched_update_wakeupq()}は、kernel/sched.cの157～183行目で定義しています。

//image[sleep_3][スリープの流れ(3)][scale=0.5]{
タイマー割り込み発生
uptimeをランキューへ戻す
//}

なお、全てのタスクがスリープ等でランキューから抜けた場合、「カーネルタスク」が動作します(@<img>{kernel_task})。カーネルタスクの実体は初期化完了後の@<code>{kern_init}関数(kernel/init.c)で、80〜83行目でx86の@<code>{hlt}命令を無限ループで何度も実行しているため、カーネルタスクがスケジュールされると、割り込みが入るまでCPUは@<code>{hlt}命令で休むことになります。なお、@<code>{schedule}関数内において、81〜87行目の条件分岐がカーネルタスクへコンテキストスイッチしている箇所です。

//image[kernel_task][カーネルタスク][scale=0.6]{
ランキューに何も無いときはカーネルタスクを実行
カーネルタスクはhlt命令を実行し、CPUを休ませる
//}

==== スリープ後のコンテキストスイッチの問題
タスクがスリープした後のコンテキストスイッチにはちょっとした問題があります。例えば、shellがタイムスライス(10ms)の途中でキー入力待ちでスリープしたとします(@<img>{sleep_ctxsw_issue_1})。shellが"SYSCALL_SCHED_WAKEUP_EVENT"のシステムコールを発行することになり、kernel/sched.cの@<code>{wakeup_after_event}関数が呼ばれます。

//image[sleep_ctxsw_issue_1][スリープ後のコンテキストスイッチの問題(1)][scale=0.7]{
shellがキー入力待ちでスリープしたとする
//}

shellがランキューから外され、uptimeが次に実行するタスクとして選択されたとします。すると、shellのタイムスライスの残り時間が経過するとコンテキストスイッチされてしまいます。タイムスライスを10msとしている以上、タスクには10msは実行させてあげるべきなので、これではちょっと不平等です(@<img>{sleep_ctxsw_issue_2})。

//image[sleep_ctxsw_issue_2][スリープ後のコンテキストスイッチの問題(2)][scale=0.7]{
その後uptimeがスケジュールされたとすると、
uptimeは次のタイマー割り込みによるコンテキストスイッチまで、
少ししか実行できない
//}

そのため、OS5カーネルのスケジューラでは、タスクがスリープした場合はタイムスライスの残りを次のタスクへプレゼントしたものと考え、スリープ後のタイマー割り込みではコンテキストスイッチしないようにしています(@<img>{sleep_ctxsw_issue_3})。

//image[sleep_ctxsw_issue_3][スリープ後のコンテキストスイッチの問題(3)][scale=0.7]{
自らスリープした時は、「タイムスライスの残りを次のタスクへプレゼント」と判断
uptimeが継続して実行する
//}

実装としては、スリープでのコンテキストスイッチ時にフラグ変数@<code>{is_task_switched_in_time_slice}をセットします(@<code>{wakeup_after_msec}と@<code>{wakeup_after_event}関数内でセットしています)。そして、@<code>{schedule}関数内で、@<code>{is_task_switched_in_time_slice}をチェックし、セットされていた場合は@<code>{current_task->task_switched_in_time_slice}をセットしています(92行目、102行目)。この時の@<code>{current_task}はコンテキストスイッチ後のタスクを指しています。コンテキストスイッチの後、タイマー割り込みが発生しても、タイマー割り込みハンドラ(kernel/timer.cの@<code>{do_ir_timer}関数)で@<code>{current_task->task_switched_in_time_slice}をチェックし、セットされている場合は@<code>{schedule}関数を呼び出さないようにしています(kernel/timer.cの12～18行目)。

====[column] 過去の遺産"task_instance_table"
@<tt>{task_instance_table}はカーネルタスクにしか使っていないです。元々はこのテーブルにすべてのタスクが並んでいたのですが、メモリの動的確保を実装し、タスクの生成時に動的に確保するようにしたため、今ではカーネルタスクだけ@<tt>{task_instance_table}に残っている状態です。

#@# タスク終了のウェイクアップイベントにおいて、現状、タスクIDの指定はできません。何らかのタスクが終了した場合、イベント発生待ちのタスクが復帰する、となります。タスク終了待ちが必要なのは、シェルで外部コマンドを実行した際にコマンドの実行終了を待ってからプロンプトを出すためです。

=== kernel/include/kern_task.h
//listnum[kernel_include_kern_task_h][kernel/include/kern_task.h][c]{
#ifndef _KERN_TASK_H_
#define _KERN_TASK_H_

#define KERN_TASK_ID		0

void kern_task_init(void);

#endif /* _KERN_TASK_H_ */
//}

カーネルタスクの@<tt>{task_instance_table}内のインデックス(@<tt>{KERN_TASK_ID})の定義と、初期化関数(@<tt>{kern_task_init})のプロトタイプ宣言を行っています。

=== kernel/kern_task_init.c
//listnum[kernel_kern_task_init_c][kernel/kern_task_init.c][c]{
#include <kern_task.h>
#include <cpu.h>
#include <sched.h>

#define KERN_TASK_GDT_IDX	5

static void kern_task_context_switch(void)
{
	__asm__("ljmp	$0x28, $0");
}

void kern_task_init(void)
{
	static struct tss kern_task_tss;
	unsigned int old_cr3, cr3 = 0x0008f018;
	unsigned short segment_selector = 8 * KERN_TASK_GDT_IDX;

	kern_task_tss.esp0 = 0x0007f800;
	kern_task_tss.ss0 = GDT_KERN_DS_OFS;
	kern_task_tss.__cr3 = 0x0008f018;
	init_gdt(KERN_TASK_GDT_IDX, (unsigned int)&kern_task_tss,
		 sizeof(kern_task_tss), 0);
	__asm__("movl	%%cr3, %0":"=r"(old_cr3):);
	cr3 |= old_cr3 & 0x00000fe7;
	__asm__("movl	%0, %%cr3"::"r"(cr3));
	__asm__("ltr %0"::"r"(segment_selector));

	/* Setup context switch function */
	task_instance_table[KERN_TASK_ID].context_switch =
		kern_task_context_switch;
}
//}

カーネルタスクの初期化を行う@<code>{kern_task_init}関数を定義しています。今実行中のコンテキストをタスクとして登録する点が、その他のタスク登録とは異なります。(そのため、今だに@<tt>{kern_task_init}関数が残っています。)

やっていることは以下の3つです。

 1. TSSの設定、GDTへの登録(18〜22行目)
 2. CR3レジスタの設定(23〜25行目)
 3. ltr命令でTSSのロード(26行目)
 4. コンテキストスイッチ関数の登録(28〜30行目)

1.について、タスクステートセグメント(TSS)の設定を行っています。x86 CPUはタスク管理の機能を持っており、x86 CPUの枠組みでタスクを管理する際の構造がTSSです。TSSもセグメントなので、GDTへ登録します(21〜22行目)。

#@# 2.は実質何も影響しない行です。まず、23行目の@<code>{__asm__("movl	%%cr3, %0":"=r"(old_cr3):)}のインラインアセンブラでは、CR3レジスタの内容を変数old_cr3へ格納しています。なお、CPUのCR3レジスタには上位20ビット(ビット31～12)にページディレクトリのベースアドレスが格納されていて@<fn>{pdbr}、下位12ビット(ビット11～0)にキャッシュ関係の設定ビットがあります。24行目では、CR3の下位12ビットのビット4と3をクリア「しようと」しています。しかし、@<code>{cr3 |= old_cr3 & 0x00000fe7}という書き方では、定数(@<code>{0x00000fe7})でマスクした後、cr3変数とORした結果をcr3へ代入しているので、結果的にビット3と4は

#@# 操作しようとしているのはビット4とビット3です。ビット4は、PCD(ページキャッシュディスエーブル)というビットで、セットされているとページディレクトリのキャッシュが抑制されます。そしてビット3はPWT(ページレベル書き込み透過)のビットで、セットされるとライトスルーキャッシングが有効になる。クリアされるとライトバックキャッシングが有効になる。
#@# //footnote[pdbr][そのため、CR3は、PDBR(ページディレクトリベースレジスタ)とも呼ばれます。]

#@# の処理は、PWT(bit3、ページレベル書き込み透過、セットされるとライトスルーキャッシングが有効になる。クリアされるとライトバックキャッシングが有効になる。)とPCD(bit4、ページレベルキャッシュディスエーブル、セットされているとページディレクトリのキャッシュが抑制される)のビットを落としたいと見受けられますが、 `old_cr3` との論理積の結果を論理和でcr3へ代入しているので、24行目は実質、何も影響しない行になっていますね。

2.について、CR3はページディレクトリのベースアドレスとキャッシュの設定を行うレジスタです@<fn>{pdbr}。ページディレクトリのベースアドレスは4KB(0x1000)の倍数でなければならないので、下位12ビットは必ず0です。そこで、CR3の下位12ビットにページディレクトリの設定を行うビットがあります。設定ビットはビット4(PCD@<fn>{pcd})とビット3(PWT@<fn>{pwt})で、それ以外のビット(ビット11〜5とビット2〜0)は予約ビットです。CR3へ設定したい内容は、15行目で変数@<code>{cr3}へ設定しています。ページディレクトリの開始アドレスが0x0008 f000で(@<img>{aboutos5|memmap_phys})、PCDとPWTを共にセットするため、CR3へ設定する値は"0x0008 f018"です。なお、CR3の予約ビットへは、CR3を読み出して得られた値を書き込まなければならないとデータシートに記載されています。そのため、23〜25行目では、CR3レジスタを読み出し(23行目)、読みだした値(@<code>{old_cr3}変数)の予約ビットのみ@<code>{cr3}変数へ反映し(24行目)、その後@<code>{cr3}変数の値をCR3レジスタへ格納(25行目)ということを行っています。
//footnote[pdbr][そのため、CR3は、PDBR(ページディレクトリベースレジスタ)とも呼ばれます。]
//footnote[pcd][ページキャッシュディスエーブルです。セットされているとページディレクトリのキャッシュが抑制されます。]
//footnote[pwt][ページレベル書き込み透過です。セットされるとライトスルーキャッシングが有効になり、クリアされるとライトバックキャッシングが有効になります。]

3.は@<code>{ltr}命令を使用して1.で登録したTSSをタスクレジスタ(TR)へ登録しています。

4.はコンテキストスイッチ用の関数を@<code>{struct task}構造体のエントリへ登録しているところです。@<code>{struct task}はOS5のカーネルでタスクを管理する構造体です。kernel/sched.cでも使用していますが、@<code>{task_instance_table}配列は@<code>{struct task}構造体の配列です。@<code>{kernel_task_context_switch}関数が登録される関数で、やっていることは@<code>{ljmp}命令のインラインアセンブラ1行です。@<code>{ljmp}命令はオペランドにGDT内のTSSのオフセットを与えるとそのタスクへコンテキストスイッチできます。カーネルタスクのTSSのGDT内でのオフセットは0x28なので、@<code>{ljmp}命令で0x28を指定することでカーネルタスクへコンテキストスイッチできます。なお、第2オペランドはコンテキストスイッチの場合、無視されます。

====[column] 古い実装"task_instalce_table"
4.の@<code>{task_instalce_table}は、実は古い実装で、今は使用しているのはカーネルタスクのみです。その他のユーザーランドのタスクでは@<code>{struct task}はタスク生成時に動的に確保します(kernel/task.cで説明します)。

=== kernel/include/task.h
//listnum[kernel_include_task_h][kernel/include/task.h][c]{
#ifndef _TASK_H_
#define _TASK_H_

#include <cpu.h>
#include <fs.h>

#define CONTEXT_SWITCH_FN_SIZE	12
#define CONTEXT_SWITCH_FN_TSKNO_FIELD	8

struct task {
	/* ランキュー・ウェイクアップキュー(時間経過待ち・イベント待ち)の
	 * いずれかに繋がれる(同時に複数のキューに存在することが無いよう
	 * 運用する) */
	struct task *prev;
	struct task *next;

	unsigned short task_id;
	struct tss tss;
	void (*context_switch)(void);
	unsigned char context_switch_func[CONTEXT_SWITCH_FN_SIZE];
	char task_switched_in_time_slice;
	unsigned int wakeup_after_msec;
	unsigned char wakeup_after_event;
};

extern unsigned char context_switch_template[CONTEXT_SWITCH_FN_SIZE];

void task_init(struct file *f, int argc, char *argv[]);
void task_exit(struct task *t);

#endif /* _TASK_H_ */
//}

タスクの構造体(@<code>{struct task})に関する定義と、タスク生成時の初期化関数(@<code>{task_init})とタスク終了関数(@<code>{task_exit})のプロトタイプ宣言です。

=== kernel/task.c
//listnum[kernel_task_c][kernel/task.c][c]{
#include <task.h>
#include <memory.h>
#include <fs.h>
#include <sched.h>
#include <common.h>
#include <lock.h>
#include <kernel.h>
#include <cpu.h>

#define GDT_IDX_OFS	5
#define APP_ENTRY_POINT	0x20000030
#define APP_STACK_BASE_USER	0xffffe800
#define APP_STACK_BASE_KERN	0xfffff000
#define APP_STACK_SIZE	4096
#define GDT_USER_CS_OFS	0x0018
#define GDT_USER_DS_OFS	0x0020

/*
00000000 <context_switch>:
   0:   55                      push   %ebp
   1:   89 e5                   mov    %esp,%ebp
   3:   ea 00 00 00 00 00 00    ljmp   $0x00,$0x0
   a:   5d                      pop    %ebp
   b:   c3                      ret
 */
unsigned char context_switch_template[CONTEXT_SWITCH_FN_SIZE] = {
	0x55,
	0x89, 0xe5,
	0xea, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x5d,
	0xc3
};

static unsigned short task_id_counter = 1;

static int str_get_len(const char *src)
{
	int len;
	for (len = 0; src[len] != '\0'; len++);
	return len + 1;
}

void task_init(struct file *f, int argc, char *argv[])
{
	struct page_directory_entry *pd_base_addr, *pde;
	struct page_table_entry *pt_base_addr, *pt_stack_base_addr, *pte;
	struct task *new_task;
	unsigned int paging_base_addr, phys_stack_base, phys_stack_base2;
	unsigned int i;
	unsigned int len = 0;
	unsigned int argv_space_num, vsp, arg_size;
	unsigned char *sp, *sp2;
	char *t;

	/* Allocate task resources */
	pd_base_addr = (struct page_directory_entry *)mem_alloc();
	pt_base_addr = (struct page_table_entry *)mem_alloc();
	pt_stack_base_addr = (struct page_table_entry *)mem_alloc();
	new_task = (struct task *)mem_alloc();
	phys_stack_base = (unsigned int)mem_alloc();
	phys_stack_base2 = (unsigned int)mem_alloc();

	/* Initialize task page directory */
	pde = pd_base_addr;
	pde->all = 0;
	pde->p = 1;
	pde->r_w = 1;
	pde->pt_base = 0x00090;
	pde++;
	for (i = 1; i < 0x080; i++) {
		pde->all = 0;
		pde++;
	}
	pde->all = 0;
	pde->p = 1;
	pde->r_w = 1;
	pde->u_s = 1;
	pde->pt_base = (unsigned int)pt_base_addr >> 12;
	pde++;
	for (i++; i < 0x3ff; i++) {
		pde->all = 0;
		pde++;
	}
	pde->all = 0;
	pde->p = 1;
	pde->r_w = 1;
	pde->u_s = 1;
	pde->pt_base = (unsigned int)pt_stack_base_addr >> 12;
	pde++;

	/* Initialize task page table */
	pte = pt_base_addr;
	paging_base_addr = (unsigned int)f->data_base_addr >> 12;
	for (i = 0; i < f->head->block_num; i++) {
		pte->all = 0;
		pte->p = 1;
		pte->r_w = 1;
		pte->u_s = 1;
		pte->page_base = paging_base_addr++;
		pte++;
	}
	for (; i < 0x400; i++) {
		pte->all = 0;
		pte++;
	}

	/* Initialize stack page table */
	pte = pt_stack_base_addr;
	for (i = 0; i < 0x3fd; i++) {
		pte->all = 0;
		pte++;
	}
	paging_base_addr = phys_stack_base >> 12;
	pte->all = 0;
	pte->p = 1;
	pte->r_w = 1;
	pte->u_s = 1;
	pte->page_base = paging_base_addr;
	pte++;
	paging_base_addr = phys_stack_base2 >> 12;
	pte->all = 0;
	pte->p = 1;
	pte->r_w = 1;
	pte->u_s = 1;
	pte->page_base = paging_base_addr;
	pte++;
	pte->all = 0;
	pte++;

	/* Setup task_id */
	new_task->task_id = task_id_counter++;

	/* Setup context switch function */
	copy_mem(context_switch_template, new_task->context_switch_func,
		 CONTEXT_SWITCH_FN_SIZE);
	new_task->context_switch_func[CONTEXT_SWITCH_FN_TSKNO_FIELD] =
		8 * (new_task->task_id + GDT_IDX_OFS);
	new_task->context_switch = (void (*)(void))new_task->context_switch_func;

	/* Setup GDT for task_tss */
	init_gdt(new_task->task_id + GDT_IDX_OFS, (unsigned int)&new_task->tss,
		 sizeof(struct tss), 3);

	/* Setup task stack */
	/* スタックにint argcとchar *argv[]を積み、
	 * call命令でジャンプした直後を再現する。
	 *
	 * 例) argc=3, argv={"HOGE", "P", "FUGAA"}
	 * | VA          | 内容              | 備考                           |
	 * |-------------+-------------------+--------------------------------|
	 * | 0x2000 17d0 |                   |                                |
	 * | 0x2000 17d4 | (Don't Care)      | ESPはここを指した状態にする(*) |
	 * | 0x2000 17d8 | 3                 | argc                           |
	 * | 0x2000 17dc | 0x2000 17e4       | argv                           |
	 * | 0x2000 17e0 | (Don't Care)      |                                |
	 * | 0x2000 17e4 | 0x2000 17f0       | argv[0]                        |
	 * | 0x2000 17e8 | 0x2000 17f5       | argv[1]                        |
	 * | 0x2000 17ec | 0x2000 17f7       | argv[2]                        |
	 * | 0x2000 17f0 | 'H'  'O' 'G'  'E' |                                |
	 * | 0x2000 17f4 | '\0' 'P' '\0' 'F' |                                |
	 * | 0x2000 17f8 | 'U'  'G' 'A'  'A' |                                |
	 * | 0x2000 17fc | '\0'              |                                |
	 * |-------------+-------------------+--------------------------------|
	 * | 0x2000 1800 |                   |                                |
	 * (*) call命令はnearジャンプ時、call命令の次の命令のアドレスを
	 * 復帰時のEIPとしてスタックに積むため。
	 */
	for (i = 0; i < (unsigned int)argc; i++) {
		len += str_get_len(argv[i]);
	}
	argv_space_num = (len / 4) + 1;
	arg_size = 4 * (4 + argc + argv_space_num);

	sp = (unsigned char *)(phys_stack_base2 + (APP_STACK_SIZE / 2));
	sp -= arg_size;

	sp += 4;

	*(int *)sp = argc;
	sp += 4;

	*(unsigned int *)sp = APP_STACK_BASE_USER - (4 * (argc + argv_space_num));
	sp += 4;

	sp += 4;

	vsp = APP_STACK_BASE_USER - (4 * argv_space_num);
	sp2 = sp + (4 * argc);
	for (i = 0; i < (unsigned int)argc; i++) {
		*(unsigned int *)sp = vsp;
		sp += 4;
		t = argv[i];
		for (; *t != '\0'; t++) {
			vsp++;
			*sp2++ = *t;
		}
		*sp2++ = '\0';
		vsp++;
	}

	/* Setup task_tss */
	new_task->tss.eip = APP_ENTRY_POINT;
	new_task->tss.esp = APP_STACK_BASE_USER - arg_size;
	new_task->tss.eflags = 0x00000200;
	new_task->tss.esp0 = APP_STACK_BASE_KERN;
	new_task->tss.ss0 = GDT_KERN_DS_OFS;
	new_task->tss.es = GDT_USER_DS_OFS | 0x0003;
	new_task->tss.cs = GDT_USER_CS_OFS | 0x0003;
	new_task->tss.ss = GDT_USER_DS_OFS | 0x0003;
	new_task->tss.ds = GDT_USER_DS_OFS | 0x0003;
	new_task->tss.fs = GDT_USER_DS_OFS | 0x0003;
	new_task->tss.gs = GDT_USER_DS_OFS | 0x0003;
	new_task->tss.__cr3 = (unsigned int)pd_base_addr | 0x18;

	/* Add task to run_queue */
	sched_runq_enq(new_task);
}

void task_exit(struct task *t)
{
	unsigned char if_bit;
	struct page_directory_entry *pd_base_addr, *pde;
	struct page_table_entry *pt_base_addr, *pt_stack_base_addr, *pte;
	unsigned int phys_stack_base, phys_stack_base2;

	kern_lock(&if_bit);

	sched_update_wakeupevq(EVENT_TYPE_EXIT);
	sched_runq_del(t);

	pd_base_addr =
		(struct page_directory_entry *)(t->tss.__cr3 & PAGE_ADDR_MASK);
	pde = pd_base_addr + 0x080;
	pt_base_addr = (struct page_table_entry *)(pde->pt_base << 12);
	pde = pd_base_addr + 0x3ff;
	pt_stack_base_addr = (struct page_table_entry *)(pde->pt_base << 12);
	pte = pt_stack_base_addr + 0x3fd;
	phys_stack_base = pte->page_base << 12;
	pte = pt_stack_base_addr + 0x3fe;
	phys_stack_base2 = pte->page_base << 12;

	mem_free((void *)phys_stack_base2);
	mem_free((void *)phys_stack_base);
	mem_free(t);
	mem_free(pt_stack_base_addr);
	mem_free(pt_base_addr);
	mem_free(pd_base_addr);

	schedule();

	kern_unlock(&if_bit);
}
//}

タスク@<fn>{task}の実行開始時の初期化を行う@<code>{task_init}関数と、タスク終了時の終了処理を行う@<code>{task_exit}関数を定義しています。また、カーネル内ではここでしか使わないために@<code>{str_get_len}関数もここで定義しています。なお、@<code>{task_init}はOS5の中で最も長い関数です(175行)。
//footnote[task][OS5ではx86 CPUの言い回しに合わせて「タスク」と呼んでいます。なお、カーネルより上位のユーザーランドで話すときは分かり易さから「アプリケーション」と呼んでいます。実体は共に同じものです。]

@<code>{task_init}はタスクの生成から、スケジューラへの登録までを行います。以下の流れです。

 1. タスクが存在する為のメモリを確保(55〜61行目)
 2. タスクのページディレクトリ/テーブルを設定(63〜128行目)
 3. タスクIDを生成、設定(130〜131行目)
 4. コンテキストスイッチ関数を生成、設定(133〜138行目)
 5. タスクステートセグメント(TSS)をGDTへ登録(140〜142行目)
 6. タスクのスタック領域へ実行に必要な値を積む(144〜199行目)
 7. タスクステートセグメント(TSS)設定(201〜213行目)
 8. ランキューへタスクを登録(215〜216行目)

1.では@<code>{mem_alloc}を、「ページディレクトリ」、「ページテーブル(コード・データ領域)」、「ページテーブル(スタック領域)」、「@<code>{struct task}」、「スタック領域(x2)」の合計6回呼び出しています。4KB毎のアロケーションなので、合計すると24KBがタスク一つ当たりに必要なメモリです。

2.について、ページディレクトリ・ページテーブルの構成を@<img>{pd_pt}に示します。例としてshellとuptimeの2つのタスクについて描いています。共にカーネルのページテーブルを指しているのは、システムコール呼び出しでユーザーモードからカーネルモードへ権限昇格した際に、カーネル空間の関数を呼び出せるようにするためです。
//footnote[paging][ページという単位で物理アドレスを仮想アドレスに対応付けます。この対応表をページディレクトリ、ページテーブルと呼びます。2つあるのは階層構造になっているためです。ページディレクトリ→ページテーブルという構造で、ページディレクトリ1つに1024個のページディレクトリを持ちます。]

//image[pd_pt][OS5のページディレクトリ・ページテーブル構成][scale=0.5]{
//}

4.では、コンテキストスイッチの関数のバイナリを動的に生成しています。kernel/kern_task_init.cでも説明しましたが、@<code>{ljmp}命令はオペランドにGDT内のTSSのオフセットを指定することで、コンテキストスイッチできます。データシートを読む限り、このオペランドはレジスタを指定することもできるようなのですが、少なくともQEMUで正常動作を確認できていません。そこで、苦肉の策として、@<code>{ljmp}命令を含むコンテキストスイッチ用の関数のコンパイル後のバイナリを予め用意しておき(18〜32行目の@<code>{context_switch_template}配列)、@<code>{task_init}で新しいタスクを生成する際に、@<code>{context_switch_template}からコピーしてオペランドの部分のバイナリのみ書き換える事を行っています。

6.について、タスクを実行開始する際に、あたかもランキューに以前から居たかのようにコンテキストスイッチできるよう、確保したばかりのスタック領域へ値を積んでいます。

#@# `str_get_len` 関数は、「現状、ここでしか使われていない」とはいえ、task.cの中で定義しているのはなんとも場違いです。common.cへ移動させるべきですね。

== ファイルシステム
=== kernel/include/fs.h
//listnum[kernel_include_fs_h][kernel/include/fs.h][c]{
#ifndef _FS_H_
#define _FS_H_

#include <list.h>

#define MAX_FILE_NAME			32
#define RESERVED_FILE_HEADER_SIZE	15

struct file_head {
	struct list lst;
	unsigned char num_files;
};

struct file_header {
	char name[MAX_FILE_NAME];
	unsigned char block_num;
	unsigned char reserve[RESERVED_FILE_HEADER_SIZE];
};

struct file {
	struct list lst;
	struct file_header *head;
	void *data_base_addr;
};

extern struct file *fshell;

void fs_init(void *fs_base_addr);
struct file *fs_open(const char *name);
int fs_close(struct file *f);

#endif /* _FS_H_ */
//}

ファイルシステム周りの構造体等を定義しているソースコードです。

=== kernel/fs.c
//listnum[kernel_fs_c][kernel/fs.c][c]{
#include <fs.h>
#include <stddef.h>
#include <memory.h>
#include <list.h>
#include <queue.h>
#include <common.h>

struct file_head fhead;
struct file *fshell;

void fs_init(void *fs_base_addr)
{
	struct file *f;
	unsigned char i;
	unsigned char *file_start_addr = fs_base_addr;

	queue_init((struct list *)&fhead);
	fhead.num_files = *(unsigned char *)fs_base_addr;

	file_start_addr += PAGE_SIZE;
	for (i = 1; i <= fhead.num_files; i++) {
		f = (struct file *)mem_alloc();
		f->head = (struct file_header *)file_start_addr;
		f->data_base_addr =
			(char *)file_start_addr + sizeof(struct file_header);
		file_start_addr += PAGE_SIZE * f->head->block_num;
		queue_enq((struct list *)f, (struct list *)&fhead);
	}
	fshell = (struct file *)fhead.lst.next;
}

struct file *fs_open(const char *name)
{
	struct file *f;

	/* 将来的には、struct fileのtask_idメンバにopenしたタスクの
	 * TASK_IDを入れるようにする。そして、openしようとしているファ
	 * イルのtask_idが既に設定されていれば、fs_openはエラーを返す
	 * ようにする */

	for (f = (struct file *)fhead.lst.next; f != (struct file *)&fhead;
	     f = (struct file *)f->lst.next) {
		if (!str_compare(name, f->head->name))
			return f;
	}

	return NULL;
}

int fs_close(struct file *f __attribute__ ((unused)))
{
	/* 将来的には、fidに対応するstruct fileのtask_idメンバーを設定
	 * なし(0)にする。 */
	return 0;
}
//}

ファイルシステムの初期化と操作を行う関数群です。カーネル初期化(@<code>{kern_init}関数)で@<code>{fs_init}を呼び、openシステムコールから@<code>{fs_open}が呼ばれます。

@<code>{fs_init}は引数でファイルシステムが配置されている領域の先頭アドレスを受け取り、ファイルシステムの内容をスキャンして@<code>{struct file}という構造体のリンクリストを作成します。OS5では「ファイルシステムの1番最初のエントリをカーネル起動後最初に起動させるアプリケーションバイナリとする」ルールにしています。そこで、1番最初のエントリを@<code>{struct file *fshell}へ設定しています。なお、ファイルシステムの1番目のエントリは、実行ファイルであればシェルで無くとも良いです。特に意味もなく@<code>{fshell}という変数名のままになっています。

@<code>{fs_open}は引数で与えられたファイル名と一致する@<code>{struct file}エントリをリンクリストから検索して@<code>{struct file}のポインタを返します。

== システムコール
=== kernel/include/syscall.h
//listnum[kernel_include_syscall_h][kernel/include/syscall.h][c]{
#ifndef _SYSCALL_H_
#define _SYSCALL_H_

unsigned int do_syscall(unsigned int syscall_id, unsigned int arg1,
			unsigned int arg2, unsigned int arg3);

#endif /* _SYSCALL_H_ */
//}

システムコール割り込みから呼び出されてシステムコールを実行する@<tt>{do_syscall}関数のプロトタイプ宣言のみです。

=== kernel/syscall.c
//listnum[kernel_syscall_c][kernel/syscall.c][c]{
#include <syscall.h>
#include <kernel.h>
#include <timer.h>
#include <sched.h>
#include <fs.h>
#include <task.h>
#include <console_io.h>
#include <cpu.h>

unsigned int do_syscall(unsigned int syscall_id, unsigned int arg1,
			unsigned int arg2, unsigned int arg3)
{
	unsigned int result = -1;
	unsigned int gdt_idx;
	unsigned int tss_base_addr;

	switch (syscall_id) {
	case SYSCALL_TIMER_GET_GLOBAL_COUNTER:
		result = timer_get_global_counter();
		break;
	case SYSCALL_SCHED_WAKEUP_MSEC:
		wakeup_after_msec(arg1);
		result = 0;
		break;
	case SYSCALL_SCHED_WAKEUP_EVENT:
		wakeup_after_event(arg1);
		result = 0;
		break;
	case SYSCALL_CON_GET_CURSOR_POS_Y:
		result = (unsigned int)cursor_pos.y;
		break;
	case SYSCALL_CON_PUT_STR:
		put_str((char *)arg1);
		result = 0;
		break;
	case SYSCALL_CON_PUT_STR_POS:
		put_str_pos((char *)arg1, (unsigned char)arg2,
			    (unsigned char)arg3);
		result = 0;
		break;
	case SYSCALL_CON_DUMP_HEX:
		dump_hex(arg1, arg2);
		result = 0;
		break;
	case SYSCALL_CON_DUMP_HEX_POS:
		dump_hex_pos(arg1, arg2, (unsigned char)(arg3 >> 16),
			     (unsigned char)(arg3 & 0x0000ffff));
		result= 0;
		break;
	case SYSCALL_CON_GET_LINE:
		result = get_line((char *)arg1, arg2);
		break;
	case SYSCALL_OPEN:
		result = (unsigned int)fs_open((char *)arg1);
		break;
	case SYSCALL_EXEC:
		task_init((struct file *)arg1, (int)arg2, (char **)arg3);
		result = 0;
		break;
	case SYSCALL_EXIT:
		gdt_idx = x86_get_tr() / 8;
		tss_base_addr = (gdt[gdt_idx].base2 << 24) |
			(gdt[gdt_idx].base1 << 16) | (gdt[gdt_idx].base0);
		task_exit((struct task *)(tss_base_addr - 0x0000000c));
		result = 0;
		break;
	}

	return result;
}
//}

システムコールのソースファイルです。このソースファイルではシステムコールの入り口処理(@<code>{do_syscall}関数)を定義しています。

システムコール呼び出しの流れを説明します。「shellが"Hello"とコンソール画面へ表示したい」とします(@<img>{syscall_1})。

//image[syscall_1][システムコール実行の流れ(1)][scale=0.6]{
shellが特権レベル3で「"Hello"と画面表示したい」
//}

コンソール画面へ文字列を表示するためのシステムコールは"CON_PUT_STR"です。システムコール呼び出しにおいて、アプリケーションとカーネルでパラメータの受け渡しには汎用レジスタ(EAX、EBX、ECX、EDX)を使用します。CON_PUT_STRを実行するために、汎用レジスタへ必要なパラメータを設定します(@<img>{syscall_2})。

//image[syscall_2][システムコール実行の流れ(2)][scale=0.6]{
CON_PUT_STRシステムコールを実行するために、
EAXへシステムコールID(CON_PUT_STR)を、
EBXへ"Hello"の先頭アドレスを設定
//}

システムコールを発行するトリガーはソフトウェア割り込みです。OS5では割り込み番号128番をシステムコールとしています。そのため、shellは128番のソフトウェア割り込みを実行します(@<img>{syscall_3})。

//image[syscall_3][システムコール実行の流れ(3)][scale=0.6]{
shellがソフトウェア割り込み実行
//}

すると、カーネル側で128番の割り込みハンドラが呼び出され、同時に特権レベルが昇格するので、カーネル空間の関数を呼び出せるようになります(@<img>{syscall_4})。なお、割り込みハンドラの入り口はkernel/sys.Sの@<code>{syscall_handler}ラベルの箇所です(174〜193行目)。@<code>{syscall_handler}からkernel/syscall.cの@<code>{do_syscall}関数を呼び出しています。

//image[syscall_4][システムコール実行の流れ(4)][scale=0.6]{
カーネル内の対応する割り込みハンドラが呼び出される
同時に、特権レベルが0へ昇格する
//}

割り込みハンドラ内では、汎用レジスタに設定された値に従って、カーネル空間内の関数を呼び出します(@<img>{syscall_5})。

//image[syscall_5][システムコール実行の流れ(5)][scale=0.6]{
割り込みハンドラ内で以下を実行
1. EAXで対象の
　システムコールを確認
2. システムコールに対応
　するカーネル内の関数
　を実行
3. 戻り値をEAXに設定
4. 割り込みからリターン
//}

割り込みハンドラからreturnすると、元の特権レベルに戻ります(@<img>{syscall_6})。そして、元のアプリケーションの処理を再開します。

//image[syscall_6][システムコール実行の流れ(6)][scale=0.6]{
元の特権レベル(3)に戻る
//}

#@# システムコールはアプリケーションからソフトウェア割り込み(割り込み番号128番)で呼び出します。ソフトウェア割り込みを発生させる命令は@<code>{int}です。@<code>{int}命令を実行する直前に汎用レジスタに値を設定しておくことで、カーネル側の割り込みハンドラ(kernel/sys.Sのsyscall_handler)が呼び出されたときに、汎用レジスタ経由で値を受け取ることができます。同様に、ハンドラを抜けるときに汎用レジスタに値を設定しておくことで、呼び出し元のアプリケーションへ値を返すことができます。OS5では、システムコール時の汎用レジスタの使い方を@<table>{syscallreg}のように決めています。

現状、用意しているシステムコールは@<table>{syscalllist}の通りです。

//table[syscalllist][システムコール一覧]{
定数名(番号)	機能
------------
SYSCALL_TIMER_GET_GLOBAL_COUNTER(1)	タイマカウンタ取得
SYSCALL_SCHED_WAKEUP_MSEC(2)	ウェイクアップ時間(ms)設定
SYSCALL_SCHED_WAKEUP_EVENT(3)	ウェイクアップイベント設定
SYSCALL_CON_GET_CURSOR_POS_Y(4)	カーソルY座標取得
SYSCALL_CON_PUT_STR(5)	コンソールへ文字列出力(座標指定なし)
SYSCALL_CON_PUT_STR_POS(6)	コンソールへ文字列出力(座標指定あり)
SYSCALL_CON_DUMP_HEX(7)	コンソールへ16進で数値出力(座標指定なし)
SYSCALL_CON_DUMP_HEX_POS(8)	コンソールへ16進で数値出力(座標指定あり)
SYSCALL_CON_GET_LINE(9)	コンソール入力を1行取得
SYSCALL_OPEN(10)	ファイルオープン
SYSCALL_EXEC(11)	ファイル実行
SYSCALL_EXIT(12)	タスク終了
//}

#@# システムコールの粒度がまちまちなのは、「必要になったらシステムコール化する」としていたためです。システムコール番号の順序についても同様です。

== デバイスドライバ
=== kernel/include/cpu.h
//listnum[kernel_include_cpu_h][kernel/include/cpu.h][c]{
#ifndef _CPU_H_
#define _CPU_H_

#include <asm/cpu.h>

#define X86_EFLAGS_IF	0x00000200
#define GDT_KERN_DS_OFS	0x0010

#define sti()	__asm__ ("sti"::)
#define cli()	__asm__ ("cli"::)
#define x86_get_eflags()	({			\
unsigned int _v;					\
__asm__ volatile ("\tpushf\n"				\
		  "\tpopl	%0\n":"=r"(_v):);	\
_v;							\
})
#define x86_get_tr()		({			\
unsigned short _v;					\
__asm__ volatile ("\tstr	%0\n":"=r"(_v):);	\
_v;							\
})
#define x86_halt()	__asm__ ("hlt"::)

struct segment_descriptor {
	union {
		struct {
			unsigned int a;
			unsigned int b;
		};
		struct {
			unsigned short limit0;
			unsigned short base0;
			unsigned short base1: 8, type: 4, s: 1, dpl: 2, p: 1;
			unsigned short limit1: 4, avl: 1, l: 1, d: 1, g: 1,
				base2: 8;
		};
	};
};

struct tss {
	unsigned short		back_link, __blh;
	unsigned int		esp0;
	unsigned short		ss0, __ss0h;
	unsigned int		esp1;
	unsigned short		ss1, __ss1h;
	unsigned int		esp2;
	unsigned short		ss2, __ss2h;
	unsigned int		__cr3;
	unsigned int		eip;
	unsigned int		eflags;
	unsigned int		eax;
	unsigned int		ecx;
	unsigned int		edx;
	unsigned int		ebx;
	unsigned int		esp;
	unsigned int		ebp;
	unsigned int		esi;
	unsigned int		edi;
	unsigned short		es, __esh;
	unsigned short		cs, __csh;
	unsigned short		ss, __ssh;
	unsigned short		ds, __dsh;
	unsigned short		fs, __fsh;
	unsigned short		gs, __gsh;
	unsigned short		ldt, __ldth;
	unsigned short		trace;
	unsigned short		io_bitmap_base;
};

extern struct segment_descriptor gdt[GDT_SIZE];

void init_gdt(unsigned int idx, unsigned int base, unsigned int limit,
	      unsigned char dpl);

#endif /* _CPU_H_ */
//}

x86 CPUに依存するインラインアセンブラのマクロや、構造体等を定義しています。

=== kernel/cpu.c
//listnum[kernel_cpu_c][kernel/cpu.c][c]{
#include <cpu.h>

void init_gdt(unsigned int idx, unsigned int base, unsigned int limit,
	      unsigned char dpl)
{
	gdt[idx].limit0 = limit & 0x0000ffff;
	gdt[idx].limit1 = (limit & 0x000f0000) >> 16;

	gdt[idx].base0 = base & 0x0000ffff;
	gdt[idx].base1 = (base & 0x00ff0000) >> 16;
	gdt[idx].base2 = (base & 0xff000000) >> 24;

	gdt[idx].dpl = dpl;

	gdt[idx].type = 9;
	gdt[idx].p = 1;
}
//}

x86 CPUに依存する処理を記述しているソースファイルです。今のところ、GDTへのエントリ追加を行う@<code>{init_gdt}関数のみです。

#@# エントリ追加を行う関数なのに、@<code>{init_}という関数名なのは少し微妙かもですね。

=== kernel/include/io_port.h
//listnum[kernel_include_io_port_h][kernel/include/io_port.h][c]{
#ifndef _IO_PORT_H_
#define _IO_PORT_H_

#define outb(value, port)				\
__asm__ ("outb %%al,%%dx"::"a" (value),"d" (port))

#define inb(port) ({ \
unsigned char _v; \
__asm__ volatile ("inb %%dx,%%al":"=a" (_v):"d" (port)); \
_v; \
})

#define outb_p(value, port) \
__asm__ ("outb %%al,%%dx\n" \
		"\tjmp 1f\n" \
		"1:\tjmp 1f\n" \
		"1:"::"a" (value),"d" (port))

#define inb_p(port) ({ \
unsigned char _v; \
__asm__ volatile ("inb %%dx,%%al\n" \
	"\tjmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:":"=a" (_v):"d" (port)); \
_v; \
})

#endif /* _IO_PORT_H_ */
//}

x86 CPUはI/Oのアドレス空間は分かれており、I/Oへのアクセスには@<code>{in}、@<code>{out}という命令があります。このソースファイルではこれらの命令をインラインアセンブラでマクロ化しています。

=== kernel/include/console_io.h
//listnum[kernel_include_console_io_h][kernel/include/console_io.h][c]{
#ifndef _CONSOLE_IO_H_
#define _CONSOLE_IO_H_

#define IOADR_KBC_DATA				0x0060
#define IOADR_KBC_DATA_BIT_BRAKE	0x80
#define IOADR_KBC_STATUS			0x0064
#define IOADR_KBC_STATUS_BIT_OBF	0x01

#define COLUMNS						80
#define ROWS						25

#define ASCII_ESC					0x1b
#define ASCII_BS					0x08
#define ASCII_HT					0x09

#ifndef COMPILE_APP

#define INTR_IR_KB					1
#define INTR_NUM_KB					33
#define INTR_MASK_BIT_KB			0x02
#define SCREEN_START				0xb8000
#define ATTR						0x07
#define CHATT_CNT					1

struct cursor_position {
	unsigned int x, y;
};

extern unsigned char keyboard_handler;
extern struct cursor_position cursor_pos;

void con_init(void);
void update_cursor(void);
void put_char_pos(char c, unsigned char x, unsigned char y);
void put_char(char c);
void put_str(char *str);
void put_str_pos(char *str, unsigned char x, unsigned char y);
void dump_hex(unsigned int val, unsigned int num_digits);
void dump_hex_pos(unsigned int val, unsigned int num_digits,
		  unsigned char x, unsigned char y);
unsigned char get_keydata_noir(void);
unsigned char get_keydata(void);
unsigned char get_keycode(void);
unsigned char get_keycode_pressed(void);
unsigned char get_keycode_released(void);
char get_char(void);
unsigned int get_line(char *buf, unsigned int buf_size);

#endif /* COMPILE_APP */

#endif /* _CONSOLE_IO_H_ */
//}

コンソールドライバのヘッダファイルです。

@<tt>{#ifndef COMPILE_APP}では、ユーザーランド側でincludeされた場合に関数定義等を参照させないようにしています。コンソールドライバは、@<tt>{CON_PUT_STR}システムコール等でユーザーランドから呼び出します。システムコールにパラメータを与える際に、コンソール1画面の行数・列数等が必要になるため、それらの情報はkernel/以下のヘッダファイルをincludeさせるようにしています。

ただし関数などは、アプリケーションレベルの特権レベルで動作しているユーザーランドからはアクセスできないので、@<tt>{ifndef}で無効化しています。

=== kernel/console_io.c
//listnum[kernel_console_io_c][kernel/console_io.c][c]{
#include <cpu.h>
#include <intr.h>
#include <io_port.h>
#include <console_io.h>
#include <sched.h>
#include <lock.h>
#include <kernel.h>

#define QUEUE_BUF_SIZE	256

const char keymap[] = {
	0x00, ASCII_ESC, '1', '2', '3', '4', '5', '6',
	'7', '8', '9', '0', '-', '^', ASCII_BS, ASCII_HT,
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
	'o', 'p', '@', '[', '\n', 0x00, 'a', 's',
	'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
	':', 0x00, 0x00, ']', 'z', 'x', 'c', 'v',
	'b', 'n', 'm', ',', '.', '/', 0x00, '*',
	0x00, ' ', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, '7',
	'8', '9', '-', '4', '5', '6', '+', '1',
	'2', '3', '0', '.', 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, '_', 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, '\\', 0x00, 0x00
};

struct queue {
	unsigned char buf[QUEUE_BUF_SIZE];
	unsigned char start, end;
	unsigned char is_full;
} keycode_queue;

struct cursor_position cursor_pos;

static unsigned char error_status;

static void enqueue(struct queue *q, unsigned char data)
{
	unsigned char if_bit;

	if (q->is_full) {
		error_status = 1;
	} else {
		error_status = 0;
		kern_lock(&if_bit);
		q->buf[q->end] = data;
		q->end++;
		if (q->start == q->end) q->is_full = 1;
		kern_unlock(&if_bit);
	}
}

static unsigned char dequeue(struct queue *q)
{
	unsigned char data = 0;
	unsigned char if_bit;

	kern_lock(&if_bit);
	if (!q->is_full && (q->start == q->end)) {
		error_status = 1;
	} else {
		error_status = 0;
		data = q->buf[q->start];
		q->start++;
		q->is_full = 0;
	}
	kern_unlock(&if_bit);

	return data;
}

void do_ir_keyboard(void)
{
	unsigned char status, data;

	status = inb_p(IOADR_KBC_STATUS);
	if (status & IOADR_KBC_STATUS_BIT_OBF) {
		data = inb_p(IOADR_KBC_DATA);
		enqueue(&keycode_queue, data);
	}
	sched_update_wakeupevq(EVENT_TYPE_KBD);
	outb_p(IOADR_MPIC_OCW2_BIT_MANUAL_EOI | INTR_IR_KB,
		IOADR_MPIC_OCW2);
}

void con_init(void)
{
	keycode_queue.start = 0;
	keycode_queue.end = 0;
	keycode_queue.is_full = 0;
	error_status = 0;
}

void update_cursor(void)
{
	unsigned int cursor_address = (cursor_pos.y * 80) + cursor_pos.x;
	unsigned char cursor_address_msb = (unsigned char)(cursor_address >> 8);
	unsigned char cursor_address_lsb = (unsigned char)cursor_address;
	unsigned char if_bit;

	kern_lock(&if_bit);
	outb_p(0x0e, 0x3d4);
	outb_p(cursor_address_msb, 0x3d5);
	outb_p(0x0f, 0x3d4);
	outb_p(cursor_address_lsb, 0x3d5);
	kern_unlock(&if_bit);

	if (cursor_pos.y >= ROWS) {
		unsigned int start_address = (cursor_pos.y - ROWS + 1) * 80;
		unsigned char start_address_msb =
			(unsigned char)(start_address >> 8);
		unsigned char start_address_lsb = (unsigned char)start_address;

		kern_lock(&if_bit);
		outb_p(0x0c, 0x3d4);
		outb_p(start_address_msb, 0x3d5);
		outb_p(0x0d, 0x3d4);
		outb_p(start_address_lsb, 0x3d5);
		kern_unlock(&if_bit);
	}
}

void put_char_pos(char c, unsigned char x, unsigned char y)
{
	unsigned char *pos;

	pos = (unsigned char *)(SCREEN_START + (((y * COLUMNS) + x) * 2));
	*(unsigned short *)pos = (unsigned short)((ATTR << 8) | c);
}

void put_char(char c)
{
	switch (c) {
	case '\r':
		cursor_pos.x = 0;
		break;

	case '\n':
		cursor_pos.y++;
		break;

	default:
		put_char_pos(c, cursor_pos.x, cursor_pos.y);
		if (cursor_pos.x < COLUMNS - 1) {
			cursor_pos.x++;
		} else {
			cursor_pos.x = 0;
			cursor_pos.y++;
		}
		break;
	}

	update_cursor();
}

void put_str(char *str)
{
	while (*str != '\0') {
		put_char(*str);
		str++;
	}
}

void put_str_pos(char *str, unsigned char x, unsigned char y)
{
	while (*str != '\0') {
		switch (*str) {
		case '\r':
			x = 0;
			break;

		case '\n':
			y++;
			break;

		default:
			put_char_pos(*str, x, y);
			if (x < COLUMNS - 1) {
				x++;
			} else {
				x = 0;
				y++;
			}
			break;
		}
		str++;
	}
}

void dump_hex(unsigned int val, unsigned int num_digits)
{
	unsigned int new_x = cursor_pos.x + num_digits;
	unsigned int dump_digit = new_x - 1;

	while (num_digits) {
		unsigned char tmp_val = val & 0x0000000f;
		if (tmp_val < 10) {
			put_char_pos('0' + tmp_val, dump_digit, cursor_pos.y);
		} else {
			put_char_pos('A' + tmp_val - 10, dump_digit, cursor_pos.y);
		}
		val >>= 4;
		dump_digit--;
		num_digits--;
	}

	cursor_pos.x = new_x;

	update_cursor();
}

void dump_hex_pos(unsigned int val, unsigned int num_digits,
		  unsigned char x, unsigned char y)
{
	unsigned int new_x = x + num_digits;
	unsigned int dump_digit = new_x - 1;

	while (num_digits) {
		unsigned char tmp_val = val & 0x0000000f;
		if (tmp_val < 10) {
			put_char_pos('0' + tmp_val, dump_digit, y);
		} else {
			put_char_pos('A' + tmp_val - 10, dump_digit, y);
		}
		val >>= 4;
		dump_digit--;
		num_digits--;
	}
}

unsigned char get_keydata_noir(void)
{
	while (!(inb_p(IOADR_KBC_STATUS) & IOADR_KBC_STATUS_BIT_OBF));
	return inb_p(IOADR_KBC_DATA);
}

unsigned char get_keydata(void)
{
	unsigned char data;
	unsigned char dequeuing = 1;
	unsigned char if_bit;

	while (dequeuing) {
		kern_lock(&if_bit);
		data = dequeue(&keycode_queue);
		if (!error_status)
			dequeuing = 0;
		kern_unlock(&if_bit);
		if (dequeuing)
			wakeup_after_event(EVENT_TYPE_KBD);
	}

	return data;
}

unsigned char get_keycode(void)
{
	return get_keydata() & ~IOADR_KBC_DATA_BIT_BRAKE;
}

unsigned char get_keycode_pressed(void)
{
	unsigned char keycode;
	while ((keycode = get_keydata()) & IOADR_KBC_DATA_BIT_BRAKE);
	return keycode & ~IOADR_KBC_DATA_BIT_BRAKE;
}

unsigned char get_keycode_released(void)
{
	unsigned char keycode;
	while (!((keycode = get_keydata()) & IOADR_KBC_DATA_BIT_BRAKE));
	return keycode & ~IOADR_KBC_DATA_BIT_BRAKE;
}

char get_char(void)
{
	return keymap[get_keycode_pressed()];
}

unsigned int get_line(char *buf, unsigned int buf_size)
{
	unsigned int i;

	for (i = 0; i < buf_size - 1;) {
		buf[i] = get_char();
		if (buf[i] == ASCII_BS) {
			if (i == 0) continue;
			cursor_pos.x--;
			update_cursor();
			put_char_pos(' ', cursor_pos.x, cursor_pos.y);
			i--;
		} else {
			put_char(buf[i]);
			if (buf[i] == '\n') {
				put_char('\r');
				break;
			}
			i++;
		}
	}
	buf[i] = '\0';

	return i;
}
//}

コンソールのデバイスドライバです。OS5ではキーボード入力とテキストモードでの画面出力をコンソールとして抽象化しています。そのため、このソースファイルでキー入力と画面出力を共に扱っています。なお、単体のソースファイルの行数としてはboot/boot.sの328行に次いで2番目に長いソースファイルです(307行)。

このソースファイル内で重要なのは@<code>{get_char}関数と、@<code>{put_char}関数です。1行分の入力を取得する@<code>{get_line}関数や文字列を画面表示する@<code>{put_str}関数は@<code>{get_char}関数と@<code>{put_char}関数を内部で呼び出しているため、ここでは@<code>{get_char}関数と@<code>{put_char}関数の動作の流れを説明します。

@<code>{get_char}関数の呼び出しによって何が起こるのかというと、キューからキーコードを含むデータ(キーデータ)を取り出し、ASCIIコードへ変換し戻り値として返します。@<code>{get_keycode_pressed}関数が押下時のキーコードを返す関数で、@<code>{keymap}はキーコードをASCIIコードへ変換する配列です。キーボードコントローラ(KBC)が返すキーデータにはBRAKEというビットが有り、このビットが立っている場合、該当のキーから指が離された事を示します。@<code>{get_keydata_pressed}では@<code>{get_keydata}関数でキューから取得したキーデータにBRAKEのビットが立っている間、ブロックします(押下中を示すキーデータが取得できるまで呼び出し元へreturnしない)。なお、キューにキーデータを積む関数は@<code>{do_ir_keyboard}です。kernel/sys.S の@<code>{keyboard_handler}ハンドラから呼び出されます。

@<code>{put_char}関数の呼び出しでは何が起こるのかというと、引数で渡されたASCIIコード値をカーソル位置に対応したVRAMのアドレスへ書き込みます。グローバル変数の@<code>{struct cursor_position cursor_pos}がカーソル位置を保持している変数で、@<code>{put_char_pos}関数が指定された座標のVRAMアドレスへASCIIコードを書き込む関数です。定数@<code>{SCREEN_START}がVRAMの先頭アドレスです。

kernel/init.cの@<code>{kern_init}関数からはカーソルの設定(@<code>{cursor_pos}の初期化と@<code>{update_cursor}関数によるカーソル位置と表示開始位置の更新)とコンソールドライバの初期化(@<code>{con_init}関数によるキーコードのキューの初期化とエラーステータスの初期化)を行っています。

====[column] 失敗談: キー入力時にゴミが入る(ロックは大切)
キー入力していると、コンソール画面にゴミが出力されることがありました。

当初、全く理由が分からず、少なくとも2週間程、悩んでいた覚えがあります。KBCの割り込み契機のエンキューでゴミが入っているのか、デキュー処理に問題があるのかと、問題を切り分けていきました。

結論としては、@<code>{get_keydata}関数内にて、@<code>{dequeue}関数呼び出しと、その後の@<code>{error_status}変数のチェックの間にKBC割り込みが入ることがあり、その際に割り込みハンドラから呼び出されるエンキュー処理で@<code>{error_status}変数を上書きしてしまう事が原因でした。そのため、正しいキーデータを取得できていないのに、@<code>{get_keydata}関数内のwhileループを抜けてしまい、@<code>{get_keydata}関数は正常ではないキーデータをreturnしていました。

そのため、@<code>{get_keydata}関数内の@<code>{dequeue}関数呼び出しから@<code>{error_status}チェックの間は割り込み禁止(ロック)するようにしています@<fn>{get_keydata_patch}。割り込みハンドラ内とそれ以外で同じリソース(変数等)へアクセスする場合は、正しくロックする必要がある、ということでした。なお、今見ていると、変数@<code>{error_status}をエンキュー用とデキュー用で分けても良かったと思います。
//footnote[get_keydata_patch][https://github.com/cupnes/os5/commit/8f0ffffdf1811a150ec8e95aa6f706940c356850]

====[column] _noirと名の付く関数は?
noirは"NO InteRrupt"の略で、割り込み禁止区間内(主に割り込みハンドラ)で呼び出される事を想定した関数です。これは当初、カーネルのロック処理がネストに対応して居なかったため(kernel/lock.cで説明します)で、割り込み禁止区間内から呼び出されるか、そうでないかで関数を分けていました。今はロック機能がネストに対応しているので、_noirの関数は不要なのですが、割り込みハンドラからの呼び出しではまだ修正されずに残っています。

====[column] OS5はフォントを持たない
OS5ではBIOSで画面モードをテキストモードに設定しています。@<code>{SCREEN_START}はテキストモードでのVRAMの先頭アドレスで、MC6845というCRTコントローラ(CRTC)のVRAMです。

このCRTCはテキストモードでの使用が前提であるため、コントローラ内にフォントを内蔵しており、VRAMのアドレス空間へASCIIで値を書き込むだけで画面表示ができます。カーソル位置と表示開始位置の設定も可能です。カーソル位置の設定により任意の場所にカーソルを設置できます。また、表示開始位置の設定に関しては、そもそもMC6845は表示領域(80x25文字)以上のVRAM領域を持っており、VRAM内のどこからを表示するかを「何文字目からスタート」という形で指定できます。これにより、表示開始位置の設定を行うだけで、画面スクロールが実現できます。

そのため、OS5はフォントも持っていないし、画面スクロールの処理もCRTCへ設定しているだけで、ソフトウェアで処理しているわけではありません。

このように、ハードウェアがどんな機能を持っているかを知っているとソフトウェアでの実装を減らせて便利です(「ハードウェア」を「API」と読み替えても同じことですね)。

=== kernel/include/timer.h
//listnum[kernel_include_timer_h][kernel/include/timer.h][c]{
#ifndef _TIMER_H_
#define _TIMER_H_

#define IOADR_PIT_COUNTER0	0x0040
#define IOADR_PIT_CONTROL_WORD	0x0043
#define IOADR_PIT_CONTROL_WORD_BIT_COUNTER0		0x00
#define IOADR_PIT_CONTROL_WORD_BIT_16BIT_READ_LOAD	0x30
#define IOADR_PIT_CONTROL_WORD_BIT_MODE2		0x04
						/* Rate Generator */

#define INTR_IR_TIMER		0
#define INTR_NUM_TIMER		32
#define INTR_MASK_BIT_TIMER	0x01

#define TIMER_TICK_MS		10

extern unsigned char timer_handler;
extern unsigned int global_counter;

void timer_init(void);
unsigned int timer_get_global_counter(void);

#endif /* _TIMER_H_ */
//}

PIC(Programmable Interval Timer)のレジスタのIOアドレスと割り込み番号などの定数の定義と、関数のプロトタイプ宣言です。

=== kernel/timer.c
//listnum[kernel_timer_c][kernel/timer.c][c]{
#include <timer.h>
#include <io_port.h>
#include <intr.h>
#include <sched.h>

unsigned int global_counter = 0;

void do_ir_timer(void)
{
	global_counter += TIMER_TICK_MS;
	sched_update_wakeupq();
	if (!current_task || !current_task->task_switched_in_time_slice) {
		/* タイムスライス中のコンテキストスイッチではない */
		schedule();
	} else {
		/* タイムスライス中のコンテキストスイッチである */
		current_task->task_switched_in_time_slice = 0;
	}
	outb_p(IOADR_MPIC_OCW2_BIT_MANUAL_EOI | INTR_IR_TIMER, IOADR_MPIC_OCW2);
}

void timer_init(void)
{
	/* Setup PIT */
	outb_p(IOADR_PIT_CONTROL_WORD_BIT_COUNTER0
	       | IOADR_PIT_CONTROL_WORD_BIT_16BIT_READ_LOAD
	       | IOADR_PIT_CONTROL_WORD_BIT_MODE2, IOADR_PIT_CONTROL_WORD);
	/* 割り込み周期11932(0x2e9c)サイクル(=100Hz、10ms毎)に設定 */
	outb_p(0x9c, IOADR_PIT_COUNTER0);
	outb_p(0x2e, IOADR_PIT_COUNTER0);
}

unsigned int timer_get_global_counter(void)
{
	return global_counter;
}
//}

タイマーの初期化と設定を行う関数群を定義しています。

@<code>{timer_init}関数がkernel/init.cの@<code>{kern_init}関数から呼ばれるタイマー初期化の関数です。OS5では10ms周期の割り込みに設定しています。タイマーの挙動は@<img>{timer}の通りです。

//image[timer][タイマー割り込みの振る舞い][scale=0.4]{
カウンタが溜まって、0x2E9C(11932)で割り込みが発生する図
//}

また、@<code>{do_ir_timer}関数が kern/sys.S の@<code>{timer_handler}ハンドラから呼ばれる関数です。

== ライブラリ
=== kernel/include/stddef.h
//listnum[kernel_include_stddef_h][kernel/include/stddef.h][c]{
#ifndef _STDDEF_H_
#define _STDDEF_H_

#define NULL	((void *)0)

#endif /* _STDDEF_H_ */
//}

汎用的に使用する定数の定義です。今のところ、@<tt>{NULL}のみです。

=== kernel/include/common.h
//listnum[kernel_include_common_h][kernel/include/common.h][c]{
#ifndef _COMMON_H_
#define _COMMON_H_

int str_compare(const char *src, const char *dst);
void copy_mem(const void *src, void *dst, unsigned int size);

#endif /* _COMMON_H_ */
//}

共通で使用される関数をkernel/common.cにまとめています。このヘッダファイルではプロトタイプ宣言を行っています。

=== kernel/common.c
//listnum[kernel_common_c][kernel/common.c][c]{
#include <common.h>

int str_compare(const char *src, const char *dst)
{
	char is_equal = 1;

	for (; (*src != '\0') && (*dst != '\0'); src++, dst++) {
		if (*src != *dst) {
			is_equal = 0;
			break;
		}
	}

	if (is_equal) {
		if (*src != '\0') {
			return 1;
		} else if (*dst != '\0') {
			return -1;
		} else {
			return 0;
		}
	} else {
		return (int)(*src - *dst);
	}
}

void copy_mem(const void *src, void *dst, unsigned int size)
{
	unsigned char *d = (unsigned char *)dst;
	unsigned char *s = (unsigned char *)src;

	for (; size > 0; size--) {
		*d = *s;
		d++;
		s++;
	}
}
//}

common.cには、汎用的に使用されるような関数を集めています。

#@# OS5では関数の命名規則がまちまちです。"[クラス名]_[メソッド名]"というような命名規則を考えていた時期があって、@<code>{str_compare}という関数名にしていますが、次の@<code>{copy_mem}で既に崩れていますね。(ただし、@<code>{copy_mem}は@<code>{str_compare}よりも時期的にもっと新しく、common.c にまとめる前は別のソースファイルでした。)

=== kernel/include/lock.h
//listnum[kernel_include_lock_h][kernel/include/lock.h][c]{
#ifndef _LOCK_H_
#define _LOCK_H_

void kern_lock(unsigned char *if_bit);
void kern_unlock(unsigned char *if_bit);

#endif /* _LOCK_H_ */
//}

カーネルのロック機能についての関数のプロトタイプ宣言です。

=== kernel/lock.c
//listnum[kernel_lock_c][kernel/lock.c][c]{
#include <lock.h>
#include <cpu.h>

void kern_lock(unsigned char *if_bit)
{
	/* Save EFlags.IF */
	*if_bit = (x86_get_eflags() & X86_EFLAGS_IF) ? 1 : 0;

	/* if saved IF == true, then cli */
	if (*if_bit)
		cli();
}

void kern_unlock(unsigned char *if_bit)
{
	/* if saved IF == true, then sti */
	if (*if_bit)
		sti();
}
//}

ロック機能に関するソースコードです。ロックとはある処理を実行中に、割り込みなどでCPUが別の処理を行わないようにする機能です。@<code>{kern_lock}では、@<code>{cli}命令で割り込みを無効化し、@<code>{kern_unlock}では、@<code>{sti}命令で割り込みを有効化します。

引数の@<code>{unsigned char *if_bit}は、@<code>{kern_lock}実行時の割り込み有効/無効状態を保持するために使用します。例えば、親の関数で@<code>{kern_lock}を実行していて既に割り込み無効区間(ロック区間)内であるにも関わらず、子側の@<code>{kern_lock}〜@<code>{kern_unlock}で割り込みを有効化してしまうと、親側のロックに影響します。このようなネストした@<code>{kern_lock}/@<code>{kern_unlock}の呼び出しのために@<code>{if_bit}を使用します。

=== kernel/include/list.h
//listnum[kernel_include_list_h][kernel/include/list.h][c]{
#ifndef _LIST_H_
#define _LIST_H_

struct list {
	struct list *next;
	struct list *prev;
};

#endif /* _LIST_H_ */
//}

リンクリストはカーネル内で頻繁に使用するため専用のヘッダファイルを用意しています。@<tt>{struct list}を構造体の一つ目のメンバとすることで、構造体にリンクリストの機能を持たせることができます。例としては、kernel/include/fs.hの@<tt>{struct file}の定義を見てみてください。(このヘッダファイルは、kernel/include/stddef.hへまとめても良さそうな気がします。)

#@# なお、古いリンクリストだと@<tt>{struct list}がまだ無く、使用していなかったりします。例えば、kernel/include/task.hの@<tt>{struct task}です。

=== kernel/include/queue.h
//listnum[kernel_include_queue_h][kernel/include/queue.h][c]{
#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <list.h>

void queue_init(struct list *head);
void queue_enq(struct list *entry, struct list *head);
void queue_del(struct list *entry);
void queue_dump(struct list *head);

#endif /* _QUEUE_H_ */
//}

==== 概要
キュー構造の関数のプロトタイプ宣言です。キュー構造はカーネル内で頻繁に登場するため、専用のヘッダファイルを用意しています。

=== kernel/queue.c
//listnum[kernel_queue_c][kernel/queue.c][c]{
#include <queue.h>
#include <list.h>
#include <console_io.h>

void queue_init(struct list *head)
{
	head->next = head;
	head->prev = head;
}

void queue_enq(struct list *entry, struct list *head)
{
	entry->prev = head->prev;
	entry->next = head;
	head->prev->next = entry;
	head->prev = entry;
}

void queue_del(struct list *entry)
{
	entry->prev->next = entry->next;
	entry->next->prev = entry->prev;
}

void queue_dump(struct list *head)
{
	unsigned int n;
	struct list *entry;

	put_str("h =");
	dump_hex((unsigned int)head, 8);
	put_str(": p=");
	dump_hex((unsigned int)head->prev, 8);
	put_str(", n=");
	dump_hex((unsigned int)head->next, 8);
	put_str("\r\n");

	for (entry = head->next, n = 0; entry != head; entry = entry->next, n++) {
		dump_hex(n, 2);
		put_str("=");
		dump_hex((unsigned int)entry, 8);
		put_str(": p=");
		dump_hex((unsigned int)entry->prev, 8);
		put_str(", n=");
		dump_hex((unsigned int)entry->next, 8);
		put_str("\r\n");
	}
}
//}

カーネル内で汎用的に使えるよう、ここでキュー構造を定義しています。

#@# なお、現時点では、キューから取り出したエントリを使う用途が無いので、「dequeue」の関数は無く、「delete」の関数がある状態です。「今必要な事だけを実装」をかたくなに守っている所ですが、ここは「dequeue」にしておいて、取り出したエントリは呼び出し側で捨てる方が良いですね。

=== kernel/include/debug.h
//listnum[kernel_include_debug_h][kernel/include/debug.h][c]{
#ifndef __DEBUG_H__
#define __DEBUG_H__

extern volatile unsigned char _flag;

void debug_init(void);
void test_excp_de(void);
void test_excp_pf(void);

#endif /* __DEBUG_H__ */
//}

デバッグ機能に関するフラグ変数のexternとプロトタイプ宣言があります。

=== kernel/debug.c
//listnum[kernel_debug_c][kernel/debug.c][c]{
#include <debug.h>

volatile unsigned char _flag;

void debug_init(void)
{
	_flag = 0;
}

/* Test divide by zero exception */
void test_excp_de(void)
{
	__asm__("\tmovw	$8, %%ax\n" \
		"\tmovb	$0, %%bl\n" \
		"\tdivb	%%bl"::);
}

/* Test page fault exception */
void test_excp_pf(void)
{
	volatile unsigned char tmp;
	__asm__("movb	0x000b8000, %0":"=r"(tmp):);
}
//}

カーネルデバッグのための機能です。デバッグフラグ@<tt>{_flag}は、debug.hをincludeして1をセットすると、カーネルのデバッグログをコンソールへ出力するように用意しています。(ただし、@<tt>{_flag}は誰も使っていないです。。)

