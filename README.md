# 連番画像出力
JPEGまたはPNG形式の連番画像出力を行う，AviUtlの出力プラグインです．
本家にある連番BMP出力の変形で，libjpeg および libpng を使って不可逆/可逆圧縮画像を出力します．

## インストール
AviUtlのルートまたは plugins ディレクトリに `image_output_ks.auo` をコピーします．
使うだけならバイナリの `image_output_ks.auo` を使えばよく，コンパイルの必要はないと思います．

## ビルド
MSYS2上で src ディレクトリに入り，`make`します．
ビルドには libjpeg および libpng が必要です．MSYS2上の gcc 以外でのコンパイルは想定されていませんので，必要なら `Makefile` を参照の上，適当にビルドしてください．

## 設定項目
### 出力形式
JPEGかPNGかをラジオボタンで選択します．
JPEGの場合は，品質を0～100の数値で入力します．
拡張子は，この出力形式に関わらず，出力ファイル名で決定されます．

### ファイル名フォーマット/オフセット
連番をファイル名に変換する際の [`std::vformat`](https://cpprefjp.github.io/reference/format/vformat.html) のフォーマット文字列を入力します．

左側の入力欄は，ファイル名フォーマット文字列の入力欄です．
フォーマット文字列を `format`，フレーム番号を `frame_num`，出力ファイル名の拡張子より前の部分を`filename`として，
`std::vformat(format, std::make_format_args(frame_num, filename))` の形で連番ファイル名を生成します．
デフォルト値は `{1:s}_{0:04}`で，フォーマット文字列として不正な場合，このデフォルト値に変更します．
`{}` が1つだけなら `frame_num` のみが使われ，3つ以上はエラーとなります．

右側の入力欄は，連番のオフセットの入力欄です．(出力範囲の)フレーム番号(0オリジン)+オフセットが前述の `frame_num` となります．

実際の出力ファイル名は，このフォーマット文字列で変換した文字列に，拡張子をつけたものになります．
拡張子は出力ファイル名で入力したものがそのまま使われます．
例えば，出力ファイル名が「abc.jpg」，フォーマット文字列がデフォルトの場合，出力ファイル名は「abc_0000.jpg」のようになります．

## 制限
JPEG，PNGともに，RGB24bit形式での出力にしか対応していません．

## 参考・謝辞
本プラグインの作成にあたり，下記サイトを参考にしました．感謝します．

- [碧色工房 JPEG画像の入出力](https://www.mm2d.net/main/prog/c/image_io-16.html)
- [碧色工房 PNG画像の入出力](https://www.mm2d.net/main/prog/c/image_io-15.html)
- [万年ビギナーによるビギナーのためのプログラミング講座 第25章 年・月・曜日の書式設定メニューを作る](https://web.archive.org/web/20181107035421/http://www.geocities.jp/midorinopage/Beginner/beginner25.html) (Webarchive)

## Contributing
バグ報告等は https://github.com/cycloawaodorin/image_output_ks の Issue 等でお願いいたします．
