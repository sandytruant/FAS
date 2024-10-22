# Minimum Feedback Arc Set

该项目是 https://github.com/PKUcoldkeyboard/FAS 的精简版，可以更方便、更简洁地调用 PageRankFAS 解决 Minimum Feedback Arc Set 问题。

- 删除了原仓库的 test 和 benchmark
- 删除了原仓库的数据集
- 删除了原仓库展示成果的部分
- 删除了原仓库关于 spdlog 的部分
- 删除了原仓库的 GreedyFAS 和 SortFAS 算法

## Usage

1. 安装boost库

   Arch / Manjaro

   ```Bash
   sudo pacman -S boost
   ```

   Ubuntu

   ```Bash
   sudo apt-get install libboost-all-dev
   ```

2. 编译

   ```
   cmake -B build && cmake --build build
   ```

3. 运行

   ```
   ./build/FASSolver [path/to/graph]
   ```