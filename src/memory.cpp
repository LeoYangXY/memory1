#include "../include/memory.h"
#include <vector>
#include <iostream>

namespace mP {
	// �� C++ �У���ľ�̬��Ա�������� std::once_flag��������������ʽ���壬�������������Ҳ�������ʵ�֣�h�ļ��ж����ˣ��������cpp�ļ���ҲҪ����һ�Σ�
	std::vector<std::unique_ptr<MemoryPool>> HashBucket::pools;
	std::once_flag HashBucket::initFlag;

	//�������ⲿ�����Ա����ʱ������ʹ�� ClassName:: ��ָ���ú��������ĸ��� ����ע��.�����ڶ�����ó�Ա�������������ڶ��庯��
	//�� C++ �У����캯����Constructor��������������Destructor������Ҫ������������.
	//����ĺ�����Ȼ��ͷ�ļ��������˷������ͣ���cpp�ļ�����ʽ�����ʱ��ҲҪ����д����
	MemoryPool::MemoryPool(size_t blockSize, size_t slotSize) :
		blockSize_(blockSize),
		slotSize_(slotSize),
		firstBlock_(nullptr),
		curSlot_(nullptr),
		freeList_(nullptr),
		lastSlot_(nullptr)
	{
	}
	//1.����{}�ڲ�дblockSize_=blockSize�����Ч�ʻ��һЩ������߼����ӿ���������д��
	//2.ʹ��blockSize_(blockSize)���ַ�ʽ��ʱ�򣺲�����thisָ�룬�����ĳ�Ա�������Ͳ������Ͳ�����һ����


	MemoryPool::~MemoryPool() {
		slot* cur = this->firstBlock_;
		while (cur) {
			slot* next = cur->next;
			operator delete(reinterpret_cast<void*>(cur));
			//cur��slot*���ͣ�Ҫת����void*���Ͳ���ʹ��operator delete
			//delete�ĵײ������operator delete�������ǵĳ�����ֻ��ʹ����ԭʼ��operator delete
			cur = next;
		}
	}


	void* MemoryPool::allocate() {
		std::lock_guard<std::mutex> lock(mutexForBlock_);
		// ͳһ��ס�����������,ȷ�����й�����Դ���ʶ���������
		//ע�������������mutexForFreeList_�����ǵ�mutexForBlock����allocateNewBlock() ���̰߳�ȫ����ֹ����߳�ͬʱ���� allocateNewBlock() �����ظ������ڴ��
		if (freeList_ != nullptr) {
			std::lock_guard<std::mutex> lock(mutexForFreeList_);
			//RAII���mutexForFreeList_��һ������lock_guardʵ����������lock�൱������������ܹܼң�������mutexForFreeList_�Զ���ס&����
			if (freeList_ != nullptr) {
				//�������ڶ��μ���ԭ��������߳���thread A��threadBͬʱͨ����һ�μ�飬���ǻ����ν����ٽ���
				//Ȼ�����thread A�Ȼ�ȡ��������ô�����൱��thread A�Ȳ������ٴμ��freeList_�Ƿ�Ϊ��->ȡ�߿��вۣ���freeList_Ϊ��->�ͷ���
				//�����û������ڶ��μ�飬thread B��ΪҲͨ���˵�һ�μ�飬��ô��A�ͷ���֮��Ҳ���������Ĳ�������ô�ͻ�ȡ����Ч��freeList_
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
				//��������curSlot_�����ƶ�slotSize_��������ָ����������㲢�������ֽ�Ϊ��λ�ģ�������ָ����ָ�����͵Ĵ�СΪ��λ�ġ�
				//���磬�����һ��int*ָ�룬ִ��ptr += 1��ʵ�ʵ�ַ������sizeof(int)�ֽ�
				//�����������Ҫ����sizeof��slot��
			}//�ڴ˴��ͷ����������ͼ����Ӧ��ֻ������ curSlot_ �� lastSlot_ ���޸ģ��������� else �飬����������������ã�Ӱ������
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
		// ���¿���ӵ�����ͷ�����������
		std::lock_guard<std::mutex> lock(mutexForBlock_);
		reinterpret_cast<slot*>(newBlock)->next = firstBlock_;
		firstBlock_ = reinterpret_cast<slot*>(newBlock);

		//ʹ��char*  �Ա�����ֽڼ���ַ����(body+һ����������������������Ҫ���ӵ��ֽ�����
		char* body = reinterpret_cast<char*>(newBlock) + sizeof(slot*);

		//body ָ�����ڴ����������һ�� Slot* ָ����λ�á������ƿ�����Ϊ�����ڴ��ͷ��Ԥ��һ��ָ��ռ䣬������������������ӵ���һ���ڴ�飩
		//		| ------------------ - Block �ڴ沼��------------------ - |
		//		| Next ָ��(Slot*) | ��������� | Slot 0 | Slot 1 | ... |
		//		^                  ^            ^
		//		newBlock           body         curSlot_ ...
		size_t paddingSize = padPointer(body, slotSize_);
		curSlot_ = reinterpret_cast<slot*>(body + paddingSize);

		lastSlot_ = reinterpret_cast<slot*>(reinterpret_cast<size_t>(newBlock) + blockSize_ - slotSize_ + 1);
		//reinterpret_cast<size_t>(newBlock):��ָ�� newBlock ת��Ϊ�������ڴ��ַ����ֵ��ʾ�����Ա������������.�� newBlock �ĵ�ַ�� 0x1000��ת����õ����� 0x1000
		//+ BlockSize_:����ַ�ƶ����ڴ���ĩβ���� BlockSize_ = 4096���� 0x1000 + 4096 = 0x2000�����ڴ�����һ���ֽڵ���һ����ַ��
		//- SlotSize_:����һ���ڴ�۵Ĵ�С����λ�����һ���ڴ�۵���ʼ��ַ���� SlotSize_ = 64���� 0x2000 - 64 = 0x1FC0�������һ���ڴ����ʼ�� 0x1FC0��
		//+1:��ĩβλ������Ϊ���һ�������۵���ʼλ�õ���һ����ȷ����ʹ�� slotSize-1 �ֽڵ���Ƭ�ռ䣬Ҳ�������ڴ��(block)����
		freeList_ = nullptr;
	}

	size_t MemoryPool::padPointer(char* p, size_t align) {
		//align��һ���۴�С��slotͷ+ʵ�ʴ洢���ݿ��ڴ�Ĵ�С֮�ͣ�.����Ҫ����p��align������������������pΪ10000��alignΪ8�����ģ�
		size_t address = reinterpret_cast<size_t>(p);
		if (address % align == 0) {
			return 0;
		}
		else {
			return ((address / align) + 1) * align - address;
		}
	}

	void HashBucket::initMemoryPool() {
		std::call_once(initFlag, []() {//ȷ���̰߳�ȫ����Ҫ�������ͬʱ��ʼ�����ǵ�pools
			mP::HashBucket::pools.reserve(MEMORY_POOL_NUM); // Ԥ�ȷ��� vector �����������⶯̬����ʱ�����ƶ�����
			for (int i = 0; i < MEMORY_POOL_NUM; i++) {
				// ֱ�ӹ��� unique_ptr ���� MemoryPool
				mP::HashBucket::pools.emplace_back(std::make_unique<MemoryPool>(4096, (i + 1) * SLOT_BASE_SIZE));
				//��vector��ʹ��push_back������������ȥԭ�ع���ȥ���Ч��
				//��дpools.emplace_back(MemoryPool(4096, (i+1)*SLOT_BASE_SIZE)) ���������๹����ʱ����ֱ�Ӱ��ڴ��1ʵ����Ҫ�Ĳ�������emplace_back
				//�� HashBucket::pools �ĳ�ʼ���У�ȷ������Ԫ��ͨ�� emplace_back ֱ�ӹ��죬���ǿ������ƶ�
			}
			});
	}

	MemoryPool& HashBucket::getMemoryPool(int index)//��ʼ���������ڴ�ص�λ�ã������Ǽӵ���һ��vector���棬������󷵻���Ҫ���ڴ��
	{

		//һ��ʼ��дstatic MemoryPool memoryPoolsList[MEMORY_POOL_NUM]������Ĵ��뻻��
		//ͨ��static������̬����memoryPoolsList�����������������ͬ,�״ε���ʱ��ʼ������������ֱ�ӷ����Ѵ��ڵ�ʵ��
		//��һ���鷳�ĵ��ǣ� ��̬����ĳ�ʼ���б�����ڱ���ʱ��ȫȷ�������������С��ÿ��Ԫ�صĳ�ʼֵ��
		//����дstatic MemoryPool pools[3] = {MemoryPool(1 * SLOT_BASE_SIZE),MemoryPool(2 * SLOT_BASE_SIZE)} 
		//����ʹ��forѭ�����������Ԫ�أ�forѭ��������ʱ�߼����������ڱ�������ɣ�
		//ΪʲôҪʹ��static�أ�����ϣ�� memoryPoolsList �ڶ�ε����б���״̬���Ƴ� static �ᵼ��ÿ�ε��ö����³�ʼ�����飬Υ������

		return *(mP::HashBucket::pools[index]);
	}

	void* HashBucket::useMemory(size_t size) {
		//��ʹ��hashbucket�ķ����ҵ�һ���ڴ��(һ���ڴ�ؾ���һ��block)����һ��block�ɺܶ������ɡ�useMemory���ʹ��allocate��������Ƕ���ѡ����block������һ����
		if (size <= 0) {
			return nullptr;
		}
		else if (size > MAX_SLOT_SIZE) // ����512�ֽڵ��ڴ棬��ʹ��new
			return operator new(size);
		else {
			return getMemoryPool(((size + 7) / SLOT_BASE_SIZE) - 1).allocate();//// �൱��size / 8 ����ȡ������Ϊ�����ڴ�ֻ�ܴ���С
		}
	}


	void HashBucket::freeMemory(void* ptr, size_t size) {
		//ptrָ��Ҫ�ͷŵ��ڴ���е�ĳ���ڴ�ۣ�������useMemory�����ҷǿ�  ��size�����û���ȷ�ش������ǣ����ڶ�λ���ĸ��ڴ��
		if (!ptr)
			return;
		if (size > MAX_SLOT_SIZE)//�������size��Ӧ���ڴ治�������ǵ��ڴ��֮��
		{
			operator delete(ptr);
			return;
		}
		mP::HashBucket::getMemoryPool(((size + 7) / SLOT_BASE_SIZE) - 1).deallocate(ptr);
	}



}
