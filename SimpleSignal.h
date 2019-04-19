#pragma once

#include <memory>
#include <functional>
#include <list>
#include <vector>
#include <algorithm>


namespace Lib {

template<typename,typename> class ProtoSignal;   

template<typename,typename> struct CollectorInvocation;

template<typename Result>
struct CollectorLast {
  using CollectorResult = Result;
  explicit        CollectorLast ()              : last_() {}
  inline bool     operator()    (Result r)      { last_ = r; return true; }
  CollectorResult result        ()              { return last_; }
private:
  Result last_;
};

template<typename Result>
struct CollectorDefault : CollectorLast<Result>
{};

template<>
struct CollectorDefault<void> {
  using CollectorResult = void;
  void                  result     ()           {}
  inline bool           operator() (void)       { return true; }
};

template<class Collector, class R, class... Args>
struct CollectorInvocation<Collector, R (Args...)> {
  inline bool
  invoke (Collector &collector, const std::function<R (Args...)> &cbf, Args... args) const
  {
    return collector (cbf (args...));
  }
};

template<class Collector, class... Args>
struct CollectorInvocation<Collector, void (Args...)> {
  inline bool
  invoke (Collector &collector, const std::function<void (Args...)> &cbf, Args... args) const
  {
    cbf (args...); return collector();
  }
};

template<class Collector, class R, class... Args>
class ProtoSignal<R (Args...), Collector> : private CollectorInvocation<Collector, R (Args...)> {
protected:
  using CbFunction = std::function<R (Args...)>;
  using Result = typename CbFunction::result_type;
  using CollectorResult = typename Collector::CollectorResult;

private:
  ProtoSignal (const ProtoSignal&) = delete;
  ProtoSignal&  operator=   (const ProtoSignal&) = delete;

  using CallbackSlot = std::shared_ptr<CbFunction>;
  using CallbackList = std::list<CallbackSlot>;
  CallbackList callback_list_;

  size_t add_cb(const CbFunction& cb)
  {
    callback_list_.emplace_back(std::make_shared<CbFunction>(cb));
    return size_t (callback_list_.back().get());
  }

  bool remove_cb(size_t id)
  {
    auto it =std::remove_if(begin(callback_list_), end(callback_list_),
                            [id](const CallbackSlot& slot) { return size_t(slot.get()) == id; });
    bool const removed = it != end(callback_list_);
    callback_list_.erase(it, end(callback_list_));
    return removed;
  }

public:
  ProtoSignal (const CbFunction &method)
  {
    if (method)
      add_cb(method);
  }
  ~ProtoSignal ()
  {
  }

  size_t connect (const CbFunction &cb)      { return add_cb(cb); }
  bool   disconnect (size_t connection)         { return remove_cb(connection); }

  CollectorResult
  call (Args... args) const
  {
    Collector collector;
    for (auto &slot : callback_list_) {
        if (slot) {
            const bool continue_emission = this->invoke (collector, *slot, args...);
            if (!continue_emission)
              break;
        }
    }
    return collector.result();
  }
  std::size_t
  size ()
  {
    return callback_list_.size();
  }
};

} // Lib

template <typename SignalSignature, class Collector = Lib::CollectorDefault<typename std::function<SignalSignature>::result_type> >
struct Signal /*final*/ :
    Lib::ProtoSignal<SignalSignature, Collector>
{
  using ProtoSignal = Lib::ProtoSignal<SignalSignature, Collector>;
  using CbFunction = typename ProtoSignal::CbFunction;
  Signal (const CbFunction &method = CbFunction()) : ProtoSignal (method) {}
};

template<class Instance, class Class, class R, class... Args> std::function<R (Args...)>
slot (Instance &object, R (Class::*method) (Args...))
{
  return [&object, method] (Args... args) { return (object .* method) (args...); };
}

template<class Class, class R, class... Args> std::function<R (Args...)>
slot (Class *object, R (Class::*method) (Args...))
{
  return [object, method] (Args... args) { return (object ->* method) (args...); };
}

template<typename Result>
struct CollectorUntil0 {
  using CollectorResult = Result;
  explicit                      CollectorUntil0 ()      : result_() {}
  const CollectorResult&        result          ()      { return result_; }
  inline bool
  operator() (Result r)
  {
    result_ = r;
    return result_ ? true : false;
  }
private:
  CollectorResult result_;
};

template<typename Result>
struct CollectorWhile0 {
  using CollectorResult = Result;
  explicit                      CollectorWhile0 ()      : result_() {}
  const CollectorResult&        result          ()      { return result_; }
  inline bool
  operator() (Result r)
  {
    result_ = r;
    return result_ ? false : true;
  }
private:
  CollectorResult result_;
};

template<typename Result>
struct CollectorVector {
  using CollectorResult = std::vector<Result>;
  const CollectorResult&        result ()       { return result_; }
  inline bool
  operator() (Result r)
  {
    result_.push_back (r);
    return true;
  }
private:
  CollectorResult result_;
};

