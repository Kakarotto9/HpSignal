#include<functional>
#include<memory>
#include<vector>
#include<mutex>
#include<assert.h>
#include<iostream>


template<typename callback>
class SlotImpl;

template<typename callback>
class SignalImpl
{
	public:
		typedef std::vector<std::weak_ptr<SlotImpl<callback>>> SlotList;

		std::shared_ptr<SlotList> sp_slot_list_;
		std::mutex mutex_;
	public:
		SignalImpl() :sp_slot_list_(new SlotList) {
		
		}
		//对SlotList的写操作会调用他，前提已经加锁。
		void copy_on_write()
		{
			if (!sp_slot_list_.unique())
			{
				sp_slot_list_.reset(new SlotList(*sp_slot_list_));

			}
			assert(sp_slot_list_.unique());

		}
		/*删除SlotList中已经失效的weak_ptr<>类型的值和对应传入参数相等的值*/
		void clean(SlotImpl<callback>* sp)   
		{
			std::lock_guard<std::mutex> lock(mutex_);
			copy_on_write();
			SlotList& slot_list_ref(*sp_slot_list_);
			typename SlotList::iterator iter = slot_list_ref.begin();
			while (iter != slot_list_ref.end())
			{
				if (iter->expired()||iter->lock().get()==sp){
					iter = slot_list_ref.erase(iter);
				}
				++iter;
			}
		}
		/*删除SlotList中已经失效的weak_ptr<>类型的值*/
		void clean()
		{
			std::lock_guard<std::mutex> lock(mutex_);
			copy_on_write();
			SlotList& slot_list_ref(*sp_slot_list_);
			typename SlotList::iterator iter = slot_list_ref.begin();
			while (iter != slot_list_ref.end())
			{
				if (iter->expired())
					iter = slot_list_ref.erase(iter);
				else
					iter++;
			}
		}
};
template<typename callback>
class SlotImpl
{
	public:
		std::weak_ptr<SignalImpl<callback>> signal_;
		callback cb_;
		std::weak_ptr<void> tie_;
		bool tied_;
	public:
		SlotImpl(const std::shared_ptr<SignalImpl<callback>>& signal_args_, callback&& cb_arg_)
			:signal_(signal_args_), cb_(cb_arg_), tie_(), tied_(false)
		{

		}
		SlotImpl(const std::shared_ptr<SignalImpl<callback>>& signal_args_, callback&& cb_arg_, const std::shared_ptr<void> tie_arg)
			:signal_(signal_args_), cb_(cb_arg_), tie_(tie_arg), tied_(true)
		{

		}
		~SlotImpl()
		{
			std::shared_ptr<SignalImpl<callback>> sp_signal_impl_(signal_.lock());
			if (sp_signal_impl_)
				sp_signal_impl_->clean(this);
		}

};

typedef std::shared_ptr<void> Slot;


template<typename Signature>
class Signal;

template<typename RET, typename... ARGS>
class Signal<RET(ARGS...)>
{
	public:
		typedef std::function<void(ARGS...)> callback;
		/* 这样写就会报错，必须加上：：才不出错，
		   typedef SignalImpl<callback> SignalImpl;
		   typedef SlotImpl<callback> SlotImpl;
		*/
		typedef ::SignalImpl<callback> SignalImpl;
		typedef ::SlotImpl<callback> SlotImpl;
	private:
		std::shared_ptr<SignalImpl> sp_signal_impl_;

		void add(const std::shared_ptr<SlotImpl>& slot)//写操作
		{
			SignalImpl&  signal_impl_ref_(*sp_signal_impl_);
			{
				std::lock_guard<std::mutex> lock(signal_impl_ref_.mutex_);
				signal_impl_ref_.copy_on_write();
				signal_impl_ref_.sp_slot_list_->push_back(slot);
			}
		}
	public:
		Signal() :sp_signal_impl_(new SignalImpl)
		{

		}		

		~Signal() {

		}		

		Slot connect(callback&& cb_)
		{
			std::shared_ptr<SlotImpl> ret(new SlotImpl(sp_signal_impl_, std::forward<callback>(cb_)));
			add(ret);
			return ret;
		}
		Slot connect(callback&& cb_, const std::shared_ptr<void>& tie)
		{
			std::shared_ptr<SlotImpl> ret(new SlotImpl(sp_signal_impl_, std::forward<callback>(cb_), tie));
			add(ret);
			return ret;

		}
		void call(ARGS... args)
		{
			SignalImpl&  signal_impl_ref_(*sp_signal_impl_);
			std::shared_ptr<typename SignalImpl::SlotList> sp_slot_list_;//让引用计数加1，另外线程对siganl_impl的slot_list进行写操作时就需要复制一份新的操作
			{
				std::lock_guard<std::mutex> lock(signal_impl_ref_.mutex_);
				sp_slot_list_ = signal_impl_ref_.sp_slot_list_;
			}

			typename SignalImpl::SlotList&  slot_list_ref_(*sp_slot_list_);//这时就可以大胆的取 槽数组 了
			for (typename SignalImpl::SlotList::iterator iter = slot_list_ref_.begin(); iter != slot_list_ref_.end(); iter++)
			{
				std::shared_ptr<SlotImpl> sp_slot_impl_= iter->lock();
				if (sp_slot_impl_)//感知生命，对应操作
				{
					std::shared_ptr<void> guard;
					if (sp_slot_impl_->tied_)
					{
						guard = sp_slot_impl_->tie_.lock();
						if (guard)
							sp_slot_impl_->cb_(args...);

					}
					else
						sp_slot_impl_->cb_(args...);
				}

			}

		}

};
