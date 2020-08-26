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