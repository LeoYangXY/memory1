#pragma once
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
namespace mP {//�����Ӱ�����Ķ���������һ�������ռ�����


#define SLOT_BASE_SIZE 8//������С���䵥λ�������ڴ�صĲ۴�С�� 8 �ı�����
#define MEMORY_POOL_NUM 64//�����ڴ�طּ�������֧�ִ� 8B �� 512B��8��64���ķ���
#define MAX_SLOT_SIZE 512//�����ڴ�����ޣ����� 512B �Ķ�����ڴ������������ʹ��ϵͳ����

	struct slot {//һ���ڴ����һ��slotͷ��������ڲ�ͬ�ڴ�۵Ļ������ӣ�ʹ��slotͷ���л������ӣ���Ȼ�������洢���ݵĲ��ֲ���slotͷ
		slot* next;
	};

	class MemoryPool {
	public:
		// ���ÿ������캯���Ϳ�����ֵ
		MemoryPool(const MemoryPool&) = delete;
		MemoryPool& operator=(const MemoryPool&) = delete;

		// �����ƶ����캯�����ƶ���ֵ
		MemoryPool(MemoryPool&&) = delete;
		MemoryPool& operator=(MemoryPool&&) = delete;
	public:
		MemoryPool(size_t blockSize, size_t slotSize);//���캯�����ڷ��������ڴ��Ĵ�С��Ĭ��4096��+ȷ��ÿ�����״�Ŷ��Ķ���
		~MemoryPool();
		void* allocate();//����һ��ָ����������ָ��.  ����ֵ��Ϊ��void*����ô�������Ͳ���
		void deallocate(void*);//��ָ�ŵĿռ����������� void*������һ��ָ�룬�����Ϳ�������.�����������������Բ�д������β�����ֻдһ���β�����
	private:
		void allocateNewBlock();//�ڴ�صĺ���Ŀ�Ĳ�����ȫ����ϵͳ���ã�����ͨ����ǰԤ���ռ����ϵͳ���á�allocateNewBlock�Ǹ��ڴ�����ݵĲ���
		size_t padPointer(char* p, size_t align);//��p�����ַ��ʼ������Ҫ���Ƕ��뵽�۴�С�ı���λ��(align�ǲ۴�С��������������������Ҫ���ֽ���
	private:
		//��Ա�������� _ ��׺�����⵽ʱ���뺯�����������ظ��������鷳�������������β���������Ȼ
		//block�����кܶ�С�ۣ�ÿ��С�۰���һ��slotͷ��ȥָ����һ��С�ۣ��Լ�ʵ�ʴ洢���ݵ��ڴ�����
		//block��ͷ��Ҳ��һ��slotͷ���������ӱ��block
		int blockSize_;
		int slotSize_;//һ��slot�����������ܴ洢���ݲ��ֵĿռ��С
		slot* firstBlock_;//һ��ָ�룬ָ������˺ܶ��С�۵Ĵ���ڴ��
		slot* curSlot_;//ָ��ǰ��δ��ʹ�õĲ�
		slot* freeList_;//ָ����������ͷ�ڵ��ָ��
		slot* lastSlot_;
		std::mutex mutexForFreeList_;//һ���������ڱ�����������freeList�����̰߳�ȫ�����������ʵ��ǰ�����ڱ�֤ pushFreeList/popFreeList ��ԭ����
		std::mutex mutexForBlock_;//���� allocateNewBlock() ���̰߳�ȫ����ֹ����߳�ͬʱ���� allocateNewBlock() �����ظ������ڴ��
	};

	class HashBucket {

	public:
		static void initMemoryPool();
		//��̬���������౾�����ʵ�������ڳ�ʼ������ HashBucket ���������Դ.���� HashBucketʵ������ͬһ���ڴ�أ��� 64 ����ͬ�۴�С�� MemoryPool���������ظ���ʼ�� 
		static MemoryPool& getMemoryPool(int index);
		//�ڴ��ȫ��Ψһ�ԣ�HashBucket �������� Ԥ�ȶ���Ĺ̶�����ڴ�����飨�� 64 ����ͬ�۴�С�� MemoryPool������Щ�ڴ���豻���д��빲������ÿ������ʵ������ӵ�С�
		//����������Ϊstatic����ȥ�ҵ��ڴ�أ�useMemory�Ž���ʵ�ʷ��䣩

		static void* useMemory(size_t size);//������ȥ�����ڴ�
		static void freeMemory(void* ptr, size_t size);//�ͷ�ʱ��Ҫ���� size ����ȷ�����ڴ�������ĸ��ӳ�



		//newElement/deleteElement �� �ڴ�����ߺ��������� HashBucket �ĺ��Ĺ��ܡ�
		//���� operator new/delete ��ȫ����ƣ������û�ֱ�ӵ��� newElement<MyClass>()������ͨ�� HashBucket ����
		//������ǰ����Ƕ�����������棬����Ҫ����Ϊ��Ԫ�����ܷ���������ĺ���
		template<typename T, typename... Args>
		friend T* newElement(Args&&... args);
		template<typename T>
		friend void deleteElement(T* p);
	public:
		static std::vector<std::unique_ptr<MemoryPool>> pools;// ��̬��Ա����,ȷ��ȫ��ֻ��ʼ��һ�Ρ�pools��һ��vecore�����汣���Ŷ��ָ�룬ÿ��ָ��ָ��һ���ڴ��
		static std::once_flag initFlag;        // ��̬��Ա����

	};

	//ģ�庯�����뽫�������ͷ�ļ��У����ܷ��뵽.cpp��
	template<typename T, typename ...Args>//typename T��Ҫ�����Ķ�������  typename... Args���ɱ����ģ�壬��ʾ����T�Ĺ��캯�������������б�
	T* newElement(Args&&... args) {//&& ��ʾͨ�����ã�֧����ֵ����ֵ����   ... ��ʾ������չ�������ڴ����������������͵Ĳ�����
		T* p = nullptr;
		if ((p = reinterpret_cast<T*>(HashBucket::useMemory(sizeof(T)))) != nullptr) {
			new(p) T(std::forward<Args>(args)...);
			// ���� new���� A* a = new A();��  ϵͳ�Զ������ַ�������Զ����������٣�����placement new�� A* a = new (ptr) A();����������� Ԥ�ȷ�����ڴ��ַ��ptr���ϵ��ù��캯�����������ڴ����
			// new(p) T(...)��Placement New �﷨�����Ѿ�������ڴ��ַp���湹�����
			// ��placement new�����ֶ�������������������delete��������ȷ���ڴ����ȷ�ͷš����ɣ�placement newֻ�������е��ڴ�λ���Ϲ���������������ڴ�ķ���
			// std::forward<Args>(args)...������ת����������������ֵ / ��ֵ���ԣ�ȷ��������ȷ�Ĺ��캯������
		}
		return p;
	}

	template<typename T>
	void deleteElement(T* p) {
		if (p) {
			p->~T();//��������
			HashBucket::freeMemory(reinterpret_cast<void*>(p), sizeof(T));//�ͷſռ�
		}
	}

}