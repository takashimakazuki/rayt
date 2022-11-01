## Ray tracing チュートリアル
こちらの記事の内容に沿って，レイトレーシングによるCG描画をC++で実装．

https://zenn.dev/mebiusbox/books/8d9c42883df9f6/viewer/b85221

- theta: the angle of field of view

![chapter1](./img/chapter1.png)


### レンダリング処理の並列化

1分35秒(openMP並列なし)→18秒(threads=8)に短縮

```bash
# without openmp
./rayt  95.32s user 0.28s system 99% cpu 1:35.84 total

# omp num_threads=8
./rayt  135.03s user 0.32s system 751% cpu 18.011 total
```