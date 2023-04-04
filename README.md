# RSRX

RS232C Binary File Receiver for X680x0/Human68k and Python

---

### About This

これは RS232C クロス接続した相手の X680x0/PC/Mac/Linux からファイルを受信するためのプログラムです。
X680x0/Human68k版とPython汎用版がそれぞれ用意されています。

相手方には [RSTX](https://github.com/tantanGH/rstx/) を導入してセットで使います。

MOメディアなどでのやりとりと異なり、ファイル名を大文字8+3に変更する必要がありません。長いファイル名も小文字もそのまま転送できます。
タイムスタンプも維持されます。ただし最大18+3文字まで、日本語は使えません。

---

### Install (X680x0/Human68k版)

RSRXxxx.ZIP をダウンロードして展開し、RSRX.X をパスの通ったディレクトリに置きます。

高速RS232Cドライバとして TMSIO.X を推奨します。X68030+060turboであれば38400bpsでの安定した転送が可能です。

X68000Zでも19200bpsでの通信が可能です。

http://retropc.net/x68000/software/system/rs232c/tmsio/

X68000 LIBRARY からダウンロードして導入しておきます。

Human68k 3.02付属のRSDRV.SYSを組み込んだ場合は19200bpsまでの通信が可能です。

なお、本ソフトウェアは TMSIO.X を改造した BSIO.X には対応してません。(ハードウェアフロー制御固定となっているため)

---

### Install (Python版)

    pip install git+https://github.com/tantanGH/rsrx.git

[Windowsユーザ向けPython導入ガイド](https://github.com/tantanGH/distribution/blob/main/windows_python_for_x68k.md)

---

### 使い方 (X680x0/Human68k版)

RSDRV.SYS を CONFIG.SYS で組み込んでいない場合は、TMSIO.X を常駐させておく必要があります。

    TMSIO.X -b128

などとして十分な通信バッファを確保しておいてください。

    RSRX.X - RS232C File Receiver 0.x.x
    usage: rsrx [options]
    options:
         -d <download directory>   (default:current directory)
         -s <baud rate>  (default:9600)
         -t <timeout[sec]> (default:120)
         -f ... overwrite existing file (default:no)
         -h ... show version and help message

何もオプションをつけずに実行すると、19200bps で受信待機の状態になります。

ボーレートは `-s` オプションで変更することができます。
RSDRV.SYS の場合 19200bps が最大となります。
TMSIO.X の場合、19200bps を超える通信速度に対応していますが、実際に通信が確立できるかは環境によります。
安定的に利用するのであれば、9600~38400bpsが無難です。(X68030の場合)

`-d` でダウンロード先のディレクトリを指定できます。何も指定しないとカレントディレクトリになります。

`-t` でタイムアウト秒を設定できます。動作中にESCキーを押すことでいつでもキャンセル可能です。
中途半端な状態で終了してしまった場合や、通信がうまく確立できない場合は、一度 TMSIO を `tmsio -r` で常駐解除し、
もう一度常駐させてバッファをクリアしてあげると良い場合があります。

`-f` をつけると既存のファイルを上書きします。デフォルトではオフになっています。オフの場合に同じファイルを見つけると、ファイル名の最後の文字を0,1,2,.. の順に9まで変えてみて重複が無いようならその名前で保存します。ファイル名そのものは送信側から与えられます。ファイルの更新時刻も送信側から与えられ、転送が正常に完了した時点でその時刻に書き換えられます。

実際の実行の際は、先に RSRX を実行して待機状態にしてからタイムアウトになる前に RSTX を起動して送信を開始します。

---

### 使い方 (Python版)

    usage: rsrx [-h] [--device DEVICE] [-d DOWNLOAD_PATH] [-s BAUDRATE] [-t TIMEOUT] [-f]

`--device` (Python版のみ) RS232Cポートのデバイス名を指定します。Windowsであれば `COM1` みたいな奴です。Macで USB-RS232C ケーブルを使っている場合は `/dev/tty.usbserial-xxxx` みたいになります。デフォルトで入っているのは我が家のMacのデバイス名ですw

`-d` でダウンロード先のディレクトリを指定します。指定が無い場合はカレントディレクトリになります。

`-s` で通信速度を指定します。相手方の RSRX での設定値に合わせてあげる必要があります。デフォルトは19200です。

`-t` でタイムアウト(秒)を設定します。

`-f` をつけると既存のファイルを上書きします。デフォルトではオフになっています。オフの場合に同じファイルを見つけると、ファイル名の最後に数字のsuffixを付加します。重複が無いようならその名前で保存します。ファイル名そのものは送信側から与えられます。ファイルの更新時刻も送信側から与えられ、転送が正常に完了した時点でその時刻に書き換えられます。

実際の実行の際は、先に RSRX を実行して待機状態にしてからタイムアウトになる前に RSTX を起動して送信を開始します。

