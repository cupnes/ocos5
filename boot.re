= ブートローダー
PCの電源を入れると、「BIOS」と呼ばれるマザーボードに元から書き込まれているソフトウェアが、起動ディスクの第1セクタ(MBRと呼ばれる)をRAMへロードし、CPUに実行させます。1セクタは512バイトです。ブートローダー・カーネル・ユーザーランドが512バイトに収まるはずもないので、この512バイトのプログラムで適宜RAMへロードする必要があります。なお、OS5のブートローダーは512バイトに収まっているので、OS5の場合は512バイトのプログラムだけでブートローダーは完結しています@<fn>{about_multi_boot}。
//footnote[about_multi_boot][GRUB等の高機能なブートローダーは512バイトに収まらないので、ブートローダーを512バイトの初段と、そこからロードされる2段目という形で多段ブートしていたりします。]

ブートローダーでは、カーネルとユーザーランドのRAMへのロードと、CPU設定を行っています。CPU設定は、割り込みや各種ディスクリプタテーブル@<fn>{descriptor_table}の設定などを行っています。CPU設定で特に重要なのは、リアルモード(16ビットのモード)からプロテクトモード(32ビットのモード)への移行です。CPUのモードをプロテクトモードへ移行させた後、カーネルの先頭アドレスへジャンプします。
//footnote[descriptor_table][x86 CPUには「セグメント」という単位でメモリを分割して管理するメモリ管理方法があり、そのための設定です。各セグメントの設定は「ディスクリプタ」というデータ構造で行います。「ディスクリプタ」の「テーブル」なので「ディスクリプタテーブル」です。]
== MBR部
=== boot/Makefile
//listnum[boot_Makefile][boot/Makefile][make]{
.s.o:
	as --32 -o $@ $<

boot.bin: boot.o
	ld -m elf_i386 -o $@ $< -T boot.ld -Map boot.map

boot.o: boot.s

clean:
	rm -f *~ *.o *.bin *.dat *.img *.map

.PHONY: clean
//}

ブートローダーのMakefileです。現状、ブートローダーはすべてアセンブラ書いています(512バイトと小さく、Cで処理を書くほどのことをしていない為)。ソースファイルはboot.sだけです。Makefileとしても、この単一のアセンブラファイルをas(GNUアセンブラ)でアセンブルし、ld(GNUリンカ)でリンク、を行っています。

コンパイルを行う環境はx86_64の環境なので、x86_32のバイナリを生成するために、asコマンドでは"--32"オプションを、ldコマンドでは"-m elf_i386"のオプションをつけています。なお、恥ずかしながら、当初はこれらのオプションを知りませんでした。そのため、開発環境をx86_32からx86_64へ変えたときは、x86_32の仮想環境を用意していました。x86_32のバイナリを生成するこれらのオプションは、パッチを作成された方がいて、マージさせていただいたものです。(これがマージさせてもらった初めてで、今のところ唯一のパッチです。)

あと、Makefileの書き方ですが、@<code>{.s.o:}という書き方は、@<code>{boot.o: boot.s}のように個々のターゲットを記述しなければならないので、あんまり良くないなぁと思います。@<code>{%.o: %.s}の書き方で汎用的なターゲット指定を書いておけば、@<code>{boot.o: boot.s}の記述を消せますね。なお、本書で説明はしていないのですが、ドキュメントディレクトリ@<fn>{doc_makefile}のMakefileでは'%'の書き方で汎用的なターゲット指定を行っています。(ブートローダー等の古いMakefileも直すべきで、自分のタスクリストには入っていたのですが、優先度:低で放置していました。言い訳ですが。。。)
//footnote[doc_makefile][OS5のソースディレクトリ直下のdocディレクトリ]

=== boot/boot.ld
//listnum[boot_boot_ld][boot/boot.ld][]{
OUTPUT_FORMAT("binary");

SECTIONS
{
	.text	: {*(.text)}
	.rodata	: {
		*(.strings)
		*(.rodata)
		*(.rodata.*)
	}
	.data	: {*(.data)}
	.bss	: {*(.bss)}

	. = 510;
	.sign	: {SHORT(0xaa55)}
}
//}

ブートローダーのリンカスクリプトです。

MBRの512バイトの先頭へジャンプしてくるので、textセクションが先頭に来るように並べています。

また、BIOSが「起動可能なディスクか否か」の判別として、「MBRの末尾2バイトが"0xaa55"であるか」をチェックしているので、リンカスクリプトで先頭から510バイト目に"0xaa55"を配置するようにしています。

=== boot/boot.s
//listnum[boot_boot_s][boot/boot.s][s]{
	.code16

	.text
	cli

	movw	$0x07c0, %ax
	movw	%ax, %ds
	movw	$0x0000, %ax
	movw	%ax, %ss
	movw	$0x1000, %sp

	/* ビデオモード設定(画面クリア) */
	movw	$0x0003, %ax
	int	$0x10

	movw	$msg_welcome, %si
	call	print_msg

	movw	$msg_now_loading, %si
	call	print_msg

	/* ディスクサービス セクタ読み込み
	 * int 0x13, AH=0x02
	 * 入力
	 * - AL: 読み込むセクタ数
	 * - CH: トラックの下位8ビット
	 * - CL(上位2ビット): トラックの上位2ビット
	 * - CL(下位6ビット): セクタを指定
	 * - DH: ヘッド番号を指定
	 * - DL: セクタを読み込むドライブ番号を指定
	 * - ES: 読み込み先のセグメント指定
	 * - BX: 読み込み先のオフセットアドレス指定
	 * 出力
	 * - EFLAGSのCFビット: 0=成功, 1=失敗
	 * - AH: エラーコード(0x00=成功)
	 * - AL: 読み込んだセクタ数
	 * 備考
	 * - トラック番号: 0始まり
	 * - ヘッド番号: 0始まり
	 * - セクタ番号: 1始まり
	 * - セクタ数/トラック: 2HDは18
	 * - セクタ18の次は、別トラック(裏面)へ
	 * - 64KB境界を超えて読みだすことはできない
	 *   (その際は、2回に分ける)
	 */

	/* トラック0, ヘッド0, セクタ2以降
	 * src: トラック0, ヘッド0のセクタ2以降
	 *      (17セクタ = 8704バイト = 0x2200バイト)
	 * dst: 0x0000 7e00 〜 0x0000 bfff
	 */
load_track0_head0:
	movw	$0x0000, %ax
	movw	%ax, %es
	movw	$0x7e00, %bx
	movw	$0x0000, %dx
	movw	$0x0002, %cx
	movw	$0x0211, %ax
	int	$0x13
	jc	load_track0_head0

	/* トラック0, ヘッド1, 全セクタ
	 * src: トラック0, ヘッド1の全セクタ
	 *      (18セクタ = 9216バイト = 0x2400バイト)
	 * dst: 0x0000 a000 〜 0x0000 c3ff
	 */
load_track0_head1:
	movw	$0x0000, %ax
	movw	%ax, %es
	movw	$0xa000, %bx
	movw	$0x0100, %dx
	movw	$0x0001, %cx
	movw	$0x0212, %ax
	int	$0x13
	jc	load_track0_head1

	/* トラック1, ヘッド0, 全セクタ
	 * src: トラック1, ヘッド0の全セクタ
	 *      (18セクタ = 9216バイト = 0x2400バイト)
	 * dst: 0x0000 c400 〜 0x0000 e7ff
	 */
load_track1_head0:
	movw	$0x0000, %ax
	movw	%ax, %es
	movw	$0xc400, %bx
	movw	$0x0000, %dx
	movw	$0x0101, %cx
	movw	$0x0212, %ax
	int	$0x13
	jc	load_track1_head0

	/* トラック1, ヘッド1, セクタ1 - 12
	 * src: トラック1, ヘッド1の12セクタ
	 *      (12セクタ = 6144バイト = 0x1800バイト)
	 * dst: 0x0000 e800 〜 0x0000 ffff
	 */
load_track1_head1_1:
	movw	$0x0000, %ax
	movw	%ax, %es
	movw	$0xe800, %bx
	movw	$0x0100, %dx
	movw	$0x0101, %cx
	movw	$0x020c, %ax
	int	$0x13
	jc	load_track1_head1_1

	/* トラック1, ヘッド1, セクタ13 - 18
	 * src: トラック1, ヘッド1の6セクタ
	 *      (6セクタ = 3072バイト = 0xc00バイト)
	 * dst: 0x0001 0000 〜 0x0001 0bff
	 */
load_track1_head1_2:
	movw	$0x1000, %ax
	movw	%ax, %es
	movw	$0x0000, %bx
	movw	$0x0100, %dx
	movw	$0x010d, %cx
	movw	$0x0206, %ax
	int	$0x13
	jc	load_track1_head1_2

	/* トラック2, ヘッド0, 全セクタ
	 * src: トラック2, ヘッド0の全セクタ
	 *      (18セクタ = 9216バイト = 0x2400バイト)
	 * dst: 0x0001 0c00 〜 0x0001 2fff
	 */
load_track2_head0:
	movw	$0x1000, %ax
	movw	%ax, %es
	movw	$0x0c00, %bx
	movw	$0x0000, %dx
	movw	$0x0201, %cx
	movw	$0x0212, %ax
	int	$0x13
	jc	load_track2_head0

	/* トラック2, ヘッド1, 全セクタ
	 * src: トラック2, ヘッド1の全セクタ
	 *      (18セクタ = 9216バイト = 0x2400バイト)
	 * dst: 0x0001 3000 〜 0x0001 53ff
	 */
load_track2_head1:
	movw	$0x1000, %ax
	movw	%ax, %es
	movw	$0x3000, %bx
	movw	$0x0100, %dx
	movw	$0x0201, %cx
	movw	$0x0212, %ax
	int	$0x13
	jc	load_track2_head1

	/* トラック3, ヘッド0, 全セクタ
	 * src: トラック3, ヘッド0の全セクタ
	 *      (18セクタ = 9216バイト = 0x2400バイト)
	 * dst: 0x0001 5400 〜 0x0001 77ff
	 */
load_track3_head0:
	movw	$0x1000, %ax
	movw	%ax, %es
	movw	$0x5400, %bx
	movw	$0x0000, %dx
	movw	$0x0301, %cx
	movw	$0x0212, %ax
	int	$0x13
	jc	load_track3_head0

	/* トラック3, ヘッド1, 全セクタ
	 * src: トラック3, ヘッド1の全セクタ
	 *      (18セクタ = 9216バイト = 0x2400バイト)
	 * dst: 0x0001 7800 〜 0x0001 9bff
	*/
load_track3_head1:
	movw	$0x1000, %ax
	movw	%ax, %es
	movw	$0x7800, %bx
	movw	$0x0100, %dx
	movw	$0x0301, %cx
	movw	$0x0212, %ax
	int	$0x13
	jc	load_track3_head1

	/* トラック4, ヘッド0, 全セクタ
	 * src: トラック4, ヘッド0の全セクタ
	 *      (18セクタ = 9216バイト = 0x2400バイト)
	 * dst: 0x0001 9c00 〜 0x0001 bfff
	*/
load_track4_head0:
	movw	$0x1000, %ax
	movw	%ax, %es
	movw	$0x9c00, %bx
	movw	$0x0000, %dx
	movw	$0x0401, %cx
	movw	$0x0212, %ax
	int	$0x13
	jc	load_track4_head0

	/* トラック4, ヘッド1, 全セクタ
	 * src: トラック4, ヘッド1の全セクタ
	 *      (18セクタ = 9216バイト = 0x2400バイト)
	 * dst: 0x0001 c000 〜 0x0001 e3ff
	*/
load_track4_head1:
	movw	$0x1000, %ax
	movw	%ax, %es
	movw	$0xc000, %bx
	movw	$0x0100, %dx
	movw	$0x0401, %cx
	movw	$0x0212, %ax
	int	$0x13
	jc	load_track4_head1

	/* トラック5, ヘッド0, セクタ1 - 14
	 * src: トラック5, ヘッド0のセクタ1〜14
	 *      (14セクタ = 7168バイト = 0x1c00バイト)
	 * dst: 0x0001 e400 〜 0x0001 ffff
	*/
load_track5_head0_1:
	movw	$0x1000, %ax
	movw	%ax, %es
	movw	$0xe400, %bx
	movw	$0x0000, %dx
	movw	$0x0501, %cx
	movw	$0x020e, %ax
	int	$0x13
	jc	load_track5_head0_1

	movw	$msg_completed, %si
	call	print_msg

	/* マスタPICの初期化 */
	movb	$0x10, %al
	outb	%al, $0x20	/* ICW1 */
	movb	$0x00, %al
	outb	%al, $0x21	/* ICW2 */
	movb	$0x04, %al
	outb	%al, $0x21	/* ICW3 */
	movb	$0x01, %al
	outb	%al, $0x21	/* ICW4 */
	movb	$0xff, %al
	outb	%al, $0x21	/* OCW1 */

	/* スレーブPICの初期化 */
	movb	$0x10, %al
	outb	%al, $0xa0	/* ICW1 */
	movb	$0x00, %al
	outb	%al, $0xa1	/* ICW2 */
	movb	$0x02, %al
	outb	%al, $0xa1	/* ICW3 */
	movb	$0x01, %al
	outb	%al, $0xa1	/* ICW4 */
	movb	$0xff, %al
	outb	%al, $0xa1	/* OCW1 */

	call	waitkbdout
	movb	$0xd1, %al
	outb	%al, $0x64
	call	waitkbdout
	movb	$0xdf, %al
	outb	%al, $0x60
	call	waitkbdout

	/* GDTを0x0009 0000から配置 */
	movw	$0x07c0, %ax	/* src */
	movw	%ax, %ds
	movw	$gdt, %si
	movw	$0x9000, %ax	/* dst */
	movw	%ax, %es
	subw	%di, %di
	movw	$12, %cx	/* words */
	rep	movsw

	movw	$0x07c0, %ax
	movw	%ax, %ds
	lgdtw	gdt_descr

	movw	$0x0001, %ax
	lmsw	%ax

	movw	$2*8, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movw	%ax, %ss

	ljmp	$8, $0x7e00

print_msg:
	lodsb
	andb	%al, %al
	jz	print_msg_ret
	movb	$0xe, %ah
	movw	$7, %bx
	int	$0x10
	jmp	print_msg
print_msg_ret:
	ret
waitkbdout:
	inb	$0x60, %al
	inb	$0x64, %al
	andb	$0x02, %al
	jnz	waitkbdout
	ret

	.data
gdt_descr:
	.word	3*8-1
	.word	0x0000, 0x09
	/* .word gdt,0x07c0
	 * と設定しても、
	 * GDTRには、ベースアドレスが
	 * 0x00c0 [gdtの場所]
	 * と読み込まれてしまう
	 */
gdt:
	.quad	0x0000000000000000	/* NULL descriptor */
	.quad	0x00cf9a000000ffff	/* 4GB(r-x:Code) */
	.quad	0x00cf92000000ffff	/* 4GB(rw-:Data) */

msg_welcome:
	.ascii	"Welcome to OS5!\r\n"
	.byte	0
msg_now_loading:
	.ascii	"Now Loading ... "
	.byte	0
msg_completed:
	.ascii	"Completed!\r\n"
	.byte	0
//}

ブートローダー本体のソースコードです。アセンブラは、時間がたつと真っ先に読めなくなる箇所なので、少し詳しく説明します。

このソースコードで行っている処理の流れは以下の通りです。

 1. CPU設定(1～21行目)
 2. FDからRAMへロード(22～228行目)
 3. PIC(Programmable Interrupt Controller)初期化(230～252行目)
 4. CPU設定(254～284行目)
 5. カーネルへジャンプ(286行目)

「1. CPU設定」について、まず、".code16"が16bit命令のアセンブラであることを示しています。"cli"ではすべての割り込みを無効化し、ブートローダーの処理中の、まだハードウェアの設定を行っていない段階で割り込みを受け付けないようにしています。6～10行目の"movw"命令の辺りではセグメントの設定とスタックポインタの設定をしています。OS5ではブートローダーの段階ではスタック領域のベースを0x0000 1000に設定しています。そして、12～14行目ではBIOSの機能を使ってビデオモードの設定と画面クリアを行っています。BIOSの機能は「1. 汎用レジスタにパラメータをセット」、「2. ソフトウェア割り込み」の流れです。ここではテキストモードを示す"0x03"をAXレジスタの下位8ビット(ALレジスタ)にセットし、画面モードに関する機能を呼び出す0x10のソフトウェア割り込みを実行しています。

「2. FDからRAMへロード」について、ブートローダーの行数の大半がこの処理です。24～227行目までの203行あり、全328行の内の6割程あります。見ればわかる通りですが、47～60行目のようなコード片を繰り返し並べています。64KB境界をまたぐ場合を除き、1つのコードブロックで1つのトラックをロードします。1トラックずつなのはBIOSの機能でまとまってロードできるのが1トラックずつだったためです。ループ等を使わずにコード片を何度も書いているのは、アセンブラの領域はあまり凝ったことをすると保守できなくなる(2～3か月後とかに見たとき、処理の流れを追えなくなる)気がしたからです。FDからのロード処理は、ロードサイズを増やす等で後々に処理を修正する可能性があることが分かっていたので、自分にとって平易な書き方をしています。そのため、ここで使用しているディスクサービスのBIOS命令については、説明をコメントで書いています。

「PIC(Programmable Interrupt Controller)初期化」では、マスタとスレーブのPICの設定で、すべての割り込みを無効化しています。PICのIOレジスタの使い方はIntel 8259のデータシートを確認してください。(あるいは、自作OS系のサイトや書籍などでも紹介しています。)

「4. CPU設定」では、カーネルへジャンプする直前のCPU設定を行っています。254～260行目はキーボードコントローラ(KBC)の設定です。waitkbdoutでは、キーボードコントローラのステータスがビジー抜けるまで待っています。262～270行目では、0x7c00 XXXXにあるGDTを0x0009 0000へコピーしています。なお、このGDTは、カーネルへロングジャンプするためにしか使いません(カーネル側では別途GDTを設定します)。わざわざ0x0009 0000へコピーしている理由は、lgdtw命令でGDTをロードする際に指定するGDTのセグメントセレクタに0x07c0を指定できなかったためです(何か間違えているのか、どうなのか、不明)。その後、lgdtw命令でGDTRへgst_descrの内容をロードします(272～274行目)。

ここまででプロテクトモード(32ビットのモード)へ移行するための準備が完了です。276～277行目ではMSRの最下位ビットに1をセットし、プロテクトモードへ移行させています。その後、279～284行目でセグメントレジスタへセグメントセレクタを設定しています(ここでは、プロテクトモードでのセグメントセレクタを設定しています)。そして、カーネルの先頭アドレスへジャンプします(286行目)。OS5では、カーネルは0x0000 7e00から配置するように決めています。物理アドレス空間のメモリマップは、@<img>{aboutos5|memmap_phys}を参照してください。

