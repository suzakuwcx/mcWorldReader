# MCWorldReader

A high-performance python library to parse and read minecraft level data, support 1.21.1.

一个高效的用于解析 minecraft 地图数据的 python 库，支持 1.21.1.

Unlike tranditional CV, every block in Minecraft is a discrete token that without size relationship because of that Minecraft has a discrete 3-dimension structure. Compare to other problem with same type, it is very easy to collect data which dictates that it is a great material to reserch how to apply ar-base model on 3D-generation model. However, most of the relevant libraries are either inefficient to parse, or the supported version is too old, which is a hindrance to a certain extent. This repository focus on the efficent throughout the development improve the performance and enable multi-core CPU parsing at the same time. Now the parsing speed of this project is much more faster than commonly known libraries.

Minecraft 拥有一个离散化的三维空间结构，不像传统的 CV, 在 Minecraft 中每一个方块都是一个离散的 token, 没有大小关系，且相对其他同类型问题而言，数据集比较容易收集，是一个非常适合用于研究将自回归模型应用到三维生成类模型的素材，但是当前大多数相关类型的库，要么解析效率极低，要么支持的版本号太低，一定程度上造成了阻碍。本仓库从开发初期就时刻重视效率问题，来尽可能降低性能消耗，同时尽可能利用多核 CPU 解析，目前能做到解析速度远快于已知的常用库。

We hope to promote the development of AI in minecraft, may be a minecraft modality will be add in LLM in the future OvO

总之就是希望能够促进一下 AI 在 minecraft 中的发展，多模态加一个 Minecraft 模态也不是不可以 OvO

# Installation

Clone this repository and install the package

> Note: For Chinese Mainline User, if you stuck on build project, this is because that
> CMake will try to download dependency, see '3rdparty/CMakeLists.txt', please change
> download link or using http proxy

```shell
git clone https://github.com/suzakuwcx/mcWorldReader.git
pip install mcWorldReader/
```

# Usage

## Initilize

```python
import mcworldreader as mwr
world = mwr.World("<path/to/saves>") # 'saves' is the directory that contains 'region' directory
```

## get a block

```python
print(world.get_block(0, -64, 0))
```

## iterator all blocks
```python
for coord, block in world.iter_all_blocks():
    print("{} | {}".format(coord, block))
```

# Credits

- [libnbtplusplus](https://github.com/PrismLauncher/libnbtplusplus)
- [mcworldlib](https://github.com/MestreLion/mcworldlib)
