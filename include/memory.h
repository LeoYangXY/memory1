#pragma once
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
namespace mP {//这样子把下面的东西都放在一个命名空间里面


#define SLOT_BASE_SIZE 8//定义最小分配单位，所有内存池的槽大小是 8 的倍数。
#define MEMORY_POOL_NUM 64//定义内存池分级数量，支持从 8B 到 512B（8×64）的分配
#define MAX_SLOT_SIZE 512//定义内存池上限，超过 512B 的对象的内存分配请求我们使用系统分配

	struct slot {//一个内存槽有一个slot头，这个用于不同内存槽的互相连接（使用slot头进行互相连接），然后真正存储数据的部分不是slot头
		slot* next;
	};

	class MemoryPool {
	public:
		// 禁用拷贝构造函数和拷贝赋值
		MemoryPool(const MemoryPool&) = delete;
		MemoryPool& operator=(const MemoryPool&) = delete;

		// 禁用移动构造函数和移动赋值
		MemoryPool(MemoryPool&&) = delete;
		MemoryPool& operator=(MemoryPool&&) = delete;
	public:
		MemoryPool(size_t blockSize, size_t slotSize);//构造函数用于分配整个内存块的大小（默认4096）+确定每个到底存放多大的对象
		~MemoryPool();
		void* allocate();//返回一个指向空闲区域的指针.  返回值因为是void*，那么就是类型不定
		void deallocate(void*);//把指着的空间加入空闲链表。 void*代表传入一个指针，但类型可以任意.而这里是声明，可以不写具体的形参名，只写一个形参类型
	private:
		void allocateNewBlock();//内存池的核心目的不是完全避免系统调用，而是通过提前预留空间减少系统调用。allocateNewBlock是给内存池扩容的操作
		size_t padPointer(char* p, size_t align);//从p这个地址开始，对其要求是对齐到槽大小的倍数位置(align是槽大小），返回做到对齐所需要的字节数
	private:
		//成员变量名加 _ 后缀（避免到时候与函数参数出现重复带来的麻烦），而函数的形参名保持自然
		//block下面有很多小槽，每个小槽包含一个slot头（去指向下一个小槽）以及实际存储数据的内存区域
		//block的头部也有一个slot头，用于连接别的block
		int blockSize_;
		int slotSize_;//一个slot加上其真正能存储数据部分的空间大小
		slot* firstBlock_;//一个指针，指向包含了很多个小槽的大的内存块
		slot* curSlot_;//指向当前的未被使用的槽
		slot* freeList_;//指向空闲链表的头节点的指针
		slot* lastSlot_;
		std::mutex mutexForFreeList_;//一个锁，用于保护空闲链表（freeList）的线程安全：在无锁设计实现前，用于保证 pushFreeList/popFreeList 的原子性
		std::mutex mutexForBlock_;//保护 allocateNewBlock() 的线程安全：防止多个线程同时调用 allocateNewBlock() 导致重复申请内存块
	};

	class HashBucket {

	public:
		static void initMemoryPool();
		//静态方法属于类本身而非实例，用于初始化所有 HashBucket 对象共享的资源.所有 HashBucket实例共享同一组内存池（如 64 个不同槽大小的 MemoryPool），避免重复初始化 
		static MemoryPool& getMemoryPool(int index);
		//内存池全局唯一性：HashBucket 类管理的是 预先定义的固定规格内存池数组（如 64 个不同槽大小的 MemoryPool），这些内存池需被所有代码共享，而非每个对象实例独立拥有。
		//因此我们设计为static方法去找到内存池（useMemory才进行实际分配）

		static void* useMemory(size_t size);//真正地去分配内存
		static void freeMemory(void* ptr, size_t size);//释放时需要根据 size 参数确定该内存块属于哪个子池



		//newElement/deleteElement 是 内存管理工具函数，而非 HashBucket 的核心功能。
		//类似 operator new/delete 的全局设计，允许用户直接调用 newElement<MyClass>()，无需通过 HashBucket 对象
		//因此我们把它们定义在类的外面，但是要声明为友元，才能访问类里面的函数
		template<typename T, typename... Args>
		friend T* newElement(Args&&... args);
		template<typename T>
		friend void deleteElement(T* p);
	public:
		static std::vector<std::unique_ptr<MemoryPool>> pools;// 静态成员变量,确保全局只初始化一次。pools是一个vecore，里面保存着多个指针，每个指针指向一个内存池
		static std::once_flag initFlag;        // 静态成员变量

	};

	//模板函数必须将定义放在头文件中（不能分离到.cpp）
	template<typename T, typename ...Args>//typename T：要创建的对象类型  typename... Args：可变参数模板，表示类型T的构造函数参数的类型列表
	T* newElement(Args&&... args) {//&& 表示通用引用，支持左值和右值参数   ... 表示参数包展开，用于传递任意数量和类型的参数。
		T* p = nullptr;
		if ((p = reinterpret_cast<T*>(HashBucket::useMemory(sizeof(T)))) != nullptr) {
			new(p) T(std::forward<Args>(args)...);
			// 常规 new（如 A* a = new A();）  系统自动分配地址（并且自动绑定了能销毁）；而placement new如 A* a = new (ptr) A();仅构造对象：在 预先分配的内存地址（ptr）上调用构造函数，不负责内存分配
			// new(p) T(...)是Placement New 语法：在已经分配的内存地址p上面构造对象。
			// 用placement new后，需手动调用析构函数（而非delete），且需确保内存的正确释放。理由：placement new只是在现有的内存位置上构造对象，它不负责内存的分配
			// std::forward<Args>(args)...是完美转发，保留参数的左值 / 右值属性，确保调用正确的构造函数重载
		}
		return p;
	}

	template<typename T>
	void deleteElement(T* p) {
		if (p) {
			p->~T();//对象析构
			HashBucket::freeMemory(reinterpret_cast<void*>(p), sizeof(T));//释放空间
		}
	}

}