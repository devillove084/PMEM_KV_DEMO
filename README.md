### 赛题

本题设计一个基于傲腾持久化内存(Aep)的KeyValue单机引擎，支持Set和Get的数据接口，同时对于热点访问具有良好的性能。  

### 逻辑



* 评测  
引擎使用的内存和持久化内存限制在 4G Dram和 74G Aep。  
每个线程分别写入约48M个Key大小为16Bytes，Value大小为80Bytes的KV对象，接着以95：5的读写比例访问调用48M次。其中95%的读访问具有热点的特征，大部分的读访问集中在少量的Key上面。  
* 评测  
引擎使用的内存和持久化内存限制在16G Dram和 128G Aep，复赛要求数据具有持久化和可恢复(Crash-Recover)的能力，确保在接口调用后，即使掉电的时候能保证数据的完整性和正确恢复。  
运行流程和初赛一致，差别是Value从80bytes定长变为80Bytes~1024Bytes之间的随机值，同时在程序正确性验证加入Crash-Recover的测试。

### 资料
[Intel 傲腾持久化内存介绍](https://software.intel.com/content/www/us/en/develop/videos/overview-of-the-new-intel-optane-dc-memory.html)

[持久化内存编程系列](https://software.intel.com/content/www/us/en/develop/videos/the-nvm-programming-model-persistent-memory-programming-series.html)

[如何模拟持久化内存设备(必看)](https://software.intel.com/en-us/articles/how-to-emulate-persistent-memory-on-an-intel-architecture-server)

[基于持久化内存的HelloWorld例子(libpmem)](https://software.intel.com/content/www/us/en/develop/articles/code-sample-create-a-c-persistent-memory-hello-world-program-using-libpmem.html)

[基于持久化内存的HelloWorld例子(mmap)](docs/appdirect-tips)

##### 赛题中的数据计算单位是?
> 1024
> 74GB = 74 * 1024 * 1024 * 1024 Byte
> 48M = 48 * 1024 * 1024

##### 纯写入有重复的Key吗？

> 存在低于0.2% 的重复key。

##### 读写混合是重复的Key吗？

> 全部是更新操作，均是重复的key。

##### 如何输出Log

> 引擎接口里有log_file 文件指针，允许输出log的测试中该指针不为空，可直接写入到此文件中，大小限制在5MB以内。
将每5MB清空一次log文件。

##### Log文件显示Correctness Failed 或 Performance Judge Failed是什么意思。

> 正确性或性能评测时失败。具体失败原因有很多，需要自行排查。


##### 内存的上限是多少？

> 评测程序为选手预留了320MB 左右的用户空间，不计算在4GB DRAM内。

##### AEP的上限是多少？

> 74GB

##### libpmem需要自己包含在工程吗，还是测试环境已经有
> 已有

##### Linux 与编译
> - Linux 4.19.91-19.1.al7.x86_64 x86_64
> - gcc version 4.8.5

##### 评测时间

> 性能评测时限15分钟，一次写入测试，10次读写测试，取读写测试最慢速度作为读写成绩。

##### 初赛一定要依赖libpmem或PMDK吗？
> 没有必要，可以不依赖库，直接作为内存使用即可。
