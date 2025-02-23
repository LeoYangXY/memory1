#include "../include/memory.h"
#include <vector>
#include <iostream>

namespace mP {
	// 在 C++ 中，类的静态成员变量（如 std::once_flag）必须在类外显式定义，否则链接器会找不到符号实现（h文件中定义了，在这里的cpp文件中也要定义一次）
	std::vector<std::unique_ptr<MemoryPool>> HashBucket::pools;
	std::once_flag HashBucket::initFlag;

	//当在类外部定义成员函数时，必须使用 ClassName:: 来指明该函数属于哪个类 而且注意.号用于对象调用成员函数，不能用于定义函数
	//在 C++ 中，构造函数（Constructor）和析构函数（Destructor）不需要声明返回类型.
	//而别的函数虽然在头文件中声明了返回类型，在cpp文件中正式定义的时候也要显性写出来
	MemoryPool::MemoryPool(size_t blockSize, size_t slotSize) :
		blockSize_(blockSize),
		slotSize_(slotSize),
		firstBlock_(nullptr),
		curSlot_(nullptr),
		freeList_(nullptr),
		lastSlot_(nullptr)
	{
	}
	//1.不在{}内部写blockSize_=blockSize，这个效率会低一些（如果逻辑复杂可以在里面写）
	//2.使用blockSize_(blockSize)这种方式的时候：不能用this指针，因此类的成员变量名和参数名就不能是一样的


	MemoryPool::~MemoryPool() {
		slot* cur = this->firstBlock_;
		while (cur) {
			slot* next = cur->next;
			operator delete(reinterpret_cast<void*>(cur));
			//cur是slot*类型，要转化成void*类型才能使用operator delete
			//delete的底层调用了operator delete，在我们的场景下只需使用最原始的operator delete
			cur = next;
		}
	}


	void* MemoryPool::allocate() {
		std::lock_guard<std::mutex> lock(mutexForBlock_);
		// 统一锁住整个分配过程,确保所有共享资源访问都被锁保护
		//注意这里的锁不是mutexForFreeList_，我们的mutexForBlock保护allocateNewBlock() 的线程安全：防止多个线程同时调用 allocateNewBlock() 导致重复申请内存块
		if (freeList_ != nullptr) {
			std::lock_guard<std::mutex> lock(mutexForFreeList_);
			//RAII风格，mutexForFreeList_是一个锁，lock_guard实例化出来的lock相当于这个锁的智能管家，可以让mutexForFreeList_自动锁住&解锁
			if (freeList_ != nullptr) {
				//这里做第二次检查的原因：若多个线程如thread A和threadB同时通过第一次检查，它们会依次进入临界区
				//然后假如thread A先获取了锁，那么就是相当于thread A先操作：再次检查freeList_是否为空->取走空闲槽，置freeList_为空->释放锁
				//那如果没有这个第二次检查，thread B因为也通过了第一次检查，那么在A释放锁之后也会进行这里的操作，那么就会取到无效的freeList_
				slot* temp = freeList_;
				freeList_ = freeList_->next;
				return temp;
			}
		}
		else {
			slot* temp;
			{
				if (curSlot_ >= lastSlot_) {
					allocateNewBlock();
				}
				temp = curSlot_;
				curSlot_ = curSlot_ + slotSize_ / sizeof(slot);
				//我们想让curSlot_往后移动slotSize_个，但是指针的算术运算并不是以字节为单位的，而是以指针所指向类型的大小为单位的。
				//例如，如果有一个int*指针，执行ptr += 1，实际地址会增加sizeof(int)字节
				//因此这里我们要除以sizeof（slot）
			}//在此处释放锁。设计意图：锁应该只保护对 curSlot_ 和 lastSlot_ 的修改，而非整个 else 块，这样避免持有锁过久，影响性能
			return temp;
		}
	}

	void MemoryPool::deallocate(void* ptr) {
		if (ptr) {
			std::lock_guard<std::mutex> lock(mutexForFreeList_);
			reinterpret_cast<slot*>(ptr)->next = freeList_;
			freeList_ = reinterpret_cast<slot*>(ptr);
		}
	}

	void MemoryPool::allocateNewBlock() {
		void* newBlock = operator new(blockSize_);
		// 将新块添加到链表头部（需加锁）
		std::lock_guard<std::mutex> lock(mutexForBlock_);
		reinterpret_cast<slot*>(newBlock)->next = firstBlock_;
		firstBlock_ = reinterpret_cast<slot*>(newBlock);

		//使用char*  以便进行字节级地址计算(body+一个数就是正常代表我们想要增加的字节数）
		char* body = reinterpret_cast<char*>(newBlock) + sizeof(slot*);

		//body 指向新内存块中跳过第一个 Slot* 指针后的位置。这个设计可能是为了在内存块头部预留一个指针空间，用于链表管理（例如链接到下一个内存块）
		//		| ------------------ - Block 内存布局------------------ - |
		//		| Next 指针(Slot*) | 对齐填充区 | Slot 0 | Slot 1 | ... |
		//		^                  ^            ^
		//		newBlock           body         curSlot_ ...
		size_t paddingSize = padPointer(body, slotSize_);
		curSlot_ = reinterpret_cast<slot*>(body + paddingSize);

		lastSlot_ = reinterpret_cast<slot*>(reinterpret_cast<size_t>(newBlock) + blockSize_ - slotSize_ + 1);
		//reinterpret_cast<size_t>(newBlock):将指针 newBlock 转换为整数（内存地址的数值表示），以便进行算术运算.若 newBlock 的地址是 0x1000，转换后得到整数 0x1000
		//+ BlockSize_:将地址移动到内存块的末尾。若 BlockSize_ = 4096，则 0x1000 + 4096 = 0x2000（即内存块最后一个字节的下一个地址）
		//- SlotSize_:回退一个内存槽的大小，定位到最后一个内存槽的起始地址。若 SlotSize_ = 64，则 0x2000 - 64 = 0x1FC0，即最后一个内存槽起始于 0x1FC0。
		//+1:将末尾位置设置为最后一个完整槽的起始位置的下一个。确保即使有 slotSize-1 字节的碎片空间，也触发新内存块(block)分配
		freeList_ = nullptr;
	}

	size_t MemoryPool::padPointer(char* p, size_t align) {
		//align是一个槽大小（slot头+实际存储数据块内存的大小之和）.我们要做到p是align的整数倍（比如做到p为10000，align为8这样的）
		size_t address = reinterpret_cast<size_t>(p);
		if (address % align == 0) {
			return 0;
		}
		else {
			return ((address / align) + 1) * align - address;
		}
	}

	void HashBucket::initMemoryPool() {
		std::call_once(initFlag, []() {//确保线程安全，不要多个进程同时初始化我们的pools
			mP::HashBucket::pools.reserve(MEMORY_POOL_NUM); // 预先分配 vector 的容量，避免动态扩容时触发移动操作
			for (int i = 0; i < MEMORY_POOL_NUM; i++) {
				// 直接构造 unique_ptr 管理 MemoryPool
				mP::HashBucket::pools.emplace_back(std::make_unique<MemoryPool>(4096, (i + 1) * SLOT_BASE_SIZE));
				//对vector不使用push_back，而是这样子去原地构造去提高效率
				//不写pools.emplace_back(MemoryPool(4096, (i+1)*SLOT_BASE_SIZE)) 这样会冗余构造临时对象，直接把内存池1实例需要的参数传给emplace_back
				//在 HashBucket::pools 的初始化中，确保所有元素通过 emplace_back 直接构造，而非拷贝或移动
			}
			});
	}

	MemoryPool& HashBucket::getMemoryPool(int index)//初始化了所有内存池的位置，把它们加到了一个vector里面，并且最后返回想要的内存池
	{

		//一开始想写static MemoryPool memoryPoolsList[MEMORY_POOL_NUM]，后面的代码换了
		//通过static声明静态数组memoryPoolsList其生命周期与程序相同,首次调用时初始化，后续调用直接返回已存在的实例
		//有一个麻烦的点是： 静态数组的初始化列表必须在编译时完全确定，包括数组大小和每个元素的初始值。
		//比如写static MemoryPool pools[3] = {MemoryPool(1 * SLOT_BASE_SIZE),MemoryPool(2 * SLOT_BASE_SIZE)} 
		//不能使用for循环往里面添加元素（for循环是运行时逻辑，而不是在编译期完成）
		//为什么要使用static呢：若你希望 memoryPoolsList 在多次调用中保持状态，移除 static 会导致每次调用都重新初始化数组，违背需求。

		return *(mP::HashBucket::pools[index]);
	}

	void* HashBucket::useMemory(size_t size) {
		//先使用hashbucket的方法找到一个内存池(一个内存池就是一个block)，而一个block由很多个槽组成。useMemory最后使用allocate的意义就是对于选定的block，分配一个槽
		if (size <= 0) {
			return nullptr;
		}
		else if (size > MAX_SLOT_SIZE) // 大于512字节的内存，则使用new
			return operator new(size);
		else {
			return getMemoryPool(((size + 7) / SLOT_BASE_SIZE) - 1).allocate();//// 相当于size / 8 向上取整（因为分配内存只能大不能小
		}
	}


	void HashBucket::freeMemory(void* ptr, size_t size) {
		//ptr指向要释放的内存池中的某个内存槽，必须由useMemory分配且非空  ；size：由用户正确地传给我们，用于定位是哪个内存池
		if (!ptr)
			return;
		if (size > MAX_SLOT_SIZE)//代表这个size对应的内存不是在我们的内存池之中
		{
			operator delete(ptr);
			return;
		}
		mP::HashBucket::getMemoryPool(((size + 7) / SLOT_BASE_SIZE) - 1).deallocate(ptr);
	}



}
