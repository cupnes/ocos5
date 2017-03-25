= ユーザーランド
アプリケーションとしては、shellとuptime、whoareyouという3つです。現状、カーネルの動作確認程度のものでしかないです。

shellはその名の通りシェルで、CUIを提供します。shellの組み込みコマンドとしては、echoとメモリ/IOへの直接read/writeのコマンド(readb,readw,readl,ioreadb,writeも同様のコマンド名)、そしてbgというコマンドがあります。bgは今回のリリースで追加したコマンドで、引数で指定したコマンドをバックグラウンド実行します。(これまでは、実行したコマンドの終了を待つことができなかったので、常にバックグラウンド実行でした。)uptime・whoareyouもshellから起動します。これらのコマンド名をshell上で入力すると、shellはexecシステムコール(OS5ではexecをシステムコールとしています)を使用して実行します。

uptimeはマルチタスクの動作確認をするためのコマンドです。コンソール画面の右上で、16進数で起動時間をカウントし続けます。自ら終了することがないコマンドなので、バックグラウンド実行しないとプロンプトが帰ってこなくなります。

whoareyouは今回のリリースで新たに追加したコマンドです。argcとargvによりコマンドライン引数を受け取れることを確認するためのコマンドです。

また、今回のリリースでは、main()をエントリポイントとする変更や、静的ライブラリの仕組みも導入しており、よく見るCのソースコードのようにアプリケーションを書けるようになりました。

話は変わって、ユーザーランドのファイルシステムイメージは、makeの過程で、シェルスクリプトで作成します。ファイルシステムは、簡単に、ファイル名とバイナリのみを管理するだけのもので、シェルスクリプトでバイナリを並べて連結しています。ファイルシステムについても詳しくは上述のスライドをご覧ください。

これまでをまとめると、アプリケーションが実行されるまでの流れは以下のとおりです。

 1. ファイルシステムイメージをブートローダーがRAM上の決まったアドレスへロード
 2. カーネルは、初期化の過程でファイルシステムが配置されているRAM上の領域をチェック
 3. カーネルは、ファイルシステム上の1つ目のファイルを、カーネル起動後に実行する最初のアプリとして実行する(ここでshellが実行される)
== 共通部分
=== apps/Makefile
//listnum[apps_Makefile][apps/Makefile][make]{
LIB_DIR = .lib
BIN_DIR = .bin
APP_LD = ../app.ld
LIB_DIRS = libkernel libcommon libconsole libstring
APP_DIRS = $(shell find . -maxdepth 1 -type d '!' -iname '.*' '!'	\
-iname 'include' '!' -iname 'lib*')
CFLAGS = -Wall -Wextra
CFLAGS += -nostdinc -nostdlib -fno-builtin -c
CFLAGS += -I../include
CFLAGS += -m32
CFLAGS += -DCOMPILE_APP
LDFLAGS = -L../$(LIB_DIR)
LIBS = -lstring -lconsole -lcommon -lkernel

apps.img: lib app
	../tools/make_os5_fs.sh $(BIN_DIR)/* > $@

lib: $(LIB_DIRS)
	[ -d $(LIB_DIR) ] || mkdir $(LIB_DIR)
	for libdir in $^; do \
		make -C $$libdir LIB_DIR=../$(LIB_DIR)	\
		APP_LD=$(APP_LD) CFLAGS="$(CFLAGS)"	\
		LDFLAGS="$(LDFLAGS)" LIBS="$(LIBS)"; \
	done

app: $(APP_DIRS)
	[ -d $(BIN_DIR) ] || mkdir $(BIN_DIR)
	for appdir in $^; do \
		make -C $$appdir BIN_DIR=../$(BIN_DIR)	\
		APP_LD=$(APP_LD) CFLAGS="$(CFLAGS)"	\
		LDFLAGS="$(LDFLAGS)" LIBS="$(LIBS)"; \
	done

clean:
	rm -rf *~ *.o *.a *.bin *.dat *.img *.map $(LIB_DIR) $(BIN_DIR)
	for dir in $(LIB_DIRS) $(APP_DIRS); do \
		make -C $$dir clean; \
	done

.PHONY: lib app clean
//}

ユーザーランド上の各アプリケーションのコンパイルを行う大元のMakefileです。

このMakefileでの最終生成物はapps.imgです。apps.imgを生成するために、ライブラリのコンパイル(@<tt>{lib}ターゲット)、アプリケーションのコンパイル(@<tt>{app}ターゲット)、tools/make_os5_fs.shでファイルシステム生成(@<tt>{apps.img}ターゲット)という流れです。

=== apps/app.ld
//listnum[apps_app_ld][apps/app.ld][]{
OUTPUT_FORMAT("binary");

SECTIONS
{
	. = 0x20000030;
	.text	: {
		*(.entry)
		*(.text)
	}
	.rodata	: {
		*(.strings)
		*(.rodata)
		*(.rodata.*)
	}
	.data	: {*(.data)}
	.bss	: {*(.bss)}
}
//}

アプリケーションのリンカスクリプトです。仮想アドレス空間上、ユーザー空間は0x2000 0000からで、先頭48(0x30)バイトがファイルシステムのヘッダなので、0x2000 0030からtextセクション(コードの領域)を配置しています。そのため、スケジューラでもタスクのエントリアドレスは0x2000 0030としています(kernel/task.cの@<tt>{#define APP_ENTRY_POINT	0x20000030})。

=== apps/include/app.h
//listnum[apps_include_app_h][apps/include/app.h][c]{
#ifndef _APP_H_
#define _APP_H_

int main(int argc, char *argv[]) __attribute__((section(".entry")));

#endif /* _APP_H_ */
//}

エントリ関数としてmain関数のプロトタイプ宣言をしています。

== 0shell
=== apps/0shell/Makefile
//listnum[apps_0shell_Makefile][apps/0shell/Makefile][make]{
NAME=0shell

$(BIN_DIR)/$(NAME): $(NAME).o
	ld -m elf_i386 -o $@ $< -Map $(NAME).map -s -T $(APP_LD) -x	\
	$(LDFLAGS) $(LIBS)

%.o: %.c
	gcc $(CFLAGS) -o $@ $<

clean:
	rm -rf *~ *.o *.map

.PHONY: clean
//}

シェルアプリケーション(0shell)のMakefileです。Makefileはアプリケーションごとに存在しますが、内容は@<tt>{NAME=0shell}の行を除いて共通です。

=== apps/0shell/0shell.c
//listnum[apps_0shell_0shell_c][apps/0shell/0shell.c][c]{
#include <app.h>
#include <kernel.h>
#include <common.h>
#include <string.h>
#include <console.h>

#define MAX_LINE_SIZE	512

enum {
	ECHO,
	READB,
	READW,
	READL,
	IOREADB,
	WRITEB,
	WRITEW,
	WRITEL,
	IOWRITEB,
	BG,
#ifdef DEBUG
	TEST,
#endif /* DEBUG */
	COMMAND_NUM
} _COMMAND_SET;

static int command_echo(char *args)
{
	put_str(args);
	put_str("\r\n");

	return 0;
}

static int command_readb(char *args)
{
	char first[128], other[128];
	unsigned char *addr;

	str_get_first_entry(args, first, other);
	addr = (unsigned char *)str_conv_ahex_int(first);
	dump_hex(*addr, 2);
	put_str("\r\n");

	return 0;
}

static int command_readw(char *args)
{
	char first[128], other[128];
	unsigned short *addr;

	str_get_first_entry(args, first, other);
	addr = (unsigned short *)str_conv_ahex_int(first);
	dump_hex(*addr, 4);
	put_str("\r\n");

	return 0;
}

static int command_readl(char *args)
{
	char first[128], other[128];
	unsigned int *addr;

	str_get_first_entry(args, first, other);
	addr = (unsigned int *)str_conv_ahex_int(first);
	dump_hex(*addr, 8);
	put_str("\r\n");

	return 0;
}

static int command_ioreadb(char *args)
{
	char first[128], other[128];
	unsigned short addr;

	str_get_first_entry(args, first, other);
	addr = (unsigned short)str_conv_ahex_int(first);
	dump_hex(inb_p(addr), 2);
	put_str("\r\n");

	return 0;
}

static int command_writeb(char *args)
{
	char first[16], second[32], other[128], _other[128];
	unsigned char data, *addr;

	str_get_first_entry(args, first, other);
	str_get_first_entry(other, second, _other);
	data = (unsigned char)str_conv_ahex_int(first);
	addr = (unsigned char *)str_conv_ahex_int(second);
	*addr = data;

	return 0;
}

static int command_writew(char *args)
{
	char first[16], second[32], other[128], _other[128];
	unsigned short data, *addr;

	str_get_first_entry(args, first, other);
	str_get_first_entry(other, second, _other);
	data = (unsigned short)str_conv_ahex_int(first);
	addr = (unsigned short *)str_conv_ahex_int(second);
	*addr = data;

	return 0;
}

static int command_writel(char *args)
{
	char first[16], second[32], other[128], _other[128];
	unsigned int data, *addr;

	str_get_first_entry(args, first, other);
	str_get_first_entry(other, second, _other);
	data = (unsigned int)str_conv_ahex_int(first);
	addr = (unsigned int *)str_conv_ahex_int(second);
	*addr = data;

	return 0;
}

static int command_iowriteb(char *args)
{
	char first[16], second[32], other[128], _other[128];
	unsigned char data;
	unsigned short addr;

	str_get_first_entry(args, first, other);
	str_get_first_entry(other, second, _other);
	data = (unsigned char)str_conv_ahex_int(first);
	addr = (unsigned short)str_conv_ahex_int(second);
	outb_p(data, addr);

	return 0;
}

#ifdef DEBUG
static int command_test(char *args)
{
	put_str("test\r\n");

	return 0;
}
#endif /* DEBUG */

static unsigned char get_command_id(const char *command)
{
	if (!str_compare(command, "echo")) {
		return ECHO;
	}

	if (!str_compare(command, "readb")) {
		return READB;
	}

	if (!str_compare(command, "readw")) {
		return READW;
	}

	if (!str_compare(command, "readl")) {
		return READL;
	}

	if (!str_compare(command, "ioreadb")) {
		return IOREADB;
	}

	if (!str_compare(command, "writeb")) {
		return WRITEB;
	}

	if (!str_compare(command, "writew")) {
		return WRITEW;
	}

	if (!str_compare(command, "writel")) {
		return WRITEL;
	}

	if (!str_compare(command, "iowriteb")) {
		return IOWRITEB;
	}

	if (!str_compare(command, "bg")) {
		return BG;
	}

#ifdef DEBUG
	if (!str_compare(command, "test")) {
		return TEST;
	}
#endif /* DEBUG */

	return COMMAND_NUM;
}

int main(int argc __attribute__ ((unused)),
	 char *argv[] __attribute__ ((unused)))
{
	while (1) {
		char buf[MAX_LINE_SIZE];
		char command[256], args[256];
		unsigned char command_id, is_background = 0;
		unsigned int fp;

		put_str("OS5> ");
		if (get_line(buf, MAX_LINE_SIZE) <= 0) {
			continue;
		}

		while (1) {
			str_get_first_entry(buf, command, args);
			command_id = get_command_id(command);
			if (command_id != BG)
				break;
			else {
				is_background = 1;
				copy_mem(args, buf,
					 (unsigned int)str_get_len(args));
			}
		}

		switch (command_id) {
		case ECHO:
			command_echo(args);
			break;
		case READB:
			command_readb(args);
			break;
		case READW:
			command_readw(args);
			break;
		case READL:
			command_readl(args);
			break;
		case IOREADB:
			command_ioreadb(args);
			break;
		case WRITEB:
			command_writeb(args);
			break;
		case WRITEW:
			command_writew(args);
			break;
		case WRITEL:
			command_writel(args);
			break;
		case IOWRITEB:
			command_iowriteb(args);
			break;
#ifdef DEBUG
		case TEST:
			command_test(args);
			break;
#endif /* DEBUG */
		default:
			fp = syscall(SYSCALL_OPEN, (unsigned int)command, 0, 0);
			if (fp) {
				unsigned int argc = 0, i;
				char *argv[256];
				char *start;

				argv[argc++] = command;

				start = &args[0];
				for (i = 0; ; i++) {
					if ((i == 0) && (args[i] == '\0')) {
						break;
					} else if ((args[i] == ' ') ||
						   (args[i] == '\0')) {
						argv[argc++] = start;
						start = &args[i + 1];
						if (args[i] == ' ')
							args[i] = '\0';
						else
							break;
					}
				}

				syscall(SYSCALL_EXEC, fp, argc,
					(unsigned int)argv);

				if (!is_background)
					syscall(SYSCALL_SCHED_WAKEUP_EVENT,
						EVENT_TYPE_EXIT, 0, 0);
			} else
				put_str("Command not found.\r\n");
			break;
		}
	}
}
//}

シェルアプリケーションの本体のソースコードです。OS5の中で3番目に長いソースコードで297行あります。

includeしている各ヘッダファイルについては以下の通りです。

 : app.h
   @<code>{main}関数を使用するため
 : kernel.h
   @<code>{syscall}関数を使用するため
   実体はapps/libkernel/libkernel.cにある
 : common.h
   @<code>{copy_mem}関数を使用するため
   実体はapps/libcommon/libcommon.cにある
 : string.h
   文字列操作関数を使用するため
   実体はapps/libstring/libstring.cにある
 : console.h
   コンソール操作関数を使用するため
   実体はapps/libconsole/libconsole.cにある

main関数の処理をざっくりと説明すると以下の通りです。

 1. プロンプトを表示し、@<tt>{get_line}関数で1行分の入力文字列を取得するまで待機(212～215行目)
 2. 1.の文字列から@<tt>{str_get_first_entry}関数でコマンド名を切り出し、@<tt>{get_command_id}関数でシェル組み込みコマンドのIDを取得。取得したコマンドが"bg"コマンドであった場合は、@<tt>{is_background}フラグをセットする(217～227行目)
 3. コマンドIDに応じた処理を呼び出す(229～295行目)。組み込みコマンドではなかった場合、実行ファイルとみなし、openとexecを行う(263～294行目)

なお、3.でファイルを実行するときに、2.で@<tt>{is_background}がセットされていなければ、ファイルの実行が終了するまでシェルをスリープさせます(289～291行目)。これにより、バックグラウンド実行していない場合は、実行が終了するまでプロンプトが戻ってこないようにしています。

== uptime
=== apps/uptime/Makefile
//listnum[apps_uptime_Makefile][apps/uptime/Makefile][make]{
NAME=uptime

$(BIN_DIR)/$(NAME): $(NAME).o
	ld -m elf_i386 -o $@ $< -Map $(NAME).map -s -T $(APP_LD) -x	\
	$(LDFLAGS) $(LIBS)

%.o: %.c
	gcc $(CFLAGS) -o $@ $<

clean:
	rm -rf *~ *.o *.map

.PHONY: clean
//}

uptimeアプリケーションのMakefileです。

=== apps/uptime/uptime.c
//listnum[apps_uptime_uptime_c][apps/uptime/uptime.c][c]{
#include <app.h>
#include <kernel.h>
#include <console.h>

int main(int argc __attribute__ ((unused)),
	 char *argv[] __attribute__ ((unused)))
{
	static unsigned int uptime;
	unsigned int cursor_pos_y;

	while (1) {
		uptime = get_global_counter() / 1000;
		cursor_pos_y = get_cursor_pos_y();
		if (cursor_pos_y < ROWS) {
			put_str_pos("uptime:", COLUMNS - (7 + 4), 0);
			dump_hex_pos(uptime, 4, COLUMNS - 4, 0);
		} else {
			put_str_pos("uptime:", COLUMNS - (7 + 4),
				    cursor_pos_y - ROWS + 1);
			dump_hex_pos(uptime, 4, COLUMNS - 4,
				     cursor_pos_y - ROWS + 1);
		}
		syscall(SYSCALL_SCHED_WAKEUP_MSEC, 33, 0, 0);
	}
}
//}

uptimeアプリケーションの本体のソースコードです。

uptimeは、OS5が起動してからの秒数を画面右上に表示(16進表記)するアプリケーションです。マルチタスクの動作確認のために作成しました。

処理としては、以下を無限ループで繰り返しています。

 1. 起動後の経過時間(グローバルカウンタ、ms単位)を取得(@<code>{get_global_counter}関数)し、秒へ変換(12行目)
 2. カーソルのY座標を取得(@<code>{get_cursor_pos_y}関数)(13行目)
 3. Y座標に応じた場所へ、1.の計算結果を16進数で出力(14～22行目)
 4. 33msスリープ(23行目)

3.は画面右上に固定させるための処理です。カーソルのY座標が1画面の行数(@<code>{ROWS})未満であれば、まだ一度も画面スクロールを行っていないため、出力するY座標は0で良いです。カーソルのY座標が@<code>{ROWS}以上の場合、画面はスクロールしているので、Y座標0の場所に出力しても見切れてしまいます(表示範囲外のため)。そのため、"カーソルY座標 - ROWS + 1"のY座標へ出力するようにしています(スクロール後、常にカーソルは画面最下行に張り付くため、この計算式になっています)。

4.は画面更新周期を作るためのスリープです。なお、33msは感覚的に決めた値です。スケジューリングの周期が「画面スクロール処理をしてから再度uptimeのカウンタが描画されるまでの間」に当たるとチラつきます。

== whoareyou
=== apps/whoareyou/Makefile
//listnum[apps_whoareyou_Makefile][apps/whoareyou/Makefile][make]{
NAME=whoareyou

$(BIN_DIR)/$(NAME): $(NAME).o
	ld -m elf_i386 -o $@ $< -Map $(NAME).map -s -T $(APP_LD) -x	\
	$(LDFLAGS) $(LIBS)

%.o: %.c
	gcc $(CFLAGS) -o $@ $<

clean:
	rm -rf *~ *.o *.map

.PHONY: clean
//}

whoareyouアプリケーションのMakefileです。

=== apps/whoareyou/whoareyou.c
//listnum[apps_whoareyou_whoareyou_c][apps/whoareyou/whoareyou.c][c]{
#include <app.h>
#include <kernel.h>
#include <string.h>
#include <console.h>

int main(int argc, char *argv[])
{
	if ((argc >= 2) && !str_compare(argv[1], "-v"))
		put_str("Operating System 5\r\n");
	else
		put_str("OS5\r\n");
	exit();

	return 0;
}
//}

「お前は誰だ」と問いかけるアプリケーションです。「OS5」と返してくれます。UNIXで実行したユーザーのユーザー名を表示する"whoami"コマンドのオマージュ(パロディ?)です。

これはコマンドライン引数の動作確認として用意したアプリケーションで、"-v"オプションをつけると「Operating System 5」と詳細に返してくれます。

== libkernel
=== apps/libkernel/Makefile
//listnum[apps_libkernel_Makefile][apps/libkernel/Makefile][make]{
NAME=libkernel

$(LIB_DIR)/$(NAME).a: $(NAME).o
	ar rcs $@ $<

%.o: %.c
	gcc $(CFLAGS) -o $@ $<

clean:
	rm -rf *~ *.o *.a *.map

.PHONY: clean
//}

カーネル回りのライブラリ"libkernel"のMakefileです。内容は@<code>{NAME=libkernel}の行を除いて他のライブラリのMakefileと同じです。

=== apps/include/kernel.h
//listnum[apps_include_kernel_h][apps/include/kernel.h][c]{
#ifndef _APP_KERNEL_H_
#define _APP_KERNEL_H_

#include "../../kernel/include/kernel.h"
#include "../../kernel/include/io_port.h"
#include "../../kernel/include/console_io.h"

unsigned int syscall(unsigned int syscall_id, unsigned int arg1,
		     unsigned int arg2, unsigned int arg3);
unsigned int get_global_counter(void);
void exit(void);

#endif /* _APP_KERNEL_H_ */
//}

libkernelのヘッダファイルです。カーネルとアプリケーションの間のインタフェースはシステムコールのみです。libkernelではシステムコールを関数として提供します。

=== apps/libkernel/libkernel.c
//listnum[apps_libkernel_libkernel_c][apps/libkernel/libkernel.c][c]{
#include <kernel.h>

unsigned int syscall(unsigned int syscall_id, unsigned int arg1,
		     unsigned int arg2, unsigned int arg3)
{
	unsigned int result;

	__asm__ (
		"\tint $0x80\n"
	:"=a"(result)
	:"a"(syscall_id), "b"(arg1), "c"(arg2), "d"(arg3));

	return result;
}

unsigned int get_global_counter(void)
{
	return syscall(SYSCALL_TIMER_GET_GLOBAL_COUNTER, 0, 0, 0);
}

void exit(void)
{
	syscall(SYSCALL_EXIT, 0, 0, 0);
}
//}

libkernelの本体です。システムコールを発行する@<code>{syscall}関数を定義しています。また、@<code>{get_global_counter}と@<code>{exit}は、@<code>{syscall}をラップしています。

#@# @<code>{get_global_counter}と@<code>{exit}以外のシステムコールに@<code>{syscall}関数をラップした関数が無いのは、単にまだ実装していないだけです。

== libcommon
=== apps/libcommon/Makefile
//listnum[apps_libcommon_Makefile][apps/libcommon/Makefile][make]{
NAME=libcommon

$(LIB_DIR)/$(NAME).a: $(NAME).o
	ar rcs $@ $<

%.o: %.c
	gcc $(CFLAGS) -o $@ $<

clean:
	rm -rf *~ *.o *.a *.map

.PHONY: clean
//}

共通で使用される関数をまとめるライブラリ"libcommon"のMakefileです。

=== apps/include/common.h
//listnum[apps_include_common_h][apps/include/common.h][c]{
#ifndef _COMMON_H_
#define _COMMON_H_

int pow(int num, int multer);
void copy_mem(const void *src, void *dst, unsigned int size);

#endif /* _COMMON_H_ */
//}

libcommonのヘッダファイルです。べき乗計算を行う@<code>{pow}関数とデータのコピーを行う@<code>{copy_mem}関数があります。

=== apps/libcommon/libcommon.c
//listnum[apps_libcommon_libcommon_c][apps/libcommon/libcommon.c][c]{
#include <common.h>

int pow(int num, int multer)
{
	if (multer == 0) return 1;
	return pow(num, multer - 1) * num;
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

libcommonの本体です。

== libconsole
=== apps/libconsole/Makefile
//listnum[apps_libconsole_Makefile][apps/libconsole/Makefile][make]{
NAME=libconsole

$(LIB_DIR)/$(NAME).a: $(NAME).o
	ar rcs $@ $<

%.o: %.c
	gcc $(CFLAGS) -o $@ $<

clean:
	rm -rf *~ *.o *.a *.map

.PHONY: clean
//}

コンソールに関するライブラリlibconsoleのMakefileです。

=== apps/include/console.h
//listnum[apps_include_console_h][apps/include/console.h][c]{
#ifndef _CONSOLE_H_
#define _CONSOLE_H_

unsigned int get_cursor_pos_y(void);
void put_str(char *str);
void put_str_pos(char *str, unsigned char x, unsigned char y);
void dump_hex(unsigned int val, unsigned int num_digits);
void dump_hex_pos(unsigned int val, unsigned int num_digits,
		  unsigned char x, unsigned char y);
unsigned int get_line(char *buf, unsigned int buf_size);

#endif /* _CONSOLE_H_ */
//}

libconsoleのヘッダファイルです。以下の関数を提供しています。

 * 文字出力
 ** @<code>{put_str}: カーソル位置へ文字列出力
 ** @<code>{put_str_pos}: 指定座標へ文字列出力
 ** @<code>{dump_hex}: カーソル位置へ16進で数値出力
 ** @<code>{dump_hex_pos}: 指定座標へ16進で数値出力
 * 文字入力
 ** @<code>{get_line}: 1行分の入力文字列取得
 * カーソル制御
 ** @<code>{get_cursor_pos_y}: カーソル位置のY座標取得

=== apps/libconsole/libconsole.c
//listnum[apps_libconsole_libconsole_c][apps/libconsole/libconsole.c][c]{
#include <console.h>
#include <kernel.h>

unsigned int get_cursor_pos_y(void)
{
	return syscall(SYSCALL_CON_GET_CURSOR_POS_Y, 0, 0, 0);
}

void put_str(char *str)
{
	syscall(SYSCALL_CON_PUT_STR, (unsigned int)str, 0, 0);
}

void put_str_pos(char *str, unsigned char x, unsigned char y)
{
	syscall(SYSCALL_CON_PUT_STR_POS, (unsigned int)str,
		(unsigned int)x, (unsigned int)y);
}

void dump_hex(unsigned int val, unsigned int num_digits)
{
	syscall(SYSCALL_CON_DUMP_HEX, val, num_digits, 0);
}

void dump_hex_pos(unsigned int val, unsigned int num_digits,
		  unsigned char x, unsigned char y)
{
	syscall(SYSCALL_CON_DUMP_HEX_POS, val, num_digits,
		(unsigned int)((x << 16) | y));
}

unsigned int get_line(char *buf, unsigned int buf_size)
{
	return syscall(SYSCALL_CON_GET_LINE, (unsigned int)buf, buf_size, 0);
}
//}

libconsoleの本体です。いずれの関数もシステムコールをラップしたものです。

== libstring
=== apps/libstring/Makefile
//listnum[apps_libstring_Makefile][apps/libstring/Makefile][make]{
NAME=libstring

$(LIB_DIR)/$(NAME).a: $(NAME).o
	ar rcs $@ $<

%.o: %.c
	gcc $(CFLAGS) -o $@ $<

clean:
	rm -rf *~ *.o *.a *.map

.PHONY: clean
//}

文字列操作ライブラリlibstringのMakefileです。

=== apps/include/string.h
//listnum[apps_include_string_h][apps/include/string.h][c]{
#ifndef _STRING_H_
#define _STRING_H_

int str_get_len(const char *src);
int str_find_char(const char *src, char key);
void str_get_first_entry(const char *line, char *first, char *other);
int str_conv_ahex_int(const char *hex_str);
int str_compare(const char *src, const char *dst);

#endif /* _STRING_H_ */
//}

libstringのヘッダファイルです。以下の関数を提供しています。

 * @<code>{str_get_len}: 文字列の長さを取得
 * @<code>{str_find_char}: 文字列中の文字のインデックスを返す
 * @<code>{str_get_first_entry}: 半角スペース区切りの最初のエントリを取得
 * @<code>{str_conv_ahex_int}: 16進数文字列をintへ変換
 * @<code>{str_compare}: 文字列比較

=== apps/libstring/libstring.c
//listnum[apps_libstring_libstring_c][apps/libstring/libstring.c][c]{
#include <string.h>
#include <common.h>

int str_get_len(const char *src)
{
	int len;
	for (len = 0; src[len] != '\0'; len++);
	return len + 1;
}

int str_find_char(const char *src, char key)
{
	int i;

	for (i = 0; src[i] != key; i++) {
		if (src[i] == '\0') {
			i = -1;
			break;
		}
	}

	return i;
}

void str_get_first_entry(const char *line, char *first, char *other)
{
	int line_len, first_len, other_len;

	line_len = str_get_len(line);
	first_len = str_find_char(line, ' ');
	if (first_len < 0) {
		copy_mem((void *)line, (void *)first, line_len);
		first_len = line_len;
		other_len = 0;
		other[other_len] = '\0';
	} else {
		copy_mem((void *)line, (void *)first, first_len);
		first[first_len] = '\0';
		first_len++;
		other_len = line_len - first_len;
		copy_mem((void *)(line + first_len), (void *)other, other_len);
	}

#ifdef DEBUG
	shell_put_str(line);
	shell_put_str("|");
	shell_put_str(first);
	shell_put_str(":");
	shell_put_str(other);
	shell_put_str("\r\n");
#endif /* DEBUG */
}

int str_conv_ahex_int(const char *hex_str)
{
	int len = str_get_len(hex_str);
	int val = 0, i;

	for (i = 0; hex_str[i] != '\0'; i++) {
		if (('0' <= hex_str[i]) && (hex_str[i] <= '9')) {
			val += (hex_str[i] - '0') * pow(16, len - 2 - i);
		} else {
			val += (hex_str[i] - 'a' + 10) * pow(16, len - 2 - i);
		}
	}

	return val;
}

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
//}

libstringの本体です。特に変わったことはしていないです。

