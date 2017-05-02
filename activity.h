#pragma once
#include <thread>
#include <vector>
#include <set>
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
  bool start(std::vector<std::uint16_t> core_ids = std::vector<std::uint16_t>()) {
    _IsRunning = true;

    // - Check is need to set affinity
    if (!core_ids.empty()) {
      // - Init thread attributes
      pthread_attr_t  attr;
      if (pthread_attr_init(&attr)) return false;

      // - Set affinity
      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);
      for (auto core_id : core_ids) {
        CPU_SET(core_id, &cpuset);
        _core_ids.insert(core_id);
      }
      if (pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset)) return false;

      //pthread_attr_setschedpolicy(&attr, SCHED_RR);

      // - Start thread
      if (pthread_create(&_Thread, &attr, &activity::__run__, this)){
        pthread_attr_destroy(&attr);
        return false;
      } else {
        pthread_attr_destroy(&attr);
      }
    } else {
      if (pthread_create(&_Thread, nullptr, &activity::__run__, this)) return false;
    }
    return true;
  }

  bool start(std::uint16_t core_id) {
    _IsRunning = true;

    // - Init thread attributes
    pthread_attr_t  attr;
    if (pthread_attr_init(&attr)) return false;

    // - Set affinity
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    _core_ids.insert(core_id);

    if (pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset)) return false;

    // - Start thread
    if (pthread_create(&_Thread, &attr, &activity::__run__, this)){
      pthread_attr_destroy(&attr);
      return false;
    } else {
      pthread_attr_destroy(&attr);
    }
    return true;
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
    else {
      _core_ids.clear();
      _core_ids.insert(core_id);
      return true;
    }
  }

  //! Set affinity to multiple cores
  bool setAffinity(std::vector<std::uint16_t> core_ids) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (auto core_id : core_ids) CPU_SET(core_id, &cpuset);
    if (pthread_setaffinity_np(_Thread, sizeof(cpu_set_t), &cpuset)) return false;
    else {
      for (auto core_id : core_ids) _core_ids.insert(core_id);
      return true;
    }
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
    else {
      _core_ids.insert(core_id);
      return true;
    }
  }

  //! Get affinity information
  std::vector<std::uint16_t> getAffinity(std::uint16_t max_cpu = 128) {
    std::vector<std::uint16_t> result;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    if (pthread_getaffinity_np(_Thread, sizeof(cpu_set_t), &cpuset)) return result;
    // - Check CPU
    _core_ids.clear();
    for (std::uint16_t cpu_id = 0; cpu_id < max_cpu; cpu_id++) {
      if (!CPU_ISSET(cpu_id, &cpuset)) continue;
      _core_ids.insert(cpu_id);
      result.push_back(cpu_id);
    }
    return result;
  }

  //! Set thread priority
  bool setPriority(int priority) {
    //sched_param     param;
    //param.__sched_priority = priority;
    //return (pthread_setschedparam(_Thread, SCHED_RR, &param) == 0);
    return true;
  }

private:

  //! Thread entry adapter
  static void* __run__(void* args) {
    activity* item = reinterpret_cast<activity*>(args);
    (*item->_Callback)();
    if (item->_IsRunning) item->stop();
    item->__cancel_point__();
    return nullptr;
  }

private:
  //! Thread object
  pthread_t                 _Thread;

  //! Is running
  bool                      _IsRunning;

  //! Pointer to callback function
  std::shared_ptr<callback> _Callback;

  //! Affinity
  std::set<std::uint16_t>   _core_ids;
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
  virtual void start() {
    _task.start();
  }

  //! Stop
  virtual void stop() {
    _task.stop();
  }

  //! Running
  bool running() {
    return _task.running();
  }

  //! Check cancel point
  void __cancel_point__() {
    task().__cancel_point__();
  }

  //! Access task object
  activity<runnable>& task() {
    return _task;
  }
private:
  activity<runnable>  _task;
};

