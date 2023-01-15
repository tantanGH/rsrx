# RSRX.X

RS232C Binary File Receiver for X680x0/Human68k

---

## About This

これは RS232C クロス接続した相手の PC/Mac からファイルを受信するためのプログラムです。
相手方には [RSTX](https://github.com/tantanGH/rstx/) を導入してセットで使います。

注意: 自分の環境で使うことしか考えていないので、リトライ・ACKなど実装の面倒はものは大幅に省いています。

---

## Install

RSRXxxx.ZIP をダウンロードして展開し、RSRX.X をパスの通ったディレクトリに置きます。

また、高速RS232Cドライバとして TMSIO.X が必須です。

http://retropc.net/x68000/software/system/rs232c/tmsio/

X68000 LIBRARY からダウンロードして導入しておきます。

---

## Usage

実行する前に、TMSIO.X を常駐させておく必要があります。

    TMSIO.X -b512

などとして十分な通信バッファを確保しておいてください。

    RSRX.X - RS232C File Receiver 0.x.x
    usage: rsrx [options]
    options:
         -d <download directory>   (default:current directory)
         -s <baud rate>  (default:9600)
         -t <timeout[sec]> (default:120)
         -f ... overwrite existing file (default:no)
         -r ... no CRC check
         -h ... show version and help message

何もオプションをつけずに実行すると、9600bps で受信待機の状態になります。

ボーレートは `-s` オプションで変更することができます。
TMSIO のサポートする 9600bps 以上の通信速度に対応していますが、実際に通信が確立できるかは環境によります。
安定的に利用するのであれば、9600~38400bpsが無難です。(X68030の場合)

`-d` でダウンロード先のディレクトリを指定できます。何も指定しないとカレントディレクトリになります。

`-t` でタイムアウト秒を設定できます。動作中にESCキーを押すことでいつでもキャンセル可能です。
中途半端な状態で終了してしまった場合や、通信がうまく確立できない場合は、一度 TMSIO を `tmsio -r` で常駐解除し、
もう一度常駐させてバッファをクリアしてあげると良い場合があります。

`-f` をつけると既存のファイルを上書きします。デフォルトではオフになっています。オフの場合に同じファイルを見つけると、ファイル名の最後の文字を0,1,2,.. の順に9まで変えてみて重複が無いようならその名前で保存します。ファイル名そのものは送信側から与えられます。ファイルの更新時刻も送信側から与えられ、転送が正常に完了した時点でその時刻に書き換えられます。

`-r` をつけるとCRCチェックを行いません。データは8KBごとのチャンクで送られてきますが、その都度CRCチェックを行っています。そのチェックを行わなくなります。その場合であっても最終的なファイルサイズの確認は行います。

実際の実行の際は、先に RSRX.X を実行して待機状態にしてからタイムアウトになる前に RSTX を起動して送信を開始します。

---

### 動作確認環境

受信側
* X68030 (MC68030 25.0MHz) + Human68k 3.02 + TMSIO.X 0.31

送信側
* MacBook Air (2020 M1 model)

接続ケーブル
* SANWA SUPPLY KRS-423XF-07K (25pin - 9pin RS232C クロスケーブル)
* CableCreation USB to RS232 アダプタ【FTDIチップセット内蔵】金メッキUSB 2.0（オス）- RS232（オス)
* ノーブランド USB Type-A to Type-C ケーブル
