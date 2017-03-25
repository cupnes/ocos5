= ツール類
ビルドや、リリース時の記事作成で使用するツール類を置いています。

== ファイルシステム作成

=== tools/make_os5_fs.sh
//listnum[tools_make_os5_fs_sh][tools/make_os5_fs.sh][sh]{
#!/bin/bash

block_size=4096

# make control block
echo $# | awk '{printf "%c", $1}'
dd if=/dev/zero count=$((block_size - 1)) bs=1

# make data block(s)
while [ -n "$1" ]; do
	# make header
	name=$(basename $1)
	echo -n $name
	dd if=/dev/zero count=$((32 - ${#name})) bs=1
	header_size=48
	data_size=$(stat -c '%s' $1)
	block_num=$((((header_size + data_size) / block_size) + 1))
	echo $block_num | awk '{printf "%c", $1}'
	dd if=/dev/zero count=15 bs=1

	# make data
	dd if=$1
	data_size_1st_block=$((block_size - header_size))
	if [ $data_size -le $data_size_1st_block ]; then
		dd if=/dev/zero count=$((data_size_1st_block - data_size)) bs=1
	else
		cnt=$((block_size - ((header_size + data_size) % block_size)))
		dd if=/dev/zero count=$cnt bs=1
	fi

	shift
done
//}

ユーザーランドのファイルシステムを生成するシェルスクリプトです。ファイルシステム(kernel/fs.c)で説明した通りにファイルシステムを生成します。

== ブログ記事作成支援
=== tools/cap.sh
//listnum[tools_cap_sh][tools/cap.sh][sh]{
#!/bin/sh

work_dir=cap_$(date '+%Y%m%d%H%M%S')

mkdir ${work_dir}

qemu-system-i386 -fda fd.img &

sleep 1
echo "convert -loop 0 -delay 15 ${work_dir}/cap_*.gif anime.gif"

window_id=$(xwininfo -name QEMU | grep 'Window id' | cut -d' ' -f4)

i=0
while :; do
    import -window ${window_id} "${work_dir}/$(printf 'cap_%04d.gif' $i)"
    i=$((i + 1))
    sleep 0.1
done
//}

ブログ記事に掲載しているGIFアニメを生成するシェルスクリプトです。

cap.shを実行するとQEMUが立ち上がり、cap_年月日時分秒という名前のディレクトリへ0.1秒毎にスクリーンショットを保存していきます。スクリプトの実行終了後、cap.sh実行時に表示されるconvertコマンドを実行することでGIFアニメ"anime.gif"を生成します。

