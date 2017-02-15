#pragma once
#include <thread>
#include <vector>
#define __WITH_ACTIVITY__

/**
 * @class activity
 * @author Andrey Bezborodov
 * @brief Implement activity object
 */
template <class T>
class activity {
public:
  //! Function callback
  class callback{
  public:
    virtual ~callback(){}
    virtual void operator()() = 0;
  };

  //! Callback adapter
  template< typename OwnerClass >
  class adapter : public callback{
  public:
    //! Exported function prototype
    typedef void (OwnerClass::*function)();
  private:
    //! Pointer to owner class
    OwnerClass* _class_ptr;

    //! Pointer to function with argument
    function    _callback;
  public:
    //! Constructor
    adapter(OwnerClass * class_ptr, typename adapter::function func)
    : _class_ptr(class_ptr)
    , _callback(func)
    {}
    //! Call function
    virtual void operator()(){
      (_class_ptr->*_callback)();
    }
  };

public:
  //! Constructor
  activity(T* class_ptr, typename adapter<T>::function func)
  : _IsRunning(false)
  , _Callback(new adapter<T>(class_ptr, func))
  {}

  //! Start activity
  void start() {
    _IsRunning = true;
    pthread_create(&_Thread, NULL, &activity::__run__, this);
  }

  //! Stop activity
  void stop() {
    _IsRunning = false;
    pthread_cancel(_Thread);
    join(nullptr);
  }

  //! Check thread for cancelation
  void __cancel_point__() {
    pthread_testcancel();
  }

  //! Wait for activity end
  int join(void** retval) {
    return pthread_join(_Thread, retval);
  }

  //! Check is thread in running state
  bool running() const {
    return _IsRunning;
  }

  //! Yield processor time
  void yield() const {
    pthread_yield();
  }

  //! Set thread affinity
  bool setAffinity(std::uint16_t core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    if (pthread_setaffinity_np(_Thread, sizeof(cpu_set_t), &cpuset)) return false;
    else return true;
  }

  //! Set affinity to multiple cores
  bool setAffinity(std::vector<std::uint16_t> core_ids) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (auto core_id : core_ids) CPU_SET(core_id, &cpuset);
    if (pthread_setaffinity_np(_Thread, sizeof(cpu_set_t), &cpuset)) return false;
    else return true;
  }

  //! Add thread affinity
  bool addAffinity(std::uint16_t core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);

    // - Get current affinity
    if (pthread_getaffinity_np(_Thread, sizeof(cpu_set_t), &cpuset)) return false;

    // - Add core
    CPU_SET(core_id, &cpuset);

    // - Set new affinity
    if (pthread_setaffinity_np(_Thread, sizeof(cpu_set_t), &cpuset)) return false;
    else return true;
  }

private:

  //! Thread entry adapter
  static void* __run__(void* args) {
    activity* item = reinterpret_cast<activity*>(args);
    (*item->_Callback)();
    item->_IsRunning = false;
    return nullptr;
  }

private:
  //! Thread object
  pthread_t                 _Thread;

  //! Is running
  bool                      _IsRunning;

  //! Pointer to callback function
  std::shared_ptr<callback> _Callback;
};

/**
 * @class runnable
 * @author Andrey Bezborodov
 * @brief Implement runnable class
 */
class runnable {
public:
  //! Constructor
  runnable()
  : _task(this, &runnable::run)
  {}

  //! Destructor
  virtual ~runnable() {}

  //! Run class
  virtual void run() = 0;

  //! Start
  void start() {
    _task.start();
  }

  //! Stop
  void stop() {
    _task.stop();
  }

  //! Running
  bool running() {
    return _task.running();
  }

  //! Access task object
  activity<runnable>& task() {
    return _task;
  }
private:
  activity<runnable>  _task;
};

